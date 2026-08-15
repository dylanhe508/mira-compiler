#include "mira.h"

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
}
