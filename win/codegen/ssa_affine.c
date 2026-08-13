#include "ir_ssa.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

static SsaAffineFact constant_fact(uint64_t value) {
    SsaAffineFact fact = {0};
    fact.constant = value;
    fact.proven = true;
    return fact;
}

static bool operand_fact(const SsaOperand *operand,
                         const SsaAffineFact *facts, size_t fact_count,
                         SsaAffineFact *result) {
    if (operand->kind == SSA_OPND_IMM) {
        *result = constant_fact((uint64_t)operand->u.imm);
        return true;
    }
    if (operand->kind != SSA_OPND_VREG || operand->u.vreg >= fact_count ||
        !facts[operand->u.vreg].proven) {
        return false;
    }
    *result = facts[operand->u.vreg];
    return true;
}

static bool compatible_bases(VReg left, VReg right) {
    return left == 0 || right == 0 || left == right;
}

static VReg merged_base(VReg left, VReg right) {
    return left != 0 ? left : right;
}

static bool resolve_constant(const SsaFunction *func,
                             const SsaOperand *operand, uint64_t *value) {
    if (operand->kind == SSA_OPND_IMM) {
        *value = (uint64_t)operand->u.imm;
        return true;
    }
    if (operand->kind != SSA_OPND_VREG ||
        operand->u.vreg >= (VReg)func->vreg_defs_cap) return false;
    SsaInst *def = func->vreg_defs[operand->u.vreg];
    if (!def || def->IrNode != SSA_OP_IMM ||
        def->op1.kind != SSA_OPND_IMM) return false;
    *value = (uint64_t)def->op1.u.imm;
    return true;
}

static bool affine_analyze_masked(const SsaFunction *func,
                                  SsaAffineFact *facts, size_t fact_count,
                                  const uint32_t *uses,
                                  bool permit_mask, uint64_t mask) {
    if (!func || !facts || !func->vreg_defs ||
        fact_count < (size_t)func->next_vreg) {
        return false;
    }
    memset(facts, 0, fact_count * sizeof(*facts));

    bool chained = func->block_count > 0 && func->blocks;
    int block_index = 0;
    SsaInst *cursor = chained ? func->blocks[0]->inst_head : NULL;
    VReg fallback_reg = 1;
    while ((chained && block_index < func->block_count) ||
           (!chained && fallback_reg < func->next_vreg)) {
        SsaInst *inst = NULL;
        VReg reg = 0;
        if (chained) {
            while (!cursor && ++block_index < func->block_count)
                cursor = func->blocks[block_index]->inst_head;
            if (block_index >= func->block_count) break;
            inst = cursor;
            cursor = cursor->next;
            reg = inst->dst;
        } else {
            reg = fallback_reg++;
            inst = reg < (VReg)func->vreg_defs_cap
                       ? func->vreg_defs[reg] : NULL;
        }
        if (!inst || inst->dst != reg || inst->type != SSA_TYPE_INT) continue;
        if (reg == 0 || reg >= fact_count) continue;

        SsaAffineFact left = {0};
        SsaAffineFact right = {0};
        SsaAffineFact result = {0};
        switch (inst->IrNode) {
        case SSA_OP_IMM:
            if (inst->op1.kind == SSA_OPND_IMM) {
                result = constant_fact((uint64_t)inst->op1.u.imm);
                result.instruction_count = 1;
            }
            break;
        case SSA_OP_LOAD_PARAM:
        case SSA_OP_LOAD_VAR:
            result.base = reg;
            result.coefficient = 1;
            result.instruction_count = 1;
            result.proven = true;
            break;
        case SSA_OP_COPY:
            if (operand_fact(&inst->op1, facts, fact_count, &result)) {
                result.instruction_count++;
            }
            break;
        case SSA_OP_ADD:
        case SSA_OP_SUB:
            if (!operand_fact(&inst->op1, facts, fact_count, &left) ||
                !operand_fact(&inst->op2, facts, fact_count, &right) ||
                !compatible_bases(left.base, right.base)) {
                break;
            }
            result.base = merged_base(left.base, right.base);
            if (inst->IrNode == SSA_OP_ADD) {
                result.coefficient = left.coefficient + right.coefficient;
                result.constant = left.constant + right.constant;
            } else {
                result.coefficient = left.coefficient - right.coefficient;
                result.constant = left.constant - right.constant;
            }
            result.instruction_count = left.instruction_count +
                                       right.instruction_count + 1;
            result.proven = true;
            break;
        case SSA_OP_MUL:
            if (!operand_fact(&inst->op1, facts, fact_count, &left) ||
                !operand_fact(&inst->op2, facts, fact_count, &right)) {
                break;
            }
            if (left.base == 0) {
                result.base = right.base;
                result.coefficient = right.coefficient * left.constant;
                result.constant = right.constant * left.constant;
            } else if (right.base == 0) {
                result.base = left.base;
                result.coefficient = left.coefficient * right.constant;
                result.constant = left.constant * right.constant;
            } else {
                break;
            }
            result.instruction_count = left.instruction_count +
                                       right.instruction_count + 1;
            result.proven = true;
            break;
        case SSA_OP_AND: {
            uint64_t current_mask = 0;
            if (permit_mask && uses && uses[reg] == 1 &&
                resolve_constant(func, &inst->op2, &current_mask) &&
                current_mask == mask &&
                operand_fact(&inst->op1, facts, fact_count, &result)) {
                result.instruction_count++;
            }
            break;
        }
        default:
            break;
        }
        facts[reg] = result;
    }
    return true;
}

bool ssa_affine_analyze(const SsaFunction *func, SsaAffineFact *facts,
                        size_t fact_count) {
    return affine_analyze_masked(func, facts, fact_count, NULL, false, 0);
}

static void count_operand_use(const SsaOperand *operand, uint32_t *uses,
                              size_t count) {
    if (operand->kind == SSA_OPND_VREG && operand->u.vreg < count &&
        uses[operand->u.vreg] != UINT32_MAX) {
        uses[operand->u.vreg]++;
    }
}

static bool dead_stores_are_local_bookkeeping(const SsaFunction *func) {
    if (!func || func->block_count != 1) return false;
    for (SsaInst *inst = func->blocks[0]->inst_head; inst; inst = inst->next)
        if (inst->IrNode == SSA_OP_CALL || inst->IrNode == SSA_OP_ICALL ||
            inst->IrNode == SSA_OP_LOAD_VAR)
            return false;
    return true;
}

static void count_uses(const SsaFunction *func, uint32_t *uses, size_t count,
                       bool ignore_dead_stores) {
    for (int bi = 0; bi < func->block_count; ++bi) {
        for (SsaInst *inst = func->blocks[bi]->inst_head; inst;
             inst = inst->next) {
            if (ignore_dead_stores && inst->IrNode == SSA_OP_STORE_VAR)
                continue;
            count_operand_use(&inst->op1, uses, count);
            count_operand_use(&inst->op2, uses, count);
            for (int oi = 0; inst->operands && oi < inst->operand_count; ++oi)
                count_operand_use(&inst->operands[oi], uses, count);
        }
    }
}

static bool block_is_loop_member(const SsaFunction *func,
                                 const SsaBasicBlock *block) {
    if (!func || !block || block->id < 0) return true;
    for (int li = 0; li < func->loop_count; ++li) {
        const SsaLoopInfo *loop = &func->loops[li];
        if (loop->members && (size_t)block->id < loop->member_count &&
            loop->members[block->id]) return true;
    }
    return false;
}

static bool is_low_mask(uint64_t mask) {
    return mask != 0 && (mask & (mask + 1)) == 0;
}

static bool mark_region(const SsaFunction *func, VReg root,
                        const uint32_t *uses, bool *region,
                        VReg *worklist, size_t count, uint64_t mask,
                        uint32_t *instruction_count) {
    size_t pending = 0;
    worklist[pending++] = root;
    while (pending != 0) {
        VReg reg = worklist[--pending];
        if (reg == 0 || reg >= count || region[reg]) continue;
        SsaInst *inst = reg < (VReg)func->vreg_defs_cap
                            ? func->vreg_defs[reg]
                            : NULL;
        if (!inst) return false;
        if (inst->IrNode == SSA_OP_LOAD_PARAM ||
            inst->IrNode == SSA_OP_LOAD_VAR || inst->IrNode == SSA_OP_IMM)
            continue;
        switch (inst->IrNode) {
        case SSA_OP_COPY:
        case SSA_OP_ADD:
        case SSA_OP_SUB:
        case SSA_OP_MUL:
            break;
        case SSA_OP_AND: {
            uint64_t inner_mask = 0;
            if (!resolve_constant(func, &inst->op2, &inner_mask) ||
                inner_mask != mask) return false;
            break;
        }
        default:
            return false;
        }
        region[reg] = true;
        (*instruction_count)++;
        if (inst->op1.kind == SSA_OPND_VREG) {
            if (pending >= count) return false;
            worklist[pending++] = inst->op1.u.vreg;
        }
        if (inst->IrNode != SSA_OP_AND && inst->op2.kind == SSA_OPND_VREG) {
            if (pending >= count) return false;
            worklist[pending++] = inst->op2.u.vreg;
        }
    }
    return true;
}

static bool region_owns_all_uses(const SsaFunction *func, VReg terminal_source,
                                 const uint32_t *uses, const bool *region,
                                 VReg *internal_uses, size_t count) {
    memset(internal_uses, 0, count * sizeof(*internal_uses));
    if (terminal_source < count) internal_uses[terminal_source]++;
    for (VReg reg = 1; reg < (VReg)count; ++reg) {
        if (!region[reg]) continue;
        SsaInst *inst = reg < (VReg)func->vreg_defs_cap
                            ? func->vreg_defs[reg]
                            : NULL;
        if (!inst) return false;
        if (inst->op1.kind == SSA_OPND_VREG && inst->op1.u.vreg < count)
            internal_uses[inst->op1.u.vreg]++;
        if (inst->IrNode != SSA_OP_AND && inst->op2.kind == SSA_OPND_VREG &&
            inst->op2.u.vreg < count)
            internal_uses[inst->op2.u.vreg]++;
    }
    for (VReg reg = 1; reg < (VReg)count; ++reg)
        if (region[reg] && internal_uses[reg] != uses[reg]) return false;
    return true;
}

static SsaInst *new_inst(SsaBasicBlock *block, SsaOpcode opcode, VReg dst,
                         SsaOperand left, SsaOperand right) {
    SsaInst *inst = calloc(1, sizeof(*inst));
    if (!inst) return NULL;
    inst->IrNode = opcode;
    inst->type = SSA_TYPE_INT;
    inst->dst = dst;
    inst->op1 = left;
    inst->op2 = right;
    inst->operand_count = 2;
    inst->parent = block;
    return inst;
}

static SsaOperand make_vreg(VReg reg) {
    SsaOperand operand = {0};
    operand.kind = SSA_OPND_VREG;
    operand.u.vreg = reg;
    return operand;
}

static SsaOperand make_imm(uint64_t value) {
    SsaOperand operand = {0};
    operand.kind = SSA_OPND_IMM;
    operand.u.imm = (int64_t)value;
    return operand;
}

static void free_inst(SsaInst *inst) {
    if (!inst) return;
    free(inst->operands);
    free(inst);
}

static void unlink_and_free(SsaBasicBlock *block, SsaInst *inst) {
    if (inst->prev) inst->prev->next = inst->next;
    else block->inst_head = inst->next;
    if (inst->next) inst->next->prev = inst->prev;
    else block->inst_tail = inst->prev;
    free_inst(inst);
}

static void rebuild_defs(SsaFunction *func) {
    memset(func->vreg_defs, 0,
           (size_t)func->vreg_defs_cap * sizeof(*func->vreg_defs));
    for (int bi = 0; bi < func->block_count; ++bi)
        for (SsaInst *inst = func->blocks[bi]->inst_head; inst;
             inst = inst->next)
            if (inst->dst) func->vreg_defs[inst->dst] = inst;
}

bool ssa_opt_affine_collapse(SsaFunction *func) {
    if (!func || !func->vreg_defs || func->next_vreg <= 1) return false;
    size_t count = (size_t)func->next_vreg;
    uint32_t *uses = calloc(count, sizeof(*uses));
    SsaAffineFact *facts = calloc(count, sizeof(*facts));
    bool *region = calloc(count, sizeof(*region));
    VReg *worklist = malloc(count * sizeof(*worklist));
    if (!uses || !facts || !region || !worklist) {
        free(uses); free(facts); free(region); free(worklist);
        return false;
    }
    bool ignore_dead_stores = dead_stores_are_local_bookkeeping(func);
    count_uses(func, uses, count, ignore_dead_stores);

    for (int bi = 0; bi < func->block_count; ++bi) {
        SsaBasicBlock *block = func->blocks[bi];
        if (block_is_loop_member(func, block)) continue;
        for (SsaInst *terminal = block->inst_head; terminal;
             terminal = terminal->next) {
            uint64_t mask = 0;
            if (terminal->IrNode != SSA_OP_AND || terminal->dst == 0 ||
                terminal->op1.kind != SSA_OPND_VREG ||
                !resolve_constant(func, &terminal->op2, &mask) ||
                !is_low_mask(mask)) continue;

            if (!affine_analyze_masked(func, facts, count, uses, true, mask))
                continue;
            VReg source = terminal->op1.u.vreg;
            if (source >= count || !facts[source].proven ||
                facts[source].base == 0) continue;

            memset(region, 0, count * sizeof(*region));
            uint32_t old_count = 1;
            if (!mark_region(func, source, uses, region, worklist, count, mask,
                             &old_count)) continue;
            if (!region_owns_all_uses(func, source, uses, region, worklist,
                                      count)) continue;

            const SsaAffineFact fact = facts[source];
            uint32_t new_count = 1u + (fact.coefficient != 1u) +
                                 (fact.constant != 0u);
            uint32_t new_pressure = func->estimated_scalar_pressure;
            bool collapse = decision_choose_affine(old_count, new_count,
                                       old_count * 4u,
                                       new_count * 4u,
                                       func->estimated_scalar_pressure,
                                       new_pressure, DECISION_SCALE, NULL) ==
                DECISION_AFFINE_COLLAPSE;
            if (!collapse) continue;

            uint32_t temporary_count = new_count - 1;
            if (temporary_count > UINT32_MAX - func->next_vreg) continue;
            VReg first_new = func->next_vreg;
            size_t needed = (size_t)func->next_vreg + temporary_count;
            int new_cap = func->vreg_defs_cap;
            while ((size_t)new_cap < needed) {
                if (new_cap > INT_MAX / 2) { new_cap = 0; break; }
                new_cap = new_cap ? new_cap * 2 : 64;
            }
            SsaInst **new_defs = NULL;
            if (new_cap != func->vreg_defs_cap) {
                if (new_cap == 0) continue;
                new_defs = calloc((size_t)new_cap, sizeof(*new_defs));
                if (!new_defs) continue;
            }

            SsaInst *replacement[3] = {0};
            uint32_t made = 0;
            VReg value = fact.base;
            if (fact.coefficient != 1) {
                VReg dst = first_new + made;
                replacement[made] = new_inst(block, SSA_OP_MUL, dst,
                                              make_vreg(value),
                                              make_imm(fact.coefficient));
                value = dst;
                made++;
            }
            if (fact.constant != 0) {
                VReg dst = first_new + made;
                replacement[made] = new_inst(block, SSA_OP_ADD, dst,
                                              make_vreg(value),
                                              make_imm(fact.constant));
                value = dst;
                made++;
            }
            replacement[made++] = new_inst(block, SSA_OP_AND, terminal->dst,
                                            make_vreg(value), make_imm(mask));
            bool allocation_ok = made == new_count;
            for (uint32_t ri = 0; ri < made; ++ri)
                if (!replacement[ri]) allocation_ok = false;
            if (!allocation_ok) {
                for (uint32_t ri = 0; ri < made; ++ri) free_inst(replacement[ri]);
                free(new_defs);
                continue;
            }

            SsaInst *before = terminal->prev;
            SsaInst *after = terminal->next;
            for (uint32_t ri = 0; ri < made; ++ri) {
                replacement[ri]->prev = ri ? replacement[ri - 1] : before;
                replacement[ri]->next = ri + 1 < made ? replacement[ri + 1] : after;
            }
            if (before) before->next = replacement[0];
            else block->inst_head = replacement[0];
            if (after) after->prev = replacement[made - 1];
            else block->inst_tail = replacement[made - 1];
            free_inst(terminal);

            if (ignore_dead_stores) {
                SsaInst *inst = block->inst_head;
                while (inst) {
                    SsaInst *next = inst->next;
                    if (inst->IrNode == SSA_OP_STORE_VAR &&
                        inst->op1.kind == SSA_OPND_VREG &&
                        inst->op1.u.vreg < count && region[inst->op1.u.vreg])
                        unlink_and_free(block, inst);
                    inst = next;
                }
            }

            for (VReg reg = 1; reg < (VReg)count; ++reg) {
                if (!region[reg]) continue;
                SsaInst *inst = func->vreg_defs[reg];
                if (inst) unlink_and_free(inst->parent, inst);
            }
            if (new_defs) {
                free(func->vreg_defs);
                func->vreg_defs = new_defs;
                func->vreg_defs_cap = new_cap;
            }
            func->next_vreg += temporary_count;
            rebuild_defs(func);
            free(uses); free(facts); free(region); free(worklist);
            return true;
        }
    }
    free(uses); free(facts); free(region); free(worklist);
    return false;
}
