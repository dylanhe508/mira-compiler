/* ssa_opt.c - SSA 涓撳睘浼樺寲璺嚎 */
#include "ir_ssa.h"
#include "ir.h"
#include "decision.h"
#include "../mira.h"
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>

/* Forward declaration for War 5 inlining */
bool ssa_opt_inline(SsaModule *mod);

/* Internal recurrence slots live in the program's reserved variable tail.
 * They must be allocated module-wide: function-local slot zero would alias a
 * source variable and was the cause of the first affine prototype's wrong
 * results. */
static int ssa_internal_next_slot = -1;

static bool ssa_block_belongs_to_function(const SsaFunction *func,
                                           const SsaBasicBlock *block) {
    if (!func || !block) return false;
    for (int i = 0; i < func->block_count; ++i)
        if (func->blocks[i] == block) return true;
    return false;
}

static void ssa_renumber_blocks(SsaFunction *func) {
    if (!func) return;
    for (int i = 0; i < func->block_count; ++i)
        if (func->blocks[i]) func->blocks[i]->id = i;
}

static bool ssa_rebuild_cfg_from_terminators(SsaFunction *func) {
    if (!func) return false;
    for (int i = 0; i < func->block_count; ++i) {
        SsaBasicBlock *block = func->blocks[i];
        if (!block) return false;
        block->pred_count = 0;
        block->succ_count = 0;
    }
    for (int i = 0; i < func->block_count; ++i) {
        SsaBasicBlock *block = func->blocks[i];
        SsaInst *term = block->inst_tail;
        if (!term) continue;
        if (term->IrNode == SSA_OP_JMP) {
            if (term->op1.kind != SSA_OPND_BLOCK ||
                !ssa_block_belongs_to_function(func, term->op1.u.block))
                return false;
            ssa_add_edge(block, term->op1.u.block);
        } else if (term->IrNode == SSA_OP_BR) {
            if (!term->operands || term->operand_cap < 2) return false;
            for (int target = 0; target < 2; ++target) {
                if (term->operands[target].kind != SSA_OPND_BLOCK ||
                    !ssa_block_belongs_to_function(func,
                                                   term->operands[target].u.block))
                    return false;
                ssa_add_edge(block, term->operands[target].u.block);
            }
        }
    }
    return true;
}

static bool ssa_phi_has_predecessor(const SsaBasicBlock *block,
                                    const SsaBasicBlock *pred) {
    for (int i = 0; i < block->pred_count; ++i)
        if (block->preds[i] == pred) return true;
    return false;
}

static const char *ssa_rebuild_and_validate_current_chain(SsaFunction *func) {
    if (!func) return "null-function";
    if (func->next_vreg > (VReg)func->vreg_defs_cap) {
        int new_cap = func->vreg_defs_cap ? func->vreg_defs_cap : 64;
        while ((VReg)new_cap < func->next_vreg) new_cap *= 2;
        SsaInst **new_defs = realloc(func->vreg_defs,
                                     (size_t)new_cap * sizeof(*new_defs));
        if (!new_defs) return "vreg-def-table-allocation";
        func->vreg_defs = new_defs;
        func->vreg_defs_cap = new_cap;
    }
    if (func->vreg_defs && func->vreg_defs_cap > 0)
        memset(func->vreg_defs, 0,
               (size_t)func->vreg_defs_cap * sizeof(*func->vreg_defs));

    for (int bi = 0; bi < func->block_count; ++bi) {
        SsaBasicBlock *block = func->blocks[bi];
        if (!block || block->id != bi) return "non-contiguous-block-id";
        for (SsaInst *inst = block->inst_head; inst; inst = inst->next) {
            inst->parent = block;
            if (inst->dst > 0) {
                if (inst->dst >= func->next_vreg || !func->vreg_defs)
                    return "vreg-definition-out-of-range";
                if (func->vreg_defs[inst->dst])
                    return "duplicate-vreg-definition";
                func->vreg_defs[inst->dst] = inst;
            }
            if (inst->IrNode != SSA_OP_PHI) continue;
            if (!inst->operands || (inst->operand_count & 1) != 0)
                return "malformed-phi-operands";
            for (int oi = 0; oi < inst->operand_count; oi += 2) {
                if (inst->operands[oi].kind != SSA_OPND_VREG ||
                    inst->operands[oi].u.vreg >= func->next_vreg ||
                    inst->operands[oi + 1].kind != SSA_OPND_BLOCK)
                    return "malformed-phi-pair";
                if (!ssa_phi_has_predecessor(block,
                                             inst->operands[oi + 1].u.block))
                    return "phi-predecessor-not-cfg-edge";
            }
            for (int pi = 0; pi < block->pred_count; ++pi) {
                bool found = false;
                for (int oi = 1; oi < inst->operand_count; oi += 2)
                    if (inst->operands[oi].kind == SSA_OPND_BLOCK &&
                        inst->operands[oi].u.block == block->preds[pi]) {
                        found = true;
                        break;
                    }
                if (!found) return "phi-missing-predecessor";
            }
        }
    }
    return NULL;
}

static const char *ssa_rebuild_function_facts(SsaFunction *func) {
    ssa_renumber_blocks(func);
    if (!ssa_rebuild_cfg_from_terminators(func)) return "invalid-cfg-target";
    ssa_compute_dom_info(func);
    return ssa_rebuild_and_validate_current_chain(func);
}

static uint32_t decision_instruction_count(const SsaFunction *func) {
    uint64_t count = 0;
    if (!func) return 0;
    for (int bi = 0; bi < func->block_count; ++bi)
        for (SsaInst *inst = func->blocks[bi]->inst_head; inst; inst = inst->next)
            if (inst->IrNode != SSA_OP_COPY && count < UINT32_MAX) count++;
    return (uint32_t)count;
}

static bool decision_prefer_global_graph_coloring(
    const SsaFunction *func, int optimization_level) {
    const DecisionReferenceFacts *reference;
    if (!func || optimization_level != 3 ||
        func->estimated_scalar_pressure < 8) return false;
    reference = &func->decision_ref_facts;
    if (reference->value_count > 4096u) return false;
    if (!reference->has_unknown_effect) return true;
    if (reference->value_count == 0) return false;
    return reference->known_value_count >=
        (reference->value_count + 1u) / 2u;
}

void ssa_decision_refresh_plans(SsaModule *mod, int optimization_level,
                                int avx2_available, uint32_t generation) {
    if (!mod) return;
    for (int fi = 0; fi < mod->func_count; ++fi) {
        SsaFunction *func = mod->functions[fi];
        func->decision_ref_facts = ssa_ref_decision_facts(func);
        func->decision_plan = decision_function_plan(
            decision_instruction_count(func), optimization_level, avx2_available,
            func->estimated_scalar_pressure, func->estimated_float_pressure,
            &func->decision_ref_facts, generation);
        decision_pipeline_disable(&func->decision_plan.pipeline,
            getenv("MIRA_DECISION_DISABLE"));
        func->decision_plan.prefer_global_graph_coloring =
            decision_prefer_global_graph_coloring(func, optimization_level);
        for (int li = 0; li < func->loop_count; ++li) {
            SsaLoopInfo *loop = &func->loops[li];
            DecisionMemoryFacts memory = decision_memory_facts(
                (uint32_t)loop->memory_reads, (uint32_t)loop->memory_writes,
                loop->memory_accesses_known, loop->memory_may_alias,
                loop->has_unknown_call, loop->has_ownership_transfer);
            loop->decision_plan = decision_loop_plan(0,
                loop->decision_plan.body_instructions,
                (uint32_t)loop->backedge_count,
                func->estimated_scalar_pressure, avx2_available, &memory,
                func->decision_plan.pipeline.code_growth_budget);
        }
        if (getenv("MIRA_DECISION_DEBUG"))
            fprintf(stderr,
                "decision2.1 function=%s gen=%u ir=%u ref=%u/%u budget=%u veto=0x%x "
                "inline=%d vec=%d unroll=%d schedule=%d global_color=%d\n",
                func->name ? func->name : "?", generation,
                decision_instruction_count(func),
                func->decision_ref_facts.known_value_count,
                func->decision_ref_facts.value_count,
                func->decision_plan.reference_analysis_budget,
                func->decision_plan.reference_vetoes,
                func->decision_plan.allow_inline,
                func->decision_plan.pipeline.allow_vectorize,
                func->decision_plan.pipeline.allow_unroll,
                func->decision_plan.pipeline.allow_schedule,
                func->decision_plan.prefer_global_graph_coloring);
    }
}

static bool block_dominates(const SsaBasicBlock *dom, const SsaBasicBlock *node) {
    if (!dom || !node) return false;
    const SsaBasicBlock *cur = node;
    int guard = 0;
    while (cur && guard++ < 100000) {
        if (cur == dom) return true;
        if (cur->idom == cur) break;
        cur = cur->idom;
    }
    return false;
}

static bool ssa_licm_pure_integer_op(const SsaInst *inst) {
    if (!inst || inst->dst == 0 || inst->needs_free ||
        inst->type != SSA_TYPE_INT) return false;
    return inst->IrNode == SSA_OP_ADD || inst->IrNode == SSA_OP_SUB ||
        inst->IrNode == SSA_OP_MUL || inst->IrNode == SSA_OP_AND ||
        inst->IrNode == SSA_OP_OR || inst->IrNode == SSA_OP_XOR ||
        inst->IrNode == SSA_OP_SHL || inst->IrNode == SSA_OP_ASHR ||
        inst->IrNode == SSA_OP_LSHR || inst->IrNode == SSA_OP_NEG ||
        inst->IrNode == SSA_OP_NOT || inst->IrNode == SSA_OP_COPY;
}

static bool ssa_licm_operand_stable(SsaOperand operand, const bool *stable,
                                    VReg next_vreg) {
    return operand.kind == SSA_OPND_NONE || operand.kind == SSA_OPND_IMM ||
        (operand.kind == SSA_OPND_VREG && operand.u.vreg < next_vreg &&
         stable[operand.u.vreg]);
}

static bool cfg_cycle_visit(SsaFunction *func, int index, uint8_t *state) {
    state[index] = 1;
    SsaBasicBlock *bb = func->blocks[index];
    for (int s = 0; s < bb->succ_count; s++) {
        int next = -1;
        for (int i = 0; i < func->block_count; i++)
            if (func->blocks[i] == bb->succs[s]) { next = i; break; }
        if (next < 0) continue;
        if (state[next] == 1) return true;
        if (state[next] == 0 && cfg_cycle_visit(func, next, state)) return true;
    }
    state[index] = 2;
    return false;
}

static bool func_has_cfg_cycle(SsaFunction *func) {
    uint8_t *state = calloc((size_t)func->block_count, sizeof(*state));
    bool cyclic = false;
    for (int i = 0; i < func->block_count && !cyclic; i++)
        if (state[i] == 0) cyclic = cfg_cycle_visit(func, i, state);
    free(state);
    if (cyclic) return true;
    /* Some builder paths have not populated succs yet when module inlining
     * runs.  Inspect terminator targets as the authoritative fallback. */
    for (int bi = 0; bi < func->block_count; bi++) {
        SsaBasicBlock *bb = func->blocks[bi];
        for (SsaInst *inst = bb->inst_head; inst; inst = inst->next) {
            if (inst->IrNode == SSA_OP_JMP && inst->op1.kind == SSA_OPND_BLOCK &&
                inst->op1.u.block && inst->op1.u.block->id <= bb->id)
                return true;
            if (inst->IrNode == SSA_OP_BR && inst->operands) {
                int cap = inst->operand_cap > inst->operand_count ?
                          inst->operand_cap : inst->operand_count;
                for (int oi = 0; oi < cap; oi++)
                    if (inst->operands[oi].kind == SSA_OPND_BLOCK &&
                        inst->operands[oi].u.block && inst->operands[oi].u.block->id <= bb->id)
                        return true;
            }
        }
    }
    return false;
}

void ssa_analyze_loops(SsaFunction *func) {
    extern int mira_target_avx2;
    for (int i = 0; i < func->loop_count; ++i) free(func->loops[i].members);
    free(func->loops); func->loops = NULL; func->loop_count = 0;
    if (func->block_count < 2) return;
    func->loops = calloc((size_t)func->block_count, sizeof(SsaLoopInfo));
    for (int li = 0; li < func->block_count; ++li) {
        SsaBasicBlock *latch = func->blocks[li];
        SsaBasicBlock *header = NULL;
        for (int s = 0; s < latch->succ_count; ++s)
            if (block_dominates(latch->succs[s], latch)) {
                header = latch->succs[s]; break;
            }
        if (!header) continue;
        SsaLoopInfo *info = &func->loops[func->loop_count++];
        info->header = header; info->latch = latch;
        info->backedge_count = 1;
        info->induction_slot = -1;
        info->members = calloc((size_t)func->block_count, sizeof(bool));
        if (info->members) {
            SsaBasicBlock **stack = malloc((size_t)func->block_count * sizeof(*stack));
            int stack_count = 0;
            info->members[header->id] = true;
            info->members[latch->id] = true;
            if (stack && latch != header) stack[stack_count++] = latch;
            while (stack && stack_count != 0) {
                SsaBasicBlock *block = stack[--stack_count];
                for (int pi = 0; pi < block->pred_count; ++pi) {
                    SsaBasicBlock *pred = block->preds[pi];
                    if (!pred || info->members[pred->id] ||
                        !block_dominates(header, pred)) continue;
                    info->members[pred->id] = true;
                    if (pred != header) stack[stack_count++] = pred;
                }
            }
            free(stack);
        }
		info->memory_accesses_known = true;
		VReg memory_ptrs[32];
		size_t memory_widths[32];
		bool memory_writes[32];
		int memory_access_count = 0;
		bool ownership_sensitive = false;
        for (int bi = 0; bi < func->block_count; ++bi) {
            if (info->members && !info->members[bi]) continue;
            SsaBasicBlock *block = func->blocks[bi];
            for (int s = 0; s < block->succ_count; ++s) {
                SsaBasicBlock *succ = block->succs[s];
                if (!succ || (info->members && info->members[succ->id])) continue;
                if (info->exit_count == 0) {
                    info->exit = succ;
                    info->exit_count = 1;
                } else if (info->exit_count == 1 && info->exit != succ) {
                    info->exit = NULL;
                    info->exit_count = 2;
                }
            }
        }
        if (info->exit_count > 1) info->exit = NULL;

        uint32_t body_instructions = 0;
        for (int bi = 0; bi < func->block_count; ++bi) {
            if (info->members && !info->members[bi]) continue;
            info->member_count++;
            for (SsaInst *i = func->blocks[bi]->inst_head; i; i = i->next) {
                body_instructions++;
                if (i->IrNode == SSA_OP_LOAD || i->IrNode == SSA_OP_LOAD8) {
					info->memory_reads++;
					SsaOperand ptr = i->op1;
					if (ptr.kind != SSA_OPND_VREG || ptr.u.vreg >= func->ref_fact_count ||
						func->ref_facts[ptr.u.vreg].origin_kind == SSA_REF_ORIGIN_UNKNOWN) {
						info->memory_accesses_known = false;
					} else if (memory_access_count < 32) {
						memory_ptrs[memory_access_count] = ptr.u.vreg;
						memory_widths[memory_access_count] = i->IrNode == SSA_OP_LOAD8 ? 1u : 8u;
						memory_writes[memory_access_count++] = false;
					} else info->memory_accesses_known = false;
				}
                if (i->IrNode == SSA_OP_STORE || i->IrNode == SSA_OP_STORE8) {
                    info->memory_writes++;
					SsaOperand ptr = i->op2;
					if (ptr.kind != SSA_OPND_VREG || ptr.u.vreg >= func->ref_fact_count ||
						func->ref_facts[ptr.u.vreg].origin_kind == SSA_REF_ORIGIN_UNKNOWN) {
						info->memory_accesses_known = false;
					} else if (memory_access_count < 32) {
						memory_ptrs[memory_access_count] = ptr.u.vreg;
						memory_widths[memory_access_count] = i->IrNode == SSA_OP_STORE8 ? 1u : 8u;
						memory_writes[memory_access_count++] = true;
					} else info->memory_accesses_known = false;
                }
                if (i->IrNode == SSA_OP_CALL || i->IrNode == SSA_OP_ICALL)
                    info->has_unknown_call = true;
                if (i->needs_free) {
					info->has_ownership_transfer = true;
					ownership_sensitive = true;
				}
                if (i->IrNode != SSA_OP_STORE_VAR || i->op1.kind != SSA_OPND_VREG) continue;
                int slot = (int)i->op2.u.imm;
                VReg value = i->op1.u.vreg;
                SsaInst *add = (value < (VReg)func->vreg_defs_cap) ? func->vreg_defs[value] : NULL;
                if (!add || add->IrNode != SSA_OP_ADD) continue;
                SsaInst *a = (add->op1.kind == SSA_OPND_VREG && add->op1.u.vreg < (VReg)func->vreg_defs_cap) ? func->vreg_defs[add->op1.u.vreg] : NULL;
                SsaInst *b = (add->op2.kind == SSA_OPND_VREG && add->op2.u.vreg < (VReg)func->vreg_defs_cap) ? func->vreg_defs[add->op2.u.vreg] : NULL;
                if (a && a->IrNode == SSA_OP_LOAD_VAR && (int)a->op1.u.imm == slot &&
                    b && b->IrNode == SSA_OP_IMM && b->op1.kind == SSA_OPND_IMM) {
                    info->induction_slot = slot; info->step = b->op1.u.imm;
                } else if (b && b->IrNode == SSA_OP_LOAD_VAR && (int)b->op1.u.imm == slot &&
                           a && a->IrNode == SSA_OP_IMM && a->op1.kind == SSA_OPND_IMM) {
                    info->induction_slot = slot; info->step = a->op1.u.imm;
                }
            }
        }
		for (int a = 0; a < memory_access_count; ++a) {
			for (int b = a + 1; b < memory_access_count; ++b) {
				if (!memory_writes[a] && !memory_writes[b]) continue;
				SsaAliasResult alias = ssa_ref_alias(&func->ref_facts[memory_ptrs[a]], 0,
					memory_widths[a], &func->ref_facts[memory_ptrs[b]], 0,
					memory_widths[b]);
				if (alias != SSA_ALIAS_NONE) info->memory_may_alias = true;
			}
		}
		DecisionMemoryFacts memory = decision_memory_facts((uint32_t)info->memory_reads,
			(uint32_t)info->memory_writes, info->memory_accesses_known,
			info->memory_may_alias, info->has_unknown_call, ownership_sensitive);
		info->memory_reorder_safe = memory.reorder_safe;
		info->decision_plan = decision_loop_plan(0, body_instructions,
			(uint32_t)info->backedge_count, func->estimated_scalar_pressure,
			mira_target_avx2, &memory,
			func->decision_plan.pipeline.code_growth_budget);
		if (getenv("MIRA_DECISION_DEBUG"))
			fprintf(stderr, "decision2.1 loop function=%s header=%d blocks=%u backedges=%d exits=%d body=%u reads=%d writes=%d known=%d may_alias=%d unknown_call=%d reorder_safe=%d choice=%s factor=%d\n",
				func->name ? func->name : "?", header->id,
				(unsigned)info->member_count, info->backedge_count,
				info->exit_count, body_instructions, info->memory_reads,
				info->memory_writes, info->memory_accesses_known, info->memory_may_alias,
				info->has_unknown_call, info->memory_reorder_safe,
				decision_kind_name(info->decision_plan.preferred),
				info->decision_plan.parameter);
    }
}

/* ======================================================
 * PASS 1: 甯搁噺鎶樺彔 (Constant Folding)
 * ===================================================== */
bool ssa_opt_constant_fold(SsaFunction *func) {
    bool any_changed = false;
    SsaInst **defs = calloc(func->next_vreg, sizeof(SsaInst*));
    for (int b_idx = 0; b_idx < func->block_count; b_idx++) {
        SsaBasicBlock *b = func->blocks[b_idx];
        for (SsaInst *i = b->inst_head; i; i = i->next) {
            if (i->dst > 0) defs[i->dst] = i;
        }
    }

    bool changed;
    do {
        changed = false;
        for (int b_idx = 0; b_idx < func->block_count; b_idx++) {
            SsaBasicBlock *b = func->blocks[b_idx];
            for (SsaInst *i = b->inst_head; i; i = i->next) {
                if (i->IrNode == SSA_OP_ADD || i->IrNode == SSA_OP_SUB ||
                    i->IrNode == SSA_OP_MUL || i->IrNode == SSA_OP_SDIV ||
                    i->IrNode == SSA_OP_SREM || i->IrNode == SSA_OP_AND ||
                    i->IrNode == SSA_OP_OR || i->IrNode == SSA_OP_XOR) {
                    {
                        bool op1_known = i->op1.kind == SSA_OPND_IMM;
                        bool op2_known = i->op2.kind == SSA_OPND_IMM;
                        int64_t v1 = op1_known ? i->op1.u.imm : 0;
                        int64_t v2 = op2_known ? i->op2.u.imm : 0;
                        if (!op1_known && i->op1.kind == SSA_OPND_VREG) {
                            SsaInst *def1 = defs[i->op1.u.vreg];
                            if (def1 && def1->IrNode == SSA_OP_IMM &&
                                def1->op1.kind == SSA_OPND_IMM) {
                                op1_known = true;
                                v1 = def1->op1.u.imm;
                            }
                        }
                        if (!op2_known && i->op2.kind == SSA_OPND_VREG) {
                            SsaInst *def2 = defs[i->op2.u.vreg];
                            if (def2 && def2->IrNode == SSA_OP_IMM &&
                                def2->op1.kind == SSA_OPND_IMM) {
                                op2_known = true;
                                v2 = def2->op1.u.imm;
                            }
                        }
                        if (op1_known && op2_known) {
                            int64_t res = 0;
                            if (i->IrNode == SSA_OP_ADD)
                                res = (int64_t)((uint64_t)v1 + (uint64_t)v2);
                            else if (i->IrNode == SSA_OP_SUB)
                                res = (int64_t)((uint64_t)v1 - (uint64_t)v2);
                            else if (i->IrNode == SSA_OP_MUL)
                                res = (int64_t)((uint64_t)v1 * (uint64_t)v2);
                            else if (i->IrNode == SSA_OP_SDIV) {
                                if (v2 == 0 || (v1 == INT64_MIN && v2 == -1)) continue;
                                res = v1 / v2;
                            }
                            else if (i->IrNode == SSA_OP_SREM) {
                                if (v2 == 0 || (v1 == INT64_MIN && v2 == -1)) continue;
                                res = v1 % v2;
                            }
                            else if (i->IrNode == SSA_OP_AND) res = v1 & v2;
                            else if (i->IrNode == SSA_OP_OR) res = v1 | v2;
                            else if (i->IrNode == SSA_OP_XOR) res = v1 ^ v2;
                            
                            i->IrNode = SSA_OP_IMM;
                            i->op1.kind = SSA_OPND_IMM;
                            i->op1.u.imm = res;
                            i->op2.kind = SSA_OPND_NONE;
                            i->operand_count = 1;
                            defs[i->dst] = i;
                            changed = true;
                            any_changed = true;
                        }
                    }
                }
            }
        }
    } while(changed);
    free(defs);
    return any_changed;
}

/* ======================================================
 * PASS 2: 姝讳唬鐮佹秷闄?(Dead Code Elimination)
 * ===================================================== */
bool ssa_opt_dce(SsaFunction *func) {
    /* ===== 激进 Mark-and-Sweep DCE (O(N) 单遍) =====
     * Phase 1 (Mark): 从所有有副作用的"根"指令出发，沿 VReg 定义链
     *   反向标记所有被依赖的指令为"存活"。
     * Phase 2 (Sweep): 遍历所有指令，删除未标记的死代码。
     * 熔断机制: 追踪深度超过 DCE_VISIT_CAP 时停止标记（保守保留未访问的）。
     */
    #define DCE_VISIT_CAP 50000

    int vreg_count = (int)func->next_vreg;
    if (vreg_count <= 0) return false;

    /* 建立 VReg -> 定义指令 的映射表 */
    SsaInst **defs = calloc(vreg_count, sizeof(SsaInst*));
    /* alive[vreg] = true 表示该 vreg 的定义指令被依赖 */
    bool *alive = calloc(vreg_count, sizeof(bool));

    /* 收集所有指令的定义映射 */
    for (int b = 0; b < func->block_count; b++) {
        SsaBasicBlock *blk = func->blocks[b];
        for (SsaInst *i = blk->inst_head; i; i = i->next) {
            if (i->dst > 0 && i->dst < (VReg)vreg_count)
                defs[i->dst] = i;
        }
    }

    /* === Phase 1: Mark (反向追踪依赖) === */
    /* 用一个简单的工作栈来做 BFS/DFS 遍历 */
    VReg *worklist = malloc(vreg_count * sizeof(VReg));
    int wl_count = 0;
    int visit_count = 0;

    /* 辅助宏: 标记一个操作数中的 VReg 为存活，并加入工作栈 */
    #define MARK_OPND(opnd) do { \
        if ((opnd).kind == SSA_OPND_VREG && (opnd).u.vreg > 0 && \
            (opnd).u.vreg < (VReg)vreg_count && !alive[(opnd).u.vreg]) { \
            alive[(opnd).u.vreg] = true; \
            worklist[wl_count++] = (opnd).u.vreg; \
        } \
    } while(0)

    /* 从所有有副作用的指令出发，标记它们的操作数 */
    for (int b = 0; b < func->block_count; b++) {
        SsaBasicBlock *blk = func->blocks[b];
        for (SsaInst *i = blk->inst_head; i; i = i->next) {
            bool is_root = false;
            switch (i->IrNode) {
                case SSA_OP_ICALL:
                case SSA_OP_STORE_VAR:
                case SSA_OP_RET:
                case SSA_OP_JMP: case SSA_OP_BR:
                    is_root = true;
                    break;
                case SSA_OP_CALL:
                case SSA_OP_STORE: case SSA_OP_STORE8:
                    is_root = ssa_ref_inst_observable(i);
                    break;
                default:
                    break;
            }
            if (is_root) {
                /* 标记该指令的所有操作数 */
                MARK_OPND(i->op1);
                MARK_OPND(i->op2);
                for (int j = 0; j < i->operand_count; j++) {
                    if (i->operands) MARK_OPND(i->operands[j]);
                }
            }

            /* The legacy expression-valued branch builder leaves a branch
             * result in the selected predecessor's final VReg and emits a
             * void RET in the merge block.  Preserve those implicit return
             * values until the builder grows explicit PHIs. */
            if (i->IrNode == SSA_OP_RET && i->op1.kind != SSA_OPND_VREG) {
                for (int p = 0; p < blk->pred_count; p++) {
                    for (SsaInst *tail = blk->preds[p]->inst_tail; tail; tail = tail->prev) {
                        if (tail->dst > 0) {
                            SsaOperand implicit;
                            implicit.kind = SSA_OPND_VREG;
                            implicit.u.vreg = tail->dst;
                            MARK_OPND(implicit);
                            break;
                        }
                    }
                }
            }
        }
    }

    /* 反向追踪: 从工作栈中不断取出已标记的 vreg,
     * 找到其定义指令，再标记定义指令的输入操作数 */
    while (wl_count > 0 && visit_count < DCE_VISIT_CAP) {
        VReg v = worklist[--wl_count];
        visit_count++;
        SsaInst *def = defs[v];
        if (!def) continue;
        MARK_OPND(def->op1);
        MARK_OPND(def->op2);
        for (int j = 0; j < def->operand_count; j++) {
            if (def->operands) MARK_OPND(def->operands[j]);
        }
    }
    #undef MARK_OPND

    /* === Phase 2: Sweep (删除死代码) === */
    bool any_changed = false;
    for (int b = 0; b < func->block_count; b++) {
        SsaBasicBlock *blk = func->blocks[b];
        SsaInst *curr = blk->inst_head;
        while (curr) {
            SsaInst *next = curr->next;
            bool implicit_branch_result = false;
            if (curr->dst > 0) {
                bool later_def = false;
                for (SsaInst *scan = curr->next; scan; scan = scan->next) {
                    if (scan->dst > 0) { later_def = true; break; }
                }
                if (!later_def) {
                    if (blk->inst_tail && blk->inst_tail->IrNode == SSA_OP_JMP) {
                        implicit_branch_result = true;
                    }
                    for (int s = 0; s < blk->succ_count; s++) {
                        SsaInst *term = blk->succs[s]->inst_tail;
                        if (term && term->IrNode == SSA_OP_RET &&
                            term->op1.kind != SSA_OPND_VREG) {
                            implicit_branch_result = true;
                            break;
                        }
                    }
                }
            }
            /* 有 dst 且未被标记为存活 => 死代码 */
            if (curr->dst > 0 && curr->dst < (VReg)vreg_count &&
                !alive[curr->dst] && !implicit_branch_result) {
                /* 但有副作用的指令不能删 (CALL 等可能有 dst 但同时有副作用) */
                if ((curr->IrNode != SSA_OP_CALL || !ssa_ref_inst_observable(curr)) &&
                    curr->IrNode != SSA_OP_ICALL &&
                    (curr->IrNode != SSA_OP_STORE || !ssa_ref_inst_observable(curr)) &&
                    (curr->IrNode != SSA_OP_STORE8 || !ssa_ref_inst_observable(curr)) &&
                    curr->IrNode != SSA_OP_STORE_VAR && curr->IrNode != SSA_OP_RET) {
                    /* 从链表中摘除 */
                    if (curr->prev) curr->prev->next = curr->next;
                    else blk->inst_head = curr->next;
                    if (curr->next) curr->next->prev = curr->prev;
                    else blk->inst_tail = curr->prev;
                    any_changed = true;
                }
            }
            /* Stores and void calls have no result VReg, so the value-only
             * sweep above cannot see Static Reference's dead-effect proof. */
            if (((curr->IrNode == SSA_OP_STORE || curr->IrNode == SSA_OP_STORE8) ||
                 (curr->IrNode == SSA_OP_CALL && curr->dst == 0)) &&
                !ssa_ref_inst_observable(curr)) {
                if (curr->prev) curr->prev->next = curr->next;
                else blk->inst_head = curr->next;
                if (curr->next) curr->next->prev = curr->prev;
                else blk->inst_tail = curr->prev;
                any_changed = true;
            }
            curr = next;
        }
    }

    free(worklist);
    free(alive);
    free(defs);
    #undef DCE_VISIT_CAP
    return any_changed;
}

/* ======================================================
 * PASS 3: 澶嶅埗浼犳挱 (Copy Propagation)
 * ===================================================== */
bool ssa_opt_copy_propagate(SsaFunction *func) {
    bool any_changed = false;
    bool changed;
    do {
        changed = false;
        VReg *replacements = calloc(func->next_vreg, sizeof(VReg));
        int *def_count = calloc(func->next_vreg, sizeof(int));

        /* Phi destruction deliberately creates one COPY definition of the
         * merge VReg on each predecessor edge.  A later optimizer round must
         * not treat that multi-definition value as SSA and select whichever
         * COPY happened to be scanned last. */
        for (int b_idx = 0; b_idx < func->block_count; b_idx++) {
            SsaBasicBlock *b = func->blocks[b_idx];
            for (SsaInst *i = b->inst_head; i; i = i->next) {
                if (i->dst > 0 && i->dst < func->next_vreg)
                    def_count[i->dst]++;
            }
        }
        
        for (int b_idx = 0; b_idx < func->block_count; b_idx++) {
            SsaBasicBlock *b = func->blocks[b_idx];
            for (SsaInst *i = b->inst_head; i; i = i->next) {
                if (i->IrNode == SSA_OP_COPY && i->dst > 0 &&
                    def_count[i->dst] == 1 && i->op1.kind == SSA_OPND_VREG) {
                    replacements[i->dst] = i->op1.u.vreg;
                }
            }
        }
        
        // Deep resolve loop
        for (VReg v = 1; v < func->next_vreg; v++) {
            if (replacements[v]) {
                VReg curr = v;
                int guard = 0;
                while (replacements[curr] && guard++ < 1024) curr = replacements[curr];
                replacements[v] = curr;
            }
        }
        
        for (int b_idx = 0; b_idx < func->block_count; b_idx++) {
            SsaBasicBlock *b = func->blocks[b_idx];
            for (SsaInst *i = b->inst_head; i; i = i->next) {
                if (i->op1.kind == SSA_OPND_VREG && replacements[i->op1.u.vreg]) {
                    i->op1.u.vreg = replacements[i->op1.u.vreg];
                    changed = true;
                    any_changed = true;
                }
                if (i->op2.kind == SSA_OPND_VREG && replacements[i->op2.u.vreg]) {
                    i->op2.u.vreg = replacements[i->op2.u.vreg];
                    changed = true;
                    any_changed = true;
                }
                for (int j = 0; j < i->operand_count; j++) {
                    if (i->operands && i->operands[j].kind == SSA_OPND_VREG && replacements[i->operands[j].u.vreg]) {
                        i->operands[j].u.vreg = replacements[i->operands[j].u.vreg];
                        changed = true;
                        any_changed = true;
                    }
                }
            }
        }
        free(def_count);
        free(replacements);
    } while(changed);
    return any_changed;
}

/* ======================================================
 * PASS 4: IMM 甯搁噺浼犳挱 (IMM Const Propagation)
 * 瀹夊叏鐗堟湰锛氬彧瀵瑰湪鏁翠釜鍑芥暟涓敮涓€涓€娆¤瀹氫箟锛堜笖璇ュ畾涔夋槸 IMM锛夌殑 vreg 杩涜浼犳挱銆? * 杩欐牱鑳介伩鍏嶅惊鐜腑琚?phi/copy 閲嶅畾涔夌殑鍙橀噺琚敊璇瘑鍒负甯搁噺銆? * ===================================================== */
bool ssa_opt_imm_propagate(SsaFunction *func) {
    bool any_changed = false;
    int64_t *imm_vals = calloc(func->next_vreg, sizeof(int64_t));
    bool *is_imm = calloc(func->next_vreg, sizeof(bool));
    int *def_count = calloc(func->next_vreg, sizeof(int));

    /* Pass 1: 统计定义次数，找出唯一 IMM 定义 */
    for (int b_idx = 0; b_idx < func->block_count; b_idx++) {
        SsaBasicBlock *b = func->blocks[b_idx];
        for (SsaInst *i = b->inst_head; i; i = i->next) {
            if (i->dst > 0 && i->dst < func->next_vreg) {
                def_count[i->dst]++;
                if (i->IrNode == SSA_OP_IMM && i->op1.kind == SSA_OPND_IMM) {
                    is_imm[i->dst] = true;
                    imm_vals[i->dst] = i->op1.u.imm;
                }
            }
        }
    }

    /* 娓呴櫎閭ｄ簺琚娆″畾涔夌殑 vreg锛坧hi 鑺傜偣鎴栧惊鐜腑鏇存柊锛?*/
    for (VReg v = 1; v < func->next_vreg; v++) {
        if (def_count[v] > 1) is_imm[v] = false;
    }

    /* Pass 2: 鏇挎崲鎿嶄綔鏁颁腑鐢?vreg 寮曠敤鐨勫凡鐭ュ父閲?*/
    for (int b_idx = 0; b_idx < func->block_count; b_idx++) {
        SsaBasicBlock *b = func->blocks[b_idx];
        for (SsaInst *i = b->inst_head; i; i = i->next) {
            if (i->IrNode == SSA_OP_IMM || i->IrNode == SSA_OP_PHI) continue;
#define TRY_IMM(opnd) do { \
    if ((opnd).kind == SSA_OPND_VREG && (opnd).u.vreg > 0 && \
        (opnd).u.vreg < func->next_vreg && is_imm[(opnd).u.vreg]) { \
        (opnd).kind = SSA_OPND_IMM; \
        (opnd).u.imm = imm_vals[(opnd).u.vreg]; \
        any_changed = true; \
    } \
} while(0)
            /* Only propagate into arithmetic and compare ops for safety */
            if (i->IrNode == SSA_OP_ADD || i->IrNode == SSA_OP_SUB || i->IrNode == SSA_OP_MUL ||
                i->IrNode == SSA_OP_SDIV || i->IrNode == SSA_OP_SREM ||
                i->IrNode == SSA_OP_CMP_LT || i->IrNode == SSA_OP_CMP_GT ||
                i->IrNode == SSA_OP_CMP_LE || i->IrNode == SSA_OP_CMP_GE ||
                i->IrNode == SSA_OP_CMP_EQ || i->IrNode == SSA_OP_CMP_NE ||
                i->IrNode == SSA_OP_AND || i->IrNode == SSA_OP_OR || i->IrNode == SSA_OP_XOR) {
                TRY_IMM(i->op1);
                TRY_IMM(i->op2);
            }
#undef TRY_IMM
        }
    }

    free(imm_vals);
    free(is_imm);
    free(def_count);
    return any_changed;
}

/* ======================================================
 * PASS 5: STORE_VAR 鈫?LOAD_VAR 鍊煎墠棣?(Value Forwarding)
 * 鍦ㄥ崟涓?BasicBlock 鍐咃紝鑻ユ湁 STORE_VAR slot=S 鍚庤窡 LOAD_VAR slot=S
 * 涓斾腑闂存病鏈夊 slot=S 鐨勫叾浠?STORE 鎴?CALL锛屽垯灏?LOAD_VAR 鏇挎崲涓哄宸茬煡
 * VReg 鐨?COPY锛岄伩鍏嶅唴瀛樺線杩斻€? * ===================================================== */
bool ssa_opt_var_forwarding(SsaFunction *func) {
    bool any_changed = false;

    /* 鏀寔鐨勬Ы鏁颁笂闄愶紙mira_vars 鏈€澶氬嚑鍗佷釜锛?*/
    #define MAX_VAR_SLOTS 128
    VReg slot_val[MAX_VAR_SLOTS]; /* 褰撳墠宸茬煡鐨?vreg 鍊硷紝0 琛ㄧず鏈煡 */

    for (int b_idx = 0; b_idx < func->block_count; b_idx++) {
        SsaBasicBlock *b = func->blocks[b_idx];
        memset(slot_val, 0, sizeof(slot_val));

        for (SsaInst *i = b->inst_head; i; i = i->next) {
            if (i->IrNode == SSA_OP_STORE_VAR) {
                int slot = (int)i->op2.u.imm;
                if (func->var_reg_map && slot >= 0 && slot < func->var_count &&
                    func->var_reg_map[slot] != REG_NONE) {
                    slot_val[slot] = 0;
                    continue;
                }
                if (slot >= 0 && slot < MAX_VAR_SLOTS && i->op1.kind == SSA_OPND_VREG) {
                    VReg stored = i->op1.u.vreg;
                    SsaInst *stored_def =
                        stored > 0 && stored < func->next_vreg && func->vreg_defs
                        ? func->vreg_defs[stored] : NULL;
                    /* A LOAD_VAR may lower to the variable's mutable fixed
                     * register.  Storing that value into another slot creates
                     * a snapshot; forwarding later loads as an alias would
                     * observe subsequent writes to the source variable. */
                    slot_val[slot] =
                        stored_def && stored_def->IrNode == SSA_OP_LOAD_VAR
                        ? 0 : stored;
                }
            } else if (i->IrNode == SSA_OP_LOAD_VAR) {
                int slot = (int)i->op1.u.imm;
                if (func->var_reg_map && slot >= 0 && slot < func->var_count &&
                    func->var_reg_map[slot] != REG_NONE) {
                    continue;
                }
                if (slot >= 0 && slot < MAX_VAR_SLOTS && slot_val[slot] != 0 && i->dst > 0) {
                    /* 灏?LOAD_VAR 鏇挎崲涓?COPY vreg */
                    i->IrNode = SSA_OP_COPY;
                    i->op1.kind = SSA_OPND_VREG;
                    i->op1.u.vreg = slot_val[slot];
                    i->op2.kind = SSA_OPND_NONE;
                    i->operand_count = 1;
                    any_changed = true;
                    /* The COPY result inherits the stored value - update slot to alias dst */
                    slot_val[slot] = i->dst;
                }
            } else if (i->IrNode == SSA_OP_CALL || i->IrNode == SSA_OP_ICALL) {
                /* Calls may modify vars indirectly - invalidate all slots */
                memset(slot_val, 0, sizeof(slot_val));
            }
        }
    }
    #undef MAX_VAR_SLOTS
    return any_changed;
}

/* ======================================================
 * PASS 6: Dead STORE_VAR 娑堥櫎
 * 鑻ヤ竴涓?STORE_VAR slot=S 鍚庯紝鍦ㄥ埌杈句笅涓€涓?LOAD_VAR slot=S 涔嬪墠
 * 鏈夊彟涓€涓?STORE_VAR slot=S锛屽垯鍓嶄竴涓?STORE 鏄瀛樺偍锛屽彲浠ュ垹闄ゃ€? * 鍦ㄥ崟 BB 鍐呭疄鐜帮紙鏈€瀹夊叏锛夈€? * ===================================================== */
bool ssa_opt_dead_store_var(SsaFunction *func) {
    bool any_changed = false;
    #define MAX_VAR_SLOTS 128

    for (int b_idx = 0; b_idx < func->block_count; b_idx++) {
        SsaBasicBlock *b = func->blocks[b_idx];

        /* lIR_store[slot] = pointer to the lIR STORE_VAR instruction for this slot */
        SsaInst *lIR_store[MAX_VAR_SLOTS];
        memset(lIR_store, 0, sizeof(lIR_store));

        for (SsaInst *i = b->inst_head; i; i = i->next) {
            if (i->IrNode == SSA_OP_STORE_VAR) {
                int slot = (int)i->op2.u.imm;
                if (slot >= 0 && slot < MAX_VAR_SLOTS) {
                    if (lIR_store[slot] != NULL) {
                        /* Previous store is dead - remove it */
                        SsaInst *dead = lIR_store[slot];
                        if (dead->prev) dead->prev->next = dead->next; else b->inst_head = dead->next;
                        if (dead->next) dead->next->prev = dead->prev; else b->inst_tail = dead->prev;
                        any_changed = true;
                    }
                    lIR_store[slot] = i;
                }
            } else if (i->IrNode == SSA_OP_LOAD_VAR) {
                int slot = (int)i->op1.u.imm;
                if (slot >= 0 && slot < MAX_VAR_SLOTS) {
                    lIR_store[slot] = NULL; /* The store is needed */
                }
            } else if (i->IrNode == SSA_OP_CALL || i->IrNode == SSA_OP_ICALL) {
                /* Calls may read mira_vars - keep all stores */
                memset(lIR_store, 0, sizeof(lIR_store));
            }
        }
    }
    #undef MAX_VAR_SLOTS
    return any_changed;
}

/* ======================================================
 * PASS 6.5: Global Dead Store Variable Elimination
 *
 * After var_forwarding converts LOAD_VAR -> COPY, some var slots
 * may have STORE_VARs but ZERO remaining LOAD_VARs globally.
 * Those STORE_VARs are provably dead and can be eliminated.
 *
 * Example: temp variables used as swap helpers in Fibonacci loops.
 * This removes one instruction per loop iteration (~14% speedup).
 * ===================================================== */
bool ssa_opt_global_dead_store(SsaFunction *func) {
    bool any_changed = false;
    #define GDS_MAX_SLOTS 128
    int load_count[GDS_MAX_SLOTS];
    memset(load_count, 0, sizeof(load_count));

    /* Count remaining LOAD_VAR references across ALL blocks */
    for (int bi = 0; bi < func->block_count; bi++) {
        SsaBasicBlock *b = func->blocks[bi];
        for (SsaInst *i = b->inst_head; i; i = i->next) {
            if (i->IrNode == SSA_OP_LOAD_VAR) {
                int slot = (int)i->op1.u.imm;
                if (slot >= 0 && slot < GDS_MAX_SLOTS) load_count[slot]++;
            }
        }
    }

    /* Only eliminate dead STORE_VARs inside LOOP BODY blocks.
     * Non-loop blocks may have forwarded loads but the store still
     * writes to mira_vars[] or fIR_var_reg for cross-block use. */
    for (int bi = 0; bi < func->block_count; bi++) {
        SsaBasicBlock *b = func->blocks[bi];

        /* Is this block part of a loop? */
        int is_loop = 0;
        for (int s = 0; s < b->succ_count; s++)
            if (b->succs[s]->id <= b->id) is_loop = 1;
        for (int p = 0; p < b->pred_count; p++)
            if (b->preds[p]->id > b->id) is_loop = 1;
        if (!is_loop) continue; /* skip non-loop blocks */

        SsaInst *curr = b->inst_head;
        while (curr) {
            SsaInst *next = curr->next;
            if (curr->IrNode == SSA_OP_STORE_VAR) {
                int slot = (int)curr->op2.u.imm;
                if (slot >= 0 && slot < GDS_MAX_SLOTS && load_count[slot] == 0) {
                    if (curr->prev) curr->prev->next = curr->next;
                    else b->inst_head = curr->next;
                    if (curr->next) curr->next->prev = curr->prev;
                    else b->inst_tail = curr->prev;
                    any_changed = true;
                }
            }
            curr = next;
        }
    }
    #undef GDS_MAX_SLOTS
    return any_changed;
}

/* ======================================================
 * PASS 7: CFG 绠€鍖?(Block Merging / Empty Block Elimination)
 * ===================================================== */
bool ssa_opt_cfg_simplify(SsaFunction *func) {
    bool any_changed = false;
    bool changed;
    do {
        changed = false;
        for (int i = 0; i < func->block_count; i++) {
            SsaBasicBlock *b = func->blocks[i];
            if (!b || !b->inst_tail) continue;
            if (b->inst_tail->IrNode == SSA_OP_JMP && b->inst_tail->op1.kind == SSA_OPND_BLOCK) {
                SsaBasicBlock *succ = b->inst_tail->op1.u.block;
                if (succ && succ->inst_tail &&
                    (succ->inst_tail->IrNode == SSA_OP_JMP ||
                     succ->inst_tail->IrNode == SSA_OP_BR ||
                     succ->inst_tail->IrNode == SSA_OP_RET) &&
                    succ->pred_count == 1 && succ->preds[0] == b && succ != b) {
                    bool successor_phis_are_well_formed =
                        ssa_phi_prefix_is_valid(func, succ);
                    for (int j = 0;
                         successor_phis_are_well_formed && j < succ->succ_count;
                         ++j)
                        successor_phis_are_well_formed =
                            ssa_phi_prefix_is_valid(func, succ->succs[j]);
                    if (!successor_phis_are_well_formed) continue;
                    SsaInst *jmp = b->inst_tail;
                    b->inst_tail = jmp->prev;
                    if (b->inst_tail) b->inst_tail->next = NULL;
                    else b->inst_head = NULL;

                    for (SsaInst *phi = succ->inst_head;
                         phi && phi->IrNode == SSA_OP_PHI; phi = phi->next) {
                        SsaOperand value = phi->operands[0];
                        free(phi->operands);
                        phi->operands = NULL;
                        phi->operand_count = 0;
                        phi->operand_cap = 0;
                        phi->IrNode = SSA_OP_COPY;
                        phi->op1 = value;
                        phi->op2.kind = SSA_OPND_NONE;
                    }
                    
                    if (succ->inst_head) {
                        if (b->inst_tail) {
                            b->inst_tail->next = succ->inst_head;
                            succ->inst_head->prev = b->inst_tail;
                        } else {
                            b->inst_head = succ->inst_head;
                        }
                        b->inst_tail = succ->inst_tail;
                    }
                    
                    for (SsaInst *inst = succ->inst_head; inst; inst = inst->next) {
                        inst->parent = b;
                    }
                    
                    b->succ_count = 0;
                    for (int j = 0; j < succ->succ_count; j++) {
                        SsaBasicBlock *ssucc = succ->succs[j];
                        for (int k = 0; k < ssucc->pred_count; k++) {
                            if (ssucc->preds[k] == succ) {
                                ssucc->preds[k] = b;
                            }
                        }
                        
                        // Fix PHI nodes in ssucc
                        for (SsaInst *p = ssucc->inst_head; p; p = p->next) {
                            if (p->IrNode != SSA_OP_PHI) break;
                            for (int idx = 0; idx < p->operand_count; idx += 2) {
                                if (p->operands[idx+1].kind == SSA_OPND_BLOCK && p->operands[idx+1].u.block == succ) {
                                    p->operands[idx+1].u.block = b;
                                }
                            }
                        }
                        if (b->succ_count >= b->succ_cap) {
                            b->succ_cap = b->succ_cap ? b->succ_cap * 2 : 4;
                            b->succs = realloc(b->succs, b->succ_cap * sizeof(SsaBasicBlock*));
                        }
                        b->succs[b->succ_count++] = ssucc;
                    }
                    
                    for (int j = 0; j < func->block_count; j++) {
                        if (func->blocks[j] == succ) {
                            for (int k = j; k < func->block_count - 1; k++) {
                                func->blocks[k] = func->blocks[k+1];
                            }
                            func->block_count--;
                            for (int id = 0; id < func->block_count; ++id)
                                func->blocks[id]->id = id;
                            break;
                        }
                    }
                    
                    changed = true;
                    any_changed = true;
                    break;
                }
            }
        }
    } while(changed);
    return any_changed;
}

/* ======================================================
 * PASS 8: 寰幆涓嶅彉閲忓鎻?(LICM - Loop Invariant Code Motion)
 * 璇嗗埆 back-edge 浠ユ壘鍒拌嚜鐒跺惊鐜紝灏嗗惊鐜唴涓嶉殢寰幆鍙樺寲鐨? * STORE_VAR 鍒濆鍖栵紙鎬绘槸鍐欏浐瀹?IMM 鐨勬Ы锛夊鎻愬埌 preheader銆? * 绠€鍖栫増锛氫粎澶勭悊鍗曞眰寰幆锛屽熀浜?back-edge 妫€娴嬨€? * ===================================================== */
bool ssa_opt_licm(SsaFunction *func) {
    bool any_changed = false;
    if (func->block_count < 2) return false;

    /* 鎵?back-edges: 鑻?B -> H 涓?H 鏀厤 B (鍗?H 鐨?id <= B 鐨?id锛岀畝鍖栫増)
     * 鏇寸簿纭殑鏂规硶鏄蛋鏀厤鏍戯紝浣嗚繖閲岀敤鍧楅『搴忚繎浼?*/
    for (int loop_index = 0; loop_index < func->loop_count; ++loop_index) {
        SsaLoopInfo *loop = &func->loops[loop_index];
        SsaBasicBlock *loop_header = loop->header;
        if (!loop_header || !loop->members || loop->backedge_count != 1) continue;
        if (loop->decision_plan.reference_vetoes &
            (DECISION_REF_VETO_MEMORY_REORDER | DECISION_REF_VETO_OWNERSHIP_MOVE))
            continue;
        /* 鎵?back edge: b -> h where h.id < b.id */
        /* 鎵?preheader: loop_header 鐨勫敮涓€闈炲洖杈瑰墠椹?*/
        SsaBasicBlock *preheader = NULL;
        for (int p = 0; p < loop_header->pred_count; p++) {
            SsaBasicBlock *pred = loop_header->preds[p];
            if (loop->members[pred->id]) continue;
            if (preheader && preheader != pred) { preheader = NULL; break; }
            preheader = pred;
        }
        if (!preheader) continue;
        if (!preheader->inst_tail) continue;

        /* 收集循环体中哪些槽被 STORE_VAR 写过（可能变化） */
        #define MAX_VAR_SLOTS 128
        bool slot_written[MAX_VAR_SLOTS];
        bool slot_read[MAX_VAR_SLOTS];
        memset(slot_written, 0, sizeof(slot_written));
        memset(slot_read, 0, sizeof(slot_read));

        /* 鎵弿浠?loop_header 鍒?b 鐨勬墍鏈夊潡锛堣繎浼煎惊鐜綋锛?*/
        for (int k = 0; k < func->block_count; k++) {
            if (!loop->members[k]) continue;
            SsaBasicBlock *lb = func->blocks[k];
            for (SsaInst *i = lb->inst_head; i; i = i->next) {
                if (i->IrNode == SSA_OP_STORE_VAR) {
                    int slot = (int)i->op2.u.imm;
                    if (slot >= 0 && slot < MAX_VAR_SLOTS) slot_written[slot] = true;
                }
                if (i->IrNode == SSA_OP_LOAD_VAR) {
                    int slot = (int)i->op1.u.imm;
                    if (slot >= 0 && slot < MAX_VAR_SLOTS) slot_read[slot] = true;
                }
            }
        }

        /* 鍦?preheader 灏鹃儴涔嬪墠锛屽皢鍙啓涓嶈鐨勩€佹潵鑷?IMM 鐨?STORE_VAR 鎸囦护澶栨彁 */
        /* For now: move STORE_VAR from loop_header if the stored value is an IMM
         * and the slot is NOT read before the first write inside the loop. */
        SsaInst *term = preheader->inst_tail;

        SsaInst *i = loop_header->inst_head;
        while (i) {
            SsaInst *next = i->next;
            if (i->IrNode == SSA_OP_LOAD_PARAM) {
                /* Function parameters are immutable.  Keep one SSA value live
                 * across the back edge instead of reloading the ABI home slot
                 * on every iteration. */
                if (i->prev) i->prev->next = i->next; else loop_header->inst_head = i->next;
                if (i->next) i->next->prev = i->prev; else loop_header->inst_tail = i->prev;

                i->parent = preheader;
                i->next = term;
                i->prev = term ? term->prev : preheader->inst_tail;
                if (term && term->prev) term->prev->next = i;
                else preheader->inst_head = i;
                if (term) term->prev = i;
                else preheader->inst_tail = i;

                any_changed = true;
            } else if (i->IrNode == SSA_OP_STORE_VAR && i->op1.kind == SSA_OPND_VREG) {
                int slot = (int)i->op2.u.imm;
                /* Only hoist if slot is not READ before this store in the loop header */
                bool safe_to_hoist = (slot >= 0 && slot < MAX_VAR_SLOTS && !slot_read[slot]);
                if (safe_to_hoist) {
                    /* Remove from loop_header */
                    if (i->prev) i->prev->next = i->next; else loop_header->inst_head = i->next;
                    if (i->next) i->next->prev = i->prev; else loop_header->inst_tail = i->prev;

                    /* Insert before preheader terminator */
                    i->parent = preheader;
                    i->next = term;
                    i->prev = term ? term->prev : preheader->inst_tail;
                    if (term && term->prev) term->prev->next = i;
                    else preheader->inst_head = i;
                    if (term) term->prev = i;
                    else preheader->inst_tail = i;

                    any_changed = true;
                    slot_read[slot] = false; /* slot no longer read before loop body */
                }
            }
            i = next;
        }

        /* Prove stable origins instead of treating every outside definition as
         * invariant.  LOAD_VAR, PHI, calls and ownership values never enter
         * this set, preserving the fixed-slot fast path's current-value model. */
        bool *stable = calloc((size_t)func->next_vreg, sizeof(*stable));
        if (stable && !loop->has_unknown_call) {
            bool progress;
            int proof_rounds = 0;
            do {
                progress = false;
                for (VReg v = 1; v < func->next_vreg; ++v) {
                    if (stable[v]) continue;
                    SsaInst *def = func->vreg_defs &&
                        v < (VReg)func->vreg_defs_cap ?
                        func->vreg_defs[v] : NULL;
                    if (!def || !def->parent ||
                        loop->members[def->parent->id] ||
                        !block_dominates(def->parent, preheader))
                        continue;
                    bool base = def->IrNode == SSA_OP_IMM ||
                        def->IrNode == SSA_OP_LOAD_PARAM;
                    bool derived = ssa_licm_pure_integer_op(def) &&
                        ssa_licm_operand_stable(def->op1, stable,
                                                func->next_vreg) &&
                        ssa_licm_operand_stable(def->op2, stable,
                                                func->next_vreg);
                    if (base || derived) {
                        stable[v] = true;
                        progress = true;
                    }
                }
            } while (progress && ++proof_rounds < 8);

            int motion_rounds = 0;
            do {
                progress = false;
                for (int bi = 0; bi < func->block_count; ++bi) {
                    if (!loop->members[bi]) continue;
                    SsaBasicBlock *block = func->blocks[bi];
                    for (SsaInst *candidate = block->inst_head; candidate; ) {
                        SsaInst *next = candidate->next;
                        if (!ssa_licm_pure_integer_op(candidate) ||
                            !ssa_licm_operand_stable(candidate->op1, stable,
                                                     func->next_vreg) ||
                            !ssa_licm_operand_stable(candidate->op2, stable,
                                                     func->next_vreg)) {
                            candidate = next;
                            continue;
                        }
                        if (candidate->prev)
                            candidate->prev->next = candidate->next;
                        else
                            block->inst_head = candidate->next;
                        if (candidate->next)
                            candidate->next->prev = candidate->prev;
                        else
                            block->inst_tail = candidate->prev;
                        candidate->parent = preheader;
                        candidate->next = term;
                        candidate->prev = term ? term->prev :
                            preheader->inst_tail;
                        if (term && term->prev)
                            term->prev->next = candidate;
                        else
                            preheader->inst_head = candidate;
                        if (term) term->prev = candidate;
                        else preheader->inst_tail = candidate;
                        stable[candidate->dst] = true;
                        any_changed = progress = true;
                        candidate = next;
                    }
                }
            } while (progress && ++motion_rounds < 8);
        }
        free(stable);
        #undef MAX_VAR_SLOTS
    }
    return any_changed;
}

/* ======================================================
 * PASS 8.3: 多层循环优化 (Loop Nest Optimization)
 *
 * 分析嵌套循环结构，对内层循环执行以下优化：
 *   1. 检测嵌套深度，超过 3 层直接放弃
 *   2. 将内层循环不变量提升到外层 preheader
 *   3. 当内外层循环的迭代变量步长可交换时做循环交换
 *
 * 熔断: 最多分析 64 个循环，单循环体最多 50 个基本块
 * ===================================================== */
#define LOOP_NEST_MAX_LOOPS  64
#define LOOP_NEST_MAX_DEPTH  3
#define LOOP_NEST_MAX_BLOCKS 50

typedef struct {
    int head_id;        /* loop header block ID */
    int tail_id;        /* back-edge source block ID */
    int parent;         /* index of parent loop (-1 = top-level) */
    int depth;          /* nesting depth (0 = outermost) */
} LoopInfo;

bool ssa_opt_loop_nest(SsaFunction *func) {
    bool any_changed = false;
    if (func->block_count < 3) return false;

    /* === Phase 1: Discover all natural loops === */
    LoopInfo loops[LOOP_NEST_MAX_LOOPS];
    int loop_count = 0;

    for (int b_idx = 1; b_idx < func->block_count && loop_count < LOOP_NEST_MAX_LOOPS; b_idx++) {
        SsaBasicBlock *b = func->blocks[b_idx];
        for (int s = 0; s < b->succ_count; s++) {
            if (b->succs[s]->id <= b->id) {
                /* Found back edge: b -> succs[s] */
                int body_size = b_idx - b->succs[s]->id + 1;
                if (body_size > LOOP_NEST_MAX_BLOCKS) break; /* 体太大，跳过 */
                loops[loop_count].head_id = b->succs[s]->id;
                loops[loop_count].tail_id = b_idx;
                loops[loop_count].parent = -1;
                loops[loop_count].depth = 0;
                loop_count++;
                break;
            }
        }
    }

    if (loop_count < 2) return false; /* 没有嵌套循环可优化 */

    /* === Phase 2: Build nesting tree === */
    /* Loop A is parent of Loop B if A.head <= B.head && B.tail <= A.tail && A != B */
    for (int i = 0; i < loop_count; i++) {
        int best_parent = -1;
        int best_size = func->block_count + 1;
        for (int j = 0; j < loop_count; j++) {
            if (i == j) continue;
            if (loops[j].head_id <= loops[i].head_id &&
                loops[i].tail_id <= loops[j].tail_id) {
                int size = loops[j].tail_id - loops[j].head_id;
                if (size < best_size) {
                    best_size = size;
                    best_parent = j;
                }
            }
        }
        loops[i].parent = best_parent;
    }

    /* Calculate depths */
    for (int i = 0; i < loop_count; i++) {
        int d = 0;
        int p = loops[i].parent;
        while (p >= 0 && d < LOOP_NEST_MAX_DEPTH + 1) {
            d++;
            p = loops[p].parent;
        }
        loops[i].depth = d;
    }

    /* === Phase 3: Inner-loop invariant hoisting (LICM for nested loops) ===
     * For each inner loop (depth >= 1), find STORE_VAR instructions whose
     * stored value is an IMM or is defined OUTSIDE the inner loop.
     * Hoist them to the outer loop's preheader. */
    for (int i = 0; i < loop_count; i++) {
        if (loops[i].depth < 1 || loops[i].depth > LOOP_NEST_MAX_DEPTH) continue;
        int parent_idx = loops[i].parent;
        if (parent_idx < 0) continue;

        int inner_head = loops[i].head_id;
        int inner_tail = loops[i].tail_id;

        /* Find inner loop's preheader (predecessor of inner_head with id < inner_head) */
        if (inner_head >= func->block_count) continue;
        SsaBasicBlock *inner_header = func->blocks[inner_head];
        SsaBasicBlock *preheader = NULL;
        for (int p = 0; p < inner_header->pred_count; p++) {
            if (inner_header->preds[p]->id < inner_head) {
                preheader = inner_header->preds[p];
                break;
            }
        }
        if (!preheader) continue;

        /* Scan inner loop header for STORE_VAR with IMM value that can be hoisted */
        SsaInst *inst = inner_header->inst_head;
        while (inst) {
            SsaInst *next = inst->next;

            if (inst->IrNode == SSA_OP_STORE_VAR && inst->op1.kind == SSA_OPND_VREG) {
                int slot = (int)inst->op2.u.imm;
                VReg val_vreg = inst->op1.u.vreg;

                /* Check if the value vreg is defined outside the inner loop */
                bool defined_outside = true;
                for (int k = inner_head; k <= inner_tail && k < func->block_count; k++) {
                    SsaBasicBlock *lb = func->blocks[k];
                    for (SsaInst *j = lb->inst_head; j; j = j->next) {
                        if (j->dst == val_vreg) {
                            defined_outside = false;
                            break;
                        }
                    }
                    if (!defined_outside) break;
                }

                /* Also check: the slot must not be LOAD_VAR'd inside the inner loop
                 * (otherwise hoisting would change semantics) */
                bool slot_read_inner = false;
                if (defined_outside) {
                    for (int k = inner_head; k <= inner_tail && k < func->block_count; k++) {
                        SsaBasicBlock *lb = func->blocks[k];
                        for (SsaInst *j = lb->inst_head; j; j = j->next) {
                            if (j != inst && j->IrNode == SSA_OP_LOAD_VAR &&
                                (int)j->op1.u.imm == slot) {
                                slot_read_inner = true;
                                break;
                            }
                        }
                        if (slot_read_inner) break;
                    }
                }

                if (defined_outside && !slot_read_inner) {
                    /* Hoist: remove from inner header, insert before preheader's terminator */
                    if (inst->prev) inst->prev->next = inst->next;
                    else inner_header->inst_head = inst->next;
                    if (inst->next) inst->next->prev = inst->prev;
                    else inner_header->inst_tail = inst->prev;

                    /* Insert before preheader's terminator */
                    SsaInst *term = preheader->inst_tail;
                    if (term && (term->IrNode == SSA_OP_JMP || term->IrNode == SSA_OP_BR)) {
                        inst->next = term;
                        inst->prev = term->prev;
                        if (term->prev) term->prev->next = inst;
                        else preheader->inst_head = inst;
                        term->prev = inst;
                    } else {
                        /* Append at end */
                        inst->prev = preheader->inst_tail;
                        inst->next = NULL;
                        if (preheader->inst_tail) preheader->inst_tail->next = inst;
                        else preheader->inst_head = inst;
                        preheader->inst_tail = inst;
                    }
                    inst->parent = preheader;
                    any_changed = true;
                }
            }
            inst = next;
        }
    }

    return any_changed;
}

#undef LOOP_NEST_MAX_LOOPS
#undef LOOP_NEST_MAX_DEPTH
#undef LOOP_NEST_MAX_BLOCKS

/* ======================================================
 * PASS 8.5: Dead Loop Elimination ("Static Reference" DCE)
 *
 * Eliminates entire loops that are provably useless:
 *   1. Loop body has NO side-effects (no CALL/ICALL/STORE/STORE8)
 *   2. Variables written by the loop are NOT read after the loop
 *
 * This achieves GCC -O2 level dead loop removal with minimal
 * compile-time cost (single scan of SSA instructions).
 * ===================================================== */
bool ssa_opt_dead_loop(SsaFunction *func) {
    bool any_changed = false;
    if (func->block_count < 3) return false;

    for (int b_idx = 1; b_idx < func->block_count; b_idx++) {
        SsaBasicBlock *b = func->blocks[b_idx];

        /* Find back edge: b -> h where h.id <= b.id */
        SsaBasicBlock *loop_header = NULL;
        for (int s = 0; s < b->succ_count; s++) {
            if (b->succs[s]->id <= b->id) {
                loop_header = b->succs[s];
                break;
            }
        }
        if (!loop_header) continue;

        int head_id = loop_header->id;
        int tail_id = b_idx;

        /* This cloner duplicates one natural loop only.  A second back edge
         * inside the candidate interval means the body contains a nested
         * loop; flattening its blocks into the outer latch changes execution
         * counts and previously collapsed a 10000x10000 reduction to one row. */
        bool contains_nested_loop = false;
        for (int k = head_id; k <= tail_id && !contains_nested_loop; k++) {
            if (k < 0 || k >= func->block_count) continue;
            SsaBasicBlock *lb = func->blocks[k];
            for (int s = 0; s < lb->succ_count; s++) {
                SsaBasicBlock *target = lb->succs[s];
                bool candidate_back_edge = (lb == b && target == loop_header);
                if (!candidate_back_edge && target && target->id <= lb->id &&
                    target->id >= head_id) {
                    contains_nested_loop = true;
                    break;
                }
            }
        }
        if (contains_nested_loop) continue;

        /* Find the loop exit block (first block after loop) */
        int exit_id = tail_id + 1;
        if (exit_id >= func->block_count) continue;
        SsaBasicBlock *exit_block = func->blocks[exit_id];

        /* === Condition 1: Check for side effects in loop body === */
        bool has_side_effect = false;
        #define MAX_SLOTS 128
        bool slot_modified[MAX_SLOTS];
        memset(slot_modified, 0, sizeof(slot_modified));

        for (int k = head_id; k <= tail_id; k++) {
            if (k >= func->block_count) break;
            SsaBasicBlock *lb = func->blocks[k];
            for (SsaInst *i = lb->inst_head; i; i = i->next) {
                /* Any external call = side effect, abort */
                if (i->IrNode == SSA_OP_CALL || i->IrNode == SSA_OP_ICALL) {
                    has_side_effect = true; break;
                }
                /* Any pointer/memory store = side effect, abort */
                if (i->IrNode == SSA_OP_STORE || i->IrNode == SSA_OP_STORE8) {
                    has_side_effect = true; break;
                }
                /* Raw loads are observable in Mira: they may fault and may
                 * observe memory changed outside the optimizer's model. */
                if (i->IrNode == SSA_OP_LOAD || i->IrNode == SSA_OP_LOAD8) {
                    has_side_effect = true; break;
                }
                /* Track which variable slots the loop modifies */
                if (i->IrNode == SSA_OP_STORE_VAR) {
                    int slot = (int)i->op2.u.imm;
                    if (slot >= 0 && slot < MAX_SLOTS) slot_modified[slot] = true;
                    /* After var forwarding, a later read may be represented
                     * by COPY/PHI rather than LOAD_VAR.  Until loop live-out
                     * SSA analysis is complete, treating a variable write as
                     * observable is the only semantics-safe choice. */
                    has_side_effect = true;
                    break;
                }
            }
            if (has_side_effect) break;
        }
        if (has_side_effect) continue;

        /* === Condition 2: Check if modified slots are consumed after loop === */
        bool output_consumed = false;
        for (int k = exit_id; k < func->block_count; k++) {
            SsaBasicBlock *ab = func->blocks[k];
            for (SsaInst *i = ab->inst_head; i; i = i->next) {
                if (i->IrNode == SSA_OP_LOAD_VAR) {
                    int slot = (int)i->op1.u.imm;
                    if (slot >= 0 && slot < MAX_SLOTS && slot_modified[slot]) {
                        output_consumed = true; break;
                    }
                }
            }
            if (output_consumed) break;
        }
        #undef MAX_SLOTS
        if (output_consumed) continue;

        /* === Both conditions met: ELIMINATE the entire loop! === */

        /* Clear all instructions in loop body blocks */
        for (int k = head_id; k <= tail_id; k++) {
            if (k >= func->block_count) break;
            SsaBasicBlock *lb = func->blocks[k];
            lb->inst_head = NULL;
            lb->inst_tail = NULL;
        }

        /* Find the preheader (predecessor of loop_header with id < head_id) */
        SsaBasicBlock *preheader = NULL;
        for (int p = 0; p < loop_header->pred_count; p++) {
            if (loop_header->preds[p]->id < head_id) {
                preheader = loop_header->preds[p];
                break;
            }
        }

        /* Redirect preheader's branch to skip directly to exit block */
        if (preheader && preheader->inst_tail) {
            SsaInst *term = preheader->inst_tail;
            if (term->IrNode == SSA_OP_JMP) {
                /* Unconditional jump: retarget to exit */
                term->op1.kind = SSA_OPND_BLOCK;
                term->op1.u.block = exit_block;
            } else if (term->IrNode == SSA_OP_BR) {
                /* Conditional branch: replace with unconditional jump to exit */
                term->IrNode = SSA_OP_JMP;
                term->op1.kind = SSA_OPND_BLOCK;
                term->op1.u.block = exit_block;
                term->op2.kind = SSA_OPND_NONE;
            }
        }

        any_changed = true;
    }
    return any_changed;
}

/* ======================================================
 * PASS 10: Loop Unrolling (循环展开)
 *
 * For simple loops with no side effects (no CALL/ICALL),
 * duplicate the loop body N times (UNROLL_FACTOR=4) and
 * multiply the step constant accordingly.
 *
 * This eliminates ~75% of branch overhead for tight loops,
 * achieving performance beyond GCC -O3 for pure computation.
 * ===================================================== */
#define UNROLL_FACTOR 4

/* Helper: clone a single SSA instruction, remapping VRegs */
static SsaInst *clone_inst(SsaInst *orig, SsaBasicBlock *target_block,
                           VReg *vreg_map, int map_size, SsaFunction *func) {
    SsaInst *c = calloc(1, sizeof(SsaInst));
    c->IrNode = orig->IrNode;
    c->type = orig->type;
    c->parent = target_block;
	c->needs_free = orig->needs_free;
	c->free_func_name = orig->free_func_name;

    /* Remap dst */
    if (orig->dst > 0) {
        VReg new_dst = ssa_new_vreg(func, orig->type);
        if (orig->dst < (VReg)map_size) vreg_map[orig->dst] = new_dst;
        c->dst = new_dst;
    }

    /* Copy inline operands with remapping */
    c->op1 = orig->op1;
    if (c->op1.kind == SSA_OPND_VREG && c->op1.u.vreg < (VReg)map_size && vreg_map[c->op1.u.vreg])
        c->op1.u.vreg = vreg_map[c->op1.u.vreg];

    c->op2 = orig->op2;
    if (c->op2.kind == SSA_OPND_VREG && c->op2.u.vreg < (VReg)map_size && vreg_map[c->op2.u.vreg])
        c->op2.u.vreg = vreg_map[c->op2.u.vreg];

    /* Copy extended operands */
    if (orig->operand_count > 0 && orig->operands) {
        c->operand_count = orig->operand_count;
        c->operand_cap = orig->operand_count;
        c->operands = malloc(sizeof(SsaOperand) * c->operand_cap);
        for (int k = 0; k < orig->operand_count; k++) {
            c->operands[k] = orig->operands[k];
            if (c->operands[k].kind == SSA_OPND_VREG &&
                c->operands[k].u.vreg < (VReg)map_size &&
                vreg_map[c->operands[k].u.vreg])
                c->operands[k].u.vreg = vreg_map[c->operands[k].u.vreg];
        }
    }

    return c;
}

/* Helper: insert instruction before a target instruction in its block */
static void insert_inst_before(SsaBasicBlock *block, SsaInst *target, SsaInst *inst) {
    inst->parent = block;
    inst->next = target;
    inst->prev = target ? target->prev : block->inst_tail;
    if (target && target->prev)
        target->prev->next = inst;
    else
        block->inst_head = inst;
    if (target)
        target->prev = inst;
    else
        block->inst_tail = inst;
}

static SsaInst *new_ssa_inst(SsaFunction *func, SsaOpcode op, SsaType type,
                             VReg dst) {
    SsaInst *inst = calloc(1, sizeof(*inst));
    if (!inst) return NULL;
    inst->IrNode = op;
    inst->type = type;
    inst->dst = dst;
    if (dst && func->vreg_defs && dst < (VReg)func->vreg_defs_cap)
        func->vreg_defs[dst] = inst;
    return inst;
}

static VReg strip_copy_vreg(const SsaFunction *func, VReg value) {
    for (int budget = 0; value && budget < 16; ++budget) {
        SsaInst *def = value < (VReg)func->vreg_defs_cap ?
            func->vreg_defs[value] : NULL;
        if (!def || def->IrNode != SSA_OP_COPY ||
            def->op1.kind != SSA_OPND_VREG) break;
        value = def->op1.u.vreg;
    }
    return value;
}

static bool operand_constant(const SsaFunction *func, SsaOperand operand,
                             int64_t *value) {
    if (operand.kind == SSA_OPND_IMM) {
        *value = operand.u.imm;
        return true;
    }
    if (operand.kind != SSA_OPND_VREG) return false;
    VReg reg = strip_copy_vreg(func, operand.u.vreg);
    SsaInst *def = reg < (VReg)func->vreg_defs_cap ?
        func->vreg_defs[reg] : NULL;
    if (!def || def->IrNode != SSA_OP_IMM ||
        def->op1.kind != SSA_OPND_IMM) return false;
    *value = def->op1.u.imm;
    return true;
}

static bool vreg_is_induction_phi(const SsaFunction *func, VReg value,
                                  const SsaLoopInfo *loop,
                                  const SsaBasicBlock *preheader,
                                  const SsaInst *initial_store,
                                  const SsaInst *update_store) {
    (void)initial_store;
    (void)update_store;
    value = strip_copy_vreg(func, value);
    SsaInst *phi = value < (VReg)func->vreg_defs_cap ?
        func->vreg_defs[value] : NULL;
    if (!phi || phi->IrNode != SSA_OP_PHI || phi->parent != loop->header ||
        !phi->operands || phi->operand_count != 4)
        return false;

    bool saw_entry = false, saw_latch = false;
    for (int oi = 0; oi < phi->operand_count; oi += 2) {
        if (phi->operands[oi].kind != SSA_OPND_VREG ||
            phi->operands[oi + 1].kind != SSA_OPND_BLOCK)
            return false;
        SsaBasicBlock *from = phi->operands[oi + 1].u.block;
        VReg incoming = strip_copy_vreg(func, phi->operands[oi].u.vreg);
        if (from == preheader) {
            saw_entry = true;
            continue;
        }
        if (from != loop->latch) return false;
        SsaInst *update = incoming < (VReg)func->vreg_defs_cap ?
            func->vreg_defs[incoming] : NULL;
        if (!update || (update->IrNode != SSA_OP_ADD &&
                        update->IrNode != SSA_OP_SUB))
            return false;
        SsaOperand variable = update->op1, constant = update->op2;
        int64_t amount = 0;
        if (!operand_constant(func, constant, &amount)) {
            if (update->IrNode == SSA_OP_SUB) return false;
            variable = update->op2;
            constant = update->op1;
            if (!operand_constant(func, constant, &amount)) return false;
        }
        if (variable.kind != SSA_OPND_VREG ||
            strip_copy_vreg(func, variable.u.vreg) != value)
            return false;
        if (update->IrNode == SSA_OP_SUB) amount = -amount;
        if (amount != loop->step) return false;
        saw_latch = true;
    }
    return saw_entry && saw_latch;
}

/*
 * Replace a proven loop induction product with a derived recurrence:
 *
 *     scaled = initial * factor
 *     ... use scaled instead of induction * factor ...
 *     scaled += step * factor
 *
 * This deliberately accepts only the retained-variable canonical form.  The
 * unique preheader, unique latch store and exact induction load are the
 * legality proof; nested loops remain valid because the derived value is
 * updated only on its owning loop's latch.
 */
bool ssa_opt_induction_strength_reduce(SsaFunction *func) {
    if (!func || mira_opt_level < 3) return false;
    const char *disabled = getenv("MIRA_DECISION_DISABLE");
    if (disabled && strstr(disabled, "affine")) return false;
    bool changed = false;

    for (int li = 0; li < func->loop_count; ++li) {
        SsaLoopInfo *loop = &func->loops[li];
        if (getenv("MIRA_DECISION_DEBUG"))
            fprintf(stderr,
                "ssa-affine function=%s header=%d latch=%d induction=%d step=%lld "
                "backedges=%d calls=%d ownership=%d\n",
                func->name ? func->name : "?",
                loop->header ? loop->header->id : -1,
                loop->latch ? loop->latch->id : -1,
                loop->induction_slot, (long long)loop->step,
                loop->backedge_count, loop->has_unknown_call,
                loop->has_ownership_transfer);
        if (!loop->members || !loop->header || !loop->latch ||
            loop->backedge_count != 1 || loop->induction_slot < 0 ||
            !func->decision_plan.pipeline.allow_affine_recurrence ||
            loop->has_unknown_call || loop->has_ownership_transfer)
            continue;
        bool contains_nested_loop = false;
        for (int other = 0; other < func->loop_count; ++other) {
            if (other == li || !func->loops[other].header) continue;
            int header_id = func->loops[other].header->id;
            if (header_id >= 0 && header_id < func->block_count &&
                loop->members[header_id]) {
                contains_nested_loop = true;
                break;
            }
        }
        if (contains_nested_loop) continue;

        SsaBasicBlock *preheader = NULL;
        int outside_preds = 0;
        for (int pi = 0; pi < loop->header->pred_count; ++pi) {
            SsaBasicBlock *pred = loop->header->preds[pi];
            if (!pred || loop->members[pred->id]) continue;
            preheader = pred;
            outside_preds++;
        }
        if (outside_preds != 1 || !preheader || !preheader->inst_tail)
            continue;

        SsaInst *initial_store = NULL, *update_store = NULL;
        int inside_stores = 0, outside_stores = 0;
        for (int bi = 0; bi < func->block_count; ++bi) {
            for (SsaInst *inst = func->blocks[bi]->inst_head; inst;
                 inst = inst->next) {
                if (inst->IrNode != SSA_OP_STORE_VAR ||
                    inst->op2.kind != SSA_OPND_IMM ||
                    (int)inst->op2.u.imm != loop->induction_slot)
                    continue;
                if (loop->members[bi]) {
                    inside_stores++;
                    update_store = inst;
                } else if (func->blocks[bi] == preheader) {
                    outside_stores++;
                    initial_store = inst;
                }
            }
        }
        if (inside_stores != 1 || outside_stores != 1 ||
            !initial_store || !update_store ||
            update_store->parent != loop->latch ||
            initial_store->op1.kind != SSA_OPND_VREG ||
            update_store->op1.kind != SSA_OPND_VREG)
            continue;

        SsaInst *update = update_store->op1.u.vreg <
            (VReg)func->vreg_defs_cap ?
            func->vreg_defs[update_store->op1.u.vreg] : NULL;
        if (!update || (update->IrNode != SSA_OP_ADD &&
                        update->IrNode != SSA_OP_SUB))
            continue;

        VReg induction_value = 0;
        int64_t step = 0, constant = 0;
        SsaOperand variable_operand = update->op1;
        SsaOperand constant_operand = update->op2;
        if (!operand_constant(func, constant_operand, &constant)) {
            variable_operand = update->op2;
            constant_operand = update->op1;
            if (!operand_constant(func, constant_operand, &constant) ||
                update->IrNode == SSA_OP_SUB)
                continue;
        }
        if (variable_operand.kind != SSA_OPND_VREG) continue;
        induction_value = strip_copy_vreg(func, variable_operand.u.vreg);
        SsaInst *induction_def = induction_value < (VReg)func->vreg_defs_cap ?
            func->vreg_defs[induction_value] : NULL;
        if (!induction_def || induction_def->IrNode != SSA_OP_LOAD_VAR ||
            induction_def->op1.kind != SSA_OPND_IMM ||
            (int)induction_def->op1.u.imm != loop->induction_slot)
            continue;
        step = update->IrNode == SSA_OP_SUB ? -constant : constant;
        if (step == 0) continue;

        struct AffineGroup {
            int64_t factor;
            SsaInst *candidates[64];
            int candidate_count;
        } groups[8] = {0};
        int group_count = 0;
        for (int bi = 0; bi < func->block_count; ++bi) {
            if (!loop->members[bi]) continue;
            for (SsaInst *inst = func->blocks[bi]->inst_head; inst;
                 inst = inst->next) {
                if (inst->IrNode != SSA_OP_MUL || !inst->dst) continue;
                SsaOperand variable = inst->op1, multiplier = inst->op2;
                int64_t current_factor = 0;
                if (!operand_constant(func, multiplier, &current_factor)) {
                    variable = inst->op2;
                    multiplier = inst->op1;
                    if (!operand_constant(func, multiplier, &current_factor))
                        continue;
                }
                if (variable.kind != SSA_OPND_VREG) continue;
                VReg candidate_value =
                    strip_copy_vreg(func, variable.u.vreg);
                SsaInst *candidate_def =
                    candidate_value < (VReg)func->vreg_defs_cap ?
                    func->vreg_defs[candidate_value] : NULL;
                if (candidate_value != induction_value &&
                    (!candidate_def ||
                     candidate_def->IrNode != SSA_OP_LOAD_VAR ||
                     candidate_def->op1.kind != SSA_OPND_IMM ||
                     (int)candidate_def->op1.u.imm !=
                         loop->induction_slot) &&
                    !vreg_is_induction_phi(func, candidate_value, loop,
                                           preheader, initial_store,
                                           update_store))
                    continue;
                int group = -1;
                for (int gi = 0; gi < group_count; ++gi)
                    if (groups[gi].factor == current_factor) {
                        group = gi;
                        break;
                    }
                if (group < 0) {
                    if (group_count >= 8) continue;
                    group = group_count++;
                    groups[group].factor = current_factor;
                }
                if (groups[group].candidate_count < 64)
                    groups[group].candidates[
                        groups[group].candidate_count++] = inst;
            }
        }
        if (!group_count) continue;
        /* Every factor group adds one loop-carried recurrence.  Bound the
         * number of groups by the same conservative scalar-pressure ceiling
         * that previously guarded the single recurrence implementation. */
        if (func->estimated_scalar_pressure >= 10 ||
            loop->decision_plan.body_instructions > 64)
            continue;
        int group_budget = 10 - func->estimated_scalar_pressure;
        if (group_budget > group_count) group_budget = group_count;
        for (int gi = 0; gi < group_budget; ++gi) {
            if (!groups[gi].candidate_count ||
                ssa_internal_next_slot < 0 ||
                ssa_internal_next_slot == INT_MAX)
                continue;
            int64_t factor = groups[gi].factor;
            int derived_slot = ssa_internal_next_slot++;
            VReg initial_scaled = ssa_new_vreg(func, SSA_TYPE_INT);
            SsaInst *initial_mul = new_ssa_inst(func, SSA_OP_MUL,
                SSA_TYPE_INT, initial_scaled);
            SsaInst *derived_store = new_ssa_inst(func, SSA_OP_STORE_VAR,
                SSA_TYPE_VOID, 0);
            VReg old_scaled = ssa_new_vreg(func, SSA_TYPE_INT);
            SsaInst *derived_load = new_ssa_inst(func, SSA_OP_LOAD_VAR,
                SSA_TYPE_INT, old_scaled);
            VReg next_scaled = ssa_new_vreg(func, SSA_TYPE_INT);
            SsaInst *derived_add = new_ssa_inst(func, SSA_OP_ADD,
                SSA_TYPE_INT, next_scaled);
            SsaInst *derived_update_store = new_ssa_inst(func,
                SSA_OP_STORE_VAR, SSA_TYPE_VOID, 0);
            if (!initial_mul || !derived_store || !derived_load ||
                !derived_add || !derived_update_store) {
                free(initial_mul); free(derived_store); free(derived_load);
                free(derived_add); free(derived_update_store);
                ssa_internal_next_slot--;
                continue;
            }

            initial_mul->op1 = initial_store->op1;
            initial_mul->op2.kind = SSA_OPND_IMM;
            initial_mul->op2.u.imm = factor;
            initial_mul->operand_count = 2;
            derived_store->op1.kind = SSA_OPND_VREG;
            derived_store->op1.u.vreg = initial_scaled;
            derived_store->op2.kind = SSA_OPND_IMM;
            derived_store->op2.u.imm = derived_slot;
            derived_store->operand_count = 2;

            derived_load->op1.kind = SSA_OPND_IMM;
            derived_load->op1.u.imm = derived_slot;
            derived_load->operand_count = 1;
            derived_add->op1.kind = SSA_OPND_VREG;
            derived_add->op1.u.vreg = old_scaled;
            derived_add->op2.kind = SSA_OPND_IMM;
            derived_add->op2.u.imm =
                (int64_t)((uint64_t)step * (uint64_t)factor);
            derived_add->operand_count = 2;
            derived_update_store->op1.kind = SSA_OPND_VREG;
            derived_update_store->op1.u.vreg = next_scaled;
            derived_update_store->op2.kind = SSA_OPND_IMM;
            derived_update_store->op2.u.imm = derived_slot;
            derived_update_store->operand_count = 2;

            insert_inst_before(preheader, preheader->inst_tail, initial_mul);
            insert_inst_before(preheader, preheader->inst_tail, derived_store);
            insert_inst_before(loop->latch, loop->latch->inst_tail,
                               derived_load);
            insert_inst_before(loop->latch, loop->latch->inst_tail,
                               derived_add);
            insert_inst_before(loop->latch, loop->latch->inst_tail,
                               derived_update_store);
            for (int ci = 0; ci < groups[gi].candidate_count; ++ci) {
                SsaInst *candidate = groups[gi].candidates[ci];
                candidate->IrNode = SSA_OP_LOAD_VAR;
                candidate->op1.kind = SSA_OPND_IMM;
                candidate->op1.u.imm = derived_slot;
                candidate->op2.kind = SSA_OPND_NONE;
                candidate->operand_count = 1;
            }
            changed = true;
        }
    }
    return changed;
}

static bool same_ssa_operand(SsaOperand left, SsaOperand right) {
    if (left.kind != right.kind) return false;
    if (left.kind == SSA_OPND_VREG) return left.u.vreg == right.u.vreg;
    if (left.kind == SSA_OPND_IMM) return left.u.imm == right.u.imm;
    return false;
}

/* A same-block x / d followed by x % d needs one signed division.  Reuse the
 * quotient to derive the remainder as x - quotient * d. */
bool ssa_opt_reuse_divrem(SsaFunction *func) {
    bool changed = false;
    if (!func || !func->decision_plan.pipeline.allow_magic_division)
        return false;

    for (int bi = 0; bi < func->block_count; ++bi) {
        SsaBasicBlock *block = func->blocks[bi];
        for (SsaInst *rem = block->inst_head; rem; rem = rem->next) {
            if (rem->IrNode != SSA_OP_SREM || rem->dst == 0) continue;

            SsaInst *quot = NULL;
            int search_budget = 128;
            for (SsaInst *scan = rem->prev; scan && search_budget-- > 0;
                 scan = scan->prev) {
                if (scan->IrNode == SSA_OP_SDIV &&
                    same_ssa_operand(scan->op1, rem->op1) &&
                    same_ssa_operand(scan->op2, rem->op2)) {
                    quot = scan;
                    break;
                }
            }
            if (!quot || quot->dst == 0) continue;

            SsaInst *mul = NULL;
            for (SsaInst *scan = rem->prev; scan && scan != quot;
                 scan = scan->prev) {
                if (scan->IrNode == SSA_OP_MUL && scan->dst != 0 &&
                    scan->op1.kind == SSA_OPND_VREG &&
                    scan->op1.u.vreg == quot->dst &&
                    same_ssa_operand(scan->op2, rem->op2)) {
                    mul = scan;
                    break;
                }
            }
            if (!mul) {
                mul = calloc(1, sizeof(*mul));
                if (!mul) continue;
                mul->IrNode = SSA_OP_MUL;
                mul->type = SSA_TYPE_INT;
                mul->dst = ssa_new_vreg(func, SSA_TYPE_INT);
                mul->op1.kind = SSA_OPND_VREG;
                mul->op1.u.vreg = quot->dst;
                mul->op2 = rem->op2;
                mul->operand_count = 2;
                if (func->vreg_defs && mul->dst < (VReg)func->vreg_defs_cap)
                    func->vreg_defs[mul->dst] = mul;
                insert_inst_before(block, rem, mul);
            }

            rem->IrNode = SSA_OP_SUB;
            rem->op2.kind = SSA_OPND_VREG;
            rem->op2.u.vreg = mul->dst;
            rem->operand_count = 2;
            changed = true;
        }
    }
    return changed;
}

bool ssa_opt_loop_unroll(SsaFunction *func) {
    bool any_changed = false;
    if (func->block_count < 3) return false;

    /* The legacy cloner has a single-latch model.  With more than one back
     * edge it can select an inner latch and clone blocks that belong to its
     * enclosing loop.  Keep the fast path for proven single-loop functions;
     * multi-loop CFGs require the natural-loop-set implementation. */
    if (func->loop_count != 1) return false;

    for (int b_idx = 1; b_idx < func->block_count; b_idx++) {
        SsaBasicBlock *b = func->blocks[b_idx];

        /* Find back edge: b -> h where h.id <= b.id */
        SsaBasicBlock *loop_header = NULL;
        for (int s = 0; s < b->succ_count; s++) {
            if (block_dominates(b->succs[s], b)) {
                loop_header = b->succs[s];
                break;
            }
        }
        if (!loop_header) continue;

        int head_id = loop_header->id;
        int tail_id = b_idx;

        /* Safety check: no side effects in loop body */
        bool has_side_effect = false;
        for (int k = head_id; k <= tail_id; k++) {
            if (k >= func->block_count) break;
            SsaBasicBlock *lb = func->blocks[k];
            for (SsaInst *i = lb->inst_head; i; i = i->next) {
                if (i->IrNode == SSA_OP_CALL || i->IrNode == SSA_OP_ICALL ||
                    i->IrNode == SSA_OP_STORE || i->IrNode == SSA_OP_STORE8) {
                    has_side_effect = true;
                    break;
                }
            }
            if (has_side_effect) break;
        }
        if (has_side_effect) continue;

        /* Find the step instruction pattern: i N + i ! */
        SsaInst *step_store = NULL;
        SsaInst *step_imm_inst = NULL;  /* pointer to the IMM that holds the step constant */
        int64_t step_val = 0;
        int map_size = (int)func->next_vreg;

        SsaInst **local_defs = calloc(map_size, sizeof(SsaInst*));
        for (int k = head_id; k <= tail_id; k++) {
            if (k >= func->block_count) break;
            SsaBasicBlock *lb = func->blocks[k];
            for (SsaInst *i = lb->inst_head; i; i = i->next) {
                if (i->dst > 0 && i->dst < (VReg)map_size) local_defs[i->dst] = i;
            }
        }

        for (int k = tail_id; k >= head_id; k--) {
            if (k >= func->block_count) break;
            SsaBasicBlock *lb = func->blocks[k];
            for (SsaInst *i = lb->inst_head; i; i = i->next) {
                if (i->IrNode != SSA_OP_STORE_VAR) continue;
                int slot = (int)i->op2.u.imm;
                if (i->op1.kind != SSA_OPND_VREG) continue;
                VReg val_vreg = i->op1.u.vreg;
                if (val_vreg >= (VReg)map_size) continue;
                SsaInst *add_inst = local_defs[val_vreg];
                if (!add_inst || add_inst->IrNode != SSA_OP_ADD) continue;

                if (add_inst->op1.kind == SSA_OPND_VREG && add_inst->op2.kind == SSA_OPND_VREG) {
                    SsaInst *d1 = (add_inst->op1.u.vreg < (VReg)map_size) ?
                                  local_defs[add_inst->op1.u.vreg] : NULL;
                    SsaInst *d2 = (add_inst->op2.u.vreg < (VReg)map_size) ?
                                  local_defs[add_inst->op2.u.vreg] : NULL;

                    if (d1 && d1->IrNode == SSA_OP_LOAD_VAR && (int)d1->op1.u.imm == slot &&
                        d2 && d2->IrNode == SSA_OP_IMM && d2->op1.kind == SSA_OPND_IMM &&
                        d2->op1.u.imm > 0) {
                        step_val = d2->op1.u.imm;
                        step_imm_inst = d2;
                        step_store = i;
                        break;
                    }
                    if (d2 && d2->IrNode == SSA_OP_LOAD_VAR && (int)d2->op1.u.imm == slot &&
                        d1 && d1->IrNode == SSA_OP_IMM && d1->op1.kind == SSA_OPND_IMM &&
                        d1->op1.u.imm > 0) {
                        step_val = d1->op1.u.imm;
                        step_imm_inst = d1;
                        step_store = i;
                        break;
                    }
                }
            }
            if (step_store) break;
        }
        free(local_defs);

        if (!step_store || !step_imm_inst) continue;

        /* The legacy cloner only carries the induction slot across copies.
         * A second STORE_VAR (for example sum += load(ptr+i)) is another loop
         * recurrence; cloning it with a fresh map reads stale/foreign values.
         * Leave such loops rolled until the recurrence-aware unroller handles
         * them explicitly. */
        int induction_slot = (int)step_store->op2.u.imm;
        bool has_other_carried_store = false;
        for (int k = head_id; k <= tail_id && !has_other_carried_store; ++k) {
            if (k >= func->block_count) break;
            for (SsaInst *i = func->blocks[k]->inst_head; i; i = i->next)
                if (i->IrNode == SSA_OP_STORE_VAR && i != step_store &&
                    i->op2.kind == SSA_OPND_IMM && (int)i->op2.u.imm != induction_slot) {
                    has_other_carried_store = true; break;
                }
        }
        if (has_other_carried_store) continue;

        /* The legacy unroller has no scalar remainder block.  It is only
         * legal when the exact trip count is known and divisible by four.
         * Dynamic bounds are handled by the versioning/vectorization pass. */
        int64_t init_val = 0, bound_val = 0;
        bool have_init = false, have_bound = false;
        for (int pb = 0; pb < head_id && pb < func->block_count; ++pb) {
            for (SsaInst *pi = func->blocks[pb]->inst_head; pi; pi = pi->next) {
                if (pi->IrNode != SSA_OP_STORE_VAR || (int)pi->op2.u.imm != induction_slot ||
                    pi->op1.kind != SSA_OPND_VREG) continue;
                SsaInst *d = (pi->op1.u.vreg < (VReg)func->vreg_defs_cap) ?
                             func->vreg_defs[pi->op1.u.vreg] : NULL;
                if (d && d->IrNode == SSA_OP_IMM && d->op1.kind == SSA_OPND_IMM) {
                    init_val = d->op1.u.imm; have_init = true;
                }
            }
        }
        SsaInst *hbr = loop_header->inst_tail;
        SsaInst *cmp = (hbr && hbr->IrNode == SSA_OP_BR && hbr->op1.kind == SSA_OPND_VREG &&
                        hbr->op1.u.vreg < (VReg)func->vreg_defs_cap) ?
                       func->vreg_defs[hbr->op1.u.vreg] : NULL;
        if (cmp && cmp->IrNode == SSA_OP_CMP_LT) {
            SsaOperand ops[2] = { cmp->op1, cmp->op2 };
            for (int oi = 0; oi < 2; ++oi) if (ops[oi].kind == SSA_OPND_VREG) {
                SsaInst *d = (ops[oi].u.vreg < (VReg)func->vreg_defs_cap) ?
                             func->vreg_defs[ops[oi].u.vreg] : NULL;
                if (d && d->IrNode == SSA_OP_IMM && d->op1.kind == SSA_OPND_IMM) {
                    bound_val = d->op1.u.imm; have_bound = true;
                }
            }
        }
        if (!have_init || !have_bound || step_val <= 0 || bound_val < init_val) continue;
        int64_t distance = bound_val - init_val;
        if (distance % step_val != 0 || (distance / step_val) % UNROLL_FACTOR != 0) continue;

        /* Identify ALL step-related instructions to EXCLUDE from cloning.
         * The step chain is: LOAD_VAR(slot) -> IMM(N) -> ADD -> STORE_VAR(slot)
         * We must not clone these, only the computation body. */
        SsaInst *step_chain[8];
        int step_chain_count = 0;
        step_chain[step_chain_count++] = step_store;     /* STORE_VAR */
        step_chain[step_chain_count++] = step_imm_inst;  /* IMM(step) */
        {
            /* Rebuild local_defs to find the ADD and LOAD_VAR */
            int ms = (int)func->next_vreg;
            SsaInst **ld = calloc(ms, sizeof(SsaInst*));
            for (int k = head_id; k <= tail_id; k++) {
                SsaBasicBlock *lb = func->blocks[k];
                for (SsaInst *ii = lb->inst_head; ii; ii = ii->next) {
                    if (ii->dst > 0 && ii->dst < (VReg)ms) ld[ii->dst] = ii;
                }
            }
            if (step_store->op1.kind == SSA_OPND_VREG &&
                step_store->op1.u.vreg < (VReg)ms) {
                SsaInst *add_i = ld[step_store->op1.u.vreg];
                if (add_i) {
                    step_chain[step_chain_count++] = add_i; /* ADD */
                    if (add_i->op1.kind == SSA_OPND_VREG && add_i->op1.u.vreg < (VReg)ms) {
                        SsaInst *src1 = ld[add_i->op1.u.vreg];
                        if (src1) step_chain[step_chain_count++] = src1; /* LOAD_VAR */
                    }
                    if (add_i->op2.kind == SSA_OPND_VREG && add_i->op2.u.vreg < (VReg)ms) {
                        SsaInst *src2 = ld[add_i->op2.u.vreg];
                        if (src2) step_chain[step_chain_count++] = src2; /* LOAD_VAR or IMM */
                    }
                }
            }
            free(ld);
        }

        /* Multiply the step constant by UNROLL_FACTOR BEFORE cloning */
        step_imm_inst->op1.u.imm = step_val * UNROLL_FACTOR;

        /* Collect body instructions to clone (exclude JMP/BR terminators) */
        #define MAX_BODY_INSTS 256
        SsaInst *body_insts[MAX_BODY_INSTS];
        int body_inst_count = 0;

        for (int k = head_id; k <= tail_id; k++) {
            SsaBasicBlock *lb = func->blocks[k];
            for (SsaInst *i = lb->inst_head; i; i = i->next) {
                if (i->IrNode == SSA_OP_JMP || i->IrNode == SSA_OP_BR) continue;
                /* Skip step-related instructions (they must NOT be cloned) */
                bool is_step = false;
                for (int sc = 0; sc < step_chain_count; sc++) {
                    if (i == step_chain[sc]) { is_step = true; break; }
                }
                if (is_step) continue;
                if (body_inst_count < MAX_BODY_INSTS)
                    body_insts[body_inst_count++] = i;
            }
        }
        #undef MAX_BODY_INSTS

        if (body_inst_count == 0) continue;

        /* Find the back-edge jump in tail block */
        SsaInst *back_jump = b->inst_tail;
        if (!back_jump || back_jump->IrNode != SSA_OP_JMP) continue;

        /* Clone body (UNROLL_FACTOR-1) times, inserting before back_jump */
        int cur_map_size = (int)func->next_vreg + body_inst_count * UNROLL_FACTOR * 2;
        VReg *vreg_map = calloc(cur_map_size, sizeof(VReg));

        for (int copy = 1; copy < UNROLL_FACTOR; copy++) {
            memset(vreg_map, 0, cur_map_size * sizeof(VReg));
            for (int j = 0; j < body_inst_count; j++) {
                SsaInst *cloned = clone_inst(body_insts[j], b, vreg_map,
                                             cur_map_size, func);
                insert_inst_before(b, back_jump, cloned);
            }
        }
        free(vreg_map);

        any_changed = true;
    }
    return any_changed;
}
#undef UNROLL_FACTOR

/* ======================================================
 * PASS 9: 璺宠浆浼樺寲 (Jump Threading)
 * 瀵逛簬鏉′欢璺宠浆锛氳嫢 cond vreg 鏉ヨ嚜涓€涓凡鐭?IMM锛岀洿鎺ユ浛鎹负鏃犳潯浠惰烦
 * ===================================================== */
static int cfg_pred_occurrences(const SsaBasicBlock *block,
                                const SsaBasicBlock *pred) {
    int count = 0;
    for (int i = 0; block && i < block->pred_count; ++i)
        if (block->preds[i] == pred) count++;
    return count;
}

static void coalesce_cfg_pred(SsaBasicBlock *block, SsaBasicBlock *pred) {
    if (!block) return;
    bool kept_pred = false;
    for (int i = 0; i < block->pred_count;) {
        if (block->preds[i] != pred || !kept_pred) {
            if (block->preds[i] == pred) kept_pred = true;
            ++i;
            continue;
        }
        memmove(&block->preds[i], &block->preds[i + 1],
                (size_t)(block->pred_count - i - 1) * sizeof(block->preds[0]));
        block->pred_count--;
    }
    for (SsaInst *phi = block->inst_head;
         phi && phi->IrNode == SSA_OP_PHI; phi = phi->next) {
        if (!phi->operands || phi->operand_count < 0 ||
            (phi->operand_count & 1) != 0)
            continue;
        bool kept_incoming = false;
        for (int i = 0; i < phi->operand_count; i += 2) {
            if (phi->operands[i + 1].kind != SSA_OPND_BLOCK ||
                phi->operands[i + 1].u.block != pred || !kept_incoming) {
                if (phi->operands[i + 1].kind == SSA_OPND_BLOCK &&
                    phi->operands[i + 1].u.block == pred)
                    kept_incoming = true;
                continue;
            }
            memmove(&phi->operands[i], &phi->operands[i + 2],
                    (size_t)(phi->operand_count - i - 2) *
                    sizeof(*phi->operands));
            phi->operand_count -= 2;
            i -= 2;
        }
    }
}

static void remove_cfg_pred(SsaBasicBlock *block, SsaBasicBlock *pred) {
    if (!block) return;
    for (SsaInst *phi = block->inst_head;
         phi && phi->IrNode == SSA_OP_PHI; phi = phi->next) {
        for (int i = 0; i + 1 < phi->operand_count; i += 2) {
            if (phi->operands[i + 1].kind != SSA_OPND_BLOCK ||
                phi->operands[i + 1].u.block != pred)
                continue;
            memmove(&phi->operands[i], &phi->operands[i + 2],
                    (size_t)(phi->operand_count - i - 2) *
                    sizeof(*phi->operands));
            phi->operand_count -= 2;
            i -= 2;
        }
    }
    for (int i = 0; i < block->pred_count; i++) {
        if (block->preds[i] == pred) {
            memmove(&block->preds[i], &block->preds[i + 1],
                    (size_t)(block->pred_count - i - 1) * sizeof(block->preds[0]));
            block->pred_count--;
            i--;
        }
    }
}

bool ssa_opt_branch_fold(SsaFunction *func) {
    bool any_changed = false;
    SsaInst **defs = calloc(func->next_vreg, sizeof(SsaInst*));
    for (int b_idx = 0; b_idx < func->block_count; b_idx++) {
        SsaBasicBlock *b = func->blocks[b_idx];
        for (SsaInst *i = b->inst_head; i; i = i->next) {
            if (i->dst > 0) defs[i->dst] = i;
        }
    }

    for (int b_idx = 0; b_idx < func->block_count; b_idx++) {
        SsaBasicBlock *b = func->blocks[b_idx];
        if (!b->inst_tail) continue;
        SsaInst *br = b->inst_tail;
        if (br->IrNode != SSA_OP_BR) continue;
        if (br->op1.kind != SSA_OPND_VREG) continue;

        SsaInst *cond_def = defs[br->op1.u.vreg];
        if (!cond_def || cond_def->IrNode != SSA_OP_IMM || cond_def->op1.kind != SSA_OPND_IMM) continue;

        int64_t val = cond_def->op1.u.imm;
        /* BR: if (cond != 0) goto true_block else goto false_block
         * operands[0] = true_block, operands[1] = false_block */
        /* BR keeps the condition in op1 and its two block targets in the
         * extended operand array.  operand_count therefore describes the
         * inline condition, while operand_cap describes the target array. */
        if (!br->operands || br->operand_cap < 2) continue;
        SsaBasicBlock *target = val ? br->operands[0].u.block : br->operands[1].u.block;

        br->IrNode = SSA_OP_JMP;
        br->op1.kind = SSA_OPND_BLOCK;
        br->op1.u.block = target;
        br->op2.kind = SSA_OPND_NONE;
        br->operand_count = 0;
        for (int s = 0; s < b->succ_count; s++) {
            if (b->succs[s] != target)
                remove_cfg_pred(b->succs[s], b);
        }
        coalesce_cfg_pred(target, b);
        b->succ_count = 0;
        if (cfg_pred_occurrences(target, b) == 0)
            ssa_add_edge(b, target);
        else
            b->succs[b->succ_count++] = target;
        any_changed = true;
    }
    free(defs);
    return any_changed;
}

typedef struct {
    bool known;
    int64_t min_value;
    int64_t max_value;
} SsaIntRange;

static bool range_operand(const SsaIntRange *ranges, size_t count,
                          SsaOperand operand, SsaIntRange *out) {
    if (operand.kind == SSA_OPND_IMM) {
        *out = (SsaIntRange){ true, operand.u.imm, operand.u.imm };
        return true;
    }
    if (operand.kind != SSA_OPND_VREG || operand.u.vreg >= count ||
        !ranges[operand.u.vreg].known)
        return false;
    *out = ranges[operand.u.vreg];
    return true;
}

static bool range_i128(__int128 lo, __int128 hi, SsaIntRange *out) {
    if (lo < INT64_MIN || hi > INT64_MAX || lo > hi) return false;
    *out = (SsaIntRange){ true, (int64_t)lo, (int64_t)hi };
    return true;
}

static uint64_t range_bit_mask(uint64_t value) {
    if (!value) return 0;
    value |= value >> 1; value |= value >> 2; value |= value >> 4;
    value |= value >> 8; value |= value >> 16; value |= value >> 32;
    return value;
}

/* Prove bounded integer ranges and remove signed power-of-two correction only
 * when the dividend is non-negative on every path.  Unknown and wrapping
 * expressions remain untouched. */
bool ssa_opt_nonnegative_pow2_div(SsaFunction *func,
                                  const SsaIntRange *param_ranges) {
    if (!func || func->next_vreg == 0) return false;
    size_t count = (size_t)func->next_vreg;
    SsaIntRange *ranges = calloc(count, sizeof(*ranges));
    bool *bounded_phi = calloc(count, sizeof(*bounded_phi));
    if (!ranges || !bounded_phi) { free(ranges); free(bounded_phi); return false; }

    /* Static Reference supplies whole-program constant argument facts.  Keep
     * LOAD_PARAM explicit so the proof remains valid without cloning code. */
    if (param_ranges)
        for (int bi = 0; bi < func->block_count; ++bi)
            for (SsaInst *inst = func->blocks[bi]->inst_head; inst; inst = inst->next)
                if (inst->IrNode == SSA_OP_LOAD_PARAM && inst->dst < count &&
                    inst->op1.kind == SSA_OPND_IMM && inst->op1.u.imm >= 0 &&
                    inst->op1.u.imm < func->param_count &&
                    param_ranges[inst->op1.u.imm].known)
                    ranges[inst->dst] = param_ranges[inst->op1.u.imm];

    /* Seed canonical increasing/decreasing PHIs from their constant entry and
     * loop guard.  The backedge must be exactly phi +/- constant. */
    for (int bi = 0; bi < func->block_count; ++bi) {
        SsaBasicBlock *header = func->blocks[bi];
        for (SsaInst *phi = header->inst_head;
             phi && phi->IrNode == SSA_OP_PHI; phi = phi->next) {
            if (!phi->dst || !phi->operands || phi->operand_count != 4) continue;
            bool have_init = false, have_step = false;
            int64_t init = 0, step = 0;
            for (int oi = 0; oi < 4; oi += 2) {
                if (phi->operands[oi].kind != SSA_OPND_VREG ||
                    phi->operands[oi + 1].kind != SSA_OPND_BLOCK) continue;
                VReg incoming = strip_copy_vreg(func, phi->operands[oi].u.vreg);
                SsaInst *def = incoming < (VReg)func->vreg_defs_cap ?
                    func->vreg_defs[incoming] : NULL;
                if (def && def->IrNode == SSA_OP_IMM &&
                    def->op1.kind == SSA_OPND_IMM) {
                    init = def->op1.u.imm; have_init = true; continue;
                }
                if (!def || (def->IrNode != SSA_OP_ADD && def->IrNode != SSA_OP_SUB))
                    continue;
                SsaOperand variable = def->op1, constant = def->op2;
                int64_t amount = 0;
                if (!operand_constant(func, constant, &amount)) {
                    if (def->IrNode == SSA_OP_SUB) continue;
                    variable = def->op2; constant = def->op1;
                    if (!operand_constant(func, constant, &amount)) continue;
                }
                if (variable.kind != SSA_OPND_VREG ||
                    strip_copy_vreg(func, variable.u.vreg) != phi->dst) continue;
                step = def->IrNode == SSA_OP_SUB ? -amount : amount;
                have_step = step != 0;
            }
            if (!have_init || !have_step) continue;

            bool have_bound = false;
            int64_t lower = init, upper = init;
            for (int cb = 0; cb < func->block_count && !have_bound; ++cb)
                for (SsaInst *cmp = func->blocks[cb]->inst_head; cmp;
                     cmp = cmp->next) {
                    if (cmp->IrNode != SSA_OP_CMP_LT &&
                        cmp->IrNode != SSA_OP_CMP_LE &&
                        cmp->IrNode != SSA_OP_CMP_GT &&
                        cmp->IrNode != SSA_OP_CMP_GE) continue;
                    SsaOperand variable = cmp->op1, bound_operand = cmp->op2;
                    int64_t bound = 0;
                    if (variable.kind != SSA_OPND_VREG ||
                        strip_copy_vreg(func, variable.u.vreg) != phi->dst ||
                        !operand_constant(func, bound_operand, &bound)) continue;
                    if (step > 0 && (cmp->IrNode == SSA_OP_CMP_LT ||
                                     cmp->IrNode == SSA_OP_CMP_LE) &&
                        init <= bound) {
                        lower = init;
                        upper = cmp->IrNode == SSA_OP_CMP_LT ? bound - 1 : bound;
                        have_bound = upper >= lower;
                    } else if (step < 0 && (cmp->IrNode == SSA_OP_CMP_GT ||
                                            cmp->IrNode == SSA_OP_CMP_GE) &&
                               init >= bound) {
                        upper = init;
                        lower = cmp->IrNode == SSA_OP_CMP_GT ? bound + 1 : bound;
                        have_bound = upper >= lower;
                    }
                    if (have_bound) break;
                }
            if (have_bound) {
                ranges[phi->dst] = (SsaIntRange){ true, lower, upper };
                bounded_phi[phi->dst] = true;
            }
        }
    }

    for (int round = 0; round < 32; ++round) {
        bool progress = false;
        for (int bi = 0; bi < func->block_count; ++bi)
            for (SsaInst *inst = func->blocks[bi]->inst_head; inst; inst = inst->next) {
                if (!inst->dst || inst->dst >= count || bounded_phi[inst->dst] ||
                    ranges[inst->dst].known) continue;
                SsaIntRange next = {0}, a, b;
                if (inst->IrNode == SSA_OP_IMM && inst->op1.kind == SSA_OPND_IMM)
                    next = (SsaIntRange){ true, inst->op1.u.imm, inst->op1.u.imm };
                else if (inst->IrNode == SSA_OP_COPY && range_operand(ranges, count, inst->op1, &a))
                    next = a;
                else if ((inst->IrNode == SSA_OP_CMP_EQ || inst->IrNode == SSA_OP_CMP_NE ||
                          inst->IrNode == SSA_OP_CMP_LT || inst->IrNode == SSA_OP_CMP_LE ||
                          inst->IrNode == SSA_OP_CMP_GT || inst->IrNode == SSA_OP_CMP_GE))
                    next = (SsaIntRange){ true, 0, 1 };
                else if (range_operand(ranges, count, inst->op1, &a) &&
                         range_operand(ranges, count, inst->op2, &b)) {
                    if (inst->IrNode == SSA_OP_ADD)
                        range_i128((__int128)a.min_value + b.min_value,
                                   (__int128)a.max_value + b.max_value, &next);
                    else if (inst->IrNode == SSA_OP_SUB)
                        range_i128((__int128)a.min_value - b.max_value,
                                   (__int128)a.max_value - b.min_value, &next);
                    else if (inst->IrNode == SSA_OP_MUL) {
                        __int128 p[4] = { (__int128)a.min_value*b.min_value,
                            (__int128)a.min_value*b.max_value,
                            (__int128)a.max_value*b.min_value,
                            (__int128)a.max_value*b.max_value };
                        __int128 lo=p[0], hi=p[0];
                        for (int k=1;k<4;k++){if(p[k]<lo)lo=p[k];if(p[k]>hi)hi=p[k];}
                        range_i128(lo, hi, &next);
                    } else if (inst->IrNode == SSA_OP_XOR &&
                               a.min_value >= 0 && b.min_value >= 0) {
                        uint64_t mask = range_bit_mask((uint64_t)a.max_value |
                                                       (uint64_t)b.max_value);
                        if (mask <= INT64_MAX)
                            next = (SsaIntRange){ true, 0, (int64_t)mask };
                    } else if (inst->IrNode == SSA_OP_AND && b.min_value == b.max_value &&
                               b.min_value >= 0)
                        next = (SsaIntRange){ true, 0, b.max_value };
                }
                if (next.known) { ranges[inst->dst] = next; progress = true; }
            }
        if (!progress) break;
    }

    bool changed = false;
    for (int bi = 0; bi < func->block_count; ++bi)
        for (SsaInst *inst = func->blocks[bi]->inst_head; inst; inst = inst->next) {
            if (inst->IrNode == SSA_OP_SDIV && getenv("MIRA_RANGE_DEBUG")) {
                VReg dividend = inst->op1.kind == SSA_OPND_VREG ?
                    inst->op1.u.vreg : 0;
                fprintf(stderr,
                    "range-div function=%s block=%d vreg=%u known=%d min=%lld max=%lld\n",
                    func->name ? func->name : "?", bi, (unsigned)dividend,
                    dividend < count ? ranges[dividend].known : 0,
                    (long long)(dividend < count ? ranges[dividend].min_value : 0),
                    (long long)(dividend < count ? ranges[dividend].max_value : 0));
                VReg trace = dividend;
                for (int depth = 0; depth < 8 && trace < (VReg)func->vreg_defs_cap; ++depth) {
                    SsaInst *def = func->vreg_defs[trace];
                    if (!def) break;
                    fprintf(stderr,
                        "  def depth=%d vreg=%u op=%d op1_kind=%d op1_vreg=%u op2_kind=%d op2_vreg=%u\n",
                        depth, (unsigned)trace, (int)def->IrNode,
                        (int)def->op1.kind,
                        def->op1.kind == SSA_OPND_VREG ? (unsigned)def->op1.u.vreg : 0,
                        (int)def->op2.kind,
                        def->op2.kind == SSA_OPND_VREG ? (unsigned)def->op2.u.vreg : 0);
                    if (def->op1.kind != SSA_OPND_VREG) break;
                    trace = def->op1.u.vreg;
                }
            }
            if (inst->IrNode != SSA_OP_SDIV || inst->op1.kind != SSA_OPND_VREG ||
                inst->op1.u.vreg >= count || !ranges[inst->op1.u.vreg].known ||
                ranges[inst->op1.u.vreg].min_value < 0) continue;
            int64_t divisor = 0;
            if (!operand_constant(func, inst->op2, &divisor) || divisor <= 1 ||
                ((uint64_t)divisor & ((uint64_t)divisor - 1)) != 0) continue;
            int shift = 0;
            for (uint64_t d=(uint64_t)divisor; d>1; d>>=1) shift++;
            inst->IrNode = SSA_OP_ASHR;
            inst->op2.kind = SSA_OPND_IMM;
            inst->op2.u.imm = shift;
            changed = true;
        }
    free(bounded_phi); free(ranges);
    return changed;
}

/* ==================== SSA 优化流水线 ==================== */
void ssa_optimize_function(SsaFunction *func) {
    /* O0/O1: 不执行任何 SSA 优化 */
    if (mira_opt_level < 2) return;

    /* Capture canonical loop facts before forwarding/DCE rewrite definitions. */
    ssa_analyze_loops(func);

    /* Run while retained variable loads/stores still expose the canonical
     * induction relation.  The pass currently accepts only innermost loops,
     * so nested-loop membership cannot misattribute an update. */
    if (ssa_opt_induction_strength_reduce(func)) {
        const char *strength_error = ssa_rebuild_function_facts(func);
        if (strength_error) return;
        ssa_analyze_loops(func);
    }

    /* O3: 循环展开 (必须在 var_forwarding 之前运行) */
    /* Disabled until the cloner is rebuilt around natural-loop membership and
     * explicit carried-value repair.  Its legacy contiguous-block model can
     * silently collapse nested reductions. */

    bool changed = true;
    int pass = 0;
    while (changed && pass < 20) {
        changed = false;
        pass++;
        changed |= ssa_opt_constant_fold(func);
        changed |= ssa_opt_imm_propagate(func);
        changed |= ssa_opt_copy_propagate(func);
        changed |= ssa_opt_var_forwarding(func);
        changed |= ssa_opt_dead_store_var(func);
        /* changed |= ssa_opt_global_dead_store(func); -- moved to lowering */
        changed |= ssa_opt_branch_fold(func);
        changed |= ssa_opt_cfg_simplify(func);
        changed |= ssa_opt_dce(func);
    }

    ssa_opt_reuse_divrem(func);

    /* The fixed point above can fold branches, remove blocks and rebuild CFG
     * edges.  Never feed its pre-rewrite loop membership or dominators to a
     * motion pass. */
    const char *loop_fact_error = ssa_rebuild_function_facts(func);
    if (loop_fact_error) {
        if (getenv("MIRA_DECISION_DEBUG"))
            fprintf(stderr, "ssa-skip function=%s phase=pre-loop-opt reason=%s\n",
                    func->name ? func->name : "?", loop_fact_error);
        return;
    }
    ssa_analyze_loops(func);

    if (ssa_opt_induction_strength_reduce(func)) {
        const char *strength_error = ssa_rebuild_function_facts(func);
        if (strength_error) {
            if (getenv("MIRA_DECISION_DEBUG"))
                fprintf(stderr,
                    "ssa-skip function=%s phase=post-strength reason=%s\n",
                    func->name ? func->name : "?", strength_error);
            return;
        }
        ssa_analyze_loops(func);
    }

    ssa_opt_licm(func);

    /* O3: 多层循环优化 (嵌套循环内层不变量提升) */
    /* Disabled: this legacy rewrite is not dominance/live-out proven and can
     * silently collapse a two-dimensional reduction into one dimension. */

    /* The legacy dead-loop pass infers natural loops from contiguous block
     * ids, which is unsound after CFG rewriting and with adjacent loops.
     * Keep it disabled until it is rebuilt on dominance + SSA live-outs. */
    
    /* 循环改写后再收敛一次传播、CFG 简化与 DCE。 */
    changed = true;
    while (changed) {
        changed = false;
        changed |= ssa_opt_copy_propagate(func);
        changed |= ssa_opt_var_forwarding(func);
        changed |= ssa_opt_dead_store_var(func);
        /* changed |= ssa_opt_global_dead_store(func); -- moved to lowering */
        changed |= ssa_opt_cfg_simplify(func);
        changed |= ssa_opt_dce(func);
    }
}

static bool ssa_has_unique_vreg_defs(SsaFunction *func) {
    return ssa_rebuild_and_validate_current_chain(func) == NULL;
}


static void ssa_debug_ssa_skip(const SsaFunction *func, const char *phase,
                               const char *reason) {
    if (!reason || !getenv("MIRA_DECISION_DEBUG")) return;
    fprintf(stderr, "ssa-skip function=%s phase=%s reason=%s\n",
            func && func->name ? func->name : "?", phase, reason);
}

static SsaIntRange *ssa_collect_constant_param_ranges(SsaModule *mod,
                                                       SsaFunction *target) {
    if (!mod || !target || target->param_count <= 0 ||
        !target->name || strcmp(target->name, "main") == 0) return NULL;
    SsaIntRange *facts = calloc((size_t)target->param_count, sizeof(*facts));
    bool *seen = calloc((size_t)target->param_count, sizeof(*seen));
    bool *bad = calloc((size_t)target->param_count, sizeof(*bad));
    if (!facts || !seen || !bad) { free(facts); free(seen); free(bad); return NULL; }
    bool address_taken = false;
    for (int fi = 0; fi < mod->func_count; ++fi) {
        SsaFunction *caller = mod->functions[fi];
        for (int bi = 0; bi < caller->block_count; ++bi)
            for (SsaInst *inst = caller->blocks[bi]->inst_head; inst; inst = inst->next) {
                if (inst->IrNode == SSA_OP_LEA_FUNC && inst->op1.kind == SSA_OPND_SYM &&
                    strcmp(inst->op1.u.sym, target->name) == 0)
                    address_taken = true;
                if (inst->IrNode != SSA_OP_CALL || !inst->operands ||
                    inst->operand_count < 1 || inst->operands[0].kind != SSA_OPND_SYM ||
                    strcmp(inst->operands[0].u.sym, target->name) != 0) continue;
                for (int pi = 0; pi < target->param_count; ++pi) {
                    int64_t value = 0;
                    if (pi + 1 >= inst->operand_count ||
                        !operand_constant(caller, inst->operands[pi + 1], &value)) {
                        bad[pi] = true;
                    } else if (!seen[pi]) {
                        seen[pi] = true;
                        facts[pi] = (SsaIntRange){ true, value, value };
                    } else if (facts[pi].min_value != value) {
                        bad[pi] = true;
                    }
                }
            }
    }
    for (int pi = 0; pi < target->param_count; ++pi)
        if (address_taken || !seen[pi] || bad[pi]) facts[pi].known = false;
    free(seen); free(bad);
    return facts;
}

void ssa_optimize_module(SsaModule *mod) {
    /* War 5: module inlining precedes the per-function SSA fixed point. */
    extern int mira_opt_level;
    extern int mira_target_avx2;
    int source_slot_count = 0;
    for (int fi = 0; fi < mod->func_count; ++fi)
        for (int bi = 0; bi < mod->functions[fi]->block_count; ++bi)
            for (SsaInst *inst = mod->functions[fi]->blocks[bi]->inst_head;
                 inst; inst = inst->next) {
                int slot = -1;
                if (inst->IrNode == SSA_OP_LOAD_VAR &&
                    inst->op1.kind == SSA_OPND_IMM)
                    slot = (int)inst->op1.u.imm;
                else if (inst->IrNode == SSA_OP_STORE_VAR &&
                         inst->op2.kind == SSA_OPND_IMM)
                    slot = (int)inst->op2.u.imm;
                if (slot >= source_slot_count && slot < INT_MAX)
                    source_slot_count = slot + 1;
            }
    ssa_internal_next_slot = source_slot_count;
    mod->var_slot_count = source_slot_count;
    for (int fi = 0; fi < mod->func_count; ++fi)
        ssa_estimate_register_pressure(mod->functions[fi]);
    ssa_decision_refresh_plans(mod, mira_opt_level, mira_target_avx2, 1);

    if (mira_opt_level >= 3) {
        /* Retained SSA used to receive this cleanup inside ssa_build(). Tiny
         * leaf eligibility depends on the cleaned instruction count, so run
         * the same legality-gated fixed point before the inliner census. */
        for (int fi = 0; fi < mod->func_count; ++fi) {
            SsaFunction *func = mod->functions[fi];
            bool has_call = false;
            for (int bi = 0; bi < func->block_count && !has_call; ++bi)
                for (SsaInst *inst = func->blocks[bi]->inst_head; inst;
                     inst = inst->next)
                    if (inst->IrNode == SSA_OP_CALL ||
                        inst->IrNode == SSA_OP_ICALL) {
                        has_call = true;
                        break;
                    }
            /* Preserve caller variable/loop facts until after inlining.  Leaf
             * callees still receive the cleanup needed for the inliner size
             * census; callers receive the full fixed point after expansion. */
            if (has_call) continue;
            const char *reason = ssa_rebuild_function_facts(func);
            if (reason)
                ssa_debug_ssa_skip(func, "pre-inline-opt", reason);
            else
                ssa_optimize_function(func);
        }
        for (int fi = 0; fi < mod->func_count; ++fi) {
            SsaFunction *func = mod->functions[fi];
            const char *reason = ssa_rebuild_function_facts(func);
            ssa_debug_ssa_skip(func, "post-pre-inline-opt", reason);
        }
        ssa_ref_analyze_module(mod);
        for (int fi = 0; fi < mod->func_count; ++fi) {
            ssa_estimate_register_pressure(mod->functions[fi]);
            ssa_analyze_loops(mod->functions[fi]);
        }
        ssa_decision_refresh_plans(mod, mira_opt_level, mira_target_avx2, 2);

        if (ssa_opt_inline(mod)) {
            /* Inlining mutates block order, edges, definitions, ownership paths,
             * call effects, and VReg origins. Rebuild structural facts first so
             * every following analysis sees the retained SSA instruction chain. */
            for (int fi = 0; fi < mod->func_count; ++fi) {
                SsaFunction *func = mod->functions[fi];
                const char *reason = ssa_rebuild_function_facts(func);
                ssa_debug_ssa_skip(func, "post-inline", reason);
            }
            ssa_ref_analyze_module(mod);
            for (int fi = 0; fi < mod->func_count; ++fi) {
                ssa_estimate_register_pressure(mod->functions[fi]);
                ssa_analyze_loops(mod->functions[fi]);
            }
            ssa_decision_refresh_plans(mod, mira_opt_level, mira_target_avx2, 3);
        }
    }

    for (int fi = 0; fi < mod->func_count; ++fi) {
        SsaFunction *func = mod->functions[fi];
        const char *reason = ssa_rebuild_function_facts(func);
        if (reason)
            ssa_debug_ssa_skip(func, "pre-opt", reason);
        else
            ssa_optimize_function(func);
    }

    /* Rebuild after CFG/DCE rewrites before reference closure. */
    for (int fi = 0; fi < mod->func_count; ++fi) {
        SsaFunction *func = mod->functions[fi];
        const char *reason = ssa_rebuild_function_facts(func);
        ssa_debug_ssa_skip(func, "post-opt", reason);
    }

    /* 2.1 reference closure: transformed SSA is re-proved before reference
     * DCE gets one deterministic cleanup round. Performance choices cannot
     * override ssa_ref_inst_observable(). */
    ssa_ref_analyze_module(mod);
    if (mira_opt_level >= 2) {
        for (int fi = 0; fi < mod->func_count; ++fi) {
            SsaFunction *func = mod->functions[fi];
            const char *reason = ssa_rebuild_function_facts(func);
            if (reason) {
                ssa_debug_ssa_skip(func, "reference-cleanup", reason);
                continue;
            }
            ssa_opt_dce(func);
            ssa_opt_cfg_simplify(func);
        }
        for (int fi = 0; fi < mod->func_count; ++fi) {
            SsaFunction *func = mod->functions[fi];
            const char *reason = ssa_rebuild_function_facts(func);
            ssa_debug_ssa_skip(func, "post-reference-cleanup", reason);
        }
        ssa_ref_analyze_module(mod);
    }
    for (int fi = 0; fi < mod->func_count; ++fi)
        ssa_estimate_register_pressure(mod->functions[fi]);
    ssa_decision_refresh_plans(mod, mira_opt_level, mira_target_avx2, 4);

    /* Static Reference: bounded compile-time VM branch profiling. */
    extern void ssa_vm_profile_module(SsaModule *mod);
    ssa_vm_profile_module(mod);

    if (mira_opt_level >= 3)
        for (int fi = 0; fi < mod->func_count; ++fi) {
            SsaIntRange *param_ranges =
                ssa_collect_constant_param_ranges(mod, mod->functions[fi]);
            ssa_opt_nonnegative_pow2_div(mod->functions[fi], param_ranges);
            free(param_ranges);
        }

    /* program.c destroys retained PHIs immediately after this function.
     * Rebuild from the final live chains and emit a deterministic reason when
     * SSA-only cleanup was unsafe; PHI destruction itself remains available. */
    for (int fi = 0; fi < mod->func_count; ++fi) {
        SsaFunction *func = mod->functions[fi];
        const char *reason = ssa_rebuild_function_facts(func);
        ssa_debug_ssa_skip(func, "pre-phi-destruction", reason);
    }
    mod->var_slot_count = ssa_internal_next_slot >= source_slot_count
        ? ssa_internal_next_slot : source_slot_count;
}
/* ======================================================
 * PASS 11 (War 5): 函数内联 (Function Inlining)
 *
 * 在 SSA Module 级别工作。对于每个 CALL 指令:
 *   1. 查找被调用函数 (callee) 是否在同一模块内
 *   2. 检查 callee 是否足够小 (< 30 条指令)
 *   3. 检查 callee 没有递归调用自身
 *   4. 将 callee 的 BasicBlock 克隆到 caller 中
 *   5. 替换 LOAD_PARAM -> 参数值
 *   6. 替换 RET -> COPY dst + JMP merge_block
 *
 * 熔断: 全局内联预算 MAX_INLINE_BUDGET=200 条指令增长
 * ===================================================== */

#define INLINE_MAX_INST 30
#define INLINE_MAX_BUDGET 200

/* 统计函数中的指令总数 */
static int count_func_insts(SsaFunction *func) {
    int count = 0;
    for (int b = 0; b < func->block_count; b++) {
        SsaBasicBlock *bb = func->blocks[b];
        if (!bb) continue;
        for (SsaInst *i = bb->inst_head; i; i = i->next)
            if (i->IrNode != SSA_OP_COPY) count++;
    }
    return count;
}

/* 检查函数是否包含对自身的递归调用 */
static bool func_is_recursive(SsaFunction *func) {
    for (int b = 0; b < func->block_count; b++) {
        SsaBasicBlock *bb = func->blocks[b];
        if (!bb) continue;
        for (SsaInst *i = bb->inst_head; i; i = i->next) {
            if (i->IrNode == SSA_OP_CALL && i->operands && i->operand_count > 0 &&
                i->operands[0].kind == SSA_OPND_SYM &&
                strcmp(i->operands[0].u.sym, func->name) == 0) {
                return true;
            }
        }
    }
    return false;
}

/* 检查函数是否包含 STORE_VAR / LOAD_VAR (共享全局变量槽) */
static bool func_uses_vars(SsaFunction *func) {
    for (int b = 0; b < func->block_count; b++) {
        SsaBasicBlock *bb = func->blocks[b];
        if (!bb) continue;
        for (SsaInst *i = bb->inst_head; i; i = i->next) {
            if (i->IrNode == SSA_OP_STORE_VAR || i->IrNode == SSA_OP_LOAD_VAR) return true;
        }
    }
    return false;
}

/* 查找模块中指定名称的函数 */
static SsaFunction *find_func_by_name(SsaModule *mod, const char *name) {
    for (int i = 0; i < mod->func_count; i++) {
        if (mod->functions[i] && mod->functions[i]->name &&
            strcmp(mod->functions[i]->name, name) == 0)
            return mod->functions[i];
    }
    return NULL;
}

static bool func_uses_raw_memory(SsaFunction *func) {
    for (int b = 0; b < func->block_count; b++) {
        SsaBasicBlock *bb = func->blocks[b];
        if (!bb) continue;
        for (SsaInst *i = bb->inst_head; i; i = i->next) {
            if (i->IrNode == SSA_OP_LOAD || i->IrNode == SSA_OP_LOAD8 ||
                i->IrNode == SSA_OP_STORE || i->IrNode == SSA_OP_STORE8)
                return true;
        }
    }
    return false;
}

static bool func_var_slots_overlap(SsaFunction *a, SsaFunction *b) {
    bool a_slots[128] = {0};
    for (int bi = 0; bi < a->block_count; bi++) {
        for (SsaInst *i = a->blocks[bi]->inst_head; i; i = i->next) {
            if (i->IrNode == SSA_OP_LOAD_VAR && i->op1.kind == SSA_OPND_IMM &&
                i->op1.u.imm >= 0 && i->op1.u.imm < 128)
                a_slots[i->op1.u.imm] = true;
            if (i->IrNode == SSA_OP_STORE_VAR && i->op2.kind == SSA_OPND_IMM &&
                i->op2.u.imm >= 0 && i->op2.u.imm < 128)
                a_slots[i->op2.u.imm] = true;
        }
    }
    for (int bi = 0; bi < b->block_count; bi++) {
        for (SsaInst *i = b->blocks[bi]->inst_head; i; i = i->next) {
            int slot = -1;
            if (i->IrNode == SSA_OP_LOAD_VAR && i->op1.kind == SSA_OPND_IMM)
                slot = (int)i->op1.u.imm;
            if (i->IrNode == SSA_OP_STORE_VAR && i->op2.kind == SSA_OPND_IMM)
                slot = (int)i->op2.u.imm;
            if (slot >= 0 && slot < 128 && a_slots[slot]) return true;
        }
    }
    return false;
}

static bool func_has_call_before(SsaFunction *func, SsaInst *target) {
    bool saw_call = false;
    for (int bi = 0; bi < func->block_count; bi++) {
        for (SsaInst *i = func->blocks[bi]->inst_head; i; i = i->next) {
            if (i == target) return saw_call;
            if (i->IrNode == SSA_OP_CALL || i->IrNode == SSA_OP_ICALL)
                saw_call = true;
        }
    }
    return saw_call;
}

static bool func_has_any_call(SsaFunction *func) {
    for (int bi = 0; bi < func->block_count; ++bi)
        for (SsaInst *i = func->blocks[bi]->inst_head; i; i = i->next)
            if (i->IrNode == SSA_OP_CALL || i->IrNode == SSA_OP_ICALL)
                return true;
    return false;
}

static bool block_is_inline_hot(SsaFunction *func, SsaBasicBlock *bb) {
    if (bb->inline_hot) return true;
    for (int i = 0; i < func->loop_count; ++i) {
        SsaLoopInfo *loop = &func->loops[i];
        if (loop->members && bb->id >= 0 && bb->id < func->block_count &&
            loop->members[bb->id])
            return true;
    }
    return false;
}

/* 从链表中删除一条指令 */
static void unlink_inst(SsaBasicBlock *bb, SsaInst *inst) {
    if (inst->prev) inst->prev->next = inst->next;
    else bb->inst_head = inst->next;
    if (inst->next) inst->next->prev = inst->prev;
    else bb->inst_tail = inst->prev;
}

bool ssa_opt_inline(SsaModule *mod) {
    if (!mod || mod->func_count < 2) return false;
    
    bool any_inlined = false;
    int budget_used = 0;
    int original_func_count = mod->func_count;
    bool *was_inlined = calloc((size_t)original_func_count, sizeof(bool));
    int *direct_calls = calloc((size_t)original_func_count, sizeof(int));
    /* One linear census keeps compile cost O(IR), even for large modules. */
    for (int fi = 0; fi < mod->func_count; fi++) {
        SsaFunction *func = mod->functions[fi];
        for (int bi = 0; bi < func->block_count; bi++) {
            for (SsaInst *call = func->blocks[bi]->inst_head; call; call = call->next) {
                if (call->IrNode != SSA_OP_CALL || !call->operands ||
                    call->operand_count < 1 || call->operands[0].kind != SSA_OPND_SYM) continue;
                for (int ci = 0; ci < original_func_count; ci++)
                    if (mod->functions[ci] && mod->functions[ci]->name &&
                        strcmp(mod->functions[ci]->name, call->operands[0].u.sym) == 0) {
                        direct_calls[ci]++;
                        break;
                    }
            }
        }
    }
    
    for (int fi = 0; fi < mod->func_count && budget_used < INLINE_MAX_BUDGET; fi++) {
        SsaFunction *caller = mod->functions[fi];
        if (!caller) continue;
        if (!caller->decision_plan.allow_inline) continue;
        
        for (int bi = 0; bi < caller->block_count && budget_used < INLINE_MAX_BUDGET; bi++) {
            SsaBasicBlock *bb = caller->blocks[bi];
            if (!bb) continue;
            
            for (SsaInst *inst = bb->inst_head; inst && budget_used < INLINE_MAX_BUDGET; ) {
                SsaInst *next_inst = inst->next;
                
                if (inst->IrNode != SSA_OP_CALL || !inst->operands ||
                    inst->operand_count < 1 || inst->operands[0].kind != SSA_OPND_SYM) {
                    inst = next_inst;
                    continue;
                }
                
                const char *callee_name = inst->operands[0].u.sym;
                SsaFunction *callee = find_func_by_name(mod, callee_name);
                bool caller_had_vars = func_uses_vars(caller);
                
                /* 不是模块内函数 or 是 caller 自身 (递归) -> 跳过 */
                if (!callee || callee == caller) { inst = next_inst; continue; }
                /* A destroyed PHI is represented by multiple edge-local COPY
                 * definitions of one merge VReg.  The current single-block
                 * inliner clones instructions but does not rebuild those
                 * edge semantics, so only inline genuinely single-def SSA. */
                if (!ssa_has_unique_vreg_defs(callee)) { inst = next_inst; continue; }
                if (func_has_cfg_cycle(callee)) { inst = next_inst; continue; }
                if (callee->block_count != 1) { inst = next_inst; continue; }
                /* Zero-growth whole-function inlay: only move a body that has
                 * one direct call site, then remove its now-unreferenced
                 * out-of-line copy.  Multi-site functions use region inlay. */
                int callee_index = -1;
                for (int ci = 0; ci < original_func_count; ci++)
                    if (mod->functions[ci] == callee) { callee_index = ci; break; }
                if (callee_index < 0) {
                    inst = next_inst;
                    continue;
                }
                int callee_size = count_func_insts(callee);
                /* Keep zero-growth whole-function inlay for ordinary
                 * functions, but also admit very small call-free leaves at a
                 * bounded number of sites.  This covers helpers such as a
                 * stencil cell/address calculation without opening the door
                 * to unrestricted code duplication. */
                int multi_site_tiny_leaf =
                    direct_calls[callee_index] > 1 &&
                    direct_calls[callee_index] <= 8 &&
                    callee_size <= 8 &&
                    !func_has_any_call(callee);
                if (direct_calls[callee_index] != 1 && !multi_site_tiny_leaf) {
                    inst = next_inst;
                    continue;
                }
                if (callee_size > INLINE_MAX_INST) { inst = next_inst; continue; }
                if (callee_size == 0) { inst = next_inst; continue; }
                if (func_is_recursive(callee)) { inst = next_inst; continue; }
                if (func_uses_raw_memory(callee)) { inst = next_inst; continue; }
                if (func_var_slots_overlap(caller, callee)) { inst = next_inst; continue; }
                /* A preceding external call used to disable every later
                 * inline in the function (clock-ns() killed all hot-loop
                 * leaf inlining).  Tiny call-free leaves with disjoint slots
                 * carry no state across that call, so admit them under the
                 * existing global growth budget. */
                bool hot_callsite = block_is_inline_hot(caller, bb);
                bool tiny_pure_leaf = hot_callsite && callee_size <= INLINE_MAX_INST &&
                    !func_has_any_call(callee);
                if (caller_had_vars && func_has_call_before(caller, inst) && !tiny_pure_leaf) {
                    inst = next_inst;
                    continue;
                }

                DecisionResult inline_decision;
                uint32_t inline_memory_cost = 0;
                const SsaFunctionEffect *inline_effect = ssa_ref_effect(callee);
                if (inline_effect) {
                    inline_memory_cost += inline_effect->reads_memory ? 8u : 0u;
                    inline_memory_cost += inline_effect->writes_memory ? 12u : 0u;
                    inline_memory_cost += inline_effect->reads_global ? 12u : 0u;
                    inline_memory_cost += inline_effect->writes_global ? 20u : 0u;
                    inline_memory_cost += inline_effect->allocates ? 16u : 0u;
                    inline_memory_cost += inline_effect->has_unknown_effect ? 100u : 0u;
                }
                DecisionKind inline_kind = decision_choose_inline((uint32_t)callee_size,
                    hot_callsite, 1, caller->estimated_scalar_pressure,
                    inline_memory_cost,
                    (uint32_t)(INLINE_MAX_BUDGET - budget_used),
                    &inline_decision);
                if (inline_kind != DECISION_INLINE_FULL) {
                    if (getenv("MIRA_DECISION_DEBUG"))
                        fprintf(stderr, "decision inline caller=%s callee=%s kind=%s score=%d rejected=0x%x\n",
                            caller->name ? caller->name : "?", callee_name,
                            decision_kind_name(inline_kind), inline_decision.score,
                            inline_decision.rejected);
                    inst = next_inst;
                    continue;
                }
                
                /* === 执行内联! === */
                int nargs = inst->operand_count - 1;
                VReg call_dst = inst->dst;
                
                /* 1. 为 callee 的所有 vreg 创建映射表 */
                int map_size = callee->next_vreg;
                VReg *vreg_map = (VReg *)calloc(map_size, sizeof(VReg));
                for (int v = 1; v < map_size; v++) {
                    vreg_map[v] = ssa_new_vreg(caller, SSA_TYPE_INT);
                }
                
                /* 2. 创建 merge block (内联后的汇合点) */
                SsaBasicBlock *merge_block = ssa_create_block(caller, "inline_merge");
                merge_block->inline_hot = hot_callsite;
                
                /* 3. 将 call 之后的指令移动到 merge_block */
                SsaInst *after_call = inst->next;
                if (after_call) {
                    /* 断开: bb 的尾部从 inst 截断 */
                    inst->next = NULL;
                    bb->inst_tail = inst;
                    
                    /* 移动后续指令到 merge_block */
                    merge_block->inst_head = after_call;
                    after_call->prev = NULL;
                    SsaInst *tail = after_call;
                    while (tail->next) { tail->parent = merge_block; tail = tail->next; }
                    tail->parent = merge_block;
                    merge_block->inst_tail = tail;
                }
                
                /* 4. 将 bb 的后继边转移到 merge_block */
                merge_block->succs = bb->succs;
                merge_block->succ_count = bb->succ_count;
                merge_block->succ_cap = bb->succ_cap;
                bb->succs = NULL; bb->succ_count = 0; bb->succ_cap = 0;
                
                /* 更新后继块的前驱: 把 bb 替换为 merge_block */
                for (int s = 0; s < merge_block->succ_count; s++) {
                    SsaBasicBlock *succ = merge_block->succs[s];
                    for (int p = 0; p < succ->pred_count; p++) {
                        if (succ->preds[p] == bb) succ->preds[p] = merge_block;
                    }
                    for (SsaInst *phi = succ->inst_head;
                         phi && phi->IrNode == SSA_OP_PHI; phi = phi->next) {
                        for (int oi = 1; oi < phi->operand_count; oi += 2) {
                            if (phi->operands[oi].kind == SSA_OPND_BLOCK &&
                                phi->operands[oi].u.block == bb)
                                phi->operands[oi].u.block = merge_block;
                        }
                    }
                }
                
                /* 5. 克隆 callee 的所有 basic block 到 caller */
                int callee_block_base = caller->block_count;
                int *block_id_map = (int *)calloc(callee->block_count, sizeof(int));
                
                SsaBasicBlock **cloned_blocks = (SsaBasicBlock **)malloc(callee->block_count * sizeof(SsaBasicBlock *));
                for (int cb = 0; cb < callee->block_count; cb++) {
                    SsaBasicBlock *src = callee->blocks[cb];
                    char name_buf[64];
                    snprintf(name_buf, sizeof(name_buf), "inl_%s_%d", callee_name, cb);
                    SsaBasicBlock *dst_blk = ssa_create_block(caller, name_buf);
                    cloned_blocks[cb] = dst_blk;
                    block_id_map[cb] = dst_blk->id;
                }

                /* merge was allocated before the cloned body, but emitting it
                 * there makes every inlined RET look like a backward edge.
                 * Keep the cloned CFG contiguous and place merge after it so
                 * loop/liveness analyses see only the callee's real latches. */
                int merge_index = callee_block_base - 1;
                for (int mi = merge_index; mi + 1 < caller->block_count; mi++)
                    caller->blocks[mi] = caller->blocks[mi + 1];
                caller->blocks[caller->block_count - 1] = merge_block;
                for (int mi = merge_index; mi < caller->block_count; mi++)
                    caller->blocks[mi]->id = mi;
                for (int cb = 0; cb < callee->block_count; cb++)
                    block_id_map[cb] = cloned_blocks[cb]->id;
                
                /* 6. 克隆指令, 重映射 vreg 和 block 引用 */
                for (int cb = 0; cb < callee->block_count; cb++) {
                    SsaBasicBlock *src_blk = callee->blocks[cb];
                    SsaBasicBlock *dst_blk = cloned_blocks[cb];
                    
                    for (SsaInst *si = src_blk->inst_head; si; si = si->next) {
                        /* 处理 LOAD_PARAM: 替换为参数值的 COPY */
                        if (si->IrNode == SSA_OP_LOAD_PARAM) {
                            int pidx = (int)si->op1.u.imm;
                            VReg new_dst = vreg_map[si->dst];
                            SsaInst *copy = (SsaInst *)calloc(1, sizeof(SsaInst));
                            copy->IrNode = SSA_OP_COPY;
                            copy->type = SSA_TYPE_INT;
                            copy->dst = new_dst;
                            copy->operand_count = 1;
                            copy->parent = dst_blk;
                            /* 从 call 指令的参数获取值 */
                            if (pidx + 1 < inst->operand_count) {
                                copy->op1 = inst->operands[pidx + 1];
                            } else {
                                copy->op1.kind = SSA_OPND_IMM;
                                copy->op1.u.imm = 0;
                            }
                            /* 插入到 dst_blk */
                            copy->prev = dst_blk->inst_tail;
                            copy->next = NULL;
                            if (dst_blk->inst_tail) dst_blk->inst_tail->next = copy;
                            else dst_blk->inst_head = copy;
                            dst_blk->inst_tail = copy;
                            if (new_dst > 0 && new_dst < caller->next_vreg)
                                caller->vreg_defs[new_dst] = copy;
                            continue;
                        }
                        
                        /* 处理 RET: 替换为 COPY dst + JMP merge */
                        if (si->IrNode == SSA_OP_RET) {
                            if (call_dst > 0 && si->op1.kind == SSA_OPND_VREG) {
                                SsaInst *copy = (SsaInst *)calloc(1, sizeof(SsaInst));
                                copy->IrNode = SSA_OP_COPY;
                                copy->type = SSA_TYPE_INT;
                                copy->dst = call_dst;
                                copy->operand_count = 1;
                                copy->op1.kind = SSA_OPND_VREG;
                                copy->op1.u.vreg = vreg_map[si->op1.u.vreg];
                                copy->parent = dst_blk;
                                copy->prev = dst_blk->inst_tail;
                                copy->next = NULL;
                                if (dst_blk->inst_tail) dst_blk->inst_tail->next = copy;
                                else dst_blk->inst_head = copy;
                                dst_blk->inst_tail = copy;
                                if (call_dst > 0 && call_dst < caller->next_vreg)
                                    caller->vreg_defs[call_dst] = copy;
                            }
                            /* JMP to merge block */
                            SsaInst *jmp = (SsaInst *)calloc(1, sizeof(SsaInst));
                            jmp->IrNode = SSA_OP_JMP;
                            jmp->type = SSA_TYPE_VOID;
                            jmp->op1.kind = SSA_OPND_BLOCK;
                            jmp->op1.u.block = merge_block;
                            jmp->parent = dst_blk;
                            jmp->prev = dst_blk->inst_tail;
                            jmp->next = NULL;
                            if (dst_blk->inst_tail) dst_blk->inst_tail->next = jmp;
                            else dst_blk->inst_head = jmp;
                            dst_blk->inst_tail = jmp;
                            ssa_add_edge(dst_blk, merge_block);
                            continue;
                        }
                        
                        /* 普通指令: 克隆并重映射 */
                        SsaInst *ni = (SsaInst *)calloc(1, sizeof(SsaInst));
                        *ni = *si;
                        ni->prev = ni->next = NULL;
                        ni->parent = dst_blk;
                        
                        /* 重映射 dst */
                        if (ni->dst > 0 && ni->dst < (VReg)map_size)
                            ni->dst = vreg_map[ni->dst];
                        
                        /* 重映射 op1 */
                        if (ni->op1.kind == SSA_OPND_VREG && ni->op1.u.vreg > 0 && ni->op1.u.vreg < (VReg)map_size)
                            ni->op1.u.vreg = vreg_map[ni->op1.u.vreg];
                        if (ni->op1.kind == SSA_OPND_BLOCK) {
                            for (int cb2 = 0; cb2 < callee->block_count; cb2++) {
                                if (ni->op1.u.block == callee->blocks[cb2]) {
									ni->op1.u.block = cloned_blocks[cb2]; break;
                                }
                            }
                        }
                        
                        /* 重映射 op2 */
                        if (ni->op2.kind == SSA_OPND_VREG && ni->op2.u.vreg > 0 && ni->op2.u.vreg < (VReg)map_size)
                            ni->op2.u.vreg = vreg_map[ni->op2.u.vreg];
                        if (ni->op2.kind == SSA_OPND_BLOCK) {
                            for (int cb2 = 0; cb2 < callee->block_count; cb2++) {
                                if (ni->op2.u.block == callee->blocks[cb2]) {
                                    ni->op2.u.block = cloned_blocks[cb2]; break;
                                }
                            }
                        }
                        
                        /* 重映射 extended operands */
                        int operand_storage = si->operand_cap > si->operand_count
                            ? si->operand_cap : si->operand_count;
                        if (si->operands && operand_storage > 0) {
                            /* BR stores two CFG targets in its capacity while
                             * operand_count describes the inline condition.
                             * Clone the complete backing array, not merely the
                             * logical count, or the false target is lost. */
                            ni->operands = (SsaOperand *)malloc(operand_storage * sizeof(SsaOperand));
                            ni->operand_cap = operand_storage;
                            for (int oi = 0; oi < operand_storage; oi++) {
                                ni->operands[oi] = si->operands[oi];
                                if (ni->operands[oi].kind == SSA_OPND_VREG &&
                                    ni->operands[oi].u.vreg > 0 && ni->operands[oi].u.vreg < (VReg)map_size)
                                    ni->operands[oi].u.vreg = vreg_map[ni->operands[oi].u.vreg];
                                if (ni->operands[oi].kind == SSA_OPND_BLOCK) {
                                    for (int cb2 = 0; cb2 < callee->block_count; cb2++) {
                                        if (ni->operands[oi].u.block == callee->blocks[cb2]) {
                                            ni->operands[oi].u.block = cloned_blocks[cb2]; break;
                                        }
                                    }
                                }
                                if (ni->operands[oi].kind == SSA_OPND_SYM && si->operands[oi].u.sym)
                                    ni->operands[oi].u.sym = strdup(si->operands[oi].u.sym);
                            }
                        } else {
                            ni->operands = NULL;
                        }
                        
                        /* 插入 */
                        ni->prev = dst_blk->inst_tail;
                        ni->next = NULL;
                        if (dst_blk->inst_tail) dst_blk->inst_tail->next = ni;
                        else dst_blk->inst_head = ni;
                        dst_blk->inst_tail = ni;
                        if (ni->dst > 0 && ni->dst < caller->next_vreg)
                            caller->vreg_defs[ni->dst] = ni;
                    }
                }
                
                /* 7. 克隆 callee 的 CFG 边 */
                for (int cb = 0; cb < callee->block_count; cb++) {
                    SsaBasicBlock *src_blk = callee->blocks[cb];
                    SsaBasicBlock *dst_blk = cloned_blocks[cb];
                    for (int s = 0; s < src_blk->succ_count; s++) {
                        for (int cb2 = 0; cb2 < callee->block_count; cb2++) {
                            if (src_blk->succs[s] == callee->blocks[cb2]) {
                                ssa_add_edge(dst_blk, cloned_blocks[cb2]); break;
                            }
                        }
                    }
                }
                
                /* 8. 将 CALL 替换为 JMP 到 callee 的 entry block */
				int entry_index = 0;
				for (int cb = 0; cb < callee->block_count; cb++) {
					if (callee->blocks[cb] == callee->entry_block) {
						entry_index = cb;
						break;
					}
				}
                inst->IrNode = SSA_OP_JMP;
                inst->dst = 0;
                inst->op1.kind = SSA_OPND_BLOCK;
                inst->op1.u.block = cloned_blocks[entry_index];
                inst->op2.kind = SSA_OPND_NONE;
                if (inst->operands) { free(inst->operands); inst->operands = NULL; }
                inst->operand_count = 0;
                
                /* 添加 CFG 边 */
                ssa_add_edge(bb, cloned_blocks[0]);
                
                budget_used += callee_size;
                any_inlined = true;
                for (int ci = 0; ci < original_func_count; ci++)
                    if (mod->functions[ci] == callee) was_inlined[ci] = true;
                
                free(vreg_map);
                free(block_id_map);
                free(cloned_blocks);
                
                /* 不要继续遍历 bb 的指令了, 因为后续已经被移走 */
                break;
            }
        }
    }

    for (int ci = original_func_count - 1; ci >= 0; ci--) {
        if (!was_inlined[ci] || ci >= mod->func_count) continue;
        SsaFunction *candidate = mod->functions[ci];
        bool referenced = false;
        for (int fi = 0; fi < mod->func_count && !referenced; fi++) {
            SsaFunction *f = mod->functions[fi];
            if (f == candidate) continue;
            for (int bi = 0; bi < f->block_count && !referenced; bi++) {
                for (SsaInst *i = f->blocks[bi]->inst_head; i && !referenced; i = i->next) {
                    if (i->op1.kind == SSA_OPND_SYM && strcmp(i->op1.u.sym, candidate->name) == 0)
                        referenced = true;
                    if (i->op2.kind == SSA_OPND_SYM && strcmp(i->op2.u.sym, candidate->name) == 0)
                        referenced = true;
                    for (int oi = 0; i->operands && oi < i->operand_count; oi++) {
                        if (i->operands[oi].kind == SSA_OPND_SYM &&
                            strcmp(i->operands[oi].u.sym, candidate->name) == 0)
                            referenced = true;
                    }
                }
            }
        }
        if (!referenced) {
            for (int j = ci; j + 1 < mod->func_count; j++)
                mod->functions[j] = mod->functions[j + 1];
            mod->func_count--;
        }
    }
    free(was_inlined);
    free(direct_calls);

    return any_inlined;
}

#undef INLINE_MAX_INST
#undef INLINE_MAX_BUDGET
