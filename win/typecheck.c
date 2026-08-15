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
	int origin_line[MIRA_TYPE_VOID + 1];
	int origin_col[MIRA_TYPE_VOID + 1];
	const char *origin_source[MIRA_TYPE_VOID + 1];
	const char *origin_filename[MIRA_TYPE_VOID + 1];
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
	MiraType *var_flow_types;
	int var_flow_count;
	bool reachable;
	bool strict_context;
	int pending_store_slot;
	MiraSourceScan **source_scans;
	const char *last_call_source;
	size_t last_call_close;
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
	MiraCheckedValue value = {0};
	value.type = type;
	value.type_mask = type_mask;
	value.strict = strict;
	value.line = origin ? origin->line : 1;
	value.col = origin ? origin->col : 1;
	value.source = origin ? origin->source : NULL;
	value.source_filename = origin ? origin->source_filename : NULL;
	if (type_mask) {
		value.origin_line[type] = value.line;
		value.origin_col[type] = value.col;
		value.origin_source[type] = value.source;
		value.origin_filename[type] = value.source_filename;
	}
	checker->values[checker->value_count++] = value;
}

static void checker_push_value(MiraTypeChecker *checker,
	MiraCheckedValue value) {
	if (checker->value_count == checker->value_cap) {
		int next_cap = checker->value_cap ? checker->value_cap * 2 : 16;
		MiraCheckedValue *next = realloc(checker->values,
			(size_t)next_cap * sizeof(*next));
		if (!next) mira_error_simple(1, "out of memory in type checker");
		checker->values = next;
		checker->value_cap = next_cap;
	}
	checker->values[checker->value_count++] = value;
}

static void checker_push_void(MiraTypeChecker *checker, Def *callee,
	const IrNode *origin) {
	checker_push(checker, MIRA_TYPE_VOID, true, origin);
	checker->values[checker->value_count - 1].void_source = callee;
}

static MiraCheckedValue checker_pop(MiraTypeChecker *checker) {
	if (checker->value_count == 0)
		return (MiraCheckedValue){.type = MIRA_TYPE_UNKNOWN, .line = 1, .col = 1};
	return checker->values[--checker->value_count];
}

static bool checker_value_has_type(MiraCheckedValue value, MiraType type) {
	return (value.type_mask & (1u << type)) != 0;
}

static MiraType checker_first_mismatch(MiraCheckedValue value, MiraType expected) {
	if (value.type != MIRA_TYPE_UNKNOWN && value.type != expected)
		return value.type;
	for (MiraType type = MIRA_TYPE_I64; type <= MIRA_TYPE_VOID; ++type)
		if (type != expected && checker_value_has_type(value, type)) return type;
	return MIRA_TYPE_UNKNOWN;
}

static MiraCheckedValue checker_value_origin(MiraCheckedValue value,
	MiraType type) {
	if (type > MIRA_TYPE_UNKNOWN && type <= MIRA_TYPE_VOID &&
	    value.origin_line[type] > 0) {
		value.line = value.origin_line[type];
		value.col = value.origin_col[type];
		value.source = value.origin_source[type];
		value.source_filename = value.origin_filename[type];
	}
	return value;
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

static bool checker_text_equal(const char *left, size_t left_len,
	const char *right, size_t right_len) {
	if (left_len != right_len) return false;
	for (size_t i = 0; i < left_len; ++i) {
		char a = left[i] == '-' ? '_' : left[i];
		char b = right[i] == '-' ? '_' : right[i];
		if (a != b) return false;
	}
	return true;
}

static bool checker_source_callee_matches(MiraTypeChecker *checker,
	const IrNode *node, const char *source_name, size_t source_name_len) {
	const char *dot = memchr(source_name, '.', source_name_len);
	if (dot) {
		size_t alias_len = (size_t)(dot - source_name);
		for (size_t i = 0; i < checker->compiler->modules.import_count; ++i) {
			ModuleImport *import = &checker->compiler->modules.imports[i];
			if (import->owner != node->source_module ||
			    import->alias_len != alias_len ||
			    memcmp(import->alias, source_name, alias_len) != 0)
				continue;
			ModuleRecord *target = &checker->compiler->modules.modules[import->target];
			const char *member = dot + 1;
			size_t member_len = source_name_len - alias_len - 1;
			if (node->u.word.len != target->path_len + 1 + member_len) return false;
			return checker_text_equal(node->u.word.name, target->path_len,
				target->path, target->path_len) &&
				node->u.word.name[target->path_len] == '.' &&
				checker_text_equal(node->u.word.name + target->path_len + 1,
					member_len, member, member_len);
		}
		/* Canonical names and method symbols are already stored in full on IR. */
		return checker_text_equal(node->u.word.name, node->u.word.len,
			source_name, source_name_len);
	}

	if (node->source_module != 0 &&
	    node->source_module < checker->compiler->modules.module_count) {
		ModuleRecord *module =
			&checker->compiler->modules.modules[node->source_module];
		if (node->u.word.len == module->path_len + 1 + source_name_len &&
		    checker_text_equal(node->u.word.name, module->path_len,
			    module->path, module->path_len) &&
		    node->u.word.name[module->path_len] == '.' &&
		    checker_text_equal(node->u.word.name + module->path_len + 1,
			    source_name_len, source_name, source_name_len))
			return true;
	}
	return checker_text_equal(node->u.word.name, node->u.word.len,
		source_name, source_name_len);
}

typedef struct {
	MiraParenBoundary *boundary;
	size_t name_start;
	size_t close;
} MiraCallCandidate;

static bool checker_call_candidate(MiraTypeChecker *checker, IrNode *node,
	MiraSourceScan *scan, size_t close, MiraCallCandidate *candidate) {
	MiraParenBoundary *boundary = checker_find_boundary(scan, close);
	if (!boundary) return false;
	const char *source = node->source;
	size_t name_end = boundary->open;
	while (name_end > 0 && isspace((unsigned char)source[name_end - 1])) name_end--;
	size_t name_start = name_end;
	while (name_start > 0) {
		unsigned char ch = (unsigned char)source[name_start - 1];
		if (!(isalnum(ch) || ch == '_' || ch == '-' || ch == '.' || ch == '?')) break;
		name_start--;
	}
	if (name_start == name_end || !checker_source_callee_matches(checker, node,
	    source + name_start, name_end - name_start)) return false;
	*candidate = (MiraCallCandidate){boundary, name_start, close};
	return true;
}

static bool checker_annotate_parenthesized_call(MiraTypeChecker *checker,
	IrNode *node) {
	if (!node || node->kind != IR_WORD || !node->source) return false;
	if (node->u.word.has_call_arity) {
		checker->last_call_source = node->source;
		checker->last_call_close = node->u.word.call_close_offset;
		return true;
	}
	if (node->source_offset == 0) return false;
	const char *source = node->source;
	MiraSourceScan *scan = checker_source_scan(checker, source);
	MiraCallCandidate previous = {0}, current = {0};
	bool has_previous = false, has_current = false;
	size_t cursor = node->source_offset;
	while (cursor > 0 && isspace((unsigned char)source[cursor - 1])) cursor--;
	if (cursor > 0 && source[cursor - 1] == ')')
		has_previous = checker_call_candidate(checker, node, scan,
			cursor - 1, &previous);
	if (source[node->source_offset] == ')')
		has_current = checker_call_candidate(checker, node, scan,
			node->source_offset, &current);
	if (!has_previous && !has_current) return false;

	MiraCallCandidate *chosen = has_previous ? &previous : &current;
	unsigned char mode = has_previous ? 1 : 2;
	/* eval() emits an inner call before creating an adjacent outer call at its
	 * current ')'. Recursive calls instead create the inner node after consuming
	 * its own ')', so an unseen adjacent close belongs to the previous candidate. */
	if (has_previous && has_current && checker->last_call_source == source &&
	    checker->last_call_close == previous.close) {
		chosen = &current;
		mode = 2;
	}

	checker_source_position(scan, chosen->name_start, &node->line, &node->col);
	node->u.word.has_call_arity = 1;
	node->u.word.call_boundary_mode = mode;
	node->u.word.call_argc = chosen->boundary->argc;
	node->u.word.call_close_offset = chosen->close;
	checker->last_call_source = source;
	checker->last_call_close = chosen->close;
	return true;
}

static void checker_void_value_error(MiraTypeChecker *checker,
	MiraCheckedValue value) {
	value = checker_value_origin(value, MIRA_TYPE_VOID);
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

static MiraCheckedValue checker_merge_value(MiraCheckedValue left,
	MiraCheckedValue right);

static bool checker_numeric_type(MiraType type) {
	return type == MIRA_TYPE_I64 || type == MIRA_TYPE_F64;
}

static unsigned checker_candidate_mask(MiraCheckedValue value) {
	unsigned mask = value.type_mask;
	if (value.type > MIRA_TYPE_UNKNOWN && value.type <= MIRA_TYPE_VOID)
		mask |= 1u << value.type;
	return mask;
}

static MiraType checker_first_non_numeric(MiraCheckedValue value) {
	unsigned mask = checker_candidate_mask(value);
	for (MiraType type = MIRA_TYPE_I64; type <= MIRA_TYPE_VOID; ++type)
		if ((mask & (1u << type)) && !checker_numeric_type(type)) return type;
	return MIRA_TYPE_UNKNOWN;
}

static void checker_operator_single_type_error(MiraTypeChecker *checker,
	const IrNode *node, MiraCheckedValue value, MiraType actual,
	const char *expected) {
	value = checker_value_origin(value, actual);
	mira_error(value.source ? value.source : checker->compiler->src,
		value.source_filename ? value.source_filename : checker->compiler->filename,
		value.line, value.col, 1,
		"operator %.*s: expected %s, got %s",
		(int)node->u.word.len, node->u.word.name, expected,
		mira_type_name(actual));
}

static void checker_operator_type_error(MiraTypeChecker *checker,
	const IrNode *node, MiraCheckedValue left, MiraCheckedValue right) {
	mira_error(node->source ? node->source : checker->compiler->src,
		node->source_filename ? node->source_filename : checker->compiler->filename,
		node->line, node->col, 1,
		"operator %.*s: expected matching numeric types, got %s and %s",
		(int)node->u.word.len, node->u.word.name,
		mira_type_name(left.type), mira_type_name(right.type));
}

static void checker_operator_equality_type_error(MiraTypeChecker *checker,
	const IrNode *node, MiraCheckedValue left, MiraCheckedValue right) {
	mira_error(node->source ? node->source : checker->compiler->src,
		node->source_filename ? node->source_filename : checker->compiler->filename,
		node->line, node->col, 1,
		"operator %.*s: expected matching types, got %s and %s",
		(int)node->u.word.len, node->u.word.name,
		mira_type_name(left.type), mira_type_name(right.type));
}

static bool checker_equality_word(const IrNode *node) {
	if (node->kind == IR_WORD && node->u.word.len == 2 &&
	    node->u.word.name[0] == '=' && node->u.word.name[1] == '\0')
		return true;
	return word_is(node, "=") || word_is(node, "==") || word_is(node, "!=") ||
		word_is(node, "eq") || word_is(node, "ne");
}

static void checker_binary_shape(MiraTypeChecker *checker, const IrNode *node,
	bool comparison) {
	if (checker->value_count < 2) return;
	MiraCheckedValue right = checker_pop(checker);
	MiraCheckedValue left = checker_pop(checker);
	if (checker_value_has_type(left, MIRA_TYPE_VOID)) checker_void_value_error(checker, left);
	if (checker_value_has_type(right, MIRA_TYPE_VOID)) checker_void_value_error(checker, right);
	if (checker->strict_context && comparison && checker_equality_word(node) &&
	    !node->u.word.logical_booleanize) {
		unsigned left_mask = checker_candidate_mask(left);
		unsigned right_mask = checker_candidate_mask(right);
		MiraType mismatch_left = MIRA_TYPE_UNKNOWN;
		MiraType mismatch_right = MIRA_TYPE_UNKNOWN;
		for (MiraType left_type = MIRA_TYPE_I64;
		     left_type <= MIRA_TYPE_VOID && mismatch_left == MIRA_TYPE_UNKNOWN;
		     ++left_type) {
			if (!(left_mask & (1u << left_type))) continue;
			for (MiraType right_type = MIRA_TYPE_I64;
			     right_type <= MIRA_TYPE_VOID; ++right_type) {
				if ((right_mask & (1u << right_type)) && left_type != right_type) {
					mismatch_left = left_type;
					mismatch_right = right_type;
					break;
				}
			}
		}
		if (mismatch_left != MIRA_TYPE_UNKNOWN) {
			left = checker_value_origin(left, mismatch_left);
			right = checker_value_origin(right, mismatch_right);
			left.type = mismatch_left;
			right.type = mismatch_right;
			checker_operator_equality_type_error(checker, node, left, right);
		}
	}
	if (checker->strict_context && (!comparison || !checker_equality_word(node))) {
		MiraType invalid = checker_first_non_numeric(left);
		if (invalid != MIRA_TYPE_UNKNOWN)
			checker_operator_single_type_error(checker, node, left, invalid,
				"numeric type");
		invalid = checker_first_non_numeric(right);
		if (invalid != MIRA_TYPE_UNKNOWN)
			checker_operator_single_type_error(checker, node, right, invalid,
				"numeric type");
		unsigned left_mask = checker_candidate_mask(left) &
			((1u << MIRA_TYPE_I64) | (1u << MIRA_TYPE_F64));
		unsigned right_mask = checker_candidate_mask(right) &
			((1u << MIRA_TYPE_I64) | (1u << MIRA_TYPE_F64));
		MiraType mismatch_left = MIRA_TYPE_UNKNOWN;
		MiraType mismatch_right = MIRA_TYPE_UNKNOWN;
		for (MiraType left_type = MIRA_TYPE_I64;
		     left_type <= MIRA_TYPE_F64 && mismatch_left == MIRA_TYPE_UNKNOWN;
		     ++left_type) {
			if (!(left_mask & (1u << left_type))) continue;
			for (MiraType right_type = MIRA_TYPE_I64;
			     right_type <= MIRA_TYPE_F64; ++right_type) {
				if ((right_mask & (1u << right_type)) && left_type != right_type) {
					mismatch_left = left_type;
					mismatch_right = right_type;
					break;
				}
			}
		}
		if (mismatch_left != MIRA_TYPE_UNKNOWN) {
			checker_operator_type_error(checker, node,
				checker_value_origin(left, mismatch_left),
				checker_value_origin(right, mismatch_right));
		}
	}
	MiraType result = comparison ? MIRA_TYPE_BOOL :
		(left.type == right.type ? left.type : MIRA_TYPE_UNKNOWN);
	if (comparison) {
		checker_push(checker, result, true, node);
	} else {
		MiraCheckedValue combined = checker_merge_value(left, right);
		combined.type = result;
		combined.strict = left.strict || right.strict;
		checker_push_value(checker, combined);
	}
}

static bool checker_is_binary_word(const IrNode *node, bool *comparison) {
	static const char *arithmetic[] = {
		"+", "-", "*", "/", "%", "&", "|", "^", "<<", ">>",
		"mod", NULL
	};
	static const char *comparisons[] = {
		"=", "==", "!=", "<", ">", "<=", ">=",
		"eq", "ne", "lt", "gt", "le", "ge", NULL
	};
	/* Infix `==` is lowered by replacing its two-byte token text with "=",
	 * while retaining the original length. Accept that established IR shape. */
	if (node->kind == IR_WORD && node->u.word.len == 2 &&
	    node->u.word.name[0] == '=' && node->u.word.name[1] == '\0') {
		*comparison = true;
		return true;
	}
	for (int i = 0; arithmetic[i]; ++i) {
		if (word_is(node, arithmetic[i])) {
			*comparison = false; return true;
		}
	}
	for (int i = 0; comparisons[i]; ++i) {
		if (word_is(node, comparisons[i])) {
			*comparison = true; return true;
		}
	}
	return false;
}

static void checker_check_bool_operator(MiraTypeChecker *checker,
	const IrNode *node, bool unary) {
	if (checker->value_count < (unary ? 1 : 2)) return;
	MiraCheckedValue right = checker_pop(checker);
	MiraCheckedValue left = unary ? right : checker_pop(checker);
	if (checker_value_has_type(left, MIRA_TYPE_VOID))
		checker_void_value_error(checker, left);
	if (!unary && checker_value_has_type(right, MIRA_TYPE_VOID))
		checker_void_value_error(checker, right);
	if (checker->strict_context) {
		MiraType mismatch = checker_first_mismatch(left, MIRA_TYPE_BOOL);
		if (mismatch != MIRA_TYPE_UNKNOWN)
			checker_operator_single_type_error(checker, node, left, mismatch,
				"bool");
		if (!unary) {
			mismatch = checker_first_mismatch(right, MIRA_TYPE_BOOL);
			if (mismatch != MIRA_TYPE_UNKNOWN)
				checker_operator_single_type_error(checker, node, right, mismatch,
					"bool");
		}
	}
	checker_push(checker, MIRA_TYPE_BOOL, true, node);
}

static void checker_check_neg(MiraTypeChecker *checker, const IrNode *node) {
	if (checker->value_count < 1) return;
	MiraCheckedValue value = checker_pop(checker);
	if (checker_value_has_type(value, MIRA_TYPE_VOID))
		checker_void_value_error(checker, value);
	if (checker->strict_context) {
		MiraType mismatch = checker_first_mismatch(value, MIRA_TYPE_I64);
		if (mismatch != MIRA_TYPE_UNKNOWN)
			checker_operator_single_type_error(checker, node, value, mismatch,
				"i64");
	}
	checker_push_value(checker, value);
}

static void checker_check_nodes(MiraTypeChecker *checker, IrNode *node);
static MiraTypeChecker checker_clone(const MiraTypeChecker *source);

static void checker_check_nested(MiraTypeChecker *parent, IrNode *node) {
	MiraTypeChecker nested = checker_clone(parent);
	free(nested.values);
	nested.values = NULL;
	nested.value_count = 0;
	nested.value_cap = 0;
	nested.reachable = true;
	checker_check_nodes(&nested, node);
	free(nested.values);
	free(nested.var_flow_types);
}

static void checker_check_value_context(MiraTypeChecker *parent, IrNode *node) {
	MiraTypeChecker nested = checker_clone(parent);
	free(nested.values);
	nested.values = NULL;
	nested.value_count = 0;
	nested.value_cap = 0;
	nested.reachable = true;
	checker_check_nodes(&nested, node);
	for (int i = 0; i < nested.value_count; ++i)
		if (checker_value_has_type(nested.values[i], MIRA_TYPE_VOID))
			checker_void_value_error(&nested, nested.values[i]);
	free(nested.values);
	free(nested.var_flow_types);
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
	clone.var_flow_types = NULL;
	if (source->var_flow_count > 0) {
		clone.var_flow_types = malloc((size_t)source->var_flow_count *
			sizeof(*clone.var_flow_types));
		if (!clone.var_flow_types)
			mira_error_simple(1, "out of memory in type checker");
		memcpy(clone.var_flow_types, source->var_flow_types,
			(size_t)source->var_flow_count * sizeof(*clone.var_flow_types));
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
	if (target->var_flow_count == source->var_flow_count &&
	    target->var_flow_count > 0)
		memcpy(target->var_flow_types, source->var_flow_types,
			(size_t)target->var_flow_count * sizeof(*target->var_flow_types));
}

static MiraCheckedValue checker_merge_value(MiraCheckedValue left,
	MiraCheckedValue right) {
	MiraCheckedValue merged = left;
	unsigned left_mask = left.type_mask;
	merged.type_mask = left.type_mask | right.type_mask;
	merged.strict = left.strict || right.strict;
	if (left.type == MIRA_TYPE_UNKNOWN) merged.type = right.type;
	else if (right.type == MIRA_TYPE_UNKNOWN) merged.type = left.type;
	else if (left.type != right.type) merged.type = MIRA_TYPE_UNKNOWN;
	if (!merged.void_source) merged.void_source = right.void_source;
	for (MiraType type = MIRA_TYPE_I64; type <= MIRA_TYPE_VOID; ++type) {
		if (!checker_value_has_type(right, type) || (left_mask & (1u << type)))
			continue;
		merged.origin_line[type] = right.origin_line[type];
		merged.origin_col[type] = right.origin_col[type];
		merged.origin_source[type] = right.origin_source[type];
		merged.origin_filename[type] = right.origin_filename[type];
	}
	return merged;
}

static void checker_merge_states(MiraTypeChecker *target,
	const MiraTypeChecker *left, const MiraTypeChecker *right) {
	if (!left->reachable && !right->reachable) {
		target->reachable = false;
		target->value_count = 0;
	} else if (!left->reachable) {
		checker_copy_state(target, right);
	} else if (!right->reachable) {
		checker_copy_state(target, left);
	} else {
		int common_count = left->value_count < right->value_count ?
			left->value_count : right->value_count;
		checker_copy_state(target, left);
		target->value_count = common_count;
		for (int i = 0; i < common_count; ++i)
			target->values[i] = checker_merge_value(
				left->values[i], right->values[i]);
		for (int i = 0; i < target->var_flow_count; ++i)
			if (left->var_flow_types[i] != right->var_flow_types[i])
				target->var_flow_types[i] = MIRA_TYPE_UNKNOWN;
	}
}

static void checker_accumulate_state(MiraTypeChecker *template,
	MiraTypeChecker *accumulator, bool *has_accumulator,
	MiraTypeChecker *branch) {
	if (!*has_accumulator) {
		*accumulator = *branch;
		branch->values = NULL;
		branch->value_count = 0;
		branch->value_cap = 0;
		branch->var_flow_types = NULL;
		branch->var_flow_count = 0;
		*has_accumulator = true;
		return;
	}
	MiraTypeChecker merged = checker_clone(template);
	free(merged.values);
	merged.values = NULL;
	merged.value_count = 0;
	merged.value_cap = 0;
	checker_merge_states(&merged, accumulator, branch);
	free(accumulator->values);
	free(accumulator->var_flow_types);
	free(branch->values);
	free(branch->var_flow_types);
	*accumulator = merged;
}

static void checker_condition_type_error(MiraTypeChecker *checker,
	MiraCheckedValue value) {
	MiraType mismatch = checker_first_mismatch(value, MIRA_TYPE_BOOL);
	if (mismatch == MIRA_TYPE_UNKNOWN) return;
	value = checker_value_origin(value, mismatch);
	mira_error(value.source ? value.source : checker->compiler->src,
		value.source_filename ? value.source_filename : checker->compiler->filename,
		value.line, value.col, 1, "condition: expected bool, got %s",
		mira_type_name(mismatch));
}

static void checker_check_condition(MiraTypeChecker *parent, IrNode *node) {
	MiraTypeChecker nested = checker_clone(parent);
	free(nested.values);
	nested.values = NULL;
	nested.value_count = 0;
	nested.value_cap = 0;
	nested.reachable = true;
	nested.pending_store_slot = -1;
	checker_check_nodes(&nested, node);
	for (int i = 0; i < nested.value_count; ++i)
		if (checker_value_has_type(nested.values[i], MIRA_TYPE_VOID))
			checker_void_value_error(&nested, nested.values[i]);
	if (parent->strict_context && nested.value_count > 0)
		checker_condition_type_error(&nested,
			nested.values[nested.value_count - 1]);
	free(nested.values);
	free(nested.var_flow_types);
}

static size_t checker_skip_source_space(const char *source, size_t cursor,
	size_t limit) {
	while (cursor < limit) {
		if (isspace((unsigned char)source[cursor])) {
			cursor++;
			continue;
		}
		if (source[cursor] == '#') {
			while (cursor < limit && source[cursor] != '\n') cursor++;
			continue;
		}
		break;
	}
	return cursor;
}

static size_t checker_matching_delimiter(const char *source, size_t open,
	char open_char, char close_char, size_t limit) {
	int depth = 0;
	bool in_string = false, escaped = false, in_comment = false;
	for (size_t cursor = open; cursor < limit && source[cursor]; ++cursor) {
		char ch = source[cursor];
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
		if (ch == '"') { in_string = true; continue; }
		if (ch == open_char) depth++;
		else if (ch == close_char && --depth == 0) return cursor;
	}
	return SIZE_MAX;
}

static bool checker_source_word(const char *source, size_t offset,
	const char *word) {
	size_t len = strlen(word);
	if (memcmp(source + offset, word, len) != 0) return false;
	if (offset > 0 && (isalnum((unsigned char)source[offset - 1]) ||
	    source[offset - 1] == '_')) return false;
	unsigned char after = (unsigned char)source[offset + len];
	return !(isalnum(after) || after == '_');
}

static bool checker_while_inf_literal(MiraTypeChecker *checker,
	const IrNode *node, MiraCheckedValue *out) {
	if (!node->source || node->source_offset == 0) return false;
	const char *source = node->source;
	size_t limit = node->source_offset;
	size_t best_close = 0, best_token = 0;
	MiraType best_type = MIRA_TYPE_UNKNOWN;
	bool in_string = false, escaped = false, in_comment = false;
	for (size_t cursor = 0; cursor < limit && source[cursor]; ++cursor) {
		char ch = source[cursor];
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
		if (ch == '"') { in_string = true; continue; }
		if (ch != 'w' || !checker_source_word(source, cursor, "while")) continue;
		size_t open = checker_skip_source_space(source, cursor + 5, limit);
		if (open >= limit || source[open] != '(') continue;
		size_t close = checker_matching_delimiter(source, open, '(', ')', limit);
		if (close == SIZE_MAX) continue;
		size_t token = checker_skip_source_space(source, open + 1, close);
		size_t token_end = token;
		MiraType type = MIRA_TYPE_UNKNOWN;
		if (token + 4 <= close && memcmp(source + token, "true", 4) == 0) {
			token_end = token + 4;
			type = MIRA_TYPE_BOOL;
		} else {
			if (token_end < close && (source[token_end] == '+' ||
			    source[token_end] == '-')) token_end++;
			size_t digits = token_end;
			while (token_end < close && isdigit((unsigned char)source[token_end]))
				token_end++;
			if (token_end > digits) type = MIRA_TYPE_I64;
		}
		if (type == MIRA_TYPE_UNKNOWN ||
		    checker_skip_source_space(source, token_end, close) != close) continue;
		size_t body_open = checker_skip_source_space(source, close + 1, limit);
		if (body_open >= limit || source[body_open] != '{') continue;
		size_t body_close = checker_matching_delimiter(source, body_open,
			'{', '}', limit + 1);
		/* parse_one creates IR_WHILE_INF while the lexer may still point at
		 * the loop body's closing brace, so an equal source offset is valid. */
		if (body_close == SIZE_MAX || body_close > limit || body_close < best_close)
			continue;
		best_close = body_close;
		best_token = token;
		best_type = type;
	}
	if (best_type == MIRA_TYPE_UNKNOWN) return false;
	MiraSourceScan *scan = checker_source_scan(checker, source);
	MiraCheckedValue value = {0};
	value.type = best_type;
	value.type_mask = 1u << best_type;
	value.strict = true;
	value.source = source;
	value.source_filename = node->source_filename;
	checker_source_position(scan, best_token, &value.line, &value.col);
	value.origin_line[best_type] = value.line;
	value.origin_col[best_type] = value.col;
	value.origin_source[best_type] = value.source;
	value.origin_filename[best_type] = value.source_filename;
	*out = value;
	return true;
}

static bool checker_same_origin(const IrNode *left, const IrNode *right) {
	return left && right && left->line == right->line && left->col == right->col &&
		left->source == right->source;
}

static bool checker_booleanized_block(IrNode *block, IrNode *origin,
	IrNode **prefix_tail) {
	if (!block || block->kind != IR_BLOCK || !block->u.block) return false;
	IrNode *before_zero = NULL;
	IrNode *zero = NULL;
	IrNode *last = block->u.block;
	while (last->next) {
		before_zero = zero;
		zero = last;
		last = last->next;
	}
	if (!before_zero || !zero || zero->kind != IR_INT || zero->u.i != 0 ||
	    !word_is(last, "!=") || !checker_same_origin(zero, origin) ||
	    !checker_same_origin(last, origin)) return false;
	*prefix_tail = before_zero;
	return true;
}

static bool checker_logical_if(IrNode *node, IrNode **value,
	IrNode **value_tail) {
	IrNode *constant = NULL;
	IrNode *booleanized = NULL;
	if (node->u.iff.else_b && node->u.iff.else_b->kind == IR_BLOCK &&
	    node->u.iff.else_b->u.block &&
	    node->u.iff.else_b->u.block->kind == IR_INT &&
	    !node->u.iff.else_b->u.block->next &&
	    node->u.iff.else_b->u.block->u.i == 0) {
		constant = node->u.iff.else_b;
		booleanized = node->u.iff.then_b;
	} else if (node->u.iff.then_b && node->u.iff.then_b->kind == IR_BLOCK &&
	    node->u.iff.then_b->u.block &&
	    node->u.iff.then_b->u.block->kind == IR_INT &&
	    !node->u.iff.then_b->u.block->next &&
	    node->u.iff.then_b->u.block->u.i == 1) {
		constant = node->u.iff.then_b;
		booleanized = node->u.iff.else_b;
	}
	if (!constant || !checker_same_origin(constant->u.block, node) ||
	    !booleanized ||
	    !checker_booleanized_block(booleanized, node, value_tail))
		return false;
	*value = booleanized->u.block;
	return true;
}

static void checker_check_if(MiraTypeChecker *checker, IrNode *node) {
	int entry_count = checker->value_count;
	IrNode *logical_value = NULL;
	IrNode *logical_tail = NULL;
	bool logical = checker_logical_if(node, &logical_value, &logical_tail);
	checker_check_condition(checker, node->u.iff.cond);
	if (logical && checker->strict_context) {
		IrNode *rest = logical_tail->next;
		logical_tail->next = NULL;
		checker_check_condition(checker, logical_value);
		logical_tail->next = rest;
	}
	MiraTypeChecker then_state = checker_clone(checker);
	MiraTypeChecker else_state = checker_clone(checker);
	checker_check_nodes(&then_state, node->u.iff.then_b);
	if (node->u.iff.else_b) checker_check_nodes(&else_state, node->u.iff.else_b);

	checker_merge_states(checker, &then_state, &else_state);
	if (logical && checker->value_count > entry_count) {
		MiraCheckedValue *result = &checker->values[checker->value_count - 1];
		result->type = MIRA_TYPE_BOOL;
		result->type_mask = 1u << MIRA_TYPE_BOOL;
		result->strict = true;
	}
	free(then_state.values);
	free(then_state.var_flow_types);
	free(else_state.values);
	free(else_state.var_flow_types);
}

static void checker_check_switch(MiraTypeChecker *checker, IrNode *node) {
	checker_check_value_context(checker, node->u.switch_.value);
	MiraTypeChecker accumulator = {0};
	bool has_accumulator = false;
	for (IrNode *pattern = node->u.switch_.cases; pattern;) {
		IrNode *body = pattern->next;
		if (!body) break;
		IrNode pattern_copy = *pattern;
		pattern_copy.next = NULL;
		checker_check_value_context(checker, &pattern_copy);
		MiraTypeChecker branch = checker_clone(checker);
		IrNode body_copy = *body;
		body_copy.next = NULL;
		checker_check_nodes(&branch, &body_copy);
		IrNode *next_pattern = body->next;
		checker_accumulate_state(checker, &accumulator,
			&has_accumulator, &branch);
		pattern = next_pattern;
	}

	MiraTypeChecker fallback = checker_clone(checker);
	if (node->u.switch_.default_block) {
		IrNode default_copy = *node->u.switch_.default_block;
		default_copy.next = NULL;
		checker_check_nodes(&fallback, &default_copy);
	}
	checker_accumulate_state(checker, &accumulator,
		&has_accumulator, &fallback);
	if (has_accumulator) checker_copy_state(checker, &accumulator);
	free(accumulator.values);
	free(accumulator.var_flow_types);
}

static void checker_check_try(MiraTypeChecker *checker, IrNode *node) {
	MiraTypeChecker body = checker_clone(checker);
	checker_check_nodes(&body, node->u.try_block.body);
	if (!node->u.try_block.catch_body) {
		checker_copy_state(checker, &body);
		free(body.values);
		free(body.var_flow_types);
		return;
	}
	MiraTypeChecker caught = checker_clone(checker);
	checker_check_nodes(&caught, node->u.try_block.catch_body);
	checker_merge_states(checker, &body, &caught);
	free(body.values);
	free(body.var_flow_types);
	free(caught.values);
	free(caught.var_flow_types);
}

static void checker_check_call(MiraTypeChecker *checker, IrNode *node, Def *callee) {
	bool typed = signature_is_typed(callee);
	checker_annotate_parenthesized_call(checker, node);
	int receiver_count = node->u.word.has_call_arity ?
		node->u.word.call_receiver_count : 0;
	int explicit_argc = node->u.word.has_call_arity ?
		node->u.word.call_argc : callee->param_count;
	int expected_explicit = callee->param_count - receiver_count;
	if (expected_explicit < 0) expected_explicit = 0;
	if (node->u.word.has_call_arity && explicit_argc != expected_explicit) {
		mira_error(node->source ? node->source : checker->compiler->src,
			node->source_filename ? node->source_filename : checker->compiler->filename,
			node->line, node->col, 1,
			"function '%.*s' expects %d arguments, got %d",
			(int)callee->name_len, callee->name, expected_explicit, explicit_argc);
	}
	int call_argc = explicit_argc + receiver_count;
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
			actual = checker_value_origin(actual, mismatch);
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
				value = checker_value_origin(value, mismatch);
				value.type = mismatch;
				checker_result_error(checker, value);
			}
		}
	} else if (checker->value_count == 0) {
			MiraCheckedValue value = {
			.type = MIRA_TYPE_VOID, .type_mask = 1u << MIRA_TYPE_VOID,
			.strict = true, .line = node->line, .col = node->col,
			.source = node->source, .source_filename = node->source_filename
		};
		value.origin_line[MIRA_TYPE_VOID] = value.line;
		value.origin_col[MIRA_TYPE_VOID] = value.col;
		value.origin_source[MIRA_TYPE_VOID] = value.source;
		value.origin_filename[MIRA_TYPE_VOID] = value.source_filename;
		checker_result_error(checker, value);
	} else {
		MiraCheckedValue value = checker_pop(checker);
		if (checker_value_has_type(value, MIRA_TYPE_VOID)) checker_void_value_error(checker, value);
		MiraType mismatch = checker_first_mismatch(value, checker->return_type);
		if (mismatch != MIRA_TYPE_UNKNOWN) {
			value = checker_value_origin(value, mismatch);
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
				if (slot >= 0 && slot < checker->program->var_count) {
					strict = checker->program->var_type_explicit[slot] != 0;
					if (strict) type = checker->program->var_types[slot];
					else if (slot < checker->var_flow_count)
						type = checker->var_flow_types[slot];
				}
				checker_push(checker, type, strict, node);
			} else if (node->next && word_is(node->next, "!"))
				checker->pending_store_slot = node->u.var_slot;
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
			if (word_is(node, "and") || word_is(node, "or") ||
			    word_is(node, "xor")) {
				checker_check_bool_operator(checker, node, false);
				break;
			}
			if (word_is(node, "not")) {
				checker_check_bool_operator(checker, node, true);
				break;
			}
			if (word_is(node, "neg")) {
				checker_check_neg(checker, node);
				break;
			}
			Def *callee = mira_find_signature(checker->program,
				node->u.word.name, node->u.word.len);
			if (callee) {
				checker_check_call(checker, node, callee);
				break;
			}
			if (word_is(node, "!")) {
				if (checker->value_count > 0) {
					MiraCheckedValue value = checker_pop(checker);
					if (checker_value_has_type(value, MIRA_TYPE_VOID))
						checker_void_value_error(checker, value);
					int slot = checker->pending_store_slot;
					if (slot >= 0 && slot < checker->program->var_count) {
						MiraType expected = checker->program->var_types[slot];
						if (checker->program->var_type_explicit[slot]) {
							MiraType mismatch = checker_first_mismatch(value, expected);
							if (mismatch != MIRA_TYPE_UNKNOWN) {
								value = checker_value_origin(value, mismatch);
								mira_error(value.source ? value.source : checker->compiler->src,
									value.source_filename ? value.source_filename : checker->compiler->filename,
									value.line, value.col, 1,
									"assignment to %.*s: expected %s, got %s",
									(int)checker->program->var_lens[slot],
									checker->program->var_names[slot], mira_type_name(expected),
									mira_type_name(mismatch));
							}
						} else if (slot < checker->var_flow_count) {
							checker->var_flow_types[slot] =
								value.type != MIRA_TYPE_VOID ? value.type : MIRA_TYPE_UNKNOWN;
						}
					}
				}
				checker->pending_store_slot = -1;
				break;
			}
			if (word_is(node, "print") ||
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
			checker_check_switch(checker, node);
			break;
		case IR_FOR_CSTYLE:
			checker_check_nested(checker, node->u.for_cstyle.init);
			checker_check_condition(checker, node->u.for_cstyle.cond);
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
			if (node->u.while_inf.cond)
				checker_check_condition(checker, node->u.while_inf.cond);
			checker_check_nested(checker, node->u.while_inf.body);
			break;
		case IR_WHILE_COND:
			checker_check_condition(checker, node->u.while_cond.cond);
			checker_check_nested(checker, node->u.while_cond.body);
			break;
		case IR_TRY:
			checker_check_try(checker, node);
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

static bool checker_nodes_have_explicit_local(Program *program, IrNode *node) {
	for (; node; node = node->next) {
		if (node->kind == IR_VAR && node->next && word_is(node->next, "!")) {
			int slot = node->u.var_slot;
			if (slot >= 0 && slot < program->var_count &&
			    program->var_type_explicit[slot]) return true;
		}
		switch (node->kind) {
		case IR_BLOCK:
			if (checker_nodes_have_explicit_local(program, node->u.block)) return true;
			break;
		case IR_IF:
			if (checker_nodes_have_explicit_local(program, node->u.iff.cond) ||
			    checker_nodes_have_explicit_local(program, node->u.iff.then_b) ||
			    checker_nodes_have_explicit_local(program, node->u.iff.else_b)) return true;
			break;
		case IR_SWITCH:
			if (checker_nodes_have_explicit_local(program, node->u.switch_.value) ||
			    checker_nodes_have_explicit_local(program, node->u.switch_.cases) ||
			    checker_nodes_have_explicit_local(program, node->u.switch_.default_block)) return true;
			break;
		case IR_FOR_CSTYLE:
			if (checker_nodes_have_explicit_local(program, node->u.for_cstyle.init) ||
			    checker_nodes_have_explicit_local(program, node->u.for_cstyle.cond) ||
			    checker_nodes_have_explicit_local(program, node->u.for_cstyle.step) ||
			    checker_nodes_have_explicit_local(program, node->u.for_cstyle.body)) return true;
			break;
		case IR_FOR_EXT:
			if (checker_nodes_have_explicit_local(program, node->u.for_ext.body)) return true;
			break;
		case IR_FOR_RANGE:
			if (checker_nodes_have_explicit_local(program, node->u.for_range.body)) return true;
			break;
		case IR_EACH:
			if (checker_nodes_have_explicit_local(program, node->u.each.list) ||
			    checker_nodes_have_explicit_local(program, node->u.each.body)) return true;
			break;
		case IR_WHILE_INF:
			if (checker_nodes_have_explicit_local(program, node->u.while_inf.body)) return true;
			break;
		case IR_WHILE_COND:
			if (checker_nodes_have_explicit_local(program, node->u.while_cond.cond) ||
			    checker_nodes_have_explicit_local(program, node->u.while_cond.body)) return true;
			break;
		case IR_TRY:
			if (checker_nodes_have_explicit_local(program, node->u.try_block.body) ||
			    checker_nodes_have_explicit_local(program, node->u.try_block.catch_body)) return true;
			break;
		case IR_LAMBDA:
			if (checker_nodes_have_explicit_local(program, node->u.lambda.body)) return true;
			break;
		case IR_LIST_LITERAL:
			if (checker_nodes_have_explicit_local(program, node->u.list_literal.elements)) return true;
			break;
		default:
			break;
		}
	}
	return false;
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
	checker.var_flow_count = program->var_count;
	checker.var_flow_types = program->var_count > 0 ?
		calloc((size_t)program->var_count, sizeof(*checker.var_flow_types)) : NULL;
	if (program->var_count > 0 && !checker.var_flow_types)
		mira_error_simple(1, "out of memory in type checker");
	for (int i = 0; i < program->var_count; ++i)
		if (program->var_type_explicit && program->var_type_explicit[i])
			checker.var_flow_types[i] = program->var_types[i];
	checker.strict_context = return_type_explicit ||
		(def && signature_is_typed(def)) ||
		checker_nodes_have_explicit_local(program, body);
	checker.pending_store_slot = -1;
	checker_check_nodes(&checker, body);
	if (return_type_explicit && checker.reachable) {
		if (checker.value_count > 0) {
			MiraCheckedValue value = checker.values[checker.value_count - 1];
			if (checker_value_has_type(value, MIRA_TYPE_VOID) && return_type != MIRA_TYPE_VOID)
				checker_void_value_error(&checker, value);
			MiraType mismatch = checker_first_mismatch(value, return_type);
			if (mismatch != MIRA_TYPE_UNKNOWN) {
				value = checker_value_origin(value, mismatch);
				value.type = mismatch;
				checker_result_error(&checker, value);
			}
		} else if (return_type != MIRA_TYPE_VOID) {
			MiraSourceInfo *def_source = checker_def_source(program, def);
			MiraCheckedValue missing = {
				.type = MIRA_TYPE_VOID, .type_mask = 1u << MIRA_TYPE_VOID,
				.strict = true,
				.line = def ? def->line : program->main_line,
				.col = def ? def->col : program->main_col,
				.source = body ? body->source : def_source ? def_source->source : NULL,
				.source_filename = body ? body->source_filename :
					def_source ? def_source->filename : NULL
			};
			missing.origin_line[MIRA_TYPE_VOID] = missing.line;
			missing.origin_col[MIRA_TYPE_VOID] = missing.col;
			missing.origin_source[MIRA_TYPE_VOID] = missing.source;
			missing.origin_filename[MIRA_TYPE_VOID] = missing.source_filename;
			checker_result_error(&checker, missing);
		}
	}
	free(checker.values);
	free(checker.var_flow_types);
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
		{"ptr", 3, MIRA_TYPE_PTR},
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
	case MIRA_TYPE_PTR: return "ptr";
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
		IrNode *origin = program->const_origins ? program->const_origins[i] : NULL;
		const char *source = origin && origin->source ? origin->source : compiler->src;
		const char *filename = origin && origin->source_filename ?
			origin->source_filename : compiler->filename;
		int line = origin ? origin->line : 1;
		int col = origin ? origin->col : 1;
		if (type == MIRA_TYPE_UNKNOWN)
			mira_error(source, filename, line, col, 1,
				"unknown type 'unknown'");
		if (type == MIRA_TYPE_VOID)
			mira_error(source, filename, line, col, 1,
				"type 'void' is only valid as a function result");
		MiraType actual = program->const_kinds[i] == CONST_DOUBLE ? MIRA_TYPE_F64 :
			program->const_kinds[i] == CONST_STR ? MIRA_TYPE_STR : MIRA_TYPE_I64;
		if (type != actual)
			mira_error(source, filename, line, col, 1,
				"constant '%.*s': expected %s, got %s",
				(int)program->const_lens[i], program->const_names[i],
				mira_type_name(type), mira_type_name(actual));
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
