#include <assert.h>
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
    SsaFunction caller = {0}, callee = {0};
    SsaFunction *functions[] = {&caller, &callee};
    SsaBasicBlock block = {0};
    SsaBasicBlock *blocks[] = {&block};
    SsaOperand operands[1] = {0};
    SsaInst call = {0};

    caller.name = copy_name("caller");
    callee.name = copy_name("callee");
    caller.blocks = blocks;
    caller.block_count = 1;
    operands[0].kind = SSA_OPND_SYM;
    operands[0].u.sym = callee.name;
    call.IrNode = SSA_OP_CALL;
    call.operands = operands;
    call.operand_count = 1;
    block.inst_head = block.inst_tail = &call;
    mod.functions = functions;
    mod.func_count = mod.func_cap = 2;
    mod.function_epoch = 1;

    assert(ssa_function_index_rebuild(&mod));
    assert(ssa_function_index_rebuild_call_facts(&mod));
    assert(ssa_function_index_direct_calls(&mod, &callee) == 1);
    assert(ssa_function_index_is_referenced(&mod, &callee));
    assert(!ssa_function_index_is_leaf(&mod, &caller));
    assert(ssa_function_index_is_leaf(&mod, &callee));

    ssa_function_index_invalidate_call_facts(&mod);
    assert(ssa_function_index_direct_calls(&mod, &callee) == 0);
    assert(ssa_function_index_is_referenced(&mod, &callee));
    assert(!ssa_function_index_is_leaf(&mod, &caller));

    ssa_function_index_free(&mod);
    free(caller.name);
    free(callee.name);
    return 0;
}
