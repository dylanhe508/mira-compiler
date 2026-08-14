#include "../mira.h"
#include "../parser/parser.h"
#include <assert.h>
#include <string.h>

int main(void) {
	Arena arena = {0};
	ModuleTable table;
	module_table_init(&table, &arena);

	ModuleId math = module_intern_path(&table, "std.math", 8);
	ModuleId same = module_intern_path(&table, "std.math", 8);
	assert(math == same);
	assert(module_add_import(&table, 0, math, "math", 4, NULL));
	assert(!module_add_import(&table, 0, math, "math", 4, NULL));

	const char *qualified = module_qualify_symbol(&table, math, "max", 3);
	assert(qualified != NULL);
	assert(strcmp(qualified, "std.math.max") == 0);

	module_finish_loading(&table, math);
	assert(table.modules[math].state == MODULE_LOADED);
	arena_free(&arena);
	return 0;
}
