#include <assert.h>
#include <string.h>
#include "../codegen/stdlib_builtins.h"

int main(void) {
    const StdlibBuiltin *sqrt_builtin = stdlib_builtin_lookup("__mira_math_sqrt", 16);
    assert(sqrt_builtin);
    assert(strcmp(sqrt_builtin->runtime_symbol, "mira_f_sqrt") == 0);
    assert(sqrt_builtin->arity == 1);
    assert(sqrt_builtin->result_count == 1);
    assert(sqrt_builtin->result_type == SSA_TYPE_FLOAT);
    assert(stdlib_runtime_builtin_lookup("mira_f_sqrt") == sqrt_builtin);
    assert(stdlib_builtin_is_available(sqrt_builtin));
    assert(!sqrt_builtin->owned_result);
    const StdlibBuiltin *go_builtin = stdlib_builtin_lookup("__mira_task_go", 14);
    assert(go_builtin && !go_builtin->may_suspend);

    const StdlibBuiltin *length_builtin = stdlib_builtin_lookup("__mira_string_length", 20);
    assert(length_builtin && strcmp(length_builtin->runtime_symbol, "mira_str_len") == 0);
    assert(stdlib_builtin_lookup("str-cat", 7) == NULL);
    const StdlibBuiltin *legacy_concat = stdlib_legacy_builtin_lookup("str-cat", 7);
    assert(legacy_concat && strcmp(legacy_concat->runtime_symbol, "mira_str_concat") == 0);
    assert(legacy_concat->owned_result);
    assert(strcmp(legacy_concat->free_func_name, "mem_free") == 0);

    const StdlibBuiltin *input_builtin = stdlib_builtin_lookup("__mira_io_input", 15);
    assert(input_builtin && input_builtin->result_type == SSA_TYPE_PTR);
    assert(input_builtin->legacy_result_type == SSA_TYPE_INT);
    assert(input_builtin->owned_result);

    const StdlibBuiltin *seed_builtin = stdlib_builtin_lookup("__mira_random_seed", 18);
    assert(seed_builtin && seed_builtin->result_count == 0);
    assert(seed_builtin->result_type == SSA_TYPE_VOID);

    const StdlibBuiltin *ticks_builtin = stdlib_builtin_lookup("__mira_time_ticks", 17);
    assert(ticks_builtin && strcmp(ticks_builtin->runtime_symbol, "mira_time_ms") == 0);
    assert(stdlib_legacy_builtin_lookup("clock", 5) == ticks_builtin);
    const StdlibBuiltin *sleep_builtin = stdlib_builtin_lookup("__mira_time_sleep", 17);
    assert(sleep_builtin && sleep_builtin->may_suspend);
    assert(stdlib_builtin_is_available(sleep_builtin));
    assert(sleep_builtin->platform_mask == STDLIB_PLATFORM_ALL);
    assert(stdlib_builtin_lookup("__mira_missing", 14) == NULL);
    return 0;
}
