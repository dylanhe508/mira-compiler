#include "ir_ssa.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    SsaModule module = {0};
    SsaFunction function = {0};
    SsaBasicBlock block = {0};
    SsaBasicBlock *blocks[] = { &block };
    SsaFunction *functions[] = { &function };
    SsaInst allocation = {0};
    SsaInst send = {0};
    SsaOperand send_operands[3] = {0};

    function.name = "test_send_escape";
    function.next_vreg = 2;
    function.block_count = 1;
    function.blocks = blocks;
    block.inst_head = &allocation;
    block.inst_tail = &send;

    allocation.IrNode = SSA_OP_ALLOCA;
    allocation.dst = 1;
    allocation.next = &send;
    allocation.parent = &block;
    send.prev = &allocation;
    send.parent = &block;
    send.IrNode = SSA_OP_CALL;
    send.operands = send_operands;
    send.operand_count = 3;
    send_operands[0].kind = SSA_OPND_SYM;
    send_operands[0].u.sym = "mira_channel_send_value";
    send_operands[1].kind = SSA_OPND_IMM;
    send_operands[1].u.imm = 123;
    send_operands[2].kind = SSA_OPND_VREG;
    send_operands[2].u.vreg = 1;

    module.func_count = 1;
    module.functions = functions;
    ssa_ref_analyze_module(&module);

    if (!(function.ref_facts[1].flags & SSA_REF_ESCAPED)) return 1;
    if (!(function.ref_facts[1].flags & SSA_REF_SHARED)) return 2;
    if (function.ref_facts[1].flags & SSA_REF_UNIQUE) return 3;
    if (!function.ref_effect || !function.ref_effect->has_concurrency_effect) return 4;
    if (!send.ref_analyzed || !send.ref_observable) return 5;

    ssa_ref_free_module(&module);
    puts("ssa_ref_concurrency_test: PASS");
    return 0;
}
