#include "parser.h"

static char *module_copy_text(ModuleTable *table, const char *text, size_t len) {
	char *copy = arena_alloc(table->arena, len + 1);
	memcpy(copy, text, len);
	copy[len] = '\0';
	return copy;
}

void module_table_init(ModuleTable *table, Arena *arena) {
	memset(table, 0, sizeof(*table));
	table->arena = arena;
}

ModuleId module_intern_path(ModuleTable *table, const char *path, size_t path_len) {
	for (size_t i = 0; i < table->module_count; ++i) {
		ModuleRecord *record = &table->modules[i];
		if (record->path_len == path_len && memcmp(record->path, path, path_len) == 0)
			return (ModuleId)i;
	}

	if (table->module_count == table->module_cap) {
		size_t next_cap = table->module_cap ? table->module_cap * 2 : 8;
		ModuleRecord *next = arena_alloc(table->arena, next_cap * sizeof(*next));
		if (table->modules)
			memcpy(next, table->modules, table->module_count * sizeof(*next));
		table->modules = next;
		table->module_cap = next_cap;
	}

	ModuleId id = (ModuleId)table->module_count++;
	ModuleRecord *record = &table->modules[id];
	record->path = module_copy_text(table, path, path_len);
	record->path_len = path_len;
	record->state = MODULE_LOADING;
	return id;
}

bool module_add_import(ModuleTable *table, ModuleId owner, ModuleId target,
	const char *alias, size_t alias_len, ModuleImport **out_import) {
	for (size_t i = 0; i < table->import_count; ++i) {
		ModuleImport *import = &table->imports[i];
		if (import->owner == owner && import->alias_len == alias_len &&
		    memcmp(import->alias, alias, alias_len) == 0)
			return false;
	}

	if (table->import_count == table->import_cap) {
		size_t next_cap = table->import_cap ? table->import_cap * 2 : 8;
		ModuleImport *next = arena_alloc(table->arena, next_cap * sizeof(*next));
		if (table->imports)
			memcpy(next, table->imports, table->import_count * sizeof(*next));
		table->imports = next;
		table->import_cap = next_cap;
	}

	ModuleImport *import = &table->imports[table->import_count++];
	import->owner = owner;
	import->target = target;
	import->alias = module_copy_text(table, alias, alias_len);
	import->alias_len = alias_len;
	if (out_import) *out_import = import;
	return true;
}

char *module_qualify_symbol(ModuleTable *table, ModuleId module,
	const char *name, size_t name_len) {
	if (module >= table->module_count) return NULL;
	ModuleRecord *record = &table->modules[module];
	size_t result_len = record->path_len + 1 + name_len;
	char *result = arena_alloc(table->arena, result_len + 1);
	memcpy(result, record->path, record->path_len);
	result[record->path_len] = '.';
	memcpy(result + record->path_len + 1, name, name_len);
	result[result_len] = '\0';
	return result;
}

void module_finish_loading(ModuleTable *table, ModuleId module) {
	if (module < table->module_count)
		table->modules[module].state = MODULE_LOADED;
}

ModuleId module_register_import(Compiler *compiler, const char *logical, const char *alias) {
	ModuleTable *table = &compiler->modules;
	ModuleId target = module_intern_path(table, logical, strlen(logical));
	const char *effective_alias = alias;
	if (!effective_alias || !*effective_alias) {
		const char *slash = strrchr(logical, '/');
		effective_alias = slash ? slash + 1 : logical;
	}
	if (!module_add_import(table, compiler->current_module, target,
	    effective_alias, strlen(effective_alias), NULL))
		mira_error_simple(1, "duplicate module alias '%s'", effective_alias);
	return target;
}

char *module_resolve_dotted(ModuleTable *table, ModuleId owner,
	const char *name, size_t name_len, size_t *resolved_len, int *error_kind) {
	const char *dot = memchr(name, '.', name_len);
	if (!dot || dot == name || dot + 1 >= name + name_len) return NULL;
	size_t alias_len = (size_t)(dot - name);
	ModuleImport *found = NULL;
	for (size_t i = 0; i < table->import_count; ++i) {
		ModuleImport *candidate = &table->imports[i];
		if (candidate->owner == owner && candidate->alias_len == alias_len &&
		    memcmp(candidate->alias, name, alias_len) == 0) {
			found = candidate;
			break;
		}
	}
	if (!found) { if (error_kind) *error_kind = 1; return NULL; }
	size_t member_len = name_len - alias_len - 1;
	char *result = module_qualify_symbol(table, found->target, dot + 1, member_len);
	if (resolved_len) *resolved_len = table->modules[found->target].path_len + 1 + member_len;
	if (error_kind) *error_kind = 0;
	return result;
}