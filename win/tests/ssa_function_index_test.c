#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codegen/ir_ssa.h"

static char *copy_name(const char *name) {
    size_t size = strlen(name) + 1;
    char *copy = malloc(size);
    assert(copy);
    memcpy(copy, name, size);
    return copy;
}

int main(void) {
    SsaModule mod = {0};
    mod.func_count = mod.func_cap = 800;
    mod.functions = calloc(800, sizeof(*mod.functions));
    assert(mod.functions);
    for (int i = 0; i < 800; ++i) {
        char name[32];
        snprintf(name, sizeof(name), "f%d", i);
        mod.functions[i] = calloc(1, sizeof(*mod.functions[i]));
        assert(mod.functions[i]);
        mod.functions[i]->name = copy_name(name);
    }
    mod.function_epoch = 1;
    assert(ssa_function_index_rebuild(&mod));
    for (int i = 0; i < 800; ++i) {
        char name[32];
        snprintf(name, sizeof(name), "f%d", i);
        assert(ssa_function_index_find(&mod, name) == mod.functions[i]);
        assert(ssa_function_index_ordinal(&mod, mod.functions[i]) == i);
    }
    assert(ssa_function_index_find(&mod, "missing") == NULL);
    assert(ssa_function_index_name_comparisons(&mod) < 8000);

    ssa_function_index_invalidate(&mod);
    assert(ssa_function_index_find(&mod, "f0") == NULL);
    assert(ssa_function_index_ordinal(&mod, mod.functions[0]) == -1);
    assert(ssa_function_index_rebuild(&mod));
    assert(ssa_function_index_find(&mod, "f799") == mod.functions[799]);

    ssa_function_index_free(&mod);
    for (int i = 0; i < 800; ++i) {
        free(mod.functions[i]->name);
        free(mod.functions[i]);
    }
    free(mod.functions);
    return 0;
}
