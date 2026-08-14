/* Compile-time bounded SSA trial runner.
 *
 * Static reasoning remains authoritative.  This runner is only entered for
 * internal calls whose arguments are compile-time integers.  It never folds
 * the call result: the observations are used solely to lay out hot CFG traces.
 * Unsupported operations and the 2 ms deadline are hard bail-out points.
 */
#include "ir_ssa.h"
#include "decision.h"
#include "../mira.h"
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

#define SSA_VM_BUDGET_NS 2000000LL
#define SSA_VM_MAX_ARGS 32

typedef struct {
    int64_t value;
    unsigned char known;
} VmValue;

typedef struct {
    SsaFunction *func;
    uint64_t *hits;
} VmProfile;

typedef struct {
    int direct_calls;
    unsigned char escaped;
    unsigned char known[SSA_VM_MAX_ARGS];
    unsigned char conflict[SSA_VM_MAX_ARGS];
    int64_t value[SSA_VM_MAX_ARGS];
} VmCallFacts;

static int64_t vm_now_ns(void) {
#ifdef _WIN32
    LARGE_INTEGER c, f;
    QueryPerformanceCounter(&c);
    QueryPerformanceFrequency(&f);
    return (int64_t)((c.QuadPart / f.QuadPart) * 1000000000LL +
        (c.QuadPart % f.QuadPart) * 1000000000LL / f.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
#endif
}

static SsaFunction *vm_find_func(SsaModule *mod, const char *name) {
    for (int i = 0; i < mod->func_count; ++i)
        if (mod->functions[i] && mod->functions[i]->name &&
            strcmp(mod->functions[i]->name, name) == 0)
            return mod->functions[i];
    return NULL;
}

static SsaInst **vm_build_defs(SsaFunction *func) {
    SsaInst **defs = calloc((size_t)func->next_vreg, sizeof(*defs));
    if (!defs) return NULL;
    for (int b = 0; b < func->block_count; ++b)
        for (SsaInst *i = func->blocks[b]->inst_head; i; i = i->next)
            if (i->dst > 0 && i->dst < func->next_vreg) defs[i->dst] = i;
    return defs;
}

static int vm_const_vreg(SsaInst **defs, VReg v, int64_t *out, int depth) {
    if (!v || depth > 16 || !defs[v]) return 0;
    SsaInst *d = defs[v];
    if (d->IrNode == SSA_OP_IMM && d->op1.kind == SSA_OPND_IMM) {
        *out = d->op1.u.imm;
        return 1;
    }
    if (d->IrNode == SSA_OP_COPY && d->op1.kind == SSA_OPND_VREG)
        return vm_const_vreg(defs, d->op1.u.vreg, out, depth + 1);
    return 0;
}

static int vm_has_unique_vreg_defs(SsaFunction *func) {
    if (!func || func->next_vreg <= 1) return 1;
    int *defs = calloc((size_t)func->next_vreg, sizeof(*defs));
    if (!defs) return 0;
    int unique = 1;
    for (int bi = 0; bi < func->block_count && unique; ++bi) {
        for (SsaInst *i = func->blocks[bi]->inst_head; i; i = i->next) {
            if (i->dst > 0 && i->dst < func->next_vreg &&
                ++defs[i->dst] > 1) {
                unique = 0;
                break;
            }
        }
    }
    free(defs);
    return unique;
}

/* Promote a VM prerequisite into an optimizer fact.  In a closed module, an
 * internal function parameter may be frozen when every direct call supplies
 * the same compile-time integer and the symbol never escapes as a value. */
static VmCallFacts *vm_specialize_constant_params(SsaModule *mod) {
    VmCallFacts *facts = calloc((size_t)mod->func_count, sizeof(*facts));
    if (!facts) return NULL;
    for (int ci = 0; ci < mod->func_count; ++ci) {
        SsaFunction *caller = mod->functions[ci];
        SsaInst **defs = vm_build_defs(caller);
        if (!defs) continue;
        for (int bi = 0; bi < caller->block_count; ++bi) {
            for (SsaInst *i = caller->blocks[bi]->inst_head; i; i = i->next) {
                for (int oi = 0; i->operands && oi < i->operand_count; ++oi) {
                    if (i->operands[oi].kind != SSA_OPND_SYM) continue;
                    SsaFunction *target = vm_find_func(mod, i->operands[oi].u.sym);
                    if (!target) continue;
                    int ti = 0; while (ti < mod->func_count && mod->functions[ti] != target) ti++;
                    if (ti == mod->func_count) continue;
                    if (i->IrNode != SSA_OP_CALL || oi != 0) facts[ti].escaped = 1;
                }
                if (i->IrNode != SSA_OP_CALL || !i->operands || i->operand_count < 1 ||
                    i->operands[0].kind != SSA_OPND_SYM) continue;
                SsaFunction *callee = vm_find_func(mod, i->operands[0].u.sym);
                int fi = 0; while (fi < mod->func_count && mod->functions[fi] != callee) fi++;
                /* A self-call is still a real call site.  Its changing
                 * arguments must participate in the all-call-site proof;
                 * skipping it can freeze a recursive parameter to the
                 * constant supplied by an outer entry call. */
                if (!callee || fi == mod->func_count ||
                    callee->param_count > SSA_VM_MAX_ARGS ||
                    i->operand_count - 1 != callee->param_count) continue;
                facts[fi].direct_calls++;
                for (int p = 0; p < callee->param_count; ++p) {
                    int64_t v;
                    SsaOperand op = i->operands[p + 1];
                    int ok = op.kind == SSA_OPND_IMM
                        ? (v = op.u.imm, 1)
                        : (op.kind == SSA_OPND_VREG && vm_const_vreg(defs, op.u.vreg, &v, 0));
                    if (!ok) { facts[fi].conflict[p] = 1; continue; }
                    if (!facts[fi].known[p]) {
                        facts[fi].known[p] = 1; facts[fi].value[p] = v;
                    } else if (facts[fi].value[p] != v) facts[fi].conflict[p] = 1;
                }
            }
        }
        free(defs);
    }
    extern void ssa_optimize_function(SsaFunction *func);
    for (int fi = 0; fi < mod->func_count; ++fi) {
        SsaFunction *f = mod->functions[fi];
        if (!f || facts[fi].escaped || facts[fi].direct_calls == 0) continue;
        int changed = 0;
        for (int bi = 0; bi < f->block_count; ++bi)
            for (SsaInst *i = f->blocks[bi]->inst_head; i; i = i->next)
                if (i->IrNode == SSA_OP_LOAD_PARAM && i->op1.kind == SSA_OPND_IMM) {
                    int p = (int)i->op1.u.imm;
                    if (p >= 0 && p < f->param_count && facts[fi].known[p] &&
                        !facts[fi].conflict[p]) {
                        i->IrNode = SSA_OP_IMM;
                        i->op1.kind = SSA_OPND_IMM;
                        i->op1.u.imm = facts[fi].value[p];
                        changed = 1;
                    }
                }
        /* Phi destruction creates one definition of a merge VReg on each
         * incoming edge.  Constant specialization may still replace parameter
         * loads in that form, but the single-def SSA optimizer must not run
         * again or its DCE will retain only one predecessor definition. */
        if (changed && vm_has_unique_vreg_defs(f))
            ssa_optimize_function(f);
    }
    return facts;
}

static int vm_read(VmValue *values, SsaOperand op, int64_t *out) {
    if (op.kind == SSA_OPND_IMM) { *out = op.u.imm; return 1; }
    if (op.kind != SSA_OPND_VREG || !values[op.u.vreg].known) return 0;
    *out = values[op.u.vreg].value;
    return 1;
}

static int vm_exec(SsaFunction *func, const int64_t *args, int nargs,
                   uint64_t *hits, int64_t deadline) {
    int var_count = func->var_count;
    for (int bi = 0; bi < func->block_count; ++bi)
        for (SsaInst *inst = func->blocks[bi]->inst_head; inst;
             inst = inst->next) {
            int64_t slot = -1;
            if (inst->IrNode == SSA_OP_LOAD_VAR && inst->op1.kind == SSA_OPND_IMM)
                slot = inst->op1.u.imm;
            else if (inst->IrNode == SSA_OP_STORE_VAR && inst->op2.kind == SSA_OPND_IMM)
                slot = inst->op2.u.imm;
            if (slot >= var_count && slot < INT_MAX) var_count = (int)slot + 1;
        }
    VmValue *values = calloc((size_t)func->next_vreg, sizeof(*values));
    VmValue *vars = var_count > 0
        ? calloc((size_t)var_count, sizeof(*vars)) : NULL;
    if (!values || (var_count > 0 && !vars)) { free(values); free(vars); return 0; }
    SsaBasicBlock *block = func->entry_block, *pred = NULL;
    uint64_t steps = 0;
    int ok = 1;

    while (block && ok) {
        if (block->id < 0 || block->id >= func->block_count) { ok = 0; break; }
        hits[block->id]++;
        SsaBasicBlock *next = NULL;
        for (SsaInst *i = block->inst_head; i && ok; i = i->next) {
            int64_t a = 0, b = 0, r = 0;
            if ((++steps & 255u) == 0 && vm_now_ns() >= deadline) {
                free(values);
                free(vars);
                return 1; /* A timeout is a successful bounded sample. */
            }
            switch (i->IrNode) {
            case SSA_OP_IMM:
                if (i->op1.kind != SSA_OPND_IMM) { ok = 0; break; }
                r = i->op1.u.imm; break;
            case SSA_OP_LOAD_PARAM: {
                int p = (int)i->op1.u.imm;
                if (p < 0 || p >= nargs) { ok = 0; break; }
                r = args[p]; break;
            }
            case SSA_OP_COPY:
                if (!vm_read(values, i->op1, &r)) ok = 0;
                break;
            case SSA_OP_STORE_VAR: {
                int slot = i->op2.kind == SSA_OPND_IMM ? (int)i->op2.u.imm : -1;
                if (slot < 0 || slot >= var_count ||
                    !vm_read(values, i->op1, &r)) { ok = 0; break; }
                vars[slot].value = r;
                vars[slot].known = 1;
                break;
            }
            case SSA_OP_LOAD_VAR: {
                int slot = i->op1.kind == SSA_OPND_IMM ? (int)i->op1.u.imm : -1;
                if (slot < 0 || slot >= var_count || !vars[slot].known) {
                    ok = 0; break;
                }
                r = vars[slot].value;
                break;
            }
            case SSA_OP_PHI: {
                int found = 0;
                for (int k = 0; i->operands && k + 1 < i->operand_count; k += 2) {
                    if (i->operands[k + 1].kind == SSA_OPND_BLOCK &&
                        i->operands[k + 1].u.block == pred) {
                        found = vm_read(values, i->operands[k], &r); break;
                    }
                }
                if (!found) ok = 0;
                break;
            }
            case SSA_OP_NEG:
            case SSA_OP_NOT:
                if (!vm_read(values, i->op1, &a)) { ok = 0; break; }
                r = i->IrNode == SSA_OP_NEG ? (int64_t)(0u - (uint64_t)a) : ~a;
                break;
            case SSA_OP_ADD: case SSA_OP_SUB: case SSA_OP_MUL:
            case SSA_OP_SDIV: case SSA_OP_SREM:
            case SSA_OP_AND: case SSA_OP_OR: case SSA_OP_XOR:
            case SSA_OP_SHL: case SSA_OP_ASHR: case SSA_OP_LSHR:
            case SSA_OP_CMP_EQ: case SSA_OP_CMP_NE: case SSA_OP_CMP_LT:
            case SSA_OP_CMP_LE: case SSA_OP_CMP_GT: case SSA_OP_CMP_GE:
                if (!vm_read(values, i->op1, &a) || !vm_read(values, i->op2, &b)) { ok = 0; break; }
                switch (i->IrNode) {
                case SSA_OP_ADD: r = (int64_t)((uint64_t)a + (uint64_t)b); break;
                case SSA_OP_SUB: r = (int64_t)((uint64_t)a - (uint64_t)b); break;
                case SSA_OP_MUL: r = (int64_t)((uint64_t)a * (uint64_t)b); break;
                case SSA_OP_SDIV:
                    if (!b || (a == INT64_MIN && b == -1)) { ok = 0; break; }
                    r = a / b; break;
                case SSA_OP_SREM:
                    if (!b || (a == INT64_MIN && b == -1)) { ok = 0; break; }
                    r = a % b; break;
                case SSA_OP_AND: r = a & b; break;
                case SSA_OP_OR:  r = a | b; break;
                case SSA_OP_XOR: r = a ^ b; break;
                case SSA_OP_SHL: r = (int64_t)((uint64_t)a << ((unsigned)b & 63)); break;
                case SSA_OP_ASHR:r = a >> ((unsigned)b & 63); break;
                case SSA_OP_LSHR:r = (int64_t)((uint64_t)a >> ((unsigned)b & 63)); break;
                case SSA_OP_CMP_EQ: r = a == b; break;
                case SSA_OP_CMP_NE: r = a != b; break;
                case SSA_OP_CMP_LT: r = a < b; break;
                case SSA_OP_CMP_LE: r = a <= b; break;
                case SSA_OP_CMP_GT: r = a > b; break;
                default:            r = a >= b; break;
                }
                break;
            case SSA_OP_JMP:
                if (i->op1.kind != SSA_OPND_BLOCK) { ok = 0; break; }
                next = i->op1.u.block; break;
            case SSA_OP_BR:
                /* BR keeps its condition in op1.  The two target slots are
                 * described by operand_cap; operand_count intentionally stays
                 * one for the inline condition. */
                if (!vm_read(values, i->op1, &a) || !i->operands || i->operand_cap < 2) {
                    ok = 0; break;
                }
                if (a != 0) i->vm_taken++;
                else i->vm_not_taken++;
                next = i->operands[a != 0 ? 0 : 1].u.block; break;
            case SSA_OP_RET:
                free(values); free(vars); return 1;
            default:
                /* Calls, memory, floating point and ownership operations are
                 * deliberate trial-run circuit breakers. */
                if (getenv("MIRA_VM_DEBUG"))
                    fprintf(stderr, "vm-bail %s block=%d opcode=%d steps=%llu\n",
                        func->name ? func->name : "?", block->id, (int)i->IrNode,
                        (unsigned long long)steps);
                ok = 0; break;
            }
            if (!ok) break;
            if (i->dst > 0) {
                values[i->dst].value = r;
                values[i->dst].known = 1;
            }
            if (next) break;
        }
        /* CFG cleanup may represent an unconditional fallthrough only in the
           successor list.  Treat its sole successor as the next VM block;
           otherwise profiling stops at the entry block and reports 0/0 for
           every real branch. */
        if (ok && !next && block->succ_count == 1)
            next = block->succs[0];
        if (ok && !next) {
            for (int bi = 0; bi + 1 < func->block_count; ++bi)
                if (func->blocks[bi] == block) {
                    next = func->blocks[bi + 1];
                    break;
                }
        }
        pred = block;
        block = next;
    }
    free(values);
    free(vars);
    return ok;
}

static void vm_trace_layout(SsaFunction *func, uint64_t *hits) {
    int n = func->block_count;
    if (n < 3 || !func->entry_block || hits[func->entry_block->id] == 0) return;
    SsaBasicBlock **order = malloc((size_t)n * sizeof(*order));
    unsigned char *used = calloc((size_t)n, 1);
    if (!order || !used) { free(order); free(used); return; }
    SsaBasicBlock *cur = func->entry_block;
    for (int out = 0; out < n; ++out) {
        if (!cur || used[cur->id]) {
            cur = NULL;
            uint64_t best = 0;
            for (int i = 0; i < n; ++i)
                if (!used[func->blocks[i]->id] && (!cur || hits[func->blocks[i]->id] > best)) {
                    cur = func->blocks[i]; best = hits[cur->id];
                }
        }
        order[out] = cur;
        used[cur->id] = 1;
        SsaBasicBlock *best_succ = NULL;
        SsaBasicBlock *first_succ = NULL;
        uint64_t best_hits = 0, second_hits = 0;
        int first_index = n;
        for (int s = 0; s < cur->succ_count; ++s) {
            SsaBasicBlock *cand = cur->succs[s];
            if (used[cand->id]) continue;
            for (int oi = 0; oi < n; ++oi) if (func->blocks[oi] == cand && oi < first_index) {
                first_succ = cand; first_index = oi; break;
            }
            if (!best_succ || hits[cand->id] > best_hits) {
                second_hits = best_hits;
                best_succ = cand; best_hits = hits[cand->id];
            } else if (hits[cand->id] > second_hits) {
                second_hits = hits[cand->id];
            }
        }
        /* A 2 ms sample must not turn noise into layout policy.  Require at
         * least a 60/40 edge before overriding the source/CFG order. */
        if (best_succ && second_hits >= 16 && best_hits * 2 < second_hits * 3)
            cur = first_succ;
        else
            cur = best_succ;
    }
    memcpy(func->blocks, order, (size_t)n * sizeof(*order));
    free(used);
    free(order);
}

void ssa_vm_profile_module(SsaModule *mod) {
    extern int mira_opt_level;
    if (!mod || mira_opt_level < 3) return;
    VmCallFacts *call_facts = vm_specialize_constant_params(mod);
    VmProfile *profiles = calloc((size_t)mod->func_count, sizeof(*profiles));
    if (!profiles) { free(call_facts); return; }
    for (int i = 0; i < mod->func_count; ++i) {
        profiles[i].func = mod->functions[i];
        profiles[i].hits = calloc((size_t)mod->functions[i]->block_count, sizeof(uint64_t));
    }

    for (int ci = 0; ci < mod->func_count; ++ci) {
        SsaFunction *caller = mod->functions[ci];
        SsaInst **defs = vm_build_defs(caller);
        if (!defs) continue;
        for (int bi = 0; bi < caller->block_count; ++bi) {
            for (SsaInst *call = caller->blocks[bi]->inst_head; call; call = call->next) {
                if (call->IrNode != SSA_OP_CALL || !call->operands || call->operand_count < 1 ||
                    call->operands[0].kind != SSA_OPND_SYM) continue;
                SsaFunction *callee = vm_find_func(mod, call->operands[0].u.sym);
                int nargs = call->operand_count - 1;
                if (!callee || callee == caller || nargs != callee->param_count || nargs > SSA_VM_MAX_ARGS) continue;
                int64_t args[SSA_VM_MAX_ARGS];
                int known = 1;
                for (int a = 0; a < nargs; ++a) {
                    SsaOperand op = call->operands[a + 1];
                    if (op.kind == SSA_OPND_IMM) args[a] = op.u.imm;
                    else if (op.kind == SSA_OPND_VREG && vm_const_vreg(defs, op.u.vreg, &args[a], 0)) {}
                    else { known = 0; break; }
                }
                if (!known) continue;
                for (int pi = 0; pi < mod->func_count; ++pi) if (profiles[pi].func == callee) {
                    int64_t deadline = vm_now_ns() + SSA_VM_BUDGET_NS;
                    vm_exec(callee, args, nargs, profiles[pi].hits, deadline);
                    break;
                }
            }
        }
        free(defs);
    }
    /* Constant-parameter specialization can consume the only evidence that
       the later call-driven sampler knows how to read.  Reuse the exact
       all-call-site facts gathered during specialization; never invent
       synthetic arguments for a function whose inputs remain unknown. */
    for (int pi = 0; pi < mod->func_count; ++pi) {
        SsaFunction *func = profiles[pi].func;
        if (getenv("MIRA_VM_DEBUG") && func && func->param_count > 0)
            fprintf(stderr,
                "vm-facts %s calls=%d escaped=%u entry_hits=%llu known0=%u conflict0=%u\n",
                func->name ? func->name : "?",
                call_facts ? call_facts[pi].direct_calls : 0,
                call_facts ? (unsigned)call_facts[pi].escaped : 0,
                (unsigned long long)(func->entry_block ?
                    profiles[pi].hits[func->entry_block->id] : 0),
                call_facts ? (unsigned)call_facts[pi].known[0] : 0,
                call_facts ? (unsigned)call_facts[pi].conflict[0] : 0);
        if (!call_facts || !func || func->param_count <= 0 || !func->entry_block ||
            call_facts[pi].escaped || call_facts[pi].direct_calls <= 0 ||
            profiles[pi].hits[func->entry_block->id] != 0)
            continue;
        int64_t args[SSA_VM_MAX_ARGS] = {0};
        int known = func->param_count <= SSA_VM_MAX_ARGS;
        for (int p = 0; known && p < func->param_count; ++p) {
            if (!call_facts[pi].known[p] || call_facts[pi].conflict[p])
                known = 0;
            else
                args[p] = call_facts[pi].value[p];
        }
        if (!known) continue;
        int64_t deadline = vm_now_ns() + SSA_VM_BUDGET_NS;
        vm_exec(func, args, func->param_count, profiles[pi].hits, deadline);
    }
    for (int i = 0; i < mod->func_count; ++i) {
        if (profiles[i].hits) vm_trace_layout(profiles[i].func, profiles[i].hits);
        SsaFunction *func = profiles[i].func;
        for (int bi = 0; bi < func->block_count; ++bi)
            for (SsaInst *inst = func->blocks[bi]->inst_head; inst; inst = inst->next) {
                if (inst->IrNode != SSA_OP_BR) continue;
                DecisionResult decision;
                DecisionKind kind = decision_choose_branch(inst->vm_taken,
                    inst->vm_not_taken, 1, &decision);
                if (kind == DECISION_BRANCH)
                    inst->branch_policy = SSA_BRANCH_PREFER_JUMP;
                else if (kind == DECISION_BRANCHLESS)
                    inst->branch_policy = SSA_BRANCH_PREFER_BRANCHLESS;
                else
                    inst->branch_policy = SSA_BRANCH_UNKNOWN;
                if (getenv("MIRA_VM_DEBUG"))
                    fprintf(stderr, "vm-branch %s block=%d taken=%llu not=%llu policy=%u decision=%s score=%d rejected=0x%x\n",
                        func->name ? func->name : "?", inst->parent ? inst->parent->id : -1,
                        (unsigned long long)inst->vm_taken,
                        (unsigned long long)inst->vm_not_taken,
                        (unsigned)inst->branch_policy, decision_kind_name(kind),
                        decision.score, decision.rejected);
            }
        free(profiles[i].hits);
    }
    free(call_facts);
    free(profiles);
}
