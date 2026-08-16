#include "../mira.h"
#include "../codegen/ir_ssa.h"

#include <stdio.h>

int main(void) {
    SsaInst owned = {0};
    ssa_inst_apply_checked_ownership(&owned, MIRA_OWNERSHIP_OWNED, "mem_free");
    if (owned.ownership != SSA_OWNERSHIP_OWNED || !owned.needs_free ||
        !owned.free_func_name)
        return 1;

    SsaInst mixed = {0};
    ssa_inst_apply_checked_ownership(&mixed, MIRA_OWNERSHIP_MAYBE_OWNED,
                                     "mem_free");
    if (mixed.ownership != SSA_OWNERSHIP_MAYBE_OWNED || mixed.needs_free)
        return 2;

    SsaInst borrowed = {0};
    ssa_inst_apply_checked_ownership(&borrowed, MIRA_OWNERSHIP_BORROWED, NULL);
    if (borrowed.ownership != SSA_OWNERSHIP_BORROWED || borrowed.needs_free)
        return 3;

    puts("OWNERSHIP SSA METADATA PASS");
    return 0;
}
