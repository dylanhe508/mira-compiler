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
	bool strict;
	int line;
	int col;
	Def *void_source;
} MiraCheckedValue;

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
	bool saw_return;
} MiraTypeChecker;

static bool word_is(const IrNode *node, const char *word) {
	return node->kind == IR_WORD && node->u.word.len == strlen(word) &&
		memcmp(node->u.word.name, word, node->u.word.len) == 0;
}

static void checker_push(MiraTypeChecker *checker, MiraType type, bool strict,
	int line, int col) {
	if (checker->value_count == checker->value_cap) {
		int next_cap = checker->value_cap ? checker->value_cap * 2 : 16;
		MiraCheckedValue *next = realloc(checker->values,
			(size_t)next_cap * sizeof(*next));
		if (!next) mira_error_simple(1, "out of memory in type checker");
		checker->values = next;
		checker->value_cap = next_cap;
	}
	checker->values[checker->value_count++] =
		(MiraCheckedValue){type, strict, line, col, NULL};
}

static void checker_push_void(MiraTypeChecker *checker, Def *callee,
	int line, int col) {
	checker_push(checker, MIRA_TYPE_VOID, true, line, col);
	checker->values[checker->value_count - 1].void_source = callee;
}

static MiraCheckedValue checker_pop(MiraTypeChecker *checker) {
	if (checker->value_count == 0)
		return (MiraCheckedValue){MIRA_TYPE_UNKNOWN, false, 1, 1, NULL};
	return checker->values[--checker->value_count];
}

static bool signature_is_typed(const Def *def) {
	if (def->return_type_explicit) return true;
	for (int i = 0; i < def->param_count; ++i)
		if (def->param_type_explicit && def->param_type_explicit[i]) return true;
	return false;
}

static void checker_void_value_error(MiraTypeChecker *checker,
	MiraCheckedValue value) {
	Def *def = value.void_source;
	mira_error(checker->compiler->src, checker->compiler->filename,
		value.line, value.col, 1,
		"function '%.*s' returns void and cannot be used as a value",
		def ? (int)def->name_len : 4, def ? def->name : "call");
}

static void checker_result_error(MiraTypeChecker *checker, MiraCheckedValue value) {
	mira_error(checker->compiler->src, checker->compiler->filename,
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
	if (left.type == MIRA_TYPE_VOID) checker_void_value_error(checker, left);
	if (right.type == MIRA_TYPE_VOID) checker_void_value_error(checker, right);
	MiraType result = comparison ? MIRA_TYPE_BOOL :
		(left.type == right.type ? left.type : MIRA_TYPE_UNKNOWN);
	checker_push(checker, result, left.strict && right.strict,
		node->line, node->col);
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
	checker_check_nodes(&nested, node);
	parent->saw_return = parent->saw_return || nested.saw_return;
	free(nested.values);
}

static void checker_check_call(MiraTypeChecker *checker, IrNode *node, Def *callee) {
	bool typed = signature_is_typed(callee);
	if (checker->value_count < callee->param_count) {
		if (typed)
			mira_error(checker->compiler->src, checker->compiler->filename,
				node->line, node->col, 1,
				"function '%.*s' expects %d arguments, got %d",
				(int)callee->name_len, callee->name, callee->param_count,
				checker->value_count);
	}

	int available = checker->value_count < callee->param_count ?
		checker->value_count : callee->param_count;
	int first = checker->value_count - available;
	int missing = callee->param_count - available;
	for (int i = 0; i < available; ++i) {
		MiraCheckedValue actual = checker->values[first + i];
		if (actual.type == MIRA_TYPE_VOID)
			checker_void_value_error(checker, actual);
		int param = missing + i;
		if (!callee->param_type_explicit || !callee->param_type_explicit[param]) continue;
		MiraType expected = callee->param_types[param];
		if (actual.strict && actual.type != MIRA_TYPE_UNKNOWN && actual.type != expected) {
			mira_error(checker->compiler->src, checker->compiler->filename,
				actual.line, actual.col, 1,
				"argument %d of '%.*s': expected %s, got %s", param + 1,
				(int)callee->name_len, callee->name, mira_type_name(expected),
				mira_type_name(actual.type));
		}
	}
	checker->value_count = first;
	if (callee->return_type_explicit && callee->return_type == MIRA_TYPE_VOID) {
		checker_push_void(checker, callee, node->line, node->col);
	} else {
		checker_push(checker,
			callee->return_type_explicit ? callee->return_type : MIRA_TYPE_UNKNOWN,
			callee->return_type_explicit, node->line, node->col);
	}
}

static void checker_check_return(MiraTypeChecker *checker, IrNode *node) {
	checker->saw_return = true;
	if (!checker->return_type_explicit) {
		checker->value_count = 0;
		return;
	}
	if (checker->return_type == MIRA_TYPE_VOID) {
		if (checker->value_count > 0) {
			MiraCheckedValue value = checker->values[checker->value_count - 1];
			if (value.type != MIRA_TYPE_VOID) checker_result_error(checker, value);
		}
	} else if (checker->value_count == 0) {
		MiraCheckedValue value = {MIRA_TYPE_VOID, true, node->line, node->col, NULL};
		checker_result_error(checker, value);
	} else {
		MiraCheckedValue value = checker_pop(checker);
		if (value.type == MIRA_TYPE_VOID) checker_void_value_error(checker, value);
		if (value.strict && value.type != MIRA_TYPE_UNKNOWN &&
		    value.type != checker->return_type)
			checker_result_error(checker, value);
	}
	checker->value_count = 0;
}

static void checker_check_nodes(MiraTypeChecker *checker, IrNode *node) {
	for (; node; node = node->next) {
		switch (node->kind) {
		case IR_INT:
			checker_push(checker, MIRA_TYPE_I64, true, node->line, node->col);
			break;
		case IR_FLOAT:
			checker_push(checker, MIRA_TYPE_F64, true, node->line, node->col);
			break;
		case IR_STR:
			checker_push(checker, MIRA_TYPE_STR, true, node->line, node->col);
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
			checker_push(checker, type, type != MIRA_TYPE_UNKNOWN, node->line, node->col);
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
				checker_push(checker, type, strict, node->line, node->col);
			}
			break;
		case IR_WORD: {
			if (word_is(node, "return")) {
				checker_check_return(checker, node);
				break;
			}
			if (word_is(node, "true") || word_is(node, "false")) {
				checker_push(checker, MIRA_TYPE_BOOL, true, node->line, node->col);
				break;
			}
			int param = checker_param_slot(checker, node->u.word.name, node->u.word.len);
			if (param >= 0) {
				bool strict = checker->def->param_type_explicit &&
					checker->def->param_type_explicit[param];
				checker_push(checker,
					strict ? checker->def->param_types[param] : MIRA_TYPE_UNKNOWN,
					strict, node->line, node->col);
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
					if (value.type == MIRA_TYPE_VOID)
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
			checker_check_nested(checker, node->u.block);
			break;
		case IR_IF:
			checker_check_nested(checker, node->u.iff.cond);
			checker_check_nested(checker, node->u.iff.then_b);
			checker_check_nested(checker, node->u.iff.else_b);
			checker_push(checker, MIRA_TYPE_UNKNOWN, false, node->line, node->col);
			break;
		case IR_SWITCH:
			checker_check_nested(checker, node->u.switch_.value);
			checker_check_nested(checker, node->u.switch_.cases);
			checker_check_nested(checker, node->u.switch_.default_block);
			checker_push(checker, MIRA_TYPE_UNKNOWN, false, node->line, node->col);
			break;
		case IR_FOR_CSTYLE:
			checker_check_nested(checker, node->u.for_cstyle.init);
			checker_check_nested(checker, node->u.for_cstyle.cond);
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
			checker_check_nested(checker, node->u.each.list);
			checker_check_nested(checker, node->u.each.body);
			break;
		case IR_WHILE_INF:
			checker_check_nested(checker, node->u.while_inf.body);
			break;
		case IR_WHILE_COND:
			checker_check_nested(checker, node->u.while_cond.cond);
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
			checker_check_nested(checker, node->u.list_literal.elements);
			checker_push(checker, MIRA_TYPE_UNKNOWN, false, node->line, node->col);
			break;
		}
	}
}

static void checker_check_function(Compiler *compiler, Program *program,
	Def *def, IrNode *body, MiraType return_type, bool return_type_explicit,
	const char *name, size_t name_len) {
	MiraTypeChecker checker = {0};
	checker.compiler = compiler;
	checker.program = program;
	checker.def = def;
	checker.return_type = return_type;
	checker.return_type_explicit = return_type_explicit;
	checker.function_name = name;
	checker.function_name_len = name_len;
	checker_check_nodes(&checker, body);
	if (return_type_explicit) {
		if (checker.value_count > 0) {
			MiraCheckedValue value = checker.values[checker.value_count - 1];
			if (value.type == MIRA_TYPE_VOID && return_type != MIRA_TYPE_VOID)
				checker_void_value_error(&checker, value);
			if (value.strict && value.type != MIRA_TYPE_UNKNOWN && value.type != return_type)
				checker_result_error(&checker, value);
		} else if (!checker.saw_return && return_type != MIRA_TYPE_VOID) {
			MiraCheckedValue missing = {
				MIRA_TYPE_VOID, true,
				def ? def->line : program->main_line,
				def ? def->col : program->main_col,
				NULL
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
				def->name, def->name_len);
	}
	checker_check_function(compiler, program, NULL, program->main_block,
		program->main_return_type, program->main_return_type_explicit,
		"main", 4);
}
