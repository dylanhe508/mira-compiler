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

int main(void) {
	char source[] =
		"fn helper(value: i64) -> i64 { value }\n"
		"const top_answer: i64 = 42;\n"
		"fn main() -> void {\n"
		"    const local_answer: i64 = 7\n"
		"    print(local_answer);\n"
		"}\n";
	Compiler compiler = {0};
	compiler.src = source;
	compiler.filename = "<type-metadata-test>";
	Program *program = parser_parse(&compiler);

	if (!program->main_return_type_explicit ||
	    program->main_return_type != MIRA_TYPE_VOID ||
	    program->main_line != 3 || program->main_col != 1)
		return 1;

	bool found_helper = false;
	for (Def *def = program->defs; def; def = def->next) {
		if (def->name_len != 6 || memcmp(def->name, "helper", 6) != 0) continue;
		found_helper = true;
		if (def->param_count != 1 || !def->param_type_explicit[0] ||
		    def->param_types[0] != MIRA_TYPE_I64 ||
		    !def->return_type_explicit || def->return_type != MIRA_TYPE_I64)
			return 2;
	}
	if (!found_helper) return 2;

	int top = find_const(program, "top_answer");
	int local = find_const(program, "local_answer");
	if (top < 0 || local < 0 ||
	    !program->const_type_explicit[top] || program->const_types[top] != MIRA_TYPE_I64 ||
	    !program->const_type_explicit[local] || program->const_types[local] != MIRA_TYPE_I64)
		return 3;

	puts("TYPE METADATA PASS");
	return 0;
}
