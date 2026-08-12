#include "ir_ssa.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static bool same_fact(const SsaRefFact *a, const SsaRefFact *b) {
	return a->origin_kind == b->origin_kind && a->root_id == b->root_id &&
		a->offset_min == b->offset_min && a->offset_max == b->offset_max &&
		a->access_width == b->access_width && a->flags == b->flags &&
		a->free_func_name == b->free_func_name &&
		a->return_alias_param == b->return_alias_param;
}

static SsaRefFact unknown_fact(void) {
	SsaRefFact f;
	memset(&f, 0, sizeof(f));
	f.origin_kind = SSA_REF_ORIGIN_UNKNOWN;
	f.return_alias_param = -1;
	return f;
}

static SsaRefFact join_fact(SsaRefFact a, SsaRefFact b) {
	if (a.origin_kind == SSA_REF_ORIGIN_UNKNOWN || b.origin_kind == SSA_REF_ORIGIN_UNKNOWN)
		return unknown_fact();
	if (a.origin_kind != b.origin_kind || a.root_id == 0 || a.root_id != b.root_id)
		return unknown_fact();
	if (b.offset_min < a.offset_min) a.offset_min = b.offset_min;
	if (b.offset_max > a.offset_max) a.offset_max = b.offset_max;
	a.flags &= b.flags;
	if (a.free_func_name != b.free_func_name) a.free_func_name = NULL;
	if (a.return_alias_param != b.return_alias_param) a.return_alias_param = -1;
	return a;
}

static SsaFunction *find_function(SsaModule *mod, const char *name) {
	if (!name) return NULL;
	for (int i = 0; i < mod->func_count; i++)
		if (mod->functions[i] && mod->functions[i]->name &&
			strcmp(mod->functions[i]->name, name) == 0)
			return mod->functions[i];
	return NULL;
}

static const char *call_symbol(const SsaInst *inst) {
	if (!inst || inst->IrNode != SSA_OP_CALL) return NULL;
	if (inst->operands && inst->operand_count > 0 &&
		inst->operands[0].kind == SSA_OPND_SYM)
		return inst->operands[0].u.sym;
	if (inst->op1.kind == SSA_OPND_SYM) return inst->op1.u.sym;
	return NULL;
}

static bool is_concurrency_symbol(const char *sym) {
	return sym && (
		strcmp(sym, "mira_go_start0") == 0 ||
		strcmp(sym, "mira_go_start_fast0") == 0 ||
		strcmp(sym, "mira_go_join") == 0 ||
		strcmp(sym, "mira_go_yield") == 0 ||
		strcmp(sym, "mira_go_wait_all") == 0 ||
		strcmp(sym, "mira_channel_new_value") == 0 ||
		strcmp(sym, "mira_channel_send_value") == 0 ||
		strcmp(sym, "mira_channel_recv_value") == 0 ||
		strcmp(sym, "mira_channel_close_value") == 0 ||
		strcmp(sym, "mira_channel_free_value") == 0);
}

static void select_fast_go_entries(SsaModule *mod) {
	for (int fi = 0; fi < mod->func_count; ++fi) {
		SsaFunction *func = mod->functions[fi];
		for (int bi = 0; bi < func->block_count; ++bi) {
			for (SsaInst *inst = func->blocks[bi]->inst_head; inst; inst = inst->next) {
				const char *sym = call_symbol(inst);
				if (!sym || strcmp(sym, "mira_go_start0") != 0 ||
					!inst->operands || inst->operand_count < 2 ||
					inst->operands[1].kind != SSA_OPND_VREG)
					continue;
				VReg target_reg = inst->operands[1].u.vreg;
				if (!func->vreg_defs || target_reg == 0 ||
					target_reg >= func->next_vreg)
					continue;
				SsaInst *definition = func->vreg_defs[target_reg];
				if (!definition || definition->IrNode != SSA_OP_LEA_FUNC ||
					definition->op1.kind != SSA_OPND_SYM)
					continue;
				SsaFunction *target = find_function(mod, definition->op1.u.sym);
				if (!target || !target->ref_effect || target->ref_effect->may_suspend)
					continue;
				free(inst->operands[0].u.sym);
				inst->operands[0].u.sym = strdup("mira_go_start_fast0");
			}
		}
	}
}

static bool is_suspend_symbol(const char *sym) {
	return sym && (
		strcmp(sym, "mira_win_sleep") == 0 ||
		strcmp(sym, "mira_go_join") == 0 ||
		strcmp(sym, "mira_go_yield") == 0 ||
		strcmp(sym, "mira_go_wait_all") == 0 ||
		strcmp(sym, "mira_channel_send_value") == 0 ||
		strcmp(sym, "mira_channel_recv_value") == 0);
}

static bool is_known_non_suspend_external(const char *sym) {
	if (!sym) return false;
	if (is_concurrency_symbol(sym) && !is_suspend_symbol(sym)) return true;
	return strcmp(sym, "mira_print") == 0 ||
		strcmp(sym, "mira_cr") == 0 ||
		strcmp(sym, "mem_alloc") == 0 ||
		strcmp(sym, "mem_free") == 0 ||
		strncmp(sym, "mira_str_", 9) == 0 ||
		strncmp(sym, "mira_list_", 10) == 0 ||
		strncmp(sym, "mira_dict_", 10) == 0 ||
		strcmp(sym, "mira_abs") == 0 ||
		strcmp(sym, "mira_min") == 0 ||
		strcmp(sym, "mira_max") == 0 ||
		strcmp(sym, "mira_f_sqrt") == 0 ||
		strcmp(sym, "mira_f_pow") == 0 ||
		strcmp(sym, "mira_random") == 0 ||
		strcmp(sym, "mira_random_range") == 0 ||
		strcmp(sym, "mira_time_now") == 0 ||
		strcmp(sym, "mira_time_ms") == 0 ||
		strcmp(sym, "mira_win_tick") == 0 ||
		strcmp(sym, "mira_win_tick_ns") == 0;
}

static SsaRefFact fact_for_inst(SsaFunction *func, SsaInst *inst) {
	SsaRefFact result = unknown_fact();
	if (!inst || inst->dst == 0) return result;
	switch (inst->IrNode) {
	case SSA_OP_ALLOCA:
		result.origin_kind = SSA_REF_ORIGIN_STACK;
		result.root_id = 0x40000000u | inst->dst;
		result.flags = SSA_REF_UNIQUE;
		break;
	case SSA_OP_LOAD_PARAM:
		if (inst->op1.kind == SSA_OPND_IMM && inst->op1.u.imm >= 0) {
			result.origin_kind = SSA_REF_ORIGIN_PARAM;
			result.root_id = (uint32_t)inst->op1.u.imm + 1u;
			result.return_alias_param = (int)inst->op1.u.imm;
		}
		break;
	case SSA_OP_COPY:
		if (inst->op1.kind == SSA_OPND_VREG && inst->op1.u.vreg < func->ref_fact_count)
			result = func->ref_facts[inst->op1.u.vreg];
		break;
	case SSA_OP_GETELEMENTPTR:
		if (inst->op1.kind == SSA_OPND_VREG && inst->op1.u.vreg < func->ref_fact_count) {
			result = func->ref_facts[inst->op1.u.vreg];
			if (result.origin_kind != SSA_REF_ORIGIN_UNKNOWN && inst->op2.kind == SSA_OPND_IMM) {
				result.offset_min += inst->op2.u.imm;
				result.offset_max += inst->op2.u.imm;
			} else if (inst->op2.kind != SSA_OPND_IMM) {
				result = unknown_fact();
			}
		}
		break;
	case SSA_OP_PHI: {
		bool have = false;
		for (int oi = 0; inst->operands && oi < inst->operand_count; oi++) {
			if (inst->operands[oi].kind != SSA_OPND_VREG ||
				inst->operands[oi].u.vreg >= func->ref_fact_count) continue;
			SsaRefFact f = func->ref_facts[inst->operands[oi].u.vreg];
			result = have ? join_fact(result, f) : f;
			have = true;
		}
		break;
	}
	case SSA_OP_CALL: {
		const char *sym = call_symbol(inst);
		if (sym && (strcmp(sym, "mem_alloc") == 0 ||
			strcmp(sym, "mira_list_new") == 0 ||
			strcmp(sym, "mira_channel_new_value") == 0)) {
			result.origin_kind = SSA_REF_ORIGIN_HEAP;
			result.root_id = 0x80000000u | inst->dst;
			result.flags = SSA_REF_UNIQUE;
		}
		break;
	}
	default:
		break;
	}
	if (inst->needs_free) {
		result.flags |= SSA_REF_NEEDS_FREE;
		result.free_func_name = inst->free_func_name;
	}
	return result;
}

static void mark_root_shared(SsaFunction *func, VReg value, bool escaped) {
	if (!func || !func->ref_facts || value == 0 || value >= func->ref_fact_count)
		return;
	SsaRefFact *source = &func->ref_facts[value];
	if (source->origin_kind == SSA_REF_ORIGIN_UNKNOWN) return;
	uint32_t root = source->root_id;
	for (size_t i = 1; i < func->ref_fact_count; ++i) {
		SsaRefFact *fact = &func->ref_facts[i];
		if (fact->root_id != root || fact->origin_kind == SSA_REF_ORIGIN_UNKNOWN)
			continue;
		fact->flags |= SSA_REF_SHARED;
		if (escaped) fact->flags |= SSA_REF_ESCAPED;
		fact->flags &= ~SSA_REF_UNIQUE;
	}
}

static void mark_concurrency_facts(SsaFunction *func) {
	for (int bi = 0; bi < func->block_count; ++bi) {
		for (SsaInst *inst = func->blocks[bi]->inst_head; inst; inst = inst->next) {
			const char *sym = call_symbol(inst);
			if (!is_concurrency_symbol(sym) || !inst->operands) continue;
			/* Operand 1 is the channel handle for every channel operation. */
			if (strncmp(sym, "mira_channel_", 13) == 0 &&
				strcmp(sym, "mira_channel_new_value") != 0 &&
				inst->operand_count > 1 && inst->operands[1].kind == SSA_OPND_VREG)
				mark_root_shared(func, inst->operands[1].u.vreg, true);
			/* Sending transfers observability to another task. */
			if (strcmp(sym, "mira_channel_send_value") == 0 &&
				inst->operand_count > 2 && inst->operands[2].kind == SSA_OPND_VREG)
				mark_root_shared(func, inst->operands[2].u.vreg, true);
		}
	}
}

static int reference_round_budget(const SsaFunction *func) {
	uint32_t budget = func->decision_plan.reference_analysis_budget;
	if (budget == 0) return 8; /* Bootstrap before the first 2.1 plan. */
	uint32_t values = func->next_vreg > 1 ? func->next_vreg - 1u : 1u;
	uint32_t rounds = budget / values;
	if (rounds < 2u) rounds = 2u;
	if (func->decision_plan.request_deep_reference_analysis && rounds < 8u)
		rounds = 8u;
	if (rounds > 16u) rounds = 16u;
	return (int)rounds;
}

static void analyze_facts(SsaFunction *func) {
	free(func->ref_facts);
	func->ref_fact_count = (size_t)func->next_vreg;
	func->ref_facts = calloc(func->ref_fact_count, sizeof(SsaRefFact));
	if (!func->ref_facts) { func->ref_fact_count = 0; return; }
	for (size_t i = 0; i < func->ref_fact_count; i++)
		func->ref_facts[i].return_alias_param = -1;
	int max_rounds = reference_round_budget(func);
	for (int round = 0; round < max_rounds; round++) {
		bool changed = false;
		for (int bi = 0; bi < func->block_count; bi++) {
			for (SsaInst *inst = func->blocks[bi]->inst_head; inst; inst = inst->next) {
				if (inst->dst == 0 || inst->dst >= func->ref_fact_count) continue;
				SsaRefFact next = fact_for_inst(func, inst);
				if (!same_fact(&next, &func->ref_facts[inst->dst])) {
					func->ref_facts[inst->dst] = next;
					changed = true;
				}
			}
		}
		if (!changed) break;
	}
}

static void summarize_local(SsaModule *mod, SsaFunction *func) {
	free(func->ref_effect);
	func->ref_effect = calloc(1, sizeof(SsaFunctionEffect));
	if (!func->ref_effect) return;
	func->ref_effect->return_alias_param = -1;
	for (int bi = 0; bi < func->block_count; bi++) {
		for (SsaInst *inst = func->blocks[bi]->inst_head; inst; inst = inst->next) {
			switch (inst->IrNode) {
			case SSA_OP_LOAD: case SSA_OP_LOAD8: case SSA_OP_VEC_LOAD:
				func->ref_effect->reads_memory = true; break;
			case SSA_OP_STORE: case SSA_OP_STORE8: case SSA_OP_VEC_STORE:
				func->ref_effect->writes_memory = true; break;
			case SSA_OP_LOAD_VAR:
				func->ref_effect->reads_global = true; break;
			case SSA_OP_STORE_VAR:
				func->ref_effect->writes_global = true; break;
			case SSA_OP_ICALL:
				func->ref_effect->has_unknown_effect = true;
				func->ref_effect->may_suspend = true;
				break;
			case SSA_OP_CALL: {
				const char *sym = call_symbol(inst);
				if (is_concurrency_symbol(sym)) {
					func->ref_effect->has_concurrency_effect = true;
					func->ref_effect->reads_global = true;
					func->ref_effect->writes_global = true;
				}
				if (is_suspend_symbol(sym))
					func->ref_effect->may_suspend = true;
				if (sym && (strcmp(sym, "mem_alloc") == 0 || strcmp(sym, "mira_list_new") == 0))
					func->ref_effect->allocates = true;
				else if (!find_function(mod, sym)) {
					func->ref_effect->has_unknown_effect = true;
					if (!is_known_non_suspend_external(sym))
						func->ref_effect->may_suspend = true;
				}
				break;
			}
			case SSA_OP_RET:
				if (inst->op1.kind == SSA_OPND_VREG &&
					inst->op1.u.vreg < func->ref_fact_count) {
					SsaRefFact *f = &func->ref_facts[inst->op1.u.vreg];
					func->ref_effect->return_alias_param = f->return_alias_param;
				}
				break;
			default: break;
			}
		}
	}
}

static bool merge_effect(SsaFunctionEffect *dst, const SsaFunctionEffect *src) {
	SsaFunctionEffect old = *dst;
	dst->param_reads |= src->param_reads;
	dst->param_writes |= src->param_writes;
	dst->param_full_overwrites |= src->param_full_overwrites;
	dst->param_reads_old_value |= src->param_reads_old_value;
	dst->param_captures |= src->param_captures;
	dst->param_frees |= src->param_frees;
	dst->reads_memory |= src->reads_memory;
	dst->writes_memory |= src->writes_memory;
	dst->reads_global |= src->reads_global;
	dst->writes_global |= src->writes_global;
	dst->allocates |= src->allocates;
	dst->has_unknown_effect |= src->has_unknown_effect;
	dst->has_concurrency_effect |= src->has_concurrency_effect;
	dst->may_suspend |= src->may_suspend;
	return memcmp(&old, dst, sizeof(old)) != 0;
}

typedef struct PendingWrite {
	VReg ptr;
	size_t width;
} PendingWrite;

static SsaAliasResult alias_vregs(SsaFunction *func, VReg a, size_t aw, VReg b, size_t bw) {
	if (!func->ref_facts || a >= func->ref_fact_count || b >= func->ref_fact_count)
		return SSA_ALIAS_MAY;
	if (a == b && aw == bw) {
		SsaRefFact *fact = &func->ref_facts[a];
		if (fact->root_id != 0 &&
			(fact->origin_kind == SSA_REF_ORIGIN_HEAP || fact->origin_kind == SSA_REF_ORIGIN_STACK) &&
			(fact->flags & SSA_REF_UNIQUE))
			return SSA_ALIAS_MUST;
		return SSA_ALIAS_MAY;
	}
	return ssa_ref_alias(&func->ref_facts[a], 0, aw, &func->ref_facts[b], 0, bw);
}

static void mark_overwritten_stores(SsaFunction *func) {
	for (int bi = 0; bi < func->block_count; bi++) {
		SsaBasicBlock *block = func->blocks[bi];
		int cap = 0;
		for (SsaInst *i = block->inst_head; i; i = i->next)
			if (i->IrNode == SSA_OP_STORE || i->IrNode == SSA_OP_STORE8) cap++;
		if (cap == 0) continue;
		PendingWrite *pending = calloc((size_t)cap, sizeof(*pending));
		if (!pending) continue;
		int count = 0;
		for (SsaInst *inst = block->inst_tail; inst; inst = inst->prev) {
			if (inst->IrNode == SSA_OP_CALL || inst->IrNode == SSA_OP_ICALL) {
				count = 0;
				continue;
			}
			if (inst->IrNode == SSA_OP_VEC_LOAD || inst->IrNode == SSA_OP_VEC_STORE) {
				/* Vector byte range is not yet represented precisely here. */
				count = 0;
				continue;
			}
			if ((inst->IrNode == SSA_OP_LOAD || inst->IrNode == SSA_OP_LOAD8) &&
				inst->op1.kind == SSA_OPND_VREG) {
				size_t width = inst->IrNode == SSA_OP_LOAD8 ? 1u : 8u;
				for (int p = count - 1; p >= 0; p--) {
					if (alias_vregs(func, inst->op1.u.vreg, width,
						pending[p].ptr, pending[p].width) != SSA_ALIAS_NONE) {
						pending[p] = pending[--count];
					}
				}
				continue;
			}
			if (inst->IrNode != SSA_OP_STORE && inst->IrNode != SSA_OP_STORE8) continue;
			inst->ref_analyzed = true;
			inst->ref_observable = true;
			if (inst->op2.kind != SSA_OPND_VREG) { count = 0; continue; }
			VReg ptr = inst->op2.u.vreg;
			size_t width = inst->IrNode == SSA_OP_STORE8 ? 1u : 8u;
			for (int p = 0; p < count; p++) {
				if (pending[p].width == width &&
					alias_vregs(func, ptr, width, pending[p].ptr, pending[p].width) == SSA_ALIAS_MUST) {
					inst->ref_observable = false;
					break;
				}
			}
			if (inst->ref_observable && count < cap) {
				pending[count].ptr = ptr;
				pending[count].width = width;
				count++;
			}
		}
		free(pending);
	}
}

void ssa_ref_analyze_module(SsaModule *mod) {
	if (!mod) return;
	for (int i = 0; i < mod->func_count; i++) analyze_facts(mod->functions[i]);
	for (int i = 0; i < mod->func_count; i++) mark_concurrency_facts(mod->functions[i]);
	for (int i = 0; i < mod->func_count; i++) summarize_local(mod, mod->functions[i]);
	for (int round = 0; round < 8; round++) {
		bool changed = false;
		for (int fi = 0; fi < mod->func_count; fi++) {
			SsaFunction *func = mod->functions[fi];
			if (!func->ref_effect) continue;
			for (int bi = 0; bi < func->block_count; bi++) {
				for (SsaInst *inst = func->blocks[bi]->inst_head; inst; inst = inst->next) {
					const char *sym = call_symbol(inst);
					SsaFunction *callee = find_function(mod, sym);
					if (callee && callee->ref_effect)
						changed |= merge_effect(func->ref_effect, callee->ref_effect);
				}
			}
		}
		if (!changed) break;
	}
	for (int fi = 0; fi < mod->func_count; fi++)
		mark_overwritten_stores(mod->functions[fi]);
	select_fast_go_entries(mod);
	/* Major effects seed the influence closure.  A direct user call is only a
	 * seed when its transitive summary is observable; its result dependencies
	 * can still make a pure call live through ordinary VReg tracing. */
	for (int fi = 0; fi < mod->func_count; fi++) {
		SsaFunction *func = mod->functions[fi];
		for (int bi = 0; bi < func->block_count; bi++) {
			for (SsaInst *inst = func->blocks[bi]->inst_head; inst; inst = inst->next) {
				if (inst->IrNode == SSA_OP_ICALL) {
					inst->ref_analyzed = true;
					inst->ref_observable = true;
					continue;
				}
				if (inst->IrNode != SSA_OP_CALL) continue;
				inst->ref_analyzed = true;
				SsaFunction *callee = find_function(mod, call_symbol(inst));
				if (!callee || !callee->ref_effect) {
					inst->ref_observable = true;
					continue;
				}
				SsaFunctionEffect *e = callee->ref_effect;
				inst->ref_observable = e->reads_memory || e->writes_memory ||
					e->reads_global || e->writes_global || e->has_unknown_effect;
			}
		}
	}
}

void ssa_ref_free_module(SsaModule *mod) {
	if (!mod) return;
	for (int i = 0; i < mod->func_count; i++) {
		SsaFunction *func = mod->functions[i];
		if (!func) continue;
		free(func->ref_facts); func->ref_facts = NULL; func->ref_fact_count = 0;
		free(func->ref_effect); func->ref_effect = NULL;
	}
}

SsaAliasResult ssa_ref_alias(const SsaRefFact *a, int64_t a_offset, size_t a_width,
	const SsaRefFact *b, int64_t b_offset, size_t b_width) {
	if (!a || !b || a->origin_kind == SSA_REF_ORIGIN_UNKNOWN ||
		b->origin_kind == SSA_REF_ORIGIN_UNKNOWN || a->root_id == 0 || b->root_id == 0)
		return SSA_ALIAS_MAY;
	if (a->root_id != b->root_id) return SSA_ALIAS_NONE;
	if (a->offset_min > INT64_MAX - a_offset || b->offset_min > INT64_MAX - b_offset)
		return SSA_ALIAS_MAY;
	int64_t a_start = a->offset_min + a_offset;
	int64_t b_start = b->offset_min + b_offset;
	if (a_width > (size_t)INT64_MAX || b_width > (size_t)INT64_MAX) return SSA_ALIAS_MAY;
	if (a_start > INT64_MAX - (int64_t)a_width || b_start > INT64_MAX - (int64_t)b_width)
		return SSA_ALIAS_MAY;
	int64_t a_end = a_start + (int64_t)a_width;
	int64_t b_end = b_start + (int64_t)b_width;
	if (a_end <= b_start || b_end <= a_start) return SSA_ALIAS_NONE;
	if (a->offset_min == a->offset_max && b->offset_min == b->offset_max &&
		a_start == b_start && a_width == b_width)
		return SSA_ALIAS_MUST;
	return SSA_ALIAS_MAY;
}

const SsaFunctionEffect *ssa_ref_effect(const SsaFunction *func) {
	return func ? func->ref_effect : NULL;
}

DecisionReferenceFacts ssa_ref_decision_facts(const SsaFunction *func) {
	DecisionReferenceFacts result;
	memset(&result, 0, sizeof(result));
	if (!func) return result;
	result.value_count = func->ref_fact_count > UINT32_MAX
		? UINT32_MAX : (uint32_t)func->ref_fact_count;
	for (size_t i = 1; i < func->ref_fact_count; ++i) {
		const SsaRefFact *fact = &func->ref_facts[i];
		if (fact->origin_kind != SSA_REF_ORIGIN_UNKNOWN)
			result.known_value_count++;
		if (fact->flags & SSA_REF_UNIQUE)
			result.unique_object_count++;
		if (fact->flags & SSA_REF_NEEDS_FREE)
			result.owned_object_count++;
	}
	if (func->ref_effect) {
		result.reads_memory = func->ref_effect->reads_memory;
		result.writes_memory = func->ref_effect->writes_memory;
		result.reads_global = func->ref_effect->reads_global;
		result.writes_global = func->ref_effect->writes_global;
		result.allocates = func->ref_effect->allocates;
		result.has_unknown_effect = func->ref_effect->has_unknown_effect;
	}
	return result;
}

bool ssa_ref_inst_observable(const SsaInst *inst) {
	if (!inst) return true;
	if (inst->IrNode == SSA_OP_ICALL) return true;
	if (inst->IrNode == SSA_OP_CALL)
		return !inst->ref_analyzed || inst->ref_observable;
	if (inst->IrNode == SSA_OP_STORE || inst->IrNode == SSA_OP_STORE8)
		return !inst->ref_analyzed || inst->ref_observable;
	return true;
}
