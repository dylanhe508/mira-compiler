/* Final machine-IR control-flow cleanup. */
#include "ir.h"
#include "target.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define IR_HOT_SPLIT_LOOP_MARK INT64_C(0x4d4952414854)

static int is_direct_branch(IrOpcode op) {
    switch (op) {
    case IR_JMP:
    case IR_JE: case IR_JNE: case IR_JZ: case IR_JNZ:
    case IR_JG: case IR_JGE: case IR_JL: case IR_JLE:
    case IR_JA: case IR_JAE: case IR_JB: case IR_JBE:
    case IR_JS: case IR_JNS:
        return 1;
    default:
        return 0;
    }
}

void ir_opt_remove_fallthrough_branches(IrBuffer *ir) {
    if (!ir || !ir->text || ir->text_count < 2) return;
    int w = 0;
    for (int i = 0; i < ir->text_count; ++i) {
        IrInst *inst = &ir->text[i];
        int remove = 0;
        if (is_direct_branch(inst->IrNode)) {
            /* Several labels may alias the same byte offset.  If the branch
             * target is among the labels immediately following it, taken and
             * not-taken execution are identical, so even a Jcc is dead. */
            for (int j = i + 1; j < ir->text_count; ++j) {
                if (ir->text[j].IrNode != IR_LABEL) break;
                if (ir->text[j].label_id == inst->label_id) {
                    remove = 1;
                    break;
                }
            }
        }
        if (!remove) ir->text[w++] = *inst;
    }
    ir->text_count = w;
}

static int writes_reg(const IrInst *i, IrReg reg) {
    if (i->dst != reg) return 0;
    switch (i->IrNode) {
    case IR_MOV_REG_REG: case IR_MOV_REG_IMM: case IR_MOV_REG_MEM:
    case IR_LEA: case IR_LEA_RIP: case IR_LEA_IDX:
    case IR_ADD_REG_REG: case IR_ADD_REG_IMM:
    case IR_SUB_REG_REG: case IR_SUB_REG_IMM:
    case IR_IMUL_REG_REG: case IR_IMUL_REG_IMM:
    case IR_XOR_REG_REG: case IR_XOR_REG_IMM:
    case IR_AND_REG_REG: case IR_AND_REG_IMM: case IR_OR_REG_REG:
    case IR_SHL_REG_IMM: case IR_SHR_REG_IMM: case IR_SAR_REG_IMM:
    case IR_SHL_REG_CL: case IR_SHR_REG_CL: case IR_SAR_REG_CL:
    case IR_INC_REG: case IR_NEG_REG: case IR_NOT_REG:
    case IR_MOVZX_REG_MEM8: case IR_MOVZX_REG8:
        return 1;
    default:
        return 0;
    }
}

static int reads_reg(const IrInst *i, IrReg reg) {
    if (i->src == reg || i->src2 == reg) return 1;
    switch (i->IrNode) {
    case IR_ADD_REG_REG: case IR_ADD_REG_IMM:
    case IR_SUB_REG_REG: case IR_SUB_REG_IMM:
    case IR_IMUL_REG_REG: case IR_IMUL_REG_IMM:
    case IR_XOR_REG_REG: case IR_XOR_REG_IMM:
    case IR_AND_REG_REG: case IR_AND_REG_IMM: case IR_OR_REG_REG:
    case IR_SHL_REG_IMM: case IR_SHR_REG_IMM: case IR_SAR_REG_IMM:
    case IR_INC_REG: case IR_NEG_REG: case IR_NOT_REG:
    case IR_CMP_REG_REG: case IR_CMP_REG_IMM: case IR_TEST_REG_REG:
        return i->dst == reg;
    case IR_SHL_REG_CL: case IR_SHR_REG_CL: case IR_SAR_REG_CL:
        return i->dst == reg || reg == REG_RCX;
    default:
        return 0;
    }
}

static int is_call(IrOpcode op) {
    return op == IR_CALL_LABEL || op == IR_CALL_EXTERN || op == IR_CALL_REG;
}

static int inst_mentions_reg(const IrInst *inst, IrReg reg) {
    return inst->dst == reg || inst->src == reg || inst->src2 == reg ||
        (inst->IrNode == IR_LEA_IDX && (IrReg)inst->imm == reg);
}

static int reg_mentioned_outside(const IrBuffer *ir, int begin, int end, IrReg reg) {
    int fs = begin, fe = end;
    while (fs > 0 && ir->text[fs].IrNode != IR_LABEL_NAMED) --fs;
    while (fe < ir->text_count && ir->text[fe].IrNode != IR_LABEL_NAMED) ++fe;
    for (int q = fs; q < fe; ++q)
        if ((q < begin || q >= end) && inst_mentions_reg(&ir->text[q], reg)) return 1;
    return 0;
}

static int ensure_ir_capacity(IrBuffer *ir, int needed) {
    if (needed <= ir->text_cap) return 1;
    if (needed < 0 || (size_t)needed > SIZE_MAX / sizeof(*ir->text)) return 0;
    int cap = ir->text_cap > 0 ? ir->text_cap : 64;
    while (cap < needed) {
        if (cap > INT_MAX / 2) { cap = needed; break; }
        cap *= 2;
    }
    if ((size_t)cap > SIZE_MAX / sizeof(*ir->text)) return 0;
    IrInst *grown = realloc(ir->text, (size_t)cap * sizeof(*ir->text));
    if (!grown) return 0;
    ir->text = grown;
    ir->text_cap = cap;
    return 1;
}

/* Redirect branches through label-only JMP blocks.  This is deliberately a
 * final machine-IR pass: earlier CFG transforms may create fresh merge
 * trampolines even when SSA itself was already simplified. */
void ir_opt_thread_jump_chains(IrBuffer *ir) {
    if (!ir || !ir->text || ir->text_count < 3) return;
    int *labels = malloc((size_t)ir->text_count * sizeof(*labels));
    int *targets = malloc((size_t)ir->text_count * sizeof(*targets));
    if (!labels || !targets) {
        free(labels);
        free(targets);
        return;
    }
    int chain_count = 0;
    for (int i = 0; i < ir->text_count; ++i) {
        if (ir->text[i].IrNode != IR_LABEL) continue;
        int next = i + 1;
        while (next < ir->text_count && ir->text[next].IrNode == IR_LABEL)
            ++next;
        if (next < ir->text_count && ir->text[next].IrNode == IR_JMP &&
            ir->text[next].label_id != ir->text[i].label_id) {
            labels[chain_count] = ir->text[i].label_id;
            targets[chain_count] = ir->text[next].label_id;
            ++chain_count;
        }
    }
    for (int i = 0; i < ir->text_count; ++i) {
        if (!is_direct_branch(ir->text[i].IrNode)) continue;
        int target = ir->text[i].label_id;
        for (int depth = 0; depth < chain_count; ++depth) {
            int found = -1;
            for (int c = 0; c < chain_count; ++c)
                if (labels[c] == target) { found = c; break; }
            if (found < 0 || targets[found] == target) break;
            target = targets[found];
        }
        ir->text[i].label_id = target;
    }
    free(labels);
    free(targets);
}

/* Coalesce a loop-carried affine recurrence after SSA lowering.  The SSA
 * values are distinct, but x86 two-address arithmetic can keep all versions
 * in the loop's state register:
 *
 *   mov t0,state; imul t0,k; mov t1,t0; add t1,c; mov state,t1
 *       => imul state,k; add state,c
 *
 * Exact adjacency makes the def/use proof local and complete. */
void ir_opt_coalesce_affine_recurrences(IrBuffer *ir) {
    if (!ir || !ir->text || ir->text_count < 6) return;
    int w = 0;
    for (int i = 0; i < ir->text_count;) {
		/* A recurrence commonly feeds an accumulator before the latch:
		 *
		 *   mov t0,state; imul t0,k; mov t1,t0; add t1,c; mov state,t1
		 *   add accumulator,t1; update-counter; jcc header
		 *
		 * Keep the recurrence in state and redirect the accumulator use.
		 * Requiring the complete canonical latch makes the rewrite local:
		 * neither temporary is live past the back edge. */
		if (i + 7 < ir->text_count) {
			IrInst *a = &ir->text[i], *b = &ir->text[i + 1];
			IrInst *c = &ir->text[i + 2], *d = &ir->text[i + 3];
			IrInst *e = &ir->text[i + 4], *f = &ir->text[i + 5];
			IrInst *g = &ir->text[i + 6], *h = &ir->text[i + 7];
			IrReg state = a->src, t0 = a->dst, t1 = c->dst, factor = b->src;
			int latch_update =
				g->IrNode == IR_SUB_REG_IMM || g->IrNode == IR_ADD_REG_IMM ||
				g->IrNode == IR_INC_REG;
			if (a->IrNode == IR_MOV_REG_REG && state != t0 &&
			    b->IrNode == IR_IMUL_REG_REG && b->dst == t0 &&
			    c->IrNode == IR_MOV_REG_REG && c->src == t0 &&
			    d->IrNode == IR_ADD_REG_IMM && d->dst == t1 &&
			    e->IrNode == IR_MOV_REG_REG && e->src == t1 && e->dst == state &&
			    f->IrNode == IR_ADD_REG_REG && f->src == t1 &&
			    f->dst != state && f->dst != t0 && f->dst != t1 &&
			    latch_update && is_direct_branch(h->IrNode) &&
			    h->IrNode != IR_JMP && state != t1 && t0 != t1 &&
			    factor != state && factor != t0 && factor != t1) {
				IrInst mul = *b, add = *d, accumulate = *f;
				mul.dst = state;
				add.dst = state;
				accumulate.src = state;
				ir->text[w++] = mul;
				ir->text[w++] = add;
				ir->text[w++] = accumulate;
				ir->text[w++] = *g;
				ir->text[w++] = *h;
				i += 8;
				continue;
			}
		}
		if (i + 3 < ir->text_count) {
			IrInst *a = &ir->text[i], *b = &ir->text[i + 1];
			IrInst *c = &ir->text[i + 2], *d = &ir->text[i + 3];
			IrReg temporary = a->dst, state = a->src;
			if (a->IrNode == IR_MOV_REG_REG && temporary != state &&
			    (b->IrNode == IR_ADD_REG_IMM || b->IrNode == IR_SUB_REG_IMM) &&
			    b->dst == temporary &&
			    c->IrNode == IR_MOV_REG_REG && c->dst == state && c->src == temporary &&
			    (d->IrNode == IR_CMP_REG_IMM || d->IrNode == IR_TEST_REG_REG) &&
			    d->dst == temporary &&
			    (d->IrNode != IR_TEST_REG_REG || d->src == temporary)) {
				int observed_elsewhere = reg_mentioned_outside(ir, i, i + 4, temporary);
				if (!observed_elsewhere) {
					IrInst update = *b, compare = *d;
					update.dst = state;
					compare.dst = state;
					if (compare.IrNode == IR_TEST_REG_REG) compare.src = state;
					ir->text[w++] = update;
					ir->text[w++] = compare;
					i += 4;
					continue;
				}
			}
		}
        if (i + 5 < ir->text_count) {
            IrInst *a = &ir->text[i], *b = &ir->text[i + 1];
            IrInst *c = &ir->text[i + 2], *d = &ir->text[i + 3];
            IrInst *e = &ir->text[i + 4];
            IrInst *f = &ir->text[i + 5];
            IrReg state = a->src, t0 = a->dst, t1 = c->dst, factor = b->src;
            if (a->IrNode == IR_MOV_REG_REG &&
                b->IrNode == IR_IMUL_REG_REG && b->dst == t0 &&
                c->IrNode == IR_MOV_REG_REG && c->src == t0 &&
                d->IrNode == IR_ADD_REG_IMM && d->dst == t1 &&
                e->IrNode == IR_MOV_REG_REG && e->src == t1 && e->dst == state &&
                (f->IrNode == IR_CMP_REG_IMM || f->IrNode == IR_TEST_REG_REG) &&
                f->dst == t1 && (f->IrNode != IR_TEST_REG_REG || f->src == t1) &&
                state != t0 && state != t1 && t0 != t1 &&
                factor != state && factor != t0 && factor != t1) {
				if (reg_mentioned_outside(ir, i, i + 6, t0) ||
				    reg_mentioned_outside(ir, i, i + 6, t1)) {
					ir->text[w++] = ir->text[i++];
					continue;
				}
                IrInst mul = *b, add = *d, test = *f;
                mul.dst = state;
                add.dst = state;
                test.dst = state;
                if (test.IrNode == IR_TEST_REG_REG) test.src = state;
                ir->text[w++] = mul;
                ir->text[w++] = add;
                ir->text[w++] = test;
                i += 6;
                continue;
            }
        }
        ir->text[w++] = ir->text[i++];
    }
    ir->text_count = w;
}

uint64_t ir_opt_hoist_loop_scratch_constants_counted(IrBuffer *ir) {
    uint64_t probes = 0;
    if (!ir || !ir->text) return 0;
    for (int function_start = 0; function_start < ir->text_count; ) {
        int function_end = function_start + 1;
        while (function_end < ir->text_count &&
               ir->text[function_end].IrNode != IR_LABEL_NAMED)
            function_end++;
        for (int back = function_start + 1; back < function_end; ++back) {
        if (!is_direct_branch(ir->text[back].IrNode)) continue;
        int label = -1;
        for (int i = function_start; i < back; ++i) {
            probes++;
            if (ir->text[i].IrNode == IR_LABEL &&
                ir->text[i].label_id == ir->text[back].label_id) label = i;
        }
        if (label < 1) continue;

		/* Admit both canonical test-at-header loops and rotated loops whose
		 * pre-test immediately precedes the body label.  In either form the
		 * exit must be forward of the latch. */
		int exit_label = -1;
		if (label + 2 < back &&
		    (ir->text[label + 1].IrNode == IR_CMP_REG_IMM ||
		     ir->text[label + 1].IrNode == IR_CMP_REG_REG) &&
		    is_direct_branch(ir->text[label + 2].IrNode) &&
		    ir->text[label + 2].IrNode != IR_JMP) {
			exit_label = ir->text[label + 2].label_id;
		} else if (label >= 2 &&
		           (ir->text[label - 2].IrNode == IR_CMP_REG_IMM ||
		            ir->text[label - 2].IrNode == IR_CMP_REG_REG) &&
		           is_direct_branch(ir->text[label - 1].IrNode) &&
		           ir->text[label - 1].IrNode != IR_JMP) {
			exit_label = ir->text[label - 1].label_id;
		} else {
			continue;
		}
		int exit_after_backedge = 0;
		for (int i = back + 1; i < function_end; ++i) {
			probes++;
			if (ir->text[i].IrNode == IR_LABEL_NAMED || ir->text[i].IrNode == IR_RET) break;
			if (ir->text[i].IrNode == IR_LABEL &&
			    ir->text[i].label_id == exit_label) {
				exit_after_backedge = 1;
				break;
			}
		}
		if (!exit_after_backedge) continue;

        /* The loop header must have no non-backedge entry.  Fallthrough is
         * safe because the hoisted instruction remains on that path. */
        int external_entry = 0;
        for (int i = function_start; i < function_end; ++i) {
            probes++;
            if (i >= label && i <= back) continue;
            if (is_direct_branch(ir->text[i].IrNode) &&
                ir->text[i].label_id == ir->text[label].label_id) {
                external_entry = 1; break;
            }
        }
        if (external_entry) continue;

        for (int cand = label + 1; cand < back; ++cand) {
            IrInst *m = &ir->text[cand];
            if (m->IrNode != IR_MOV_REG_IMM || m->dst == REG_RSP ||
                m->dst == REG_RBP || m->dst > REG_R15) continue;
            int writes = 0, read_before = 0, unsafe = 0;
            for (int i = label + 1; i <= back; ++i) {
                if (is_call(ir->text[i].IrNode)) { unsafe = 1; break; }
                if (i < cand && reads_reg(&ir->text[i], m->dst)) read_before = 1;
                if (writes_reg(&ir->text[i], m->dst)) writes++;
            }
            if (unsafe || read_before || writes != 1) continue;
            IrInst saved = *m;
            for (int i = cand; i > label; --i) ir->text[i] = ir->text[i - 1];
            ir->text[label] = saved; /* execute on entry, skip on backedge */
            break;
        }
        }
        function_start = function_end;
    }
    return probes;
}

void ir_opt_hoist_loop_scratch_constants(IrBuffer *ir) {
    (void)ir_opt_hoist_loop_scratch_constants_counted(ir);
}

/* Align every label that is the target of a backward edge.  This is a CFG
 * property, not a source-pattern heuristic: forward-only join blocks and cold
 * exits are left untouched.  The existing encoder emits the required NOPs. */
void ir_opt_align_loop_headers(IrBuffer *ir) {
    if (!ir || !ir->text || ir->text_count < 3 || ir->text_count > 4096) return;
    unsigned char *align = calloc((size_t)ir->text_count, 1);
    int *branch_target_pos = malloc((size_t)ir->text_count * sizeof(*branch_target_pos));
    if (!align || !branch_target_pos) { free(align); free(branch_target_pos); return; }
    for (int i = 0; i < ir->text_count; ++i) branch_target_pos[i] = -1;
    /* Resolve every direct edge once.  Candidate validation below then scans
     * only instruction positions, keeping the complete pass O(n^2). */
    for (int edge = 0; edge < ir->text_count; ++edge) {
        if (!is_direct_branch(ir->text[edge].IrNode)) continue;
        for (int label = 0; label < ir->text_count; ++label)
            if (ir->text[label].IrNode == IR_LABEL &&
                ir->text[label].label_id == ir->text[edge].label_id) {
                branch_target_pos[edge] = label;
                break;
            }
    }
    int additions = 0;
    for (int label = 0; label < ir->text_count; ++label) {
        if (ir->text[label].IrNode != IR_LABEL) continue;
		if (label + 2 >= ir->text_count ||
		    (ir->text[label + 1].IrNode != IR_CMP_REG_IMM &&
		     ir->text[label + 1].IrNode != IR_CMP_REG_REG &&
		     ir->text[label + 1].IrNode != IR_TEST_REG_REG) ||
		    !is_direct_branch(ir->text[label + 2].IrNode) ||
		    ir->text[label + 2].IrNode == IR_JMP)
			continue;
        for (int q = label + 3; q < ir->text_count; ++q) {
            if (ir->text[q].IrNode == IR_LABEL_NAMED) break;
            if (is_direct_branch(ir->text[q].IrNode) && ir->text[q].IrNode != IR_JMP &&
                q > 0 && (ir->text[q - 1].IrNode == IR_CMP_REG_IMM ||
                          ir->text[q - 1].IrNode == IR_CMP_REG_REG ||
                          ir->text[q - 1].IrNode == IR_TEST_REG_REG) &&
                ir->text[q].label_id == ir->text[label].label_id) {
                int exit_after_backedge = branch_target_pos[label + 2] > q;
                int outside_entry = 0;
                for (int r = 0; r < ir->text_count && !outside_entry; ++r) {
                    if (r >= label && r <= q) continue;
                    if (branch_target_pos[r] > label && branch_target_pos[r] <= q)
                        outside_entry = 1;
                }
                if (!exit_after_backedge || outside_entry) continue;
                if (label == 0 || ir->text[label - 1].IrNode != IR_ALIGN32) {
                    align[label] = 1;
                    additions++;
                }
                break;
            }
        }
    }
    /* Rotated loops jump directly to a body label; their compare lives at the
     * latch, so the canonical-header recognizer above cannot see them.  A
     * direct backward edge is already sufficient CFG proof.  Bound the body
     * size to avoid padding tiny retry paths or very large cold regions. */
    for (int edge = 0; edge < ir->text_count; ++edge) {
        int label = branch_target_pos[edge];
        if (!is_direct_branch(ir->text[edge].IrNode) ||
            ir->text[edge].IrNode == IR_JMP ||
            label < 0 || label >= edge)
            continue;
        int body_size = edge - label;
        if (body_size < 8 || body_size > 256) continue;
        if (!align[label] &&
            (label == 0 || ir->text[label - 1].IrNode != IR_ALIGN32)) {
            align[label] = 1;
            additions++;
        }
    }
    free(branch_target_pos);
    if (!additions) { free(align); return; }
    IrInst *out = calloc((size_t)(ir->text_count + additions), sizeof(*out));
    if (!out) { free(align); return; }
    int w = 0;
    for (int i = 0; i < ir->text_count; ++i) {
        if (align[i]) out[w++].IrNode = IR_ALIGN32;
        out[w++] = ir->text[i];
    }
    free(align);
    free(ir->text);
    ir->text = out;
    ir->text_count = w;
    ir->text_cap = w;
}

/* Spill slots are private negative RBP offsets.  If a slot is never loaded or
 * has its address formed anywhere in the same function, stores to it cannot be
 * observed.  This also removes stale writebacks emitted for code-free SSA
 * nodes such as coalesced PHIs. */
void ir_opt_remove_dead_stack_stores(IrBuffer *ir) {
    if (!ir || !ir->text || ir->text_count < 2) return;
    for (int fs = 0; fs < ir->text_count; ) {
        while (fs < ir->text_count && ir->text[fs].IrNode != IR_LABEL_NAMED) fs++;
        if (fs >= ir->text_count) break;
        int fe = fs + 1;
        while (fe < ir->text_count && ir->text[fe].IrNode != IR_LABEL_NAMED) fe++;
        for (int i = fs + 1; i < fe; ++i) {
            IrInst *s = &ir->text[i];
            if ((s->IrNode != IR_MOV_MEM_REG && s->IrNode != IR_MOV_MEM_IMM) ||
                s->dst != REG_RBP || s->imm >= 0) continue;
            int observed = 0;
            for (int q = fs + 1; q < fe; ++q) {
                IrInst *x = &ir->text[q];
                if ((x->IrNode == IR_MOV_REG_MEM || x->IrNode == IR_MOVZX_REG_MEM8) &&
                    x->src == REG_RBP && x->imm == s->imm) { observed = 1; break; }
                if (x->IrNode == IR_LEA && x->src == REG_RBP && x->imm == s->imm) {
                    observed = 1; break;
                }
            }
            if (!observed) {
                memmove(s, s + 1, (size_t)(ir->text_count - i - 1) * sizeof(*s));
                ir->text_count--; fe--; i--;
            }
        }
        fs = fe;
    }
}

/* Reuse the sign flag when a value-producing instruction is immediately
 * followed by `cmp value, 0`.  JS/JNS test SF directly, which is exactly the
 * signed x < 0 / x >= 0 predicate even when the producer overflowed. */
void ir_opt_forward_sign_flags(IrBuffer *ir) {
    if (!ir || !ir->text || ir->text_count < 3) return;
    for (int i = 0; i + 2 < ir->text_count; ++i) {
        IrInst *producer = &ir->text[i];
        IrInst *cmp = &ir->text[i + 1];
        IrInst *branch = &ir->text[i + 2];
        int defines_sign = producer->IrNode == IR_ADD_REG_REG ||
            producer->IrNode == IR_ADD_REG_IMM ||
            producer->IrNode == IR_SUB_REG_REG ||
            producer->IrNode == IR_SUB_REG_IMM ||
            producer->IrNode == IR_XOR_REG_REG ||
            producer->IrNode == IR_XOR_REG_IMM ||
            producer->IrNode == IR_AND_REG_REG ||
            producer->IrNode == IR_OR_REG_REG ||
            producer->IrNode == IR_NEG_REG;
        if (!defines_sign || cmp->IrNode != IR_CMP_REG_IMM || cmp->imm != 0 ||
            cmp->dst != producer->dst) continue;
        if (branch->IrNode == IR_JGE) branch->IrNode = IR_JNS;
        else if (branch->IrNode == IR_JL) branch->IrNode = IR_JS;
        else continue;
        memmove(cmp, branch, (size_t)(ir->text_count - i - 2) * sizeof(*cmp));
        ir->text_count--;
    }
}

static IrReg unused_scratch_gpr(const IrBuffer *ir) {
    static const IrReg candidates[] = { REG_R11, REG_R10, REG_R9, REG_R8,
        REG_RDX, REG_RCX, REG_RAX, REG_RBX, REG_RDI, REG_RSI, REG_R12 };
    for (size_t ci = 0; ci < sizeof(candidates) / sizeof(candidates[0]); ++ci) {
        IrReg r = candidates[ci];
        int used = 0;
        for (int i = 0; i < ir->text_count; ++i) {
            IrInst *x = &ir->text[i];
            if (x->dst == r || x->src == r || x->src2 == r ||
                (x->IrNode == IR_LEA_IDX && (IrReg)x->imm == r)) { used = 1; break; }
        }
        if (!used) return r;
    }
    return REG_NONE;
}

static IrReg unused_scratch_for_inst(const IrBuffer *ir, int at) {
    static const IrReg candidates[] = { REG_R11, REG_R10, REG_R9, REG_R8,
        REG_RDX, REG_RCX, REG_RAX, REG_RBX, REG_RDI, REG_RSI, REG_R12 };
    int lo = at, hi = ir->text_count;
    while (lo > 0 && ir->text[lo].IrNode != IR_LABEL_NAMED) --lo;
    for (int i = at + 1; i < ir->text_count; ++i)
        if (ir->text[i].IrNode == IR_LABEL_NAMED) { hi = i; break; }
    for (size_t ci = 0; ci < sizeof(candidates) / sizeof(candidates[0]); ++ci) {
        IrReg r = candidates[ci]; int used = 0;
        for (int i = lo; i < hi; ++i) {
            IrInst *x = &ir->text[i];
            if (x->dst == r || x->src == r || x->src2 == r ||
                (x->IrNode == IR_LEA_IDX && (IrReg)x->imm == r)) { used = 1; break; }
        }
        if (!used) return r;
    }
    return REG_NONE;
}

static IrOpcode jcc_to_cmov(IrOpcode op) {
    switch (op) {
    case IR_JE: case IR_JZ: return IR_CMOVE;
    case IR_JNE: case IR_JNZ: return IR_CMOVNE;
    case IR_JL: return IR_CMOVL; case IR_JLE: return IR_CMOVLE;
    case IR_JG: return IR_CMOVG; case IR_JGE: return IR_CMOVGE;
    case IR_JA: return IR_CMOVA; case IR_JAE: return IR_CMOVAE;
    case IR_JB: return IR_CMOVB; case IR_JBE: return IR_CMOVBE;
    default: return IR_OPCODE_COUNT;
    }
}

static int is_pure_two_address_update(const IrInst *i) {
    switch (i->IrNode) {
    case IR_MOV_REG_REG: case IR_MOV_REG_IMM:
    case IR_ADD_REG_REG: case IR_SUB_REG_REG:
    case IR_ADD_REG_IMM: case IR_SUB_REG_IMM:
        return i->dst >= REG_RAX && i->dst <= REG_R15 &&
            (i->IrNode == IR_MOV_REG_IMM || i->IrNode == IR_ADD_REG_IMM ||
             i->IrNode == IR_SUB_REG_IMM ||
             (i->src >= REG_RAX && i->src <= REG_R15));
    default:
        return 0;
    }
}

static int label_has_external_ref(const IrBuffer *ir, int label, int lo, int hi) {
    for (int i = 0; i < ir->text_count; ++i) {
        if (i >= lo && i <= hi) continue;
        if (is_direct_branch(ir->text[i].IrNode) && ir->text[i].label_id == label) return 1;
    }
    return 0;
}

/* Convert side-effect-free one-operation diamonds after SSA lowering.  The
 * proof remains structural and general: both arms update the same register
 * from the same source, no external edge enters either arm, and a register
 * unused by the complete function is available for the alternate value. */
void ir_opt_if_convert_small_diamonds(IrBuffer *ir) {
    if (!ir || !ir->text || ir->text_count < 9) return;
    /* Hot fallthrough with an outlined cold arm:
     *   cmp; jcc cold; hot-op; merge: ...; cold: cold-op; jmp merge
     * This is the layout produced for loop diamonds after block placement. */
    for (int i = 0; i + 3 < ir->text_count; ++i) {
		IrReg scratch = unused_scratch_for_inst(ir, i);
		if (scratch == REG_NONE) continue;
        IrInst cmp = ir->text[i], cond = ir->text[i + 1];
        IrOpcode cmov = jcc_to_cmov(cond.IrNode);
		if (cond.branch_policy == IR_BRANCH_PREFER_JUMP) continue;
		int hot_i = i + 2;
		while (hot_i < ir->text_count && ir->text[hot_i].IrNode == IR_LABEL) hot_i++;
		if (hot_i >= ir->text_count) continue;
		IrInst hot = ir->text[hot_i];
		int common_start = hot_i + 1;
        if ((cmp.IrNode != IR_CMP_REG_REG && cmp.IrNode != IR_CMP_REG_IMM &&
             cmp.IrNode != IR_TEST_REG_REG) || cmov == IR_OPCODE_COUNT ||
            common_start >= ir->text_count || ir->text[common_start].IrNode != IR_LABEL ||
			hot.dst > REG_R15)
            continue;
		int common_end = common_start;
		while (common_end < ir->text_count && ir->text[common_end].IrNode == IR_LABEL) common_end++;
        int cold_i = -1;
        for (int q = i + 4; q + 2 < ir->text_count; ++q) {
            if (ir->text[q].IrNode == IR_LABEL_NAMED) break;
            if (ir->text[q].IrNode == IR_LABEL && ir->text[q].label_id == cond.label_id) {
                cold_i = q; break;
            }
        }
        if (cold_i < 0) continue;
        IrInst cold = ir->text[cold_i + 1], back = ir->text[cold_i + 2];
        int arithmetic = is_pure_two_address_update(&hot) &&
            is_pure_two_address_update(&cold);
        int targets_common = 0;
		for (int q = common_start; q < common_end; ++q)
			if (ir->text[q].label_id == back.label_id) targets_common = 1;
        if (!arithmetic || hot.dst != cold.dst ||
            back.IrNode != IR_JMP || !targets_common ||
            label_has_external_ref(ir, cond.label_id, i, cold_i + 2)) continue;

        int delta = 5 - (common_start - i);
        if (delta > 0 && ir->text_count + delta > ir->text_cap) {
            ir->text_cap = ir->text_cap ? ir->text_cap * 2 : 64;
            if (ir->text_cap < ir->text_count + delta) ir->text_cap = ir->text_count + delta;
            ir->text = realloc(ir->text, (size_t)ir->text_cap * sizeof(*ir->text));
        }
        memmove(&ir->text[i + 5], &ir->text[common_start],
                (size_t)(ir->text_count - common_start) * sizeof(*ir->text));
        IrInst *t = ir->text;
        memset(&t[i], 0, 5 * sizeof(*t));
        t[i].IrNode = IR_MOV_REG_REG; t[i].dst = scratch; t[i].src = hot.dst;
        t[i + 1] = cold; t[i + 1].dst = scratch;
        t[i + 2] = hot;
        t[i + 3] = cmp;
        t[i + 4].IrNode = cmov; t[i + 4].dst = hot.dst; t[i + 4].src = scratch;
		ir->text_count += delta;
		cold_i += delta;
        memmove(&t[cold_i], &t[cold_i + 3],
                (size_t)(ir->text_count - cold_i - 3) * sizeof(*t));
        ir->text_count -= 3;
    }
    for (int i = 0; i + 8 < ir->text_count; ++i) {
		IrReg scratch = unused_scratch_for_inst(ir, i);
		if (scratch == REG_NONE) continue;
        IrInst cmp = ir->text[i], cond = ir->text[i + 1], to_false = ir->text[i + 2];
        IrInst lt = ir->text[i + 3], true_op = ir->text[i + 4], to_merge = ir->text[i + 5];
        IrInst lf = ir->text[i + 6], false_op = ir->text[i + 7], lm = ir->text[i + 8];
        IrOpcode cmov = jcc_to_cmov(cond.IrNode);
        if (cond.branch_policy == IR_BRANCH_PREFER_JUMP) continue;
        if ((cmp.IrNode != IR_CMP_REG_REG && cmp.IrNode != IR_CMP_REG_IMM &&
             cmp.IrNode != IR_TEST_REG_REG) || cmov == IR_OPCODE_COUNT ||
            to_false.IrNode != IR_JMP || lt.IrNode != IR_LABEL ||
            to_merge.IrNode != IR_JMP || lf.IrNode != IR_LABEL || lm.IrNode != IR_LABEL ||
            cond.label_id != lt.label_id || to_false.label_id != lf.label_id ||
            to_merge.label_id != lm.label_id || true_op.dst != false_op.dst ||
            true_op.dst > REG_R15)
            continue;
        int arithmetic = is_pure_two_address_update(&true_op) &&
            is_pure_two_address_update(&false_op);
        if (!arithmetic || label_has_external_ref(ir, lt.label_id, i, i + 8) ||
            label_has_external_ref(ir, lf.label_id, i, i + 8)) continue;

        IrInst repl[6] = {0};
        repl[0].IrNode = IR_MOV_REG_REG; repl[0].dst = scratch; repl[0].src = true_op.dst;
        repl[1] = true_op; repl[1].dst = scratch;
        repl[2] = false_op;
        repl[3] = cmp;
        repl[4].IrNode = cmov; repl[4].dst = true_op.dst; repl[4].src = scratch;
        repl[5] = lm;
        memcpy(&ir->text[i], repl, sizeof(repl));
        memmove(&ir->text[i + 6], &ir->text[i + 9],
                (size_t)(ir->text_count - i - 9) * sizeof(*ir->text));
        ir->text_count -= 3;
    }
}

/* Remove a scalar-FP value's needless round trip through a GPR when the
 * immediately following FP operation reloads the same bits.  Intervening
 * invariant loads are allowed, but neither endpoint may be observed. */
void ir_opt_scalar_fp_chains(IrBuffer *ir) {
    if (!ir || !ir->text || ir->text_count < 4) return;
    unsigned char *drop = calloc((size_t)ir->text_count, 1);
    for (int i = 0; i + 1 < ir->text_count; ++i) {
        IrInst *out = &ir->text[i];
        if (out->IrNode != IR_MOVQ_REG_XMM) continue;
        for (int j = i + 1; j < ir->text_count && j <= i + 4; ++j) {
            IrInst *in = &ir->text[j];
            if (in->IrNode == IR_LABEL || is_direct_branch(in->IrNode) || is_call(in->IrNode)) break;
            if (in->IrNode == IR_MOVQ_XMM_REG && in->dst == out->src && in->src == out->dst) {
                int safe = 1;
                for (int k = i + 1; k < j; ++k)
                    if (reads_reg(&ir->text[k], out->dst) || writes_reg(&ir->text[k], out->dst) ||
                        writes_reg(&ir->text[k], out->src)) safe = 0;
                if (safe) { drop[i] = drop[j] = 1; }
                break;
            }
            if (reads_reg(in, out->dst) || writes_reg(in, out->dst) || writes_reg(in, out->src)) break;
        }
    }
    int w = 0;
    for (int i = 0; i < ir->text_count; ++i) if (!drop[i]) ir->text[w++] = ir->text[i];
    ir->text_count = w;
    free(drop);

    if (!mira_target_features.fma3) return;
    /* After round-trip removal, fuse:
     *   mulsd x,m; movq a,g; addsd x,a
     * into a correctly rounded scalar FMA using XMM2 as the addend scratch. */
    for (int i = 0; i + 2 < ir->text_count; ++i) {
        IrInst original = ir->text[i];
        if (original.IrNode != IR_MULSD || original.dst == REG_XMM2 || original.src == REG_XMM2) continue;
        int load_i = i + 1;
        if (ir->text[load_i].IrNode == IR_MOV_REG_IMM) load_i++;
        if (load_i + 1 >= ir->text_count) continue;
        IrInst load = ir->text[load_i], add = ir->text[load_i + 1];
        if (load.IrNode != IR_MOVQ_XMM_REG || add.IrNode != IR_ADDSD ||
            add.dst != original.dst || add.src != load.dst) continue;
        load.dst = REG_XMM2;
        original.IrNode = IR_VFMADD132SD;
        original.src2 = original.src;
        original.src = REG_XMM2;
        if (load_i == i + 2) {
            IrInst invariant = ir->text[i + 1];
            ir->text[i] = invariant; ir->text[i + 1] = load; ir->text[i + 2] = original;
        } else {
            ir->text[i] = load; ir->text[i + 1] = original;
        }
        memmove(&ir->text[load_i + 1], &ir->text[load_i + 2],
                (size_t)(ir->text_count - load_i - 2) * sizeof(*ir->text));
        ir->text_count--;
    }
}

/* Keep a loop-carried scalar double in XMM across a conditional backedge.
 * The proof is deliberately structural: exactly one GPR->XMM load enters the
 * loop, exactly one matching XMM->GPR commit leaves an iteration, and the GPR
 * is otherwise untouched.  The commit is moved to the not-taken exit path. */
void ir_opt_promote_scalar_fp_loop_state(IrBuffer *ir) {
    if (!ir || !ir->text) return;
    for (int back = 2; back < ir->text_count; ++back) {
        IrOpcode bop = ir->text[back].IrNode;
        if (bop != IR_JNE && bop != IR_JNZ && bop != IR_JG && bop != IR_JGE &&
            bop != IR_JL && bop != IR_JLE && bop != IR_JA && bop != IR_JAE &&
            bop != IR_JB && bop != IR_JBE) continue;
        int label = -1;
        for (int i = back - 1; i >= 0; --i)
            if (ir->text[i].IrNode == IR_LABEL && ir->text[i].label_id == ir->text[back].label_id) {
                label = i; break;
            }
        if (label < 0 || label + 2 >= back) continue;
		/* This transform has one exit/one backedge semantics.  A diamond or
		 * nested branch inside the candidate range can bypass the selected
		 * commit, so it is not a scalar recurrence loop. */
		int complex_cfg = 0;
		for (int i = label + 1; i < back; ++i) {
			if (ir->text[i].IrNode == IR_LABEL || is_direct_branch(ir->text[i].IrNode) ||
			    is_call(ir->text[i].IrNode)) { complex_cfg = 1; break; }
		}
		if (complex_cfg) continue;
        int load_i = -1, store_i = -1;
        IrReg state = REG_NONE, xmm = REG_NONE;
        for (int i = label + 1; i < back; ++i) {
            IrInst *x = &ir->text[i];
            if (load_i < 0 && x->IrNode == IR_MOVQ_XMM_REG) {
                load_i = i; state = x->src; xmm = x->dst; continue;
            }
            if (load_i >= 0 && x->IrNode == IR_MOVQ_REG_XMM && x->dst == state && x->src == xmm)
                store_i = i;
        }
        if (load_i < 0 || store_i < 0 || state > REG_R15) continue;
        int state_pair_count = 0;
        for (int i = label + 1; i < back; ++i) {
            IrInst *load_candidate = &ir->text[i];
            if (load_candidate->IrNode != IR_MOVQ_XMM_REG || load_candidate->src > REG_R15)
                continue;
            for (int j = i + 1; j < back; ++j) {
                IrInst *store_candidate = &ir->text[j];
                if (store_candidate->IrNode == IR_MOVQ_REG_XMM &&
                    store_candidate->dst == load_candidate->src &&
                    store_candidate->src == load_candidate->dst) {
                    state_pair_count++;
                    break;
                }
            }
        }
        if (state_pair_count != 1) continue;
        int appearances = 0;
        for (int i = label + 1; i < back; ++i) {
            IrInst *x = &ir->text[i];
            if (x->dst == state || x->src == state || x->src2 == state) appearances++;
        }
        if (appearances != 2) continue;

        IrInst load = ir->text[load_i], store = ir->text[store_i];
        for (int i = load_i; i > label; --i) ir->text[i] = ir->text[i - 1];
        ir->text[label] = load;
        memmove(&ir->text[store_i], &ir->text[store_i + 1],
                (size_t)(ir->text_count - store_i - 1) * sizeof(*ir->text));
        ir->text_count--; back--;
        if (ir->text_count + 1 > ir->text_cap) {
            ir->text_cap = ir->text_cap ? ir->text_cap * 2 : 64;
            ir->text = realloc(ir->text, (size_t)ir->text_cap * sizeof(*ir->text));
        }
        memmove(&ir->text[back + 2], &ir->text[back + 1],
                (size_t)(ir->text_count - back - 1) * sizeof(*ir->text));
        ir->text[back + 1] = store;
        ir->text_count++;
        back++;
    }
}

/* Hoist bit-exact scalar double constants from GPRs into XMM once per loop.
 * Both source and destination must be invariant across the backedge. */
void ir_opt_hoist_scalar_fp_constants(IrBuffer *ir) {
    if (!ir || !ir->text) return;
    for (int back = 1; back < ir->text_count; ++back) {
        if (!is_direct_branch(ir->text[back].IrNode)) continue;
        int label = -1;
        for (int i = back - 1; i >= 0; --i)
            if (ir->text[i].IrNode == IR_LABEL && ir->text[i].label_id == ir->text[back].label_id) {
                label = i; break;
            }
        if (label < 0) continue;
        for (int round = 0; round < 8; ++round) {
            int cand = -1;
            for (int i = label + 1; i < back; ++i) {
                IrInst *m = &ir->text[i];
                if (m->IrNode != IR_MOVQ_XMM_REG) continue;
                int src_writes = 0, dst_writes = 0;
                for (int k = label + 1; k < back; ++k) {
                    if (writes_reg(&ir->text[k], m->src)) src_writes++;
                    if (writes_reg(&ir->text[k], m->dst)) dst_writes++;
                }
                if (src_writes == 0 && dst_writes == 1) { cand = i; break; }
            }
            if (cand < 0) break;
            IrInst saved = ir->text[cand];
            for (int i = cand; i > label; --i) ir->text[i] = ir->text[i - 1];
            ir->text[label] = saved;
            label++;
        }
    }
}

/* Sink a post-merge equality reset into the cold side of a range-guarded
 * diamond.  The hot side receives a duplicate of the proven-safe loop tail.
 *
 *   if (x < T) hot else cold; x += S; if (x == R) x = 0
 *
 * When S > 0 and R >= T + S, x == R is impossible on the hot edge. */
void ir_opt_sink_cold_range_reset(IrBuffer *ir) {
    if (!ir || !ir->text || ir->text_count < 18) return;

	/* Post-layout form used by the current SSA lowerer:
	 *
	 *   guard; hot-op; merge: state += step; if (state == reset) ...;
	 *   tail; backedge; cold: cold-op; jmp merge; reset: state=0; jmp tail
	 *
	 * Duplicate the proven register-only tail on the hot edge.  The original
	 * tail remains the cold/reset path, so no side effect is duplicated. */
	for (int body = 0; body + 13 < ir->text_count; ++body) {
		IrInst *t = ir->text;
		IrInst *guard_cmp = &t[body + 1], *guard = &t[body + 2];
		IrInst *hot_label = &t[body + 3], *hot = &t[body + 4];
		IrInst *merge = &t[body + 5], *step = &t[body + 6];
		IrInst *reset_cmp = &t[body + 7], *to_reset = &t[body + 8];
		IrInst *tail = &t[body + 9], *induct = &t[body + 10];
		IrInst *latch_cmp = &t[body + 11], *back = &t[body + 12];
		IrInst *to_exit = &t[body + 13];
		if (t[body].IrNode != IR_LABEL ||
		    guard_cmp->IrNode != IR_CMP_REG_IMM || guard->IrNode != IR_JGE ||
		    hot_label->IrNode != IR_LABEL || merge->IrNode != IR_LABEL ||
		    step->IrNode != IR_ADD_REG_IMM || step->dst != guard_cmp->dst || step->imm <= 0 ||
		    reset_cmp->IrNode != IR_CMP_REG_IMM || reset_cmp->dst != step->dst ||
		    to_reset->IrNode != IR_JE || tail->IrNode != IR_LABEL ||
		    induct->IrNode != IR_ADD_REG_IMM || induct->imm <= 0 ||
		    latch_cmp->IrNode != IR_CMP_REG_IMM || latch_cmp->dst != induct->dst ||
		    (back->IrNode != IR_JNE && back->IrNode != IR_JL) ||
		    back->label_id != t[body].label_id || to_exit->IrNode != IR_JMP ||
		    !is_pure_two_address_update(hot) || hot->dst == step->dst) continue;
		if (guard_cmp->imm > INT64_MAX - step->imm ||
		    reset_cmp->imm < guard_cmp->imm + step->imm) continue;

		int cold = -1, reset = -1;
		for (int q = body + 14; q < ir->text_count; ++q) {
			if (t[q].IrNode == IR_LABEL_NAMED || t[q].IrNode == IR_RET) break;
			if (t[q].IrNode == IR_LABEL && t[q].label_id == guard->label_id) cold = q;
			if (t[q].IrNode == IR_LABEL && t[q].label_id == to_reset->label_id) reset = q;
		}
		if (cold < 0 || reset < 0 || cold + 2 >= ir->text_count ||
		    reset + 2 >= ir->text_count) continue;
		IrInst *cold_op = &t[cold + 1], *cold_back = &t[cold + 2];
		IrInst *reset_op = &t[reset + 1], *reset_back = &t[reset + 2];
		if (!is_pure_two_address_update(cold_op) || cold_op->dst != hot->dst ||
		    cold_back->IrNode != IR_JMP || cold_back->label_id != merge->label_id ||
		    reset_op->IrNode != IR_MOV_REG_IMM || reset_op->dst != step->dst ||
		    reset_op->imm != 0 || reset_back->IrNode != IR_JMP ||
		    reset_back->label_id != tail->label_id) continue;

		int merge_refs = 0, tail_refs = 0;
		for (int q = 0; q < ir->text_count; ++q) {
			if (!is_direct_branch(t[q].IrNode)) continue;
			if (t[q].label_id == merge->label_id) merge_refs++;
			if (t[q].label_id == tail->label_id) tail_refs++;
		}
		if (merge_refs != 1 || tail_refs != 1) continue;

		IrInst duplicate[5] = {*step, *induct, *latch_cmp, *back, *to_exit};
		int insert = body + 5;
		if (ir->text_count > INT_MAX - 5 ||
		    !ensure_ir_capacity(ir, ir->text_count + 5)) return;
		memmove(&ir->text[insert + 5], &ir->text[insert],
		        (size_t)(ir->text_count - insert) * sizeof(*ir->text));
		memcpy(&ir->text[insert], duplicate, sizeof(duplicate));
		ir->text_count += 5;
		ir->text[body].extra_imm = IR_HOT_SPLIT_LOOP_MARK;
		return;
	}

    for (int merge = 1; merge + 10 < ir->text_count; ++merge) {
        IrInst *m = &ir->text[merge];
        if (m->IrNode != IR_LABEL) continue;
		int func_start = merge, func_end = merge + 1;
		while (func_start > 0 && ir->text[func_start].IrNode != IR_LABEL_NAMED) --func_start;
		while (func_end < ir->text_count && ir->text[func_end].IrNode != IR_LABEL_NAMED) ++func_end;
		if (merge + 10 >= func_end) continue;
        IrInst *cp = &ir->text[merge + 1];
        IrInst *step = &ir->text[merge + 2];
        IrInst *commit = &ir->text[merge + 3];
        IrInst *reset_cmp = &ir->text[merge + 4];
        IrInst *skip_reset = &ir->text[merge + 5];
        IrInst *reset_label = &ir->text[merge + 6];
        IrInst *reset = &ir->text[merge + 7];
        IrInst *tail_label = &ir->text[merge + 8];
        IrInst *induct = &ir->text[merge + 9];
        IrInst *back = &ir->text[merge + 10];
        if (cp->IrNode != IR_MOV_REG_REG ||
            step->IrNode != IR_ADD_REG_IMM || step->dst != cp->dst || step->imm <= 0 ||
            commit->IrNode != IR_MOV_REG_REG || commit->dst != cp->src || commit->src != cp->dst ||
            reset_cmp->IrNode != IR_CMP_REG_IMM || reset_cmp->dst != cp->dst ||
            skip_reset->IrNode != IR_JNE ||
            reset_label->IrNode != IR_LABEL ||
            reset->IrNode != IR_MOV_REG_IMM || reset->dst != cp->src || reset->imm != 0 ||
            tail_label->IrNode != IR_LABEL || tail_label->label_id != skip_reset->label_id ||
            induct->IrNode != IR_ADD_REG_IMM ||
            back->IrNode != IR_JMP) continue;
        if (reset_cmp->imm < step->imm) continue;

        /* Find the range guard that selects the cold block. */
        int guard = -1;
		for (int i = merge - 1; i > func_start && i >= merge - 16; --i) {
            if (ir->text[i].IrNode == IR_JGE &&
                ir->text[i - 1].IrNode == IR_CMP_REG_IMM &&
                ir->text[i - 1].dst == cp->src) {
                guard = i; break;
            }
        }
        if (guard < 0) continue;
        int64_t threshold = ir->text[guard - 1].imm;
        if (threshold > INT64_MAX - step->imm ||
            reset_cmp->imm < threshold + step->imm) continue;
        int cold_id = ir->text[guard].label_id;

        /* Exactly one explicit edge may enter the merge, and it must be the
         * cold block.  The hot edge is the existing fallthrough. */
        int merge_edges = 0, cold = -1, cold_jump = -1;
		for (int i = func_start; i < func_end; ++i) {
            if (is_direct_branch(ir->text[i].IrNode) && ir->text[i].label_id == m->label_id) {
                merge_edges++;
                cold_jump = i;
            }
            if (ir->text[i].IrNode == IR_LABEL && ir->text[i].label_id == cold_id) cold = i;
        }
        if (merge_edges != 1 || cold < 0 || cold_jump <= cold ||
            ir->text[cold_jump].IrNode != IR_JMP) continue;
        for (int i = cold + 1; i < cold_jump; ++i)
            if (is_call(ir->text[i].IrNode) || ir->text[i].IrNode == IR_LABEL) {
                cold = -1; break;
            }
        if (cold < 0) continue;

        /* The hot path must reach merge directly and contain no side effect
         * beyond register arithmetic; otherwise duplicating the tail could
         * alter observable ordering. */
        for (int i = guard + 1; i < merge; ++i)
            if (is_call(ir->text[i].IrNode) || is_direct_branch(ir->text[i].IrNode) ||
				ir->text[i].IrNode == IR_LABEL || ir->text[i].IrNode == IR_LABEL_NAMED ||
				ir->text[i].IrNode == IR_RET || ir->text[i].IrNode == IR_JMP_EXTERN ||
                writes_reg(&ir->text[i], cp->src)) {
                guard = -1; break;
            }
        if (guard < 0) continue;

        int loop_header_id = back->label_id;
        IrInst dup[5] = {*cp, *step, *commit, *induct, *back};
        if (ir->text_count > INT_MAX - 5 ||
            !ensure_ir_capacity(ir, ir->text_count + 5)) return;
        memmove(&ir->text[merge + 5], &ir->text[merge],
                (size_t)(ir->text_count - merge) * sizeof(*ir->text));
        memcpy(&ir->text[merge], dup, sizeof(dup));
        ir->text_count += 5;
        /* Mark the loop header whose hot/cold tails were proven and split.
         * extra_imm is metadata-only on labels and is discarded by encoding. */
        for (int i = 0; i < ir->text_count; ++i)
            if (ir->text[i].IrNode == IR_LABEL && ir->text[i].label_id == loop_header_id) {
                ir->text[i].extra_imm = IR_HOT_SPLIT_LOOP_MARK;
                break;
            }
        merge += 5;
    }
}

/* Rotate a canonical two-instruction loop test.  Entry still executes
 * LABEL,CMP,Jcc once.  Each backedge becomes
 * CMP,inverse-Jcc-to-body,JMP-exit.  The final jump is cold, while the hot
 * cmp+jcc pair remains macro-fusible.  Keeping an explicit cold exit is
 * required when outlined side blocks follow the original backedge; falling
 * through into such a block changes control flow and can make the loop
 * infinite. */
void ir_opt_rotate_canonical_loop_test(IrBuffer *ir) {
    if (!ir || !ir->text || ir->text_count < 8) return;
    for (int head = 0; head + 3 < ir->text_count; ++head) {
        IrInst *label = &ir->text[head];
        IrInst *cmp = &ir->text[head + 1];
        IrInst *exit = &ir->text[head + 2];
        IrInst *body = &ir->text[head + 3];
        if (label->IrNode != IR_LABEL || body->IrNode != IR_LABEL) continue;
        if (cmp->IrNode != IR_CMP_REG_IMM && cmp->IrNode != IR_CMP_REG_REG) continue;
        IrOpcode inv;
        switch (exit->IrNode) {
        case IR_JE: inv = IR_JNE; break; case IR_JNE: inv = IR_JE; break;
        case IR_JL: inv = IR_JGE; break; case IR_JGE: inv = IR_JL; break;
        case IR_JLE: inv = IR_JG; break; case IR_JG: inv = IR_JLE; break;
        case IR_JB: inv = IR_JAE; break; case IR_JAE: inv = IR_JB; break;
        case IR_JBE: inv = IR_JA; break; case IR_JA: inv = IR_JBE; break;
        default: continue;
        }
        for (int back = head + 4; back < ir->text_count; ++back) {
            if (ir->text[back].IrNode == IR_LABEL_NAMED ||
                ir->text[back].IrNode == IR_RET) break;
            if (ir->text[back].IrNode != IR_JMP ||
                ir->text[back].label_id != label->label_id) continue;
            if (ir->text_count + 2 > ir->text_cap) {
                int cap = ir->text_cap ? ir->text_cap * 2 : 64;
                while (cap < ir->text_count + 2) cap *= 2;
                ir->text = realloc(ir->text, (size_t)cap * sizeof(*ir->text));
                ir->text_cap = cap;
                label = &ir->text[head];
                cmp = &ir->text[head + 1];
                exit = &ir->text[head + 2];
                body = &ir->text[head + 3];
            }
            memmove(&ir->text[back + 3], &ir->text[back + 1],
                    (size_t)(ir->text_count - back - 1) * sizeof(*ir->text));
            ir->text[back] = *cmp;
            ir->text[back + 1] = *exit;
            IrOpcode latch_cond = inv;
            /* A guarded increasing induction variable cannot skip an integer
             * bound when its latch step is exactly one.  Equality is then the
             * complete exit condition and avoids carrying signed-range state
             * around the hot backedge. */
            if (exit->IrNode == IR_JGE && cmp->IrNode == IR_CMP_REG_IMM &&
                back > 0 && ir->text[back - 1].IrNode == IR_ADD_REG_IMM &&
                ir->text[back - 1].dst == cmp->dst && ir->text[back - 1].imm == 1)
                latch_cond = IR_JNE;
            ir->text[back + 1].IrNode = latch_cond;
            ir->text[back + 1].label_id = body->label_id;
            ir->text[back + 2] = *exit;
            ir->text[back + 2].IrNode = IR_JMP;
            ir->text[back + 2].label_id = exit->label_id;
            ir->text_count += 2;
            back += 2;
        }
    }
}

/* Keep a repeated wide-multiply magic constant resident across a hot loop.
 * This is restricted to leaf functions and a completely unused nonvolatile
 * register; the pass inserts the ABI save/restore itself. */
void ir_opt_hoist_repeated_wide_magic(IrBuffer *ir) {
    if (!ir || !ir->text || ir->text_count < 12) return;
    for (int back = 1; back < ir->text_count; ++back) {
        if (ir->text[back].IrNode != IR_JMP) continue;
        int header = -1;
        for (int i = back - 1; i >= 0; --i)
            if (ir->text[i].IrNode == IR_LABEL &&
                ir->text[i].label_id == ir->text[back].label_id) { header = i; break; }
        if (header < 0) continue;
        int outside_entry = 0;
        for (int i = 0; i < ir->text_count; ++i)
            if (i != back && is_direct_branch(ir->text[i].IrNode) &&
                ir->text[i].label_id == ir->text[header].label_id) { outside_entry = 1; break; }
        if (outside_entry) continue;
        int fs = header, fe = ir->text_count;
        while (fs > 0 && ir->text[fs].IrNode != IR_LABEL_NAMED) --fs;
        if (ir->text[fs].IrNode != IR_LABEL_NAMED) continue;
        for (int i = fs + 1; i < ir->text_count; ++i)
            if (ir->text[i].IrNode == IR_LABEL_NAMED) { fe = i; break; }
        int leaf = 1;
        for (int i = fs; i < fe; ++i) if (is_call(ir->text[i].IrNode)) { leaf = 0; break; }
        if (!leaf) continue;

        int64_t magic = 0;
        int magic_count = 0;
        IrReg old_reg = REG_NONE;
        for (int i = header + 1; i < back; ++i) {
            if (ir->text[i].IrNode == IR_MOV_REG_IMM && i + 1 < back &&
                ir->text[i + 1].IrNode == IR_IMUL_WIDE_REG &&
                ir->text[i + 1].src == ir->text[i].dst &&
                (ir->text[i].imm < INT32_MIN || ir->text[i].imm > INT32_MAX)) {
                if (!magic_count) { magic = ir->text[i].imm; old_reg = ir->text[i].dst; }
                if (ir->text[i].imm == magic && ir->text[i].dst == old_reg) magic_count++;
            }
        }
        if (magic_count < 2) continue;

        IrReg candidates[] = {REG_R15, REG_R12, REG_RBX, REG_RSI, REG_RDI};
        IrReg keep = REG_NONE;
        for (unsigned c = 0; c < sizeof(candidates)/sizeof(candidates[0]); ++c) {
            int used = 0;
            for (int i = fs; i < fe; ++i)
                if (reads_reg(&ir->text[i], candidates[c]) || writes_reg(&ir->text[i], candidates[c]) ||
                    ((ir->text[i].IrNode == IR_PUSH_REG || ir->text[i].IrNode == IR_POP_REG) &&
                     ir->text[i].dst == candidates[c])) { used = 1; break; }
            if (!used) { keep = candidates[c]; break; }
        }
        if (keep == REG_NONE) continue;

        int prologue = -1, epilogue = -1;
        for (int i = fs + 1; i + 1 < fe; ++i) {
            if (ir->text[i].IrNode == IR_PUSH_REG && ir->text[i].dst == REG_RBP &&
                ir->text[i + 1].IrNode == IR_MOV_REG_REG &&
                ir->text[i + 1].dst == REG_RBP && ir->text[i + 1].src == REG_RSP)
                prologue = i + 1;
            if (ir->text[i].IrNode == IR_POP_REG && ir->text[i].dst == REG_RBP &&
                i + 1 < fe && ir->text[i + 1].IrNode == IR_RET)
                epilogue = i;
        }
        if (prologue < 0 || epilogue < 0) continue;

        int remove_count = 0;
        for (int i = header + 1; i < back; ++i)
            if (ir->text[i].IrNode == IR_MOV_REG_IMM && ir->text[i].dst == old_reg &&
                ir->text[i].imm == magic && i + 1 < back &&
                ir->text[i + 1].IrNode == IR_IMUL_WIDE_REG &&
                ir->text[i + 1].src == old_reg) remove_count++;
        int extra = 3 - remove_count;
        int new_count = ir->text_count + extra;
        IrInst *out = malloc((size_t)(new_count + 8) * sizeof(*out));
        int w = 0;
        for (int i = 0; i < ir->text_count; ++i) {
            if (i == prologue + 1) {
                IrInst save = {0}; save.IrNode = IR_PUSH_REG; save.dst = keep; out[w++] = save;
            }
            if (i == header) {
                IrInst load = {0}; load.IrNode = IR_MOV_REG_IMM; load.dst = keep; load.imm = magic;
                out[w++] = load;
            }
            if (i == epilogue) {
                IrInst restore = {0}; restore.IrNode = IR_POP_REG; restore.dst = keep; out[w++] = restore;
            }
            if (i >= header + 1 && i < back &&
                ir->text[i].IrNode == IR_MOV_REG_IMM && ir->text[i].dst == old_reg &&
                ir->text[i].imm == magic && i + 1 < back &&
                ir->text[i + 1].IrNode == IR_IMUL_WIDE_REG &&
                ir->text[i + 1].src == old_reg) continue;
            IrInst inst = ir->text[i];
            if (i > header + 1 && i <= back && inst.IrNode == IR_IMUL_WIDE_REG &&
                inst.src == old_reg && ir->text[i - 1].IrNode == IR_MOV_REG_IMM &&
                ir->text[i - 1].imm == magic) inst.src = keep;
            out[w++] = inst;
        }
        free(ir->text);
        ir->text = out;
        ir->text_count = w;
        ir->text_cap = new_count + 8;
        return; /* Recompute indices before considering another function. */
    }
}

/* A VM-specialized constant loop gives us a proof, not a profile: if the
 * induction starts non-negative, advances positively to a positive constant
 * bound, and a scaled numerator cannot overflow int64, signed magic division
 * may use the shorter unsigned-high sequence. */
void ir_opt_unsigned_magic_from_constant_loop(IrBuffer *ir) {
    if (!ir || !ir->text || ir->text_count < 12) return;
    unsigned char *drop = calloc((size_t)ir->text_count, 1);
    if (!drop) return;
    int changed = 0;
    for (int back = 1; back < ir->text_count; ++back) {
        IrInst *bj = &ir->text[back];
        if (bj->IrNode != IR_JMP) continue;
        int header = -1;
        for (int h = back - 1; h >= 0; --h)
            if (ir->text[h].IrNode == IR_LABEL && ir->text[h].label_id == bj->label_id) {
                header = h; break;
            }
        if (header < 1 || header + 2 >= back) continue;
        IrInst *cmp = &ir->text[header + 1];
        if (cmp->IrNode != IR_CMP_REG_IMM || cmp->imm <= 0) continue;
        IrReg induction = cmp->dst;
        int64_t bound = cmp->imm;
        int init_ok = 0, step_ok = 0, other_write = 0;
        for (int i = header - 1; i >= 0 && ir->text[i].IrNode != IR_RET; --i)
            if (writes_reg(&ir->text[i], induction)) {
                init_ok = ir->text[i].IrNode == IR_MOV_REG_IMM && ir->text[i].imm >= 0;
                break;
            }
        for (int i = header + 2; i < back; ++i) if (writes_reg(&ir->text[i], induction)) {
            if (ir->text[i].IrNode == IR_ADD_REG_IMM && ir->text[i].imm > 0) step_ok = 1;
            else other_write = 1;
        }
        if (!init_ok || !step_ok || other_write) continue;

        for (int w = header + 2; w + 5 < back; ++w) {
            if (ir->text[w].IrNode != IR_IMUL_WIDE_REG ||
                ir->text[w + 1].IrNode != IR_ADD_REG_REG ||
                ir->text[w + 1].dst != REG_RDX ||
                ir->text[w + 2].IrNode != IR_SAR_REG_IMM ||
                ir->text[w + 2].dst != REG_RDX ||
                ir->text[w + 3].IrNode != IR_MOV_REG_REG ||
                ir->text[w + 3].src != REG_RDX ||
                ir->text[w + 4].IrNode != IR_SHR_REG_IMM ||
                ir->text[w + 4].dst != ir->text[w + 3].dst ||
                ir->text[w + 4].imm != 63 ||
                ir->text[w + 5].IrNode != IR_ADD_REG_REG ||
                ir->text[w + 5].dst != REG_RDX ||
                ir->text[w + 5].src != ir->text[w + 3].dst) continue;
            IrReg numerator = ir->text[w + 1].src;
            int proven = numerator == induction;
            if (!proven) {
                for (int d = w - 1; d >= header + 2 && d >= w - 5; --d) {
                    if (ir->text[d].IrNode == IR_MOV_REG_REG &&
                        ir->text[d].dst == numerator) {
                        numerator = ir->text[d].src;
                        if (numerator == induction) { proven = 1; break; }
                    }
                    if (ir->text[d].IrNode == IR_IMUL_REG_IMM &&
                        ir->text[d].dst == numerator && ir->text[d].src == induction &&
                        ir->text[d].imm > 0 && bound <= INT64_MAX / ir->text[d].imm) {
                        proven = 1; break;
                    }
                }
            }
            if (!proven) continue;
            ir->text[w].IrNode = IR_MUL_WIDE_REG;
            ir->text[w + 2].IrNode = IR_SHR_REG_IMM;
            drop[w + 1] = drop[w + 3] = drop[w + 4] = drop[w + 5] = 1;
            changed = 1;
        }
    }
    if (changed) {
        int out = 0;
        for (int i = 0; i < ir->text_count; ++i) if (!drop[i]) ir->text[out++] = ir->text[i];
        ir->text_count = out;
    }
    free(drop);
}

/* Schedule the proven dual-magic loop as one recurrence superblock.  Besides
 * overlapping the two multiply chains, strength-reduce (i*k) into a running
 * scaled induction and rotate the entry test into one bottom JNE. */
void ir_opt_schedule_dual_magic_loop(IrBuffer *ir) {
    if (!ir || !ir->text || ir->text_count < 20) return;
    for (int h = 1; h + 19 < ir->text_count; ++h) {
        IrInst *t = ir->text;
        if (t[h].IrNode != IR_LABEL ||
            t[h + 1].IrNode != IR_CMP_REG_IMM || t[h + 1].imm <= 0 ||
            t[h + 2].IrNode != IR_JG) continue;
        IrReg ind = t[h + 1].dst;
        int64_t bound = t[h + 1].imm;
        int s = h + 3;
        while (s < ir->text_count && t[s].IrNode == IR_LABEL) s++;
        if (t[s].IrNode != IR_IMUL_REG_IMM || t[s].src != ind || t[s].imm <= 0 ||
            bound == INT64_MAX || bound + 1 > INT64_MAX / t[s].imm ||
            t[s + 1].IrNode != IR_MOV_REG_REG || t[s + 1].src != t[s].dst ||
            t[s + 2].IrNode != IR_MOV_REG_REG || t[s + 2].dst != REG_RAX ||
            t[s + 2].src != t[s + 1].dst ||
            t[s + 3].IrNode != IR_MUL_WIDE_REG ||
            t[s + 4].IrNode != IR_SHR_REG_IMM || t[s + 4].dst != REG_RDX ||
            t[s + 5].IrNode != IR_IMUL_REG_IMM || t[s + 5].src != REG_RDX ||
            t[s + 6].IrNode != IR_MOV_REG_REG || t[s + 6].src != t[s + 1].dst ||
            t[s + 7].IrNode != IR_SUB_REG_REG || t[s + 7].dst != t[s + 6].dst ||
            t[s + 7].src != t[s + 5].dst ||
            t[s + 8].IrNode != IR_MOV_REG_REG || t[s + 8].src != ind ||
            t[s + 9].IrNode != IR_MOV_REG_REG || t[s + 9].dst != REG_RAX ||
            t[s + 9].src != t[s + 8].dst ||
            t[s + 10].IrNode != IR_LEA_IDX ||
            t[s + 11].IrNode != IR_MUL_WIDE_REG ||
            t[s + 11].src != t[s + 3].src ||
            t[s + 12].IrNode != IR_SHR_REG_IMM || t[s + 12].dst != REG_RDX ||
            t[s + 13].IrNode != IR_MOV_REG_REG || t[s + 13].src != REG_RDX ||
            t[s + 14].IrNode != IR_LEA_IDX ||
            t[s + 15].IrNode != IR_ADD_REG_IMM || t[s + 15].dst != ind ||
            t[s + 15].imm != 1 ||
            t[s + 16].IrNode != IR_JMP || t[s + 16].label_id != t[h].label_id ||
            s + 17 >= ir->text_count || t[s + 17].IrNode != IR_LABEL) continue;
        int exit_alias = 0;
        for (int e = s + 17; e < ir->text_count && t[e].IrNode == IR_LABEL; ++e)
            if (t[e].label_id == t[h + 2].label_id) { exit_alias = 1; break; }
        if (!exit_alias) continue;

        IrReg scaled = t[s].dst;
        IrReg rem = t[s + 6].dst;
        IrReg sum = t[s + 14].dst;
        IrReg magic = t[s + 3].src;
        int64_t factor = t[s].imm;
        int64_t shift = t[s + 4].imm;
        int64_t divisor = t[s + 5].imm;
        IrInst n[16]; memset(n, 0, sizeof(n));
        n[0].IrNode = IR_MOV_REG_IMM; n[0].dst = scaled; n[0].imm = factor;
        n[1] = t[h];
        n[2].IrNode = IR_MOV_REG_REG; n[2].dst = REG_RAX; n[2].src = scaled;
        n[3].IrNode = IR_MOV_REG_REG; n[3].dst = rem; n[3].src = scaled;
        n[4].IrNode = IR_ADD_REG_IMM; n[4].dst = scaled; n[4].imm = factor;
        n[5].IrNode = IR_MUL_WIDE_REG; n[5].src = magic;
        n[6].IrNode = IR_MOV_REG_REG; n[6].dst = REG_RAX; n[6].src = ind;
        n[7].IrNode = IR_ADD_REG_IMM; n[7].dst = ind; n[7].imm = 1;
        n[8].IrNode = IR_SHR_REG_IMM; n[8].dst = REG_RDX; n[8].imm = shift;
        n[9].IrNode = IR_IMUL_REG_IMM; n[9].dst = REG_RDX; n[9].src = REG_RDX; n[9].imm = divisor;
        n[10].IrNode = IR_SUB_REG_REG; n[10].dst = rem; n[10].src = REG_RDX;
        n[11].IrNode = IR_MUL_WIDE_REG; n[11].src = magic;
        n[12].IrNode = IR_SHR_REG_IMM; n[12].dst = REG_RDX; n[12].imm = shift;
        n[13].IrNode = IR_ADD_REG_REG; n[13].dst = rem; n[13].src = REG_RDX;
        n[14].IrNode = IR_ADD_REG_REG; n[14].dst = sum; n[14].src = rem;
        n[15].IrNode = IR_CMP_REG_IMM; n[15].dst = ind; n[15].imm = bound + 1;
        IrInst jne = {0}; jne.IrNode = IR_JNE; jne.label_id = t[h].label_id;

        int old_end = s + 16;
        int replacement = 18;
        int old_count = old_end - h + 1;
        int new_count = ir->text_count - old_count + replacement;
        IrInst *out = calloc((size_t)new_count + 8, sizeof(*out));
        if (!out) return;
        memcpy(out, t, (size_t)h * sizeof(*out));
        out[h] = n[0];
        out[h + 1].IrNode = IR_ALIGN32;
        memcpy(out + h + 2, n + 1, 15 * sizeof(*n));
        out[h + 17] = jne;
        memcpy(out + h + replacement, t + old_end + 1,
               (size_t)(ir->text_count - old_end - 1) * sizeof(*out));
        free(ir->text);
        ir->text = out; ir->text_count = new_count; ir->text_cap = new_count + 8;
        return;
    }
}
