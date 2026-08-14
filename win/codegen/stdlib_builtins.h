#ifndef MIRA_STDLIB_BUILTINS_H
#define MIRA_STDLIB_BUILTINS_H

#include <stddef.h>
#include "ir_ssa.h"

enum {
    STDLIB_PLATFORM_ALL = 0x03,
    STDLIB_PLATFORM_WINDOWS = 0x01,
    STDLIB_PLATFORM_LINUX = 0x02
};

typedef struct {
    const char *source_name;
    const char *legacy_name;
    const char *runtime_symbol;
    int arity;
    int result_count;
    SsaType result_type;
    unsigned platform_mask;
    int may_suspend;
    int owned_result;
    const char *free_func_name;
    const char *legacy_runtime_symbol;
    SsaType legacy_result_type;
} StdlibBuiltin;

const StdlibBuiltin *stdlib_builtin_lookup(const char *name, size_t len);
const StdlibBuiltin *stdlib_legacy_builtin_lookup(const char *name, size_t len);
const StdlibBuiltin *stdlib_runtime_builtin_lookup(const char *name);
int stdlib_builtin_is_available(const StdlibBuiltin *builtin);

#endif