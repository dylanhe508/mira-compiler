#include "ir_ssa.h"

static bool operand_is_float(const SsaOperand *operand) {
    return operand && operand->kind == SSA_OPND_FIMM;
}

bool ssa_module_has_float_ops(const SsaModule *mod) {
    if (!mod || !mod->functions || mod->func_count <= 0) return false;
    for (int fi = 0; fi < mod->func_count; ++fi) {
        const SsaFunction *func = mod->functions[fi];
        if (!func || !func->blocks) continue;
        for (int bi = 0; bi < func->block_count; ++bi) {
            const SsaBasicBlock *block = func->blocks[bi];
            if (!block) continue;
            for (const SsaInst *inst = block->inst_head; inst; inst = inst->next) {
                if (inst->type == SSA_TYPE_FLOAT ||
                    (inst->IrNode >= SSA_OP_FADD && inst->IrNode <= SSA_OP_FPTOSI) ||
                    operand_is_float(&inst->op1) || operand_is_float(&inst->op2))
                    return true;
                for (int oi = 0; inst->operands && oi < inst->operand_count; ++oi)
                    if (operand_is_float(&inst->operands[oi])) return true;
            }
        }
    }
    return false;
}

bool ssa_function_needs_preinline_cleanup(const SsaFunction *func) {
    if (!func || !func->blocks || func->block_count != 1 || !func->blocks[0])
        return false;
    const SsaBasicBlock *block = func->blocks[0];
    for (const SsaInst *inst = block->inst_head; inst; inst = inst->next)
        if (inst->IrNode == SSA_OP_CALL || inst->IrNode == SSA_OP_ICALL)
            return false;
    for (int si = 0; si < block->succ_count; ++si)
        if (block->succs && block->succs[si] == block) return false;
    return true;
}

bool ssa_function_has_signed_div(const SsaFunction *func) {
    if (!func) return false;
    for (int bi = 0; bi < func->block_count; ++bi) {
        const SsaBasicBlock *block = func->blocks[bi];
        if (!block) continue;
        for (const SsaInst *inst = block->inst_head; inst; inst = inst->next)
            if (inst->IrNode == SSA_OP_SDIV) return true;
    }
    return false;
}
