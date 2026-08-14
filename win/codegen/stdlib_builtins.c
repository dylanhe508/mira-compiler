#include "stdlib_builtins.h"
#include <string.h>

#define B(source, legacy, runtime, arity, results, type, platform, suspend, owned, release) \
    {source, legacy, runtime, arity, results, type, platform, suspend, owned, release, runtime, type}
#define BL(source, legacy, runtime, arity, results, type, platform, suspend, owned, release, legacy_runtime, legacy_type) \
    {source, legacy, runtime, arity, results, type, platform, suspend, owned, release, legacy_runtime, legacy_type}

static const StdlibBuiltin builtins[] = {
    B("__mira_list_new", "list-new", "mira_list_new", 1, 1, SSA_TYPE_PTR, STDLIB_PLATFORM_ALL, 0, 1, "mira_list_free"),
    B("__mira_list_length", "list-len", "mira_list_len", 1, 1, SSA_TYPE_INT, STDLIB_PLATFORM_ALL, 0, 0, NULL),
    B("__mira_list_get", "list-get", "mira_list_get", 2, 1, SSA_TYPE_INT, STDLIB_PLATFORM_ALL, 0, 0, NULL),
    B("__mira_list_set", "list-set", "mira_list_set", 3, 0, SSA_TYPE_VOID, STDLIB_PLATFORM_ALL, 0, 0, NULL),
    B("__mira_list_free", "list-free", "mira_list_free", 1, 0, SSA_TYPE_VOID, STDLIB_PLATFORM_ALL, 0, 0, NULL),
    B("__mira_list_push", "list-push", "mira_list_push", 2, 1, SSA_TYPE_PTR, STDLIB_PLATFORM_ALL, 0, 1, "mira_list_free"),
    B("__mira_list_pop", NULL, "mira_list_pop", 1, 1, SSA_TYPE_INT, STDLIB_PLATFORM_ALL, 0, 0, NULL),
    B("__mira_string_length", "str-len", "mira_str_len", 1, 1, SSA_TYPE_INT, STDLIB_PLATFORM_ALL, 0, 0, NULL),
    B("__mira_string_concat", "str-concat", "mira_str_concat", 2, 1, SSA_TYPE_PTR, STDLIB_PLATFORM_ALL, 0, 1, "mem_free"),
    B("__mira_string_equal", "str-eq", "mira_str_eq", 2, 1, SSA_TYPE_INT, STDLIB_PLATFORM_ALL, 0, 0, NULL),
    B("__mira_string_substring", "str-sub", "mira_str_substr", 3, 1, SSA_TYPE_PTR, STDLIB_PLATFORM_ALL, 0, 1, "mem_free"),
    B("__mira_string_contains", "str-contains", "mira_str_contains", 2, 1, SSA_TYPE_INT, STDLIB_PLATFORM_ALL, 0, 0, NULL),
    B("__mira_string_trim", "str-trim", "mira_str_trim", 1, 1, SSA_TYPE_PTR, STDLIB_PLATFORM_ALL, 0, 1, "mem_free"),
    B("__mira_string_at", "str-at", "mira_str_at", 2, 1, SSA_TYPE_INT, STDLIB_PLATFORM_ALL, 0, 0, NULL),
    B("__mira_string_to_int", "str->int", "mira_str_to_int", 1, 1, SSA_TYPE_INT, STDLIB_PLATFORM_ALL, 0, 0, NULL),
    B("__mira_int_to_string", "int->str", "mira_to_str", 1, 1, SSA_TYPE_PTR, STDLIB_PLATFORM_ALL, 0, 1, "mem_free"),
    B("__mira_int_to_float", "to-float", "mira_int_to_float", 1, 1, SSA_TYPE_FLOAT, STDLIB_PLATFORM_ALL, 0, 0, NULL),
    BL("__mira_file_read", "file-read", "mira_file_read", 1, 1, SSA_TYPE_PTR, STDLIB_PLATFORM_ALL, 0, 1, "mem_free", "mira_file_read", SSA_TYPE_INT),
    B("__mira_file_write", "file-write", "mira_file_write", 2, 0, SSA_TYPE_VOID, STDLIB_PLATFORM_ALL, 0, 0, NULL),
    B("__mira_file_append", "file-append", "mira_file_append", 2, 0, SSA_TYPE_VOID, STDLIB_PLATFORM_ALL, 0, 0, NULL),
    B("__mira_file_exists", "file-exists", "mira_file_exists", 1, 1, SSA_TYPE_INT, STDLIB_PLATFORM_ALL, 0, 0, NULL),
    B("__mira_file_delete", "file-delete", "mira_file_delete", 1, 0, SSA_TYPE_VOID, STDLIB_PLATFORM_ALL, 0, 0, NULL),
    B("__mira_math_abs", "abs", "mira_abs", 1, 1, SSA_TYPE_INT, STDLIB_PLATFORM_ALL, 0, 0, NULL),
    B("__mira_math_min", "min", "mira_min", 2, 1, SSA_TYPE_INT, STDLIB_PLATFORM_ALL, 0, 0, NULL),
    B("__mira_math_max", "max", "mira_max", 2, 1, SSA_TYPE_INT, STDLIB_PLATFORM_ALL, 0, 0, NULL),
    B("__mira_math_sqrt", "sqrt", "mira_f_sqrt", 1, 1, SSA_TYPE_FLOAT, STDLIB_PLATFORM_ALL, 0, 0, NULL),
    B("__mira_math_pow", "pow", "mira_f_pow", 2, 1, SSA_TYPE_FLOAT, STDLIB_PLATFORM_ALL, 0, 0, NULL),
    B("__mira_random_next", "rand", "mira_random", 0, 1, SSA_TYPE_INT, STDLIB_PLATFORM_ALL, 0, 0, NULL),
    B("__mira_random_seed", "srand", "mira_random_seed", 1, 0, SSA_TYPE_VOID, STDLIB_PLATFORM_ALL, 0, 0, NULL),
    B("__mira_random_range", NULL, "mira_random_range", 2, 1, SSA_TYPE_INT, STDLIB_PLATFORM_ALL, 0, 0, NULL),
    B("__mira_io_read", "read", "mira_read_int", 0, 1, SSA_TYPE_INT, STDLIB_PLATFORM_ALL, 0, 0, NULL),
    BL("__mira_io_input", "input", "mira_input", 0, 1, SSA_TYPE_PTR, STDLIB_PLATFORM_ALL, 0, 1, "mem_free", "mira_input", SSA_TYPE_INT),
    B("__mira_time_sleep", "sleep", "mira_time_sleep", 1, 0, SSA_TYPE_VOID, STDLIB_PLATFORM_ALL, 1, 0, NULL),
    B("__mira_time_now", NULL, "mira_time_now", 0, 1, SSA_TYPE_INT, STDLIB_PLATFORM_ALL, 0, 0, NULL),
    B("__mira_time_ticks", "clock", "mira_time_ms", 0, 1, SSA_TYPE_INT, STDLIB_PLATFORM_ALL, 0, 0, NULL),
    B("__mira_time_ticks_ns", "clock-ns", "mira_win_tick_ns", 0, 1, SSA_TYPE_INT, STDLIB_PLATFORM_ALL, 0, 0, NULL),
    B("__mira_task_spawn", "spawn", "mira_async_start", 1, 0, SSA_TYPE_VOID, STDLIB_PLATFORM_WINDOWS, 1, 0, NULL),
    B("__mira_task_yield", "wait", "mira_async_yield", 0, 0, SSA_TYPE_VOID, STDLIB_PLATFORM_WINDOWS, 1, 0, NULL),
    B("__mira_channel_new", "channel", "mira_channel_new_value", 1, 1, SSA_TYPE_PTR, STDLIB_PLATFORM_ALL, 0, 0, NULL),
    B("__mira_channel_send", "send", "mira_channel_send_value", 2, 1, SSA_TYPE_INT, STDLIB_PLATFORM_ALL, 1, 0, NULL),
    B("__mira_channel_recv", "recv", "mira_channel_recv_value", 1, 1, SSA_TYPE_INT, STDLIB_PLATFORM_ALL, 1, 0, NULL),
    BL("__mira_memory_allocate", "allocate", "mem_alloc", 1, 1, SSA_TYPE_PTR, STDLIB_PLATFORM_ALL, 0, 1, "mem_free", "mem_alloc", SSA_TYPE_INT),
    B("__mira_memory_free", "free", "mem_free", 1, 0, SSA_TYPE_VOID, STDLIB_PLATFORM_ALL, 0, 0, NULL),
    B("__mira_string_find", "str-find", "mira_str_find", 2, 1, SSA_TYPE_INT, STDLIB_PLATFORM_ALL, 0, 0, NULL),
    B("__mira_io_newline", "cr", "mira_cr", 0, 0, SSA_TYPE_VOID, STDLIB_PLATFORM_ALL, 0, 0, NULL),
    B("__mira_process_shell", "shell", "mira_win_shell", 1, 1, SSA_TYPE_INT, STDLIB_PLATFORM_WINDOWS, 0, 0, NULL),
    BL("__mira_process_env", "env", "mira_win_env", 1, 1, SSA_TYPE_PTR, STDLIB_PLATFORM_WINDOWS, 0, 1, "mem_free", "mira_win_env", SSA_TYPE_INT),
    BL("__mira_windows_clipboard", "clipboard", "mira_win_clip_get", 0, 1, SSA_TYPE_PTR, STDLIB_PLATFORM_WINDOWS, 0, 1, "mem_free", "mira_win_clip_get", SSA_TYPE_INT),
    B("__mira_process_id", "pid", "mira_win_pid", 0, 1, SSA_TYPE_INT, STDLIB_PLATFORM_WINDOWS, 0, 0, NULL),
    B("__mira_task_parallel", "parallel", "mira_parallel_start", 1, 1, SSA_TYPE_INT, STDLIB_PLATFORM_WINDOWS, 1, 0, NULL),
    B("__mira_task_parallel_join", "join", "mira_parallel_join", 1, 0, SSA_TYPE_VOID, STDLIB_PLATFORM_WINDOWS, 1, 0, NULL),
    B("__mira_task_go", "go", "mira_go_start0", 1, 1, SSA_TYPE_INT, STDLIB_PLATFORM_ALL, 0, 0, NULL),
    B("__mira_task_join", "join-task", "mira_go_join", 1, 0, SSA_TYPE_VOID, STDLIB_PLATFORM_ALL, 1, 0, NULL),
    B("__mira_task_yield_join", "yield-task", "mira_go_yield", 0, 0, SSA_TYPE_VOID, STDLIB_PLATFORM_ALL, 1, 0, NULL),
    B("__mira_task_wait_all", "wait-all", "mira_go_wait_all", 0, 0, SSA_TYPE_VOID, STDLIB_PLATFORM_ALL, 1, 0, NULL),
    B("__mira_channel_close", "close-channel", "mira_channel_close_value", 1, 1, SSA_TYPE_INT, STDLIB_PLATFORM_ALL, 0, 0, NULL),
    B("__mira_channel_free", "free-channel", "mira_channel_free_value", 1, 0, SSA_TYPE_VOID, STDLIB_PLATFORM_ALL, 0, 0, NULL),
    B("__mira_error_throw", "throw", "mira_throw", 1, 0, SSA_TYPE_VOID, STDLIB_PLATFORM_ALL, 0, 0, NULL),
    BL("__mira_error_current", "get-error", "mira_get_error", 0, 1, SSA_TYPE_PTR, STDLIB_PLATFORM_ALL, 0, 0, NULL, "mira_get_error", SSA_TYPE_INT),
    B("__mira_memory_move", "move", "mem_move", 3, 0, SSA_TYPE_VOID, STDLIB_PLATFORM_ALL, 0, 0, NULL),
    B("__mira_memory_erase", "erase", "mem_erase", 2, 0, SSA_TYPE_VOID, STDLIB_PLATFORM_ALL, 0, 0, NULL),
    B("__mira_memory_dump", "dump", "mira_mem_dump", 2, 0, SSA_TYPE_VOID, STDLIB_PLATFORM_ALL, 0, 0, NULL),
    B("__mira_process_exit", NULL, "exit", 1, 0, SSA_TYPE_VOID, STDLIB_PLATFORM_ALL, 0, 0, NULL),
    B("__mira_windows_message_box", "msgbox", "mira_win_msgbox", 2, 0, SSA_TYPE_VOID, STDLIB_PLATFORM_WINDOWS, 0, 0, NULL),
};

typedef struct {
    const char *name;
    const char *source_name;
} LegacyAlias;

static const LegacyAlias legacy_aliases[] = {
    {"str-cat", "__mira_string_concat"},
    {"to-int", "__mira_string_to_int"},
    {"to-str", "__mira_int_to_string"},
    {"nl", "__mira_io_newline"},
    {"md", "__mira_memory_dump"},
};

static const StdlibBuiltin *lookup_field(const char *name, size_t len, int legacy) {
    for (size_t i = 0; i < sizeof(builtins) / sizeof(builtins[0]); ++i) {
        const char *candidate = legacy ? builtins[i].legacy_name : builtins[i].source_name;
        if (candidate && strlen(candidate) == len && memcmp(candidate, name, len) == 0)
            return &builtins[i];
    }
    return NULL;
}

const StdlibBuiltin *stdlib_builtin_lookup(const char *name, size_t len) {
    return lookup_field(name, len, 0);
}

const StdlibBuiltin *stdlib_legacy_builtin_lookup(const char *name, size_t len) {
    const StdlibBuiltin *builtin = lookup_field(name, len, 1);
    if (builtin) return builtin;
    for (size_t i = 0; i < sizeof(legacy_aliases) / sizeof(legacy_aliases[0]); ++i) {
        if (strlen(legacy_aliases[i].name) == len &&
            memcmp(legacy_aliases[i].name, name, len) == 0)
            return stdlib_builtin_lookup(legacy_aliases[i].source_name,
                strlen(legacy_aliases[i].source_name));
    }
    return NULL;
}
const StdlibBuiltin *stdlib_runtime_builtin_lookup(const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < sizeof(builtins) / sizeof(builtins[0]); ++i)
        if (strcmp(builtins[i].runtime_symbol, name) == 0 ||
            strcmp(builtins[i].legacy_runtime_symbol, name) == 0)
            return &builtins[i];
    return NULL;
}

int stdlib_builtin_is_available(const StdlibBuiltin *builtin) {
    if (!builtin) return 0;
#ifdef _WIN32
    return (builtin->platform_mask & STDLIB_PLATFORM_WINDOWS) != 0;
#else
    return (builtin->platform_mask & STDLIB_PLATFORM_LINUX) != 0;
#endif
}