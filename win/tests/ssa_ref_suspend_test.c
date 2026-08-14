#include "ir_ssa.h"
#include <stdio.h>

static void init_call(SsaInst *call, SsaOperand *operands, const char *symbol) {
    call->IrNode = SSA_OP_CALL;
    call->operands = operands;
    call->operand_count = 1;
    operands[0].kind = SSA_OPND_SYM;
    operands[0].u.sym = (char *)symbol;
}

int main(void) {
    SsaModule module = {0};
    SsaFunction pure = {0}, sleeper = {0}, caller = {0};
    SsaFunction *functions[] = { &pure, &sleeper, &caller };
    SsaBasicBlock pure_block = {0}, sleep_block = {0}, caller_block = {0};
    SsaBasicBlock *pure_blocks[] = { &pure_block };
    SsaBasicBlock *sleep_blocks[] = { &sleep_block };
    SsaBasicBlock *caller_blocks[] = { &caller_block };
    SsaInst sleep_call = {0}, caller_call = {0};
    SsaOperand sleep_operands[1] = {0}, caller_operands[1] = {0};

    pure.name = "pure";
    pure.block_count = 1;
    pure.blocks = pure_blocks;
    sleeper.name = "sleeper";
    sleeper.block_count = 1;
    sleeper.blocks = sleep_blocks;
    caller.name = "caller";
    caller.block_count = 1;
    caller.blocks = caller_blocks;
    init_call(&sleep_call, sleep_operands, "mira_win_sleep");
    sleep_block.inst_head = sleep_block.inst_tail = &sleep_call;
    init_call(&caller_call, caller_operands, "sleeper");
    caller_block.inst_head = caller_block.inst_tail = &caller_call;
    module.func_count = 3;
    module.functions = functions;

    ssa_ref_analyze_module(&module);
    if (!pure.ref_effect || pure.ref_effect->may_suspend) return 1;
    if (!sleeper.ref_effect || !sleeper.ref_effect->may_suspend) return 2;
    if (!caller.ref_effect || !caller.ref_effect->may_suspend) return 3;
    ssa_ref_free_module(&module);
    puts("ssa_ref_suspend_test: PASS");
    return 0;
}
