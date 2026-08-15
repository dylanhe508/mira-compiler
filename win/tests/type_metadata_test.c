#include "../mira.h"

#include <stdio.h>

void parser_do_import(const char *path, const char *alias, int is_lib) {
	(void)path;
	(void)alias;
	(void)is_lib;
}

static int find_const(Program *program, const char *name) {
	size_t len = strlen(name);
	for (int i = 0; i < program->const_count; ++i)
		if (program->const_lens[i] == len &&
		    memcmp(program->const_names[i], name, len) == 0)
			return i;
	return -1;
}

static int find_var(Program *program, const char *name) {
	size_t len = strlen(name);
	for (int i = 0; i < program->var_count; ++i)
		if (program->var_lens[i] == len &&
		    memcmp(program->var_names[i], name, len) == 0)
			return i;
	return -1;
}

static Def *find_def(Program *program, const char *name) {
	size_t len = strlen(name);
	for (Def *def = program->defs; def; def = def->next)
		if (def->name_len == len && memcmp(def->name, name, len) == 0)
			return def;
	return NULL;
}

static int has_six_typed_params(Def *def) {
	static const MiraType expected[] = {
		MIRA_TYPE_I64, MIRA_TYPE_F64, MIRA_TYPE_BOOL, MIRA_TYPE_STR,
		MIRA_TYPE_PTR, MIRA_TYPE_I64
	};
	if (!def || def->param_count != 6 || !def->param_types || !def->param_type_explicit)
		return 0;
	for (int i = 0; i < 6; ++i)
		if (!def->param_type_explicit[i] || def->param_types[i] != expected[i])
			return 0;
	return 1;
}

int main(void) {
	char source[] =
		"extern fn foreign(a: i64, b: f64, c: bool, d: str, p: ptr, e: i64) -> void;\n"
		"fn helper(a: i64, b: f64, c: bool, d: str, p: ptr, e: i64) -> i64 { a }\n"
		"const top_answer: i64 = 42;\n"
		"fn main() -> void {\n"
		"    let local_value: i64 = 7;\n"
		"    mut mutable_value: f64 = 2.5;\n"
		"    const local_answer: i64 = 7\n"
		"    print(local_answer);\n"
		"    while(true) { break; }\n"
		"    while(1) { break; }\n"
		"}\n";
	Compiler compiler = {0};
	compiler.src = source;
	compiler.filename = "<type-metadata-test>";
	Program *program = parser_parse(&compiler);

	if (!program->main_return_type_explicit ||
	    program->main_return_type != MIRA_TYPE_VOID ||
	    program->main_line != 4 || program->main_col != 1)
		return 1;

	Def *helper = find_def(program, "helper");
	if (!has_six_typed_params(helper) || helper->is_extern ||
	    !helper->return_type_explicit || helper->return_type != MIRA_TYPE_I64)
		return 2;

	Def *foreign = find_def(program, "foreign");
	if (!has_six_typed_params(foreign) || !foreign->is_extern || foreign->body ||
	    !foreign->return_type_explicit || foreign->return_type != MIRA_TYPE_VOID)
		return 3;

	int top = find_const(program, "top_answer");
	int local = find_const(program, "local_answer");
	if (top < 0 || local < 0 ||
	    !program->const_type_explicit[top] || program->const_types[top] != MIRA_TYPE_I64 ||
	    !program->const_type_explicit[local] || program->const_types[local] != MIRA_TYPE_I64)
		return 4;

	int local_value = find_var(program, "local_value");
	int mutable_value = find_var(program, "mutable_value");
	if (local_value < 0 || mutable_value < 0 ||
	    !program->var_type_explicit[local_value] ||
	    program->var_types[local_value] != MIRA_TYPE_I64 ||
	    !program->var_type_explicit[mutable_value] ||
	    program->var_types[mutable_value] != MIRA_TYPE_F64)
		return 5;

	IrNode *first_while = NULL;
	IrNode *second_while = NULL;
	for (IrNode *node = program->main_block; node; node = node->next) {
		if (node->kind != IR_WHILE_INF) continue;
		if (!first_while) first_while = node;
		else if (!second_while) second_while = node;
	}
	if (!first_while || !second_while ||
	    !first_while->u.while_inf.cond ||
	    first_while->u.while_inf.cond->kind != IR_WORD ||
	    first_while->u.while_inf.cond->u.word.len != 4 ||
	    memcmp(first_while->u.while_inf.cond->u.word.name, "true", 4) != 0 ||
	    !second_while->u.while_inf.cond ||
	    second_while->u.while_inf.cond->kind != IR_INT ||
	    second_while->u.while_inf.cond->u.i != 1)
		return 6;

	puts("TYPE METADATA PASS");
	return 0;
}
