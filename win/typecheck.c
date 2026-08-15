#include "mira.h"

Def *mira_find_signature(Program *program, const char *name, size_t name_len) {
	if (!program || !name) return NULL;
	for (Def *def = program->defs; def; def = def->next) {
		if (def->name_len == name_len && memcmp(def->name, name, name_len) == 0)
			return def;
	}

	/* Full module-qualified names are authoritative.  The fallback accepts
	 * the existing lowered C-symbol spelling used after '-' becomes '_'. */
	for (Def *def = program->defs; def; def = def->next) {
		if (def->name_len != name_len) continue;
		bool matches = true;
		for (size_t i = 0; i < name_len; ++i) {
			char declared = def->name[i] == '-' ? '_' : def->name[i];
			char resolved = name[i] == '-' ? '_' : name[i];
			if (declared != resolved) { matches = false; break; }
		}
		if (matches) return def;
	}
	return NULL;
}

typedef struct {
	MiraType type;
	unsigned type_mask;
	bool strict;
	int line;
	int col;
	Def *void_source;
	const char *source;
	const char *source_filename;
} MiraCheckedValue;

typedef struct {
	size_t open;
	size_t close;
	int argc;
} MiraParenBoundary;

typedef struct MiraSourceScan {
	const char *source;
	MiraParenBoundary *boundaries;
	size_t boundary_count;
	size_t boundary_cap;
	size_t *line_starts;
	size_t line_count;
	size_t line_cap;
	struct MiraSourceScan *next;
} MiraSourceScan;

typedef struct {
	Compiler *compiler;
	Program *program;
	Def *def;
	MiraType return_type;
	bool return_type_explicit;
	const char *function_name;
	size_t function_name_len;
	MiraCheckedValue *values;
	int value_count;
	int value_cap;
	bool reachable;
	MiraSourceScan **source_scans;
} MiraTypeChecker;

static bool word_is(const IrNode *node, const char *word) {
	return node->kind == IR_WORD && node->u.word.len == strlen(word) &&
		memcmp(node->u.word.name, word, node->u.word.len) == 0;
}

static void checker_push(MiraTypeChecker *checker, MiraType type, bool strict,
	const IrNode *origin) {
	if (checker->value_count == checker->value_cap) {
		int next_cap = checker->value_cap ? checker->value_cap * 2 : 16;
		MiraCheckedValue *next = realloc(checker->values,
			(size_t)next_cap * sizeof(*next));
		if (!next) mira_error_simple(1, "out of memory in type checker");
		checker->values = next;
		checker->value_cap = next_cap;
	}
	unsigned type_mask = strict && type != MIRA_TYPE_UNKNOWN ? 1u << type : 0;
	checker->values[checker->value_count++] =
		(MiraCheckedValue){
			type, type_mask, strict,
			origin ? origin->line : 1, origin ? origin->col : 1, NULL,
			origin ? origin->source : NULL,
			origin ? origin->source_filename : NULL
		};
}

static void checker_push_void(MiraTypeChecker *checker, Def *callee,
	const IrNode *origin) {
	checker_push(checker, MIRA_TYPE_VOID, true, origin);
	checker->values[checker->value_count - 1].void_source = callee;
}

static MiraCheckedValue checker_pop(MiraTypeChecker *checker) {
	if (checker->value_count == 0)
		return (MiraCheckedValue){MIRA_TYPE_UNKNOWN, 0, false, 1, 1, NULL, NULL, NULL};
	return checker->values[--checker->value_count];
}

static bool checker_value_has_type(MiraCheckedValue value, MiraType type) {
	return (value.type_mask & (1u << type)) != 0;
}

static MiraType checker_first_mismatch(MiraCheckedValue value, MiraType expected) {
	for (MiraType type = MIRA_TYPE_I64; type <= MIRA_TYPE_VOID; ++type)
		if (type != expected && checker_value_has_type(value, type)) return type;
	return MIRA_TYPE_UNKNOWN;
}

static MiraSourceInfo *checker_def_source(Program *program, const Def *def) {
	if (!def || !def->name) return NULL;
	uintptr_t name = (uintptr_t)def->name;
	for (MiraSourceInfo *info = program->source_infos; info; info = info->next) {
		uintptr_t start = (uintptr_t)info->source;
		if (name >= start && name < start + info->source_len) return info;
	}
	const char *member = def->name;
	size_t member_len = def->name_len;
	for (size_t i = 0; i < def->name_len; ++i) {
		if (def->name[i] != '.') continue;
		member = def->name + i + 1;
		member_len = def->name_len - i - 1;
	}
	size_t module_len = (size_t)(member - def->name);
	if (module_len > 0) module_len--;
	for (MiraSourceInfo *info = program->source_infos; info; info = info->next)
		if (module_len > 0 && info->module_path_len == module_len &&
		    memcmp(info->module_path, def->name, module_len) == 0)
			return info;
	for (MiraSourceInfo *info = program->source_infos; info; info = info->next) {
		const char *line = info->source;
		for (int current = 1; current < def->line && *line; ++current) {
			const char *newline = strchr(line, '\n');
			if (!newline) { line += strlen(line); break; }
			line = newline + 1;
		}
		const char *end = strchr(line, '\n');
		if (!end) end = line + strlen(line);
		const char *cursor = line;
		while (cursor < end && isspace((unsigned char)*cursor)) cursor++;
		if ((size_t)(end - cursor) >= 6 && memcmp(cursor, "extern", 6) == 0 &&
		    isspace((unsigned char)cursor[6])) {
			cursor += 6;
			while (cursor < end && isspace((unsigned char)*cursor)) cursor++;
		}
		if ((size_t)(end - cursor) < 2 || memcmp(cursor, "fn", 2) != 0 ||
		    (cursor + 2 < end && !isspace((unsigned char)cursor[2])))
			continue;
		cursor += 2;
		while (cursor < end && isspace((unsigned char)*cursor)) cursor++;
		if ((size_t)(end - cursor) >= member_len &&
		    memcmp(cursor, member, member_len) == 0 &&
		    (cursor + member_len == end ||
		     !(isalnum((unsigned char)cursor[member_len]) || cursor[member_len] == '_')))
			return info;
	}
	return NULL;
}

static bool signature_is_typed(const Def *def) {
	if (def->return_type_explicit) return true;
	for (int i = 0; i < def->param_count; ++i)
		if (def->param_type_explicit && def->param_type_explicit[i]) return true;
	return false;
}

typedef struct {
	size_t open;
	int commas;
	int bracket_depth;
	int brace_depth;
	bool has_token;
} MiraParenFrame;

static void checker_scan_add_line(MiraSourceScan *scan, size_t offset) {
	if (scan->line_count == scan->line_cap) {
		size_t cap = scan->line_cap ? scan->line_cap * 2 : 32;
		size_t *next = realloc(scan->line_starts, cap * sizeof(*next));
		if (!next) mira_error_simple(1, "out of memory in type checker");
		scan->line_starts = next;
		scan->line_cap = cap;
	}
	scan->line_starts[scan->line_count++] = offset;
}

static void checker_scan_add_boundary(MiraSourceScan *scan,
	MiraParenFrame frame, size_t close) {
	if (scan->boundary_count == scan->boundary_cap) {
		size_t cap = scan->boundary_cap ? scan->boundary_cap * 2 : 32;
		MiraParenBoundary *next = realloc(scan->boundaries, cap * sizeof(*next));
		if (!next) mira_error_simple(1, "out of memory in type checker");
		scan->boundaries = next;
		scan->boundary_cap = cap;
	}
	scan->boundaries[scan->boundary_count++] = (MiraParenBoundary){
		frame.open, close, frame.has_token ? frame.commas + 1 : 0
	};
}

static MiraSourceScan *checker_build_source_scan(const char *source) {
	MiraSourceScan *scan = calloc(1, sizeof(*scan));
	if (!scan) mira_error_simple(1, "out of memory in type checker");
	scan->source = source;
	checker_scan_add_line(scan, 0);
	MiraParenFrame *frames = NULL;
	size_t frame_count = 0, frame_cap = 0;
	int bracket_depth = 0, brace_depth = 0;
	bool in_string = false, escaped = false, in_comment = false;
	for (size_t i = 0; source[i]; ++i) {
		char ch = source[i];
		if (ch == '\n') checker_scan_add_line(scan, i + 1);
		if (in_comment) {
			if (ch == '\n') in_comment = false;
			continue;
		}
		if (in_string) {
			if (escaped) escaped = false;
			else if (ch == '\\') escaped = true;
			else if (ch == '"') in_string = false;
			continue;
		}
		if (ch == '#') { in_comment = true; continue; }
		if (ch == '"') {
			in_string = true;
			if (frame_count) frames[frame_count - 1].has_token = true;
			continue;
		}
		if (ch == '(') {
			if (frame_count) frames[frame_count - 1].has_token = true;
			if (frame_count == frame_cap) {
				size_t cap = frame_cap ? frame_cap * 2 : 16;
				MiraParenFrame *next = realloc(frames, cap * sizeof(*next));
				if (!next) mira_error_simple(1, "out of memory in type checker");
				frames = next;
				frame_cap = cap;
			}
			frames[frame_count++] = (MiraParenFrame){
				i, 0, bracket_depth, brace_depth, false
			};
			continue;
		}
		if (ch == ')') {
			if (frame_count) {
				MiraParenFrame frame = frames[--frame_count];
				checker_scan_add_boundary(scan, frame, i);
				if (frame_count) frames[frame_count - 1].has_token = true;
			}
			continue;
		}
		if (ch == '[') bracket_depth++;
		else if (ch == ']' && bracket_depth > 0) bracket_depth--;
		else if (ch == '{') brace_depth++;
		else if (ch == '}' && brace_depth > 0) brace_depth--;
		if (!frame_count) continue;
		MiraParenFrame *frame = &frames[frame_count - 1];
		if (ch == ',' && bracket_depth == frame->bracket_depth &&
		    brace_depth == frame->brace_depth)
			frame->commas++;
		else if (!isspace((unsigned char)ch))
			frame->has_token = true;
	}
	free(frames);
	return scan;
}

static MiraSourceScan *checker_source_scan(MiraTypeChecker *checker,
	const char *source) {
	for (MiraSourceScan *scan = *checker->source_scans; scan; scan = scan->next)
		if (scan->source == source) return scan;
	MiraSourceScan *scan = checker_build_source_scan(source);
	scan->next = *checker->source_scans;
	*checker->source_scans = scan;
	return scan;
}

static MiraParenBoundary *checker_find_boundary(MiraSourceScan *scan,
	size_t close) {
	size_t low = 0, high = scan->boundary_count;
	while (low < high) {
		size_t mid = low + (high - low) / 2;
		if (scan->boundaries[mid].close < close) low = mid + 1;
		else high = mid;
	}
	if (low < scan->boundary_count && scan->boundaries[low].close == close)
		return &scan->boundaries[low];
	return NULL;
}

static void checker_source_position(MiraSourceScan *scan, size_t offset,
	int *line, int *col) {
	size_t low = 0, high = scan->line_count;
	while (low + 1 < high) {
		size_t mid = low + (high - low) / 2;
		if (scan->line_starts[mid] <= offset) low = mid;
		else high = mid;
	}
	*line = (int)low + 1;
	*col = (int)(offset - scan->line_starts[low]) + 1;
}

static bool checker_annotate_parenthesized_call(MiraTypeChecker *checker,
	IrNode *node) {
	if (!node || node->kind != IR_WORD || node->u.word.has_call_arity ||
	    !node->source || node->source_offset == 0)
		return false;
	const char *source = node->source;
	MiraSourceScan *scan = checker_source_scan(checker, source);
	bool use_current_close = false;
retry_close_candidate:;
	size_t cursor = node->source_offset + (use_current_close ? 1u : 0u);
	/* Shunting-yard eval() creates the call node while its ')' token is still
	 * current; recursive modern calls create it immediately after consuming
	 * ')'.  Try both close-token positions and validate the source callee. */
	while (cursor > 0 && isspace((unsigned char)source[cursor - 1])) cursor--;
	if (cursor == 0 || source[cursor - 1] != ')') {
		if (!use_current_close && source[node->source_offset] == ')') {
			use_current_close = true;
			goto retry_close_candidate;
		}
		return false;
	}

	size_t close = cursor - 1;
	MiraParenBoundary *boundary = checker_find_boundary(scan, close);
	if (!boundary) return false;
	size_t open = boundary->open;

	size_t name_end = open;
	while (name_end > 0 && isspace((unsigned char)source[name_end - 1])) name_end--;
	size_t name_start = name_end;
	while (name_start > 0) {
		unsigned char ch = (unsigned char)source[name_start - 1];
		if (!(isalnum(ch) || ch == '_' || ch == '-' || ch == '.')) break;
		name_start--;
	}
	if (name_start == name_end) return false;
	const char *source_member = source + name_start;
	size_t source_member_len = name_end - name_start;
	const char *source_dot = NULL;
	for (size_t i = 0; i < source_member_len; ++i)
		if (source_member[i] == '.') source_dot = source_member + i;
	if (source_dot) {
		source_member_len -= (size_t)(source_dot + 1 - source_member);
		source_member = source_dot + 1;
	}
	const char *node_member = node->u.word.name;
	size_t node_member_len = node->u.word.len;
	const char *node_dot = NULL;
	for (size_t i = 0; i < node_member_len; ++i)
		if (node_member[i] == '.') node_dot = node_member + i;
	if (node_dot) {
		node_member_len -= (size_t)(node_dot + 1 - node_member);
		node_member = node_dot + 1;
	}
	bool same_member = source_member_len == node_member_len;
	for (size_t i = 0; same_member && i < source_member_len; ++i) {
		char source_ch = source_member[i] == '-' ? '_' : source_member[i];
		char node_ch = node_member[i] == '-' ? '_' : node_member[i];
		if (source_ch != node_ch) same_member = false;
	}
	if (!same_member) {
		if (!use_current_close && source[node->source_offset] == ')') {
			use_current_close = true;
			goto retry_close_candidate;
		}
		return false;
	}

	int line = 1, col = 1;
	checker_source_position(scan, name_start, &line, &col);
	node->u.word.has_call_arity = 1;
	node->u.word.call_argc = boundary->argc;
	node->line = line;
	node->col = col;
	return true;
}

static void checker_void_value_error(MiraTypeChecker *checker,
	MiraCheckedValue value) {
	Def *def = value.void_source;
	mira_error(value.source ? value.source : checker->compiler->src,
		value.source_filename ? value.source_filename : checker->compiler->filename,
		value.line, value.col, 1,
		"function '%.*s' returns void and cannot be used as a value",
		def ? (int)def->name_len : 4, def ? def->name : "call");
}

static void checker_result_error(MiraTypeChecker *checker, MiraCheckedValue value) {
	mira_error(value.source ? value.source : checker->compiler->src,
		value.source_filename ? value.source_filename : checker->compiler->filename,
		value.line, value.col, 1, "function '%.*s': expected %s, got %s",
		(int)checker->function_name_len, checker->function_name,
		mira_type_name(checker->return_type), mira_type_name(value.type));
}

static int checker_param_slot(const MiraTypeChecker *checker,
	const char *name, size_t name_len) {
	if (!checker->def) return -1;
	for (int i = 0; i < checker->def->param_count; ++i) {
		if (checker->def->param_lens[i] == name_len &&
		    memcmp(checker->def->params[i], name, name_len) == 0)
			return i;
	}
	return -1;
}

static void checker_binary_shape(MiraTypeChecker *checker, const IrNode *node,
	bool comparison) {
	if (checker->value_count < 2) return;
	MiraCheckedValue right = checker_pop(checker);
	MiraCheckedValue left = checker_pop(checker);
	if (checker_value_has_type(left, MIRA_TYPE_VOID)) checker_void_value_error(checker, left);
	if (checker_value_has_type(right, MIRA_TYPE_VOID)) checker_void_value_error(checker, right);
	MiraType result = comparison ? MIRA_TYPE_BOOL :
		(left.type == right.type ? left.type : MIRA_TYPE_UNKNOWN);
	checker_push(checker, result, left.strict && right.strict, node);
}

static bool checker_is_binary_word(const IrNode *node, bool *comparison) {
	static const char *arithmetic[] = {
		"+", "-", "*", "/", "%", "&", "|", "^", "<<", ">>",
		"and", "or", "mod", NULL
	};
	static const char *comparisons[] = {
		"=", "==", "!=", "<", ">", "<=", ">=",
		"eq", "ne", "lt", "gt", "le", "ge", NULL
	};
	for (int i = 0; arithmetic[i]; ++i) {
		if (word_is(node, arithmetic[i])) { *comparison = false; return true; }
	}
	for (int i = 0; comparisons[i]; ++i) {
		if (word_is(node, comparisons[i])) { *comparison = true; return true; }
	}
	return false;
}

static void checker_check_nodes(MiraTypeChecker *checker, IrNode *node);

static void checker_check_nested(MiraTypeChecker *parent, IrNode *node) {
	MiraTypeChecker nested = *parent;
	nested.values = NULL;
	nested.value_count = 0;
	nested.value_cap = 0;
	nested.reachable = true;
	checker_check_nodes(&nested, node);
	free(nested.values);
}

static void checker_check_value_context(MiraTypeChecker *parent, IrNode *node) {
	MiraTypeChecker nested = *parent;
	nested.values = NULL;
	nested.value_count = 0;
	nested.value_cap = 0;
	nested.reachable = true;
	checker_check_nodes(&nested, node);
	for (int i = 0; i < nested.value_count; ++i)
		if (checker_value_has_type(nested.values[i], MIRA_TYPE_VOID))
			checker_void_value_error(&nested, nested.values[i]);
	free(nested.values);
}

static MiraTypeChecker checker_clone(const MiraTypeChecker *source) {
	MiraTypeChecker clone = *source;
	clone.values = NULL;
	clone.value_cap = source->value_count;
	if (source->value_count > 0) {
		clone.values = malloc((size_t)source->value_count * sizeof(*clone.values));
		if (!clone.values) mira_error_simple(1, "out of memory in type checker");
		memcpy(clone.values, source->values,
			(size_t)source->value_count * sizeof(*clone.values));
	}
	return clone;
}

static void checker_copy_state(MiraTypeChecker *target,
	const MiraTypeChecker *source) {
	if (target->value_cap < source->value_count) {
		MiraCheckedValue *next = realloc(target->values,
			(size_t)source->value_count * sizeof(*next));
		if (!next) mira_error_simple(1, "out of memory in type checker");
		target->values = next;
		target->value_cap = source->value_count;
	}
	if (source->value_count > 0)
		memcpy(target->values, source->values,
			(size_t)source->value_count * sizeof(*target->values));
	target->value_count = source->value_count;
	target->reachable = source->reachable;
}

static MiraCheckedValue checker_merge_value(MiraCheckedValue left,
	MiraCheckedValue right) {
	MiraCheckedValue merged = left;
	merged.type_mask = left.type_mask | right.type_mask;
	merged.strict = merged.type_mask != 0;
	if (left.type != right.type) merged.type = MIRA_TYPE_UNKNOWN;
	if (!merged.void_source) merged.void_source = right.void_source;
	return merged;
}

static void checker_check_if(MiraTypeChecker *checker, IrNode *node) {
	checker_check_value_context(checker, node->u.iff.cond);
	MiraTypeChecker then_state = checker_clone(checker);
	MiraTypeChecker else_state = checker_clone(checker);
	checker_check_nodes(&then_state, node->u.iff.then_b);
	if (node->u.iff.else_b) checker_check_nodes(&else_state, node->u.iff.else_b);

	if (!then_state.reachable && !else_state.reachable) {
		checker->reachable = false;
		checker->value_count = 0;
	} else if (!then_state.reachable) {
		checker_copy_state(checker, &else_state);
	} else if (!else_state.reachable) {
		checker_copy_state(checker, &then_state);
	} else {
		int common_count = then_state.value_count < else_state.value_count ?
			then_state.value_count : else_state.value_count;
		checker_copy_state(checker, &then_state);
		checker->value_count = common_count;
		for (int i = 0; i < common_count; ++i)
			checker->values[i] = checker_merge_value(
				then_state.values[i], else_state.values[i]);
	}
	free(then_state.values);
	free(else_state.values);
}

static void checker_check_call(MiraTypeChecker *checker, IrNode *node, Def *callee) {
	bool typed = signature_is_typed(callee);
	checker_annotate_parenthesized_call(checker, node);
	int call_argc = node->u.word.has_call_arity ?
		node->u.word.call_argc : callee->param_count;
	if (node->u.word.has_call_arity && call_argc != callee->param_count) {
		mira_error(node->source ? node->source : checker->compiler->src,
			node->source_filename ? node->source_filename : checker->compiler->filename,
			node->line, node->col, 1,
			"function '%.*s' expects %d arguments, got %d",
			(int)callee->name_len, callee->name, callee->param_count, call_argc);
	}
	if (checker->value_count < call_argc) {
		if (typed)
			mira_error(node->source ? node->source : checker->compiler->src,
				node->source_filename ? node->source_filename : checker->compiler->filename,
				node->line, node->col, 1,
				"function '%.*s' expects %d arguments, got %d",
				(int)callee->name_len, callee->name, call_argc,
				checker->value_count);
	}

	int available = checker->value_count < call_argc ?
		checker->value_count : call_argc;
	int first = checker->value_count - available;
	int missing = call_argc - available;
	for (int i = 0; i < available; ++i) {
		MiraCheckedValue actual = checker->values[first + i];
		if (checker_value_has_type(actual, MIRA_TYPE_VOID))
			checker_void_value_error(checker, actual);
		int param = missing + i;
		if (!callee->param_type_explicit || !callee->param_type_explicit[param]) continue;
		MiraType expected = callee->param_types[param];
		MiraType mismatch = checker_first_mismatch(actual, expected);
		if (mismatch != MIRA_TYPE_UNKNOWN) {
			mira_error(actual.source ? actual.source : checker->compiler->src,
				actual.source_filename ? actual.source_filename : checker->compiler->filename,
				actual.line, actual.col, 1,
				"argument %d of '%.*s': expected %s, got %s", param + 1,
				(int)callee->name_len, callee->name, mira_type_name(expected),
				mira_type_name(mismatch));
		}
	}
	checker->value_count = first;
	if (callee->return_type_explicit && callee->return_type == MIRA_TYPE_VOID) {
		checker_push_void(checker, callee, node);
	} else {
		checker_push(checker,
			callee->return_type_explicit ? callee->return_type : MIRA_TYPE_UNKNOWN,
			callee->return_type_explicit, node);
	}
}

static void checker_check_return(MiraTypeChecker *checker, IrNode *node) {
	if (!checker->return_type_explicit) {
		checker->value_count = 0;
		checker->reachable = false;
		return;
	}
	if (checker->return_type == MIRA_TYPE_VOID) {
		if (checker->value_count > 0) {
			MiraCheckedValue value = checker->values[checker->value_count - 1];
			MiraType mismatch = checker_first_mismatch(value, MIRA_TYPE_VOID);
			if (mismatch != MIRA_TYPE_UNKNOWN) {
				value.type = mismatch;
				checker_result_error(checker, value);
			}
		}
	} else if (checker->value_count == 0) {
		MiraCheckedValue value = {
			MIRA_TYPE_VOID, 1u << MIRA_TYPE_VOID, true,
			node->line, node->col, NULL, node->source, node->source_filename
		};
		checker_result_error(checker, value);
	} else {
		MiraCheckedValue value = checker_pop(checker);
		if (checker_value_has_type(value, MIRA_TYPE_VOID)) checker_void_value_error(checker, value);
		MiraType mismatch = checker_first_mismatch(value, checker->return_type);
		if (mismatch != MIRA_TYPE_UNKNOWN) {
			value.type = mismatch;
			checker_result_error(checker, value);
		}
	}
	checker->value_count = 0;
	checker->reachable = false;
}

static void checker_check_nodes(MiraTypeChecker *checker, IrNode *node) {
	for (; node && checker->reachable; node = node->next) {
		switch (node->kind) {
		case IR_INT:
			checker_push(checker, MIRA_TYPE_I64, true, node);
			break;
		case IR_FLOAT:
			checker_push(checker, MIRA_TYPE_F64, true, node);
			break;
		case IR_STR:
			checker_push(checker, MIRA_TYPE_STR, true, node);
			break;
		case IR_CONST: {
			int slot = node->u.const_slot;
			MiraType type = MIRA_TYPE_UNKNOWN;
			if (slot >= 0 && slot < checker->program->const_count) {
				if (checker->program->const_type_explicit[slot])
					type = checker->program->const_types[slot];
				else if (checker->program->const_kinds[slot] == CONST_DOUBLE)
					type = MIRA_TYPE_F64;
				else if (checker->program->const_kinds[slot] == CONST_STR)
					type = MIRA_TYPE_STR;
				else type = MIRA_TYPE_I64;
			}
			checker_push(checker, type, type != MIRA_TYPE_UNKNOWN, node);
			break;
		}
		case IR_VAR:
			if (node->next && word_is(node->next, "@")) {
				int slot = node->u.var_slot;
				MiraType type = MIRA_TYPE_UNKNOWN;
				bool strict = false;
				if (slot >= 0 && slot < checker->program->var_count &&
				    checker->program->var_type_explicit[slot]) {
					type = checker->program->var_types[slot];
					strict = true;
				}
				checker_push(checker, type, strict, node);
			}
			break;
		case IR_WORD: {
			if (word_is(node, "return")) {
				checker_check_return(checker, node);
				break;
			}
			if (word_is(node, "true") || word_is(node, "false")) {
				checker_push(checker, MIRA_TYPE_BOOL, true, node);
				break;
			}
			int param = checker_param_slot(checker, node->u.word.name, node->u.word.len);
			if (param >= 0) {
				bool strict = checker->def->param_type_explicit &&
					checker->def->param_type_explicit[param];
				checker_push(checker,
					strict ? checker->def->param_types[param] : MIRA_TYPE_UNKNOWN,
					strict, node);
				break;
			}
			Def *callee = mira_find_signature(checker->program,
				node->u.word.name, node->u.word.len);
			if (callee) {
				checker_check_call(checker, node, callee);
				break;
			}
			if (word_is(node, "!") || word_is(node, "print") ||
			    word_is(node, "println") || word_is(node, "drop")) {
				if (checker->value_count > 0) {
					MiraCheckedValue value = checker_pop(checker);
					if (checker_value_has_type(value, MIRA_TYPE_VOID))
						checker_void_value_error(checker, value);
				}
				break;
			}
			bool comparison = false;
			if (checker_is_binary_word(node, &comparison))
				checker_binary_shape(checker, node, comparison);
			break;
		}
		case IR_BLOCK:
			checker_check_nodes(checker, node->u.block);
			break;
		case IR_IF:
			checker_check_if(checker, node);
			break;
		case IR_SWITCH:
			checker_check_value_context(checker, node->u.switch_.value);
			checker_check_nested(checker, node->u.switch_.cases);
			checker_check_nested(checker, node->u.switch_.default_block);
			checker_push(checker, MIRA_TYPE_UNKNOWN, false, node);
			break;
		case IR_FOR_CSTYLE:
			checker_check_nested(checker, node->u.for_cstyle.init);
			checker_check_value_context(checker, node->u.for_cstyle.cond);
			checker_check_nested(checker, node->u.for_cstyle.step);
			checker_check_nested(checker, node->u.for_cstyle.body);
			break;
		case IR_FOR_EXT:
			checker_check_nested(checker, node->u.for_ext.body);
			break;
		case IR_FOR_RANGE:
			checker_check_nested(checker, node->u.for_range.body);
			break;
		case IR_EACH:
			checker_check_value_context(checker, node->u.each.list);
			checker_check_nested(checker, node->u.each.body);
			break;
		case IR_WHILE_INF:
			checker_check_nested(checker, node->u.while_inf.body);
			break;
		case IR_WHILE_COND:
			checker_check_value_context(checker, node->u.while_cond.cond);
			checker_check_nested(checker, node->u.while_cond.body);
			break;
		case IR_TRY:
			checker_check_nested(checker, node->u.try_block.body);
			checker_check_nested(checker, node->u.try_block.catch_body);
			break;
		case IR_LAMBDA:
			checker_check_nested(checker, node->u.lambda.body);
			break;
		case IR_LIST_LITERAL:
			checker_check_value_context(checker, node->u.list_literal.elements);
			checker_push(checker, MIRA_TYPE_UNKNOWN, false, node);
			break;
		}
	}
}

static void checker_check_function(Compiler *compiler, Program *program,
	Def *def, IrNode *body, MiraType return_type, bool return_type_explicit,
	const char *name, size_t name_len, MiraSourceScan **source_scans) {
	MiraTypeChecker checker = {0};
	checker.compiler = compiler;
	checker.program = program;
	checker.def = def;
	checker.return_type = return_type;
	checker.return_type_explicit = return_type_explicit;
	checker.function_name = name;
	checker.function_name_len = name_len;
	checker.source_scans = source_scans;
	checker.reachable = true;
	checker_check_nodes(&checker, body);
	if (return_type_explicit && checker.reachable) {
		if (checker.value_count > 0) {
			MiraCheckedValue value = checker.values[checker.value_count - 1];
			if (checker_value_has_type(value, MIRA_TYPE_VOID) && return_type != MIRA_TYPE_VOID)
				checker_void_value_error(&checker, value);
			MiraType mismatch = checker_first_mismatch(value, return_type);
			if (mismatch != MIRA_TYPE_UNKNOWN) {
				value.type = mismatch;
				checker_result_error(&checker, value);
			}
		} else if (return_type != MIRA_TYPE_VOID) {
			MiraSourceInfo *def_source = checker_def_source(program, def);
			MiraCheckedValue missing = {
				MIRA_TYPE_VOID, 1u << MIRA_TYPE_VOID, true,
				def ? def->line : program->main_line,
				def ? def->col : program->main_col,
				NULL,
				body ? body->source : def_source ? def_source->source : NULL,
				body ? body->source_filename : def_source ? def_source->filename : NULL
			};
			checker_result_error(&checker, missing);
		}
	}
	free(checker.values);
}

bool mira_type_from_name(const char *name, size_t len, MiraType *out) {
	static const struct {
		const char *name;
		size_t len;
		MiraType type;
	} types[] = {
		{"unknown", 7, MIRA_TYPE_UNKNOWN},
		{"i64", 3, MIRA_TYPE_I64},
		{"f64", 3, MIRA_TYPE_F64},
		{"bool", 4, MIRA_TYPE_BOOL},
		{"str", 3, MIRA_TYPE_STR},
		{"void", 4, MIRA_TYPE_VOID}
	};

	for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); ++i) {
		if (types[i].len == len && memcmp(types[i].name, name, len) == 0) {
			if (out) *out = types[i].type;
			return true;
		}
	}
	return false;
}

const char *mira_type_name(MiraType type) {
	switch (type) {
	case MIRA_TYPE_I64: return "i64";
	case MIRA_TYPE_F64: return "f64";
	case MIRA_TYPE_BOOL: return "bool";
	case MIRA_TYPE_STR: return "str";
	case MIRA_TYPE_VOID: return "void";
	case MIRA_TYPE_UNKNOWN:
	default: return "unknown";
	}
}

void mira_typecheck_program(Compiler *compiler, Program *program) {
	MiraSourceScan *source_scans = NULL;
	if (program->main_return_type_explicit && program->main_return_type == MIRA_TYPE_UNKNOWN)
		mira_error(compiler->src, compiler->filename, program->main_line, program->main_col, 1,
			"unknown type 'unknown'");

	for (Def *def = program->defs; def; def = def->next) {
		for (int i = 0; i < def->param_count; ++i) {
			if (!def->param_type_explicit || !def->param_type_explicit[i]) continue;
			MiraType type = def->param_types[i];
			if (type == MIRA_TYPE_UNKNOWN)
				mira_error(compiler->src, compiler->filename, def->line, def->col, 1,
					"unknown type 'unknown'");
			if (type == MIRA_TYPE_VOID)
				mira_error(compiler->src, compiler->filename, def->line, def->col, 1,
					"type 'void' is only valid as a function result");
		}
		if (def->return_type_explicit && def->return_type == MIRA_TYPE_UNKNOWN)
			mira_error(compiler->src, compiler->filename, def->line, def->col, 1,
				"unknown type 'unknown'");
	}

	for (int i = 0; i < program->const_count; ++i) {
		if (!program->const_type_explicit || !program->const_type_explicit[i]) continue;
		MiraType type = program->const_types[i];
		if (type == MIRA_TYPE_UNKNOWN)
			mira_error(compiler->src, compiler->filename, 1, 1, 1,
				"unknown type 'unknown'");
		if (type == MIRA_TYPE_VOID)
			mira_error(compiler->src, compiler->filename, 1, 1, 1,
				"type 'void' is only valid as a function result");
	}

	for (int i = 0; i < program->var_count; ++i) {
		if (!program->var_type_explicit || !program->var_type_explicit[i]) continue;
		MiraType type = program->var_types[i];
		if (type == MIRA_TYPE_UNKNOWN)
			mira_error(compiler->src, compiler->filename, 1, 1, 1,
				"unknown type 'unknown'");
		if (type == MIRA_TYPE_VOID)
			mira_error(compiler->src, compiler->filename, 1, 1, 1,
				"type 'void' is only valid as a function result");
	}

	for (Def *def = program->defs; def; def = def->next) {
		if (!def->is_extern)
			checker_check_function(compiler, program, def, def->body,
				def->return_type, def->return_type_explicit,
				def->name, def->name_len, &source_scans);
	}
	checker_check_function(compiler, program, NULL, program->main_block,
		program->main_return_type, program->main_return_type_explicit,
		"main", 4, &source_scans);
	while (source_scans) {
		MiraSourceScan *next = source_scans->next;
		free(source_scans->boundaries);
		free(source_scans->line_starts);
		free(source_scans);
		source_scans = next;
	}
}
