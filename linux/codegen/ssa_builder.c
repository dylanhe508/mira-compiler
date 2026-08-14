/* ssa_builder.c 閳?IR to CFG/SSA 鏉烆剚宕查崳?(TAC 瑜般垹绱? */
#include "ir_ssa.h"
#include "../mira.h"
#include <stdlib.h>
#include <string.h>

/* ========== 閸愬懎鐡ㄩ崚鍡涘帳鏉堝懎濮?========== */
void ssa_init_module(SsaModule *mod) {
	memset(mod, 0, sizeof(*mod));
	mod->functions = NULL;
	mod->func_count = 0;
	mod->func_cap = 0;
}

void ssa_free_module(SsaModule *mod) {
	ssa_function_index_free(mod);
	for(int i=0; i<mod->func_count; i++) {
		SsaFunction *f = mod->functions[i];
		for(int j=0; j<f->block_count; j++) {
			SsaBasicBlock *b = f->blocks[j];
			SsaInst *inst = b->inst_head;
			while(inst) {
				SsaInst *nxt = inst->next;
				if(inst->operands) free(inst->operands);
				free(inst);
				inst = nxt;
			}
			if(b->preds) free(b->preds);
			if(b->succs) free(b->succs);
			if(b->dom_children) free(b->dom_children);
			if(b->df) free(b->df);
			if(b->name) free(b->name);
			free(b);
		}
		if(f->blocks) free(f->blocks);
		if(f->params) free(f->params);
		if(f->param_types) free(f->param_types);
		if(f->vreg_defs) free(f->vreg_defs);
		if(f->vreg_phys_map) free(f->vreg_phys_map);
		if(f->vreg_float_phys_map) free(f->vreg_float_phys_map);
		if(f->vreg_spill_map) free(f->vreg_spill_map);
		if(f->vreg_vec_phys_map) free(f->vreg_vec_phys_map);
		if(f->var_reg_map) free(f->var_reg_map);
		if(f->var_remat_param) free(f->var_remat_param);
		if(f->var_remat_mul) free(f->var_remat_mul);
		if(f->var_remat_add) free(f->var_remat_add);
		if (f->loops) {
			for (int li = 0; li < f->loop_count; ++li) free(f->loops[li].members);
			free(f->loops);
		}
		if(f->ref_facts) free(f->ref_facts);
		if(f->ref_effect) free(f->ref_effect);
		if(f->name) free(f->name);
		free(f);
	}
	if(mod->functions) free(mod->functions);
}

SsaFunction *ssa_create_function(SsaModule *mod, const char *name, SsaType ret_type) {
	SsaFunction *func = calloc(1, sizeof(SsaFunction));
	func->name = strdup(name);
	func->return_type = ret_type;
	func->next_vreg = 1; // 0 閺勵垱妫ら弫鍫濈槑鐎涙ê娅?
	if(mod->func_count >= mod->func_cap) {
		mod->func_cap = mod->func_cap ? mod->func_cap * 2 : 16;
		mod->functions = realloc(mod->functions, mod->func_cap * sizeof(SsaFunction*));
	}
	mod->functions[mod->func_count++] = func;
	ssa_function_index_invalidate(mod);
	return func;
}

SsaBasicBlock *ssa_create_block(SsaFunction *func, const char *name) {
	SsaBasicBlock *b = calloc(1, sizeof(SsaBasicBlock));
	b->id = func->block_count;
	if (name) b->name = strdup(name);
	b->parent = func;
	
	if(func->block_count >= func->block_cap) {
		func->block_cap = func->block_cap ? func->block_cap * 2 : 16;
		func->blocks = realloc(func->blocks, func->block_cap * sizeof(SsaBasicBlock*));
	}
	func->blocks[func->block_count++] = b;
	if(!func->entry_block) func->entry_block = b;
	return b;
}

VReg ssa_new_vreg(SsaFunction *func, SsaType type) {
	VReg r = func->next_vreg++;
	if(r >= func->vreg_defs_cap) {
		int new_cap = func->vreg_defs_cap ? func->vreg_defs_cap * 2 : 64;
		while(new_cap <= r) new_cap *= 2;
		func->vreg_defs = realloc(func->vreg_defs, new_cap * sizeof(SsaInst*));
		for(int i = func->vreg_defs_cap; i < new_cap; i++) func->vreg_defs[i] = NULL;
		func->vreg_defs_cap = new_cap;
	}
	return r;
}

void ssa_add_edge(SsaBasicBlock *from, SsaBasicBlock *to) {
	if(from->succ_count >= from->succ_cap) {
		from->succ_cap = from->succ_cap ? from->succ_cap * 2 : 4;
		from->succs = realloc(from->succs, from->succ_cap * sizeof(SsaBasicBlock*));
	}
	from->succs[from->succ_count++] = to;
	
	if(to->pred_count >= to->pred_cap) {
		to->pred_cap = to->pred_cap ? to->pred_cap * 2 : 4;
		to->preds = realloc(to->preds, to->pred_cap * sizeof(SsaBasicBlock*));
	}
	to->preds[to->pred_count++] = from;
}

/* ========== 閹稿洣鎶ら崚娑樼紦 (Instruction Creation) ========== */

#include "stdlib_builtins.h"

static void apply_builtin_ownership(SsaInst *inst, const StdlibBuiltin *builtin) {
    if (!inst || !builtin || !builtin->owned_result) return;
    inst->needs_free = 1;
    inst->free_func_name = builtin->free_func_name;
}

static SsaInst *alloc_inst(SsaBasicBlock *b, SsaOpcode IrNode, SsaType type, VReg dst) {
	SsaInst *i = calloc(1, sizeof(SsaInst));
	i->IrNode = IrNode;
	i->type = type;
	i->dst = dst;
	i->parent = b;
	
	if(!b->inst_head) {
		b->inst_head = b->inst_tail = i;
	} else {
		b->inst_tail->next = i;
		i->prev = b->inst_tail;
		b->inst_tail = i;
	}
	
	if(dst > 0 && b->parent) {
		b->parent->vreg_defs[dst] = i;
	}
	return i;
}

SsaInst *ssa_emit_binop(SsaBasicBlock *b, SsaOpcode IrNode, SsaType type, VReg dst, SsaOperand left, SsaOperand right) {
	SsaInst *i = alloc_inst(b, IrNode, type, dst);
	i->op1 = left;
	i->op2 = right;
	i->operand_count = 2;
	return i;
}

SsaInst *ssa_emit_imm(SsaBasicBlock *b, VReg dst, int64_t val) {
	SsaInst *i = alloc_inst(b, SSA_OP_IMM, SSA_TYPE_INT, dst);
	i->op1.kind = SSA_OPND_IMM;
	i->op1.u.imm = val;
	i->operand_count = 1;
	return i;
}

SsaInst *ssa_emit_jmp(SsaBasicBlock *b, SsaBasicBlock *target) {
	SsaInst *i = alloc_inst(b, SSA_OP_JMP, SSA_TYPE_VOID, 0);
	i->op1.kind = SSA_OPND_BLOCK;
	i->op1.u.block = target;
	i->operand_count = 1;
	ssa_add_edge(b, target);
	return i;
}

SsaInst *ssa_emit_br(SsaBasicBlock *b, VReg cond, SsaBasicBlock *t_block, SsaBasicBlock *f_block) {
	SsaInst *i = alloc_inst(b, SSA_OP_BR, SSA_TYPE_VOID, 0);
	i->op1.kind = SSA_OPND_VREG;
	i->op1.u.vreg = cond;
	i->operand_count = 1;
	
	i->operand_cap = 2;
	i->operands = malloc(sizeof(SsaOperand) * 2);
	i->operands[0].kind = SSA_OPND_BLOCK;
	i->operands[0].u.block = t_block;
	i->operands[1].kind = SSA_OPND_BLOCK;
	i->operands[1].u.block = f_block;
	
	ssa_add_edge(b, t_block);
	ssa_add_edge(b, f_block);
	return i;
}

SsaInst *ssa_emit_alloc(SsaBasicBlock *b, VReg dst, int size_bytes) {
	SsaInst *i = alloc_inst(b, SSA_OP_ALLOCA, SSA_TYPE_PTR, dst);
	i->op1.kind = SSA_OPND_IMM;
	i->op1.u.imm = size_bytes;
	i->operand_count = 1;
	return i;
}

SsaInst *ssa_emit_load(SsaBasicBlock *b, SsaType type, VReg dst, VReg ptr) {
	SsaInst *i = alloc_inst(b, SSA_OP_LOAD, type, dst);
	i->op1.kind = SSA_OPND_VREG;
	i->op1.u.vreg = ptr;
	i->operand_count = 1;
	return i;
}

SsaInst *ssa_emit_store(SsaBasicBlock *b, VReg ptr, VReg val) {
	SsaInst *i = alloc_inst(b, SSA_OP_STORE, SSA_TYPE_VOID, 0);
	i->op1.kind = SSA_OPND_VREG;
	i->op1.u.vreg = val;
	i->op2.kind = SSA_OPND_VREG;
	i->op2.u.vreg = ptr;
	i->operand_count = 2;
	return i;
}

/* ========== Mira IR to CFG/TAC Conversion ========== */

typedef struct SsaBuilderCtx {
	SsaModule *mod;
	Program *prog;
	SsaFunction *cur_func;
	Def *cur_def;
	SsaBasicBlock *cur_block;

	/*
	 * IR 閺勵垱鐖ゅ?(Stack-based) 閻ㄥ嫸绱濋懓灞惧灉娴狀剝顩﹂悽鐔稿灇閾忔碍瀚欑€靛嫬鐡ㄩ崳?(Register-based) 閻?TAC閵?
	 * 鐢瓕顫夐惃鍕粵濞夋洘妲搁悽銊ょ娑擃亝鏆熺紒鍕躬缂傛牞鐦ч張鐔改侀幏鐔哥湴閸婂吋鐖ら敍灞界殺 `VReg` 閸樺鐖ら崪灞藉毉閺嶅牄鈧?
	 */
	VReg *vstack;
	int vstack_depth;
	int vstack_cap;

	/*
	 * 瀵逛簬琚娆¤祴鍊肩殑灞€閮ㄥ彉閲忥紝鍦ㄨ繘鍏?SSA 鏋勫缓锛坢em2reg锛変箣鍓嶏紝
	 * 閺堚偓缁犫偓閸楁洜娈戦崑姘《閺勵垱鐦℃稉顏勫綁闁插繐婀崙鑺ユ殶閸忋儱褰?ALLOCA 娑撯偓閸ф鐖ょ粚娲？閿?
	 * 閻掕泛鎮楃拠璇插晸鐏炩偓闁劌褰夐柌蹇曠倳鐠囨垳璐?LOAD 閸?STORE閵?
	 */
	VReg *local_vars; 
	int local_var_count;
	SsaType *var_types;
	int var_types_cap;

	/* 閻劋绨?break 閸?continue 閻ㄥ嫪绗傛稉瀣瀮 */
	SsaBasicBlock **loop_end_stack;
	SsaBasicBlock **loop_cont_stack;
	int loop_depth;
	int loop_cap;

	/* 瑜版挸澧犲锝呮躬缂傛牞鐦ч惃?Lambda閿涘瞼鏁ゆ禍?IR_WORD 鐟欙絾鐎借ぐ銏犲棘 */
	IrNode *current_lambda;
} SsaBuilderCtx;

extern int prog_add_var(Program *prog, const char *name, size_t len);

static void push_vreg(SsaBuilderCtx *ctx, VReg reg) {
	if(ctx->vstack_depth >= ctx->vstack_cap) {
		ctx->vstack_cap = ctx->vstack_cap ? ctx->vstack_cap * 2 : 128;
		ctx->vstack = realloc(ctx->vstack, ctx->vstack_cap * sizeof(VReg));
	}
	ctx->vstack[ctx->vstack_depth++] = reg;
}

static VReg pop_vreg(SsaBuilderCtx *ctx) {
	if(ctx->vstack_depth == 0) {
		fprintf(stderr, "error: SSA Builder: Stack underflow in func '%s' block '%s'\n", 
		        ctx->cur_func ? ctx->cur_func->name : "none", 
		        ctx->cur_block ? ctx->cur_block->name : "none");
		exit(1);
	}
	return ctx->vstack[--ctx->vstack_depth];
}

static SsaOperand make_vreg_opnd(VReg v) {
	SsaOperand opnd;
	opnd.kind = SSA_OPND_VREG;
	opnd.u.vreg = v;
	return opnd;
}

static SsaType infer_value_type(SsaFunction *f, VReg v);

static void build_op(SsaBuilderCtx *ctx, IrNode *o, IrNode *next);

static void build_ops(SsaBuilderCtx *ctx, IrNode *list) {
	for(IrNode *curr = list; curr; curr = curr->next) {
		build_op(ctx, curr, curr->next);
	}
}

static void build_op(SsaBuilderCtx *ctx, IrNode *o, IrNode *next) {
	if (!o) return;

	switch(o->kind) {
	case IR_BLOCK: {
		/* 独立出现的 `{ ... }` 块语句:展开执行内部语句链
		 * (parse_one 对 `{` 返回 IR_BLOCK,若无此分支则被静默丢弃) */
		build_ops(ctx, o->u.block);
		break;
	}
	case IR_INT: {
		VReg dst = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
		ssa_emit_imm(ctx->cur_block, dst, o->u.i);
		push_vreg(ctx, dst);
		break;
	}
	case IR_FLOAT: {
		VReg dst = ssa_new_vreg(ctx->cur_func, SSA_TYPE_FLOAT);
		SsaInst *i = alloc_inst(ctx->cur_block, SSA_OP_IMM, SSA_TYPE_FLOAT, dst);
		i->op1.kind = SSA_OPND_FIMM;
		i->op1.u.fimm = o->u.d;
		i->operand_count = 1;
		push_vreg(ctx, dst);
		break;
	}
	case IR_STR: {
		VReg dst = ssa_new_vreg(ctx->cur_func, SSA_TYPE_PTR);
		SsaInst *i = alloc_inst(ctx->cur_block, SSA_OP_IMM, SSA_TYPE_PTR, dst);
		i->op1.kind = SSA_OPND_STRING;
		i->op1.u.string.str = strdup(o->u.str.s);
		i->op1.u.string.len = o->u.str.len;
		i->operand_count = 1;
		push_vreg(ctx, dst);
		break;
	}
	case IR_CONST: {
		int slot = o->u.const_slot;
		ConstKind k = ctx->prog->const_kinds[slot];
		if (k == CONST_INT) {
			VReg dst = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
			ssa_emit_imm(ctx->cur_block, dst, ctx->prog->const_ints[slot]);
			push_vreg(ctx, dst);
		} else if (k == CONST_DOUBLE) {
			VReg dst = ssa_new_vreg(ctx->cur_func, SSA_TYPE_FLOAT);
			SsaInst *i = alloc_inst(ctx->cur_block, SSA_OP_IMM, SSA_TYPE_FLOAT, dst);
			i->op1.kind = SSA_OPND_FIMM;
			i->op1.u.fimm = ctx->prog->const_doubles[slot];
			i->operand_count = 1;
			push_vreg(ctx, dst);
		} else if (k == CONST_STR) {
			VReg dst = ssa_new_vreg(ctx->cur_func, SSA_TYPE_PTR);
			SsaInst *i = alloc_inst(ctx->cur_block, SSA_OP_IMM, SSA_TYPE_PTR, dst);
			i->op1.kind = SSA_OPND_STRING;
			i->op1.u.string.str = strdup(ctx->prog->const_strs[slot]);
			i->op1.u.string.len = ctx->prog->const_str_lens[slot];
			i->operand_count = 1;
			push_vreg(ctx, dst);
		}
		break;
	}
	case IR_VAR: {
		/*
		 * 閻╁瓨甯存担璺ㄦ暏 SSA_OP_LOAD_VAR / SSA_OP_STORE_VAR閿涘瞼绮潻?alloca+mem2reg閵?
		 * 鍙橀噺瀛樺偍鍦ㄥ叏灞€ mira_vars[slot] 鏁扮粍锛圧IP-relative 瀵诲潃锛夈€?
		 */
		int slot = o->u.var_slot;
		if (slot < 0 || slot >= ctx->local_var_count) {
			mira_error_simple(1, "SSA Builder: Variable slot %d out of bounds (%d)", slot, ctx->local_var_count);
		}

		if (next && next->kind == IR_WORD) {
			const char *n = next->u.word.name;
			size_t n_len = next->u.word.len;
			if (n_len == 1 && n[0] == '!') {
				/* store top-of-vstack into mira_vars[slot] */
				VReg val = pop_vreg(ctx);
				SsaType val_type = SSA_TYPE_INT;
				if (val > 0 && val < ctx->cur_func->next_vreg &&
				    ctx->cur_func->vreg_defs && ctx->cur_func->vreg_defs[val])
					val_type = ctx->cur_func->vreg_defs[val]->type;
				if (slot >= 0 && slot < ctx->var_types_cap)
					ctx->var_types[slot] = val_type;
				SsaInst *inst = alloc_inst(ctx->cur_block, SSA_OP_STORE_VAR, SSA_TYPE_VOID, 0);
				inst->op1.kind = SSA_OPND_VREG;
				inst->op1.u.vreg = val;
				inst->op2.kind = SSA_OPND_IMM;
				inst->op2.u.imm = slot;
				inst->operand_count = 2;
				o->next = next->next; /* consume the '!' token */
				return;
			} else if (n_len == 1 && n[0] == '@') {
				/* load mira_vars[slot] into new vreg */
				SsaType var_type = (slot >= 0 && slot < ctx->var_types_cap) ?
					ctx->var_types[slot] : SSA_TYPE_INT;
				VReg dst = ssa_new_vreg(ctx->cur_func, var_type);
				SsaInst *inst = alloc_inst(ctx->cur_block, SSA_OP_LOAD_VAR, var_type, dst);
				inst->op1.kind = SSA_OPND_IMM;
				inst->op1.u.imm = slot;
				inst->operand_count = 1;
				push_vreg(ctx, dst);
				o->next = next->next; /* consume the '@' token */
				return;
			}
		}
		
		/* 閺咁噣鈧艾褰夐柌蹇撳鏉炴枻绱欑粵澶夌幆娴?`var @`閿?*/
		SsaType var_type = (slot >= 0 && slot < ctx->var_types_cap) ?
			ctx->var_types[slot] : SSA_TYPE_INT;
		VReg dst = ssa_new_vreg(ctx->cur_func, var_type);
		SsaInst *inst = alloc_inst(ctx->cur_block, SSA_OP_LOAD_VAR, var_type, dst);
		inst->op1.kind = SSA_OPND_IMM;
		inst->op1.u.imm = slot;
		inst->operand_count = 1;
		push_vreg(ctx, dst);
		
		break;
	}
	case IR_WORD: {
		const char *n = o->u.word.name;
		size_t len = o->u.word.len;
		char sym[256];
		size_t slen = len < 255 ? len : 255;
		memcpy(sym, n, slen);
		sym[slen] = '\0';
		
		#define ISW(s) (strcmp(sym, s) == 0)

		if(ISW("true")) {
			VReg dst = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
			SsaInst *i = alloc_inst(ctx->cur_block, SSA_OP_IMM, SSA_TYPE_INT, dst);
			i->op1.kind = SSA_OPND_IMM; i->op1.u.imm = 1; i->operand_count = 1;
			push_vreg(ctx, dst);
		}
		else if(ISW("false")) {
			VReg dst = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
			SsaInst *i = alloc_inst(ctx->cur_block, SSA_OP_IMM, SSA_TYPE_INT, dst);
			i->op1.kind = SSA_OPND_IMM; i->op1.u.imm = 0; i->operand_count = 1;
			push_vreg(ctx, dst);
		}
		else if(ISW("+")) {
			VReg r = pop_vreg(ctx); VReg l = pop_vreg(ctx);
			/* Auto-detect float: if either operand is float, use FADD */
			SsaType lt = infer_value_type(ctx->cur_func, l);
			SsaType rt = infer_value_type(ctx->cur_func, r);
			int is_float = (lt == SSA_TYPE_FLOAT || rt == SSA_TYPE_FLOAT);
			VReg dst = ssa_new_vreg(ctx->cur_func, is_float ? SSA_TYPE_FLOAT : SSA_TYPE_INT);
			ssa_emit_binop(ctx->cur_block, is_float ? SSA_OP_FADD : SSA_OP_ADD, is_float ? SSA_TYPE_FLOAT : SSA_TYPE_INT, dst, make_vreg_opnd(l), make_vreg_opnd(r));
			push_vreg(ctx, dst);
		}
		else if(ISW("-")) {
			VReg r = pop_vreg(ctx); VReg l = pop_vreg(ctx);
			SsaType lt = infer_value_type(ctx->cur_func, l);
			SsaType rt = infer_value_type(ctx->cur_func, r);
			int is_float = (lt == SSA_TYPE_FLOAT || rt == SSA_TYPE_FLOAT);
			VReg dst = ssa_new_vreg(ctx->cur_func, is_float ? SSA_TYPE_FLOAT : SSA_TYPE_INT);
			ssa_emit_binop(ctx->cur_block, is_float ? SSA_OP_FSUB : SSA_OP_SUB, is_float ? SSA_TYPE_FLOAT : SSA_TYPE_INT, dst, make_vreg_opnd(l), make_vreg_opnd(r));
			push_vreg(ctx, dst);
		}
		else if(ISW("*")) {
			VReg r = pop_vreg(ctx); VReg l = pop_vreg(ctx);
			SsaType lt = infer_value_type(ctx->cur_func, l);
			SsaType rt = infer_value_type(ctx->cur_func, r);
			int is_float = (lt == SSA_TYPE_FLOAT || rt == SSA_TYPE_FLOAT);
			VReg dst = ssa_new_vreg(ctx->cur_func, is_float ? SSA_TYPE_FLOAT : SSA_TYPE_INT);
			ssa_emit_binop(ctx->cur_block, is_float ? SSA_OP_FMUL : SSA_OP_MUL, is_float ? SSA_TYPE_FLOAT : SSA_TYPE_INT, dst, make_vreg_opnd(l), make_vreg_opnd(r));
			push_vreg(ctx, dst);
		}
		else if(ISW("/")) {
			VReg r = pop_vreg(ctx); VReg l = pop_vreg(ctx);
			SsaType lt = infer_value_type(ctx->cur_func, l);
			SsaType rt = infer_value_type(ctx->cur_func, r);
			int is_float = (lt == SSA_TYPE_FLOAT || rt == SSA_TYPE_FLOAT);
			VReg dst = ssa_new_vreg(ctx->cur_func, is_float ? SSA_TYPE_FLOAT : SSA_TYPE_INT);
			ssa_emit_binop(ctx->cur_block, is_float ? SSA_OP_FDIV : SSA_OP_SDIV, is_float ? SSA_TYPE_FLOAT : SSA_TYPE_INT, dst, make_vreg_opnd(l), make_vreg_opnd(r));
			push_vreg(ctx, dst);
		}
		else if(ISW("%") || ISW("mod")) {
			VReg r = pop_vreg(ctx); VReg l = pop_vreg(ctx);
			VReg dst = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
			ssa_emit_binop(ctx->cur_block, SSA_OP_SREM, SSA_TYPE_INT, dst, make_vreg_opnd(l), make_vreg_opnd(r));
			push_vreg(ctx, dst);
		}
		else if(ISW("<<") || ISW(">>")) {
			VReg r = pop_vreg(ctx); VReg l = pop_vreg(ctx);
			VReg dst = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
			ssa_emit_binop(ctx->cur_block, ISW("<<") ? SSA_OP_SHL : SSA_OP_ASHR,
			               SSA_TYPE_INT, dst, make_vreg_opnd(l), make_vreg_opnd(r));
			push_vreg(ctx, dst);
		}
		else if(ISW("f+")) {
			VReg r = pop_vreg(ctx); VReg l = pop_vreg(ctx);
			VReg dst = ssa_new_vreg(ctx->cur_func, SSA_TYPE_FLOAT);
			ssa_emit_binop(ctx->cur_block, SSA_OP_FADD, SSA_TYPE_FLOAT, dst, make_vreg_opnd(l), make_vreg_opnd(r));
			push_vreg(ctx, dst);
		}
		else if(ISW("f-")) {
			VReg r = pop_vreg(ctx); VReg l = pop_vreg(ctx);
			VReg dst = ssa_new_vreg(ctx->cur_func, SSA_TYPE_FLOAT);
			ssa_emit_binop(ctx->cur_block, SSA_OP_FSUB, SSA_TYPE_FLOAT, dst, make_vreg_opnd(l), make_vreg_opnd(r));
			push_vreg(ctx, dst);
		}
		else if(ISW("f*")) {
			VReg r = pop_vreg(ctx); VReg l = pop_vreg(ctx);
			VReg dst = ssa_new_vreg(ctx->cur_func, SSA_TYPE_FLOAT);
			ssa_emit_binop(ctx->cur_block, SSA_OP_FMUL, SSA_TYPE_FLOAT, dst, make_vreg_opnd(l), make_vreg_opnd(r));
			push_vreg(ctx, dst);
		}
		else if(ISW("f/")) {
			VReg r = pop_vreg(ctx); VReg l = pop_vreg(ctx);
			VReg dst = ssa_new_vreg(ctx->cur_func, SSA_TYPE_FLOAT);
			ssa_emit_binop(ctx->cur_block, SSA_OP_FDIV, SSA_TYPE_FLOAT, dst, make_vreg_opnd(l), make_vreg_opnd(r));
			push_vreg(ctx, dst);
		}
		else if(ISW("f=")) {
			VReg r = pop_vreg(ctx); VReg l = pop_vreg(ctx);
			VReg dst = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
			ssa_emit_binop(ctx->cur_block, SSA_OP_FCMP_EQ, SSA_TYPE_INT, dst, make_vreg_opnd(l), make_vreg_opnd(r));
			push_vreg(ctx, dst);
		}
		else if(ISW("f!=")) {
			VReg r = pop_vreg(ctx); VReg l = pop_vreg(ctx);
			VReg dst = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
			ssa_emit_binop(ctx->cur_block, SSA_OP_FCMP_NE, SSA_TYPE_INT, dst, make_vreg_opnd(l), make_vreg_opnd(r));
			push_vreg(ctx, dst);
		}
		else if(ISW("f<")) {
			VReg r = pop_vreg(ctx); VReg l = pop_vreg(ctx);
			VReg dst = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
			ssa_emit_binop(ctx->cur_block, SSA_OP_FCMP_LT, SSA_TYPE_INT, dst, make_vreg_opnd(l), make_vreg_opnd(r));
			push_vreg(ctx, dst);
		}
		else if(ISW("f<=")) {
			VReg r = pop_vreg(ctx); VReg l = pop_vreg(ctx);
			VReg dst = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
			ssa_emit_binop(ctx->cur_block, SSA_OP_FCMP_LE, SSA_TYPE_INT, dst, make_vreg_opnd(l), make_vreg_opnd(r));
			push_vreg(ctx, dst);
		}
		else if(ISW("f>")) {
			VReg r = pop_vreg(ctx); VReg l = pop_vreg(ctx);
			VReg dst = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
			ssa_emit_binop(ctx->cur_block, SSA_OP_FCMP_GT, SSA_TYPE_INT, dst, make_vreg_opnd(l), make_vreg_opnd(r));
			push_vreg(ctx, dst);
		}
		else if(ISW("f>=")) {
			VReg r = pop_vreg(ctx); VReg l = pop_vreg(ctx);
			VReg dst = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
			ssa_emit_binop(ctx->cur_block, SSA_OP_FCMP_GE, SSA_TYPE_INT, dst, make_vreg_opnd(l), make_vreg_opnd(r));
			push_vreg(ctx, dst);
		}
		else if(ISW("i2f")) {
			VReg v = pop_vreg(ctx);
			VReg dst = ssa_new_vreg(ctx->cur_func, SSA_TYPE_FLOAT);
			SsaInst *i = alloc_inst(ctx->cur_block, SSA_OP_SITOFP, SSA_TYPE_FLOAT, dst);
			i->op1 = make_vreg_opnd(v); i->operand_count = 1;
			push_vreg(ctx, dst);
		}
		else if(ISW("f2i")) {
			VReg v = pop_vreg(ctx);
			VReg dst = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
			SsaInst *i = alloc_inst(ctx->cur_block, SSA_OP_FPTOSI, SSA_TYPE_INT, dst);
			i->op1 = make_vreg_opnd(v); i->operand_count = 1;
			push_vreg(ctx, dst);
		}
		else if(ISW("neg")) {
			VReg v = pop_vreg(ctx);
			VReg dst = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
			SsaInst *i = alloc_inst(ctx->cur_block, SSA_OP_NEG, SSA_TYPE_INT, dst);
			i->op1 = make_vreg_opnd(v);
			i->operand_count = 1;
			push_vreg(ctx, dst);
		}
		else if(ISW("@") || ISW("m64@")) {
			VReg ptr = pop_vreg(ctx);
			VReg dst = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
			SsaInst *i = alloc_inst(ctx->cur_block, SSA_OP_LOAD, SSA_TYPE_INT, dst);
			i->op1 = make_vreg_opnd(ptr);
			i->operand_count = 1;
			push_vreg(ctx, dst);
		}
		else if(ISW("!") || ISW("m64!")) {
			VReg ptr = pop_vreg(ctx);
			VReg val = pop_vreg(ctx);
			SsaInst *i = alloc_inst(ctx->cur_block, SSA_OP_STORE, SSA_TYPE_VOID, 0);
			i->op1 = make_vreg_opnd(val);
			i->op2 = make_vreg_opnd(ptr);
			i->operand_count = 2;
		}
		else if(ISW("c@") || ISW("m8@")) {
			VReg ptr = pop_vreg(ctx);
			VReg dst = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
			SsaInst *i = alloc_inst(ctx->cur_block, SSA_OP_LOAD8, SSA_TYPE_INT, dst);
			i->op1 = make_vreg_opnd(ptr);
			i->operand_count = 1;
			push_vreg(ctx, dst);
		}
		else if(ISW("c!") || ISW("m8!")) {
			VReg ptr = pop_vreg(ctx);
			VReg val = pop_vreg(ctx);
			SsaInst *i = alloc_inst(ctx->cur_block, SSA_OP_STORE8, SSA_TYPE_VOID, 0);
			i->op1 = make_vreg_opnd(val);
			i->op2 = make_vreg_opnd(ptr);
			i->operand_count = 2;
		}
		else if(ISW("<") || ISW("lt")) {
			VReg r = pop_vreg(ctx); VReg l = pop_vreg(ctx);
			VReg dst = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
			ssa_emit_binop(ctx->cur_block, SSA_OP_CMP_LT, SSA_TYPE_INT, dst, make_vreg_opnd(l), make_vreg_opnd(r));
			push_vreg(ctx, dst);
		}
		else if(ISW(">") || ISW("gt")) {
			VReg r = pop_vreg(ctx); VReg l = pop_vreg(ctx);
			VReg dst = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
			ssa_emit_binop(ctx->cur_block, SSA_OP_CMP_GT, SSA_TYPE_INT, dst, make_vreg_opnd(l), make_vreg_opnd(r));
			push_vreg(ctx, dst);
		}
		else if(ISW(">=") || ISW("ge")) {
			VReg r = pop_vreg(ctx); VReg l = pop_vreg(ctx);
			VReg dst = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
			ssa_emit_binop(ctx->cur_block, SSA_OP_CMP_GE, SSA_TYPE_INT, dst, make_vreg_opnd(l), make_vreg_opnd(r));
			push_vreg(ctx, dst);
		}
		else if(ISW("<=") || ISW("le")) {
			VReg r = pop_vreg(ctx); VReg l = pop_vreg(ctx);
			VReg dst = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
			ssa_emit_binop(ctx->cur_block, SSA_OP_CMP_LE, SSA_TYPE_INT, dst, make_vreg_opnd(l), make_vreg_opnd(r));
			push_vreg(ctx, dst);
		}
		else if(ISW("=") || ISW("==") || ISW("eq")) {
			VReg r = pop_vreg(ctx); VReg l = pop_vreg(ctx);
			VReg dst = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
			ssa_emit_binop(ctx->cur_block, SSA_OP_CMP_EQ, SSA_TYPE_INT, dst, make_vreg_opnd(l), make_vreg_opnd(r));
			push_vreg(ctx, dst);
		}
		else if(ISW("!=") || ISW("ne")) {
			VReg r = pop_vreg(ctx); VReg l = pop_vreg(ctx);
			VReg dst = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
			ssa_emit_binop(ctx->cur_block, SSA_OP_CMP_NE, SSA_TYPE_INT, dst, make_vreg_opnd(l), make_vreg_opnd(r));
			push_vreg(ctx, dst);
		}
		else if(ISW("and") || ISW("&")) {
			VReg r = pop_vreg(ctx); VReg l = pop_vreg(ctx);
			VReg dst = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
			ssa_emit_binop(ctx->cur_block, SSA_OP_AND, SSA_TYPE_INT, dst, make_vreg_opnd(l), make_vreg_opnd(r));
			push_vreg(ctx, dst);
		}
		else if(ISW("or") || ISW("|")) {
			VReg r = pop_vreg(ctx); VReg l = pop_vreg(ctx);
			VReg dst = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
			ssa_emit_binop(ctx->cur_block, SSA_OP_OR, SSA_TYPE_INT, dst, make_vreg_opnd(l), make_vreg_opnd(r));
			push_vreg(ctx, dst);
		}
		else if(ISW("xor") || ISW("^")) {
			VReg r = pop_vreg(ctx); VReg l = pop_vreg(ctx);
			VReg dst = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
			ssa_emit_binop(ctx->cur_block, SSA_OP_XOR, SSA_TYPE_INT, dst, make_vreg_opnd(l), make_vreg_opnd(r));
			push_vreg(ctx, dst);
		}
		else if(ISW("not")) {
			VReg v = pop_vreg(ctx);
			VReg dst = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
			SsaInst *i = alloc_inst(ctx->cur_block, SSA_OP_NOT, SSA_TYPE_INT, dst);
			i->op1 = make_vreg_opnd(v);
			i->operand_count = 1;
			push_vreg(ctx, dst);
		}
		else if(ISW("break")) {
			if (ctx->loop_depth > 0 && ctx->loop_end_stack[ctx->loop_depth - 1]) {
				ssa_emit_jmp(ctx->cur_block, ctx->loop_end_stack[ctx->loop_depth - 1]);
				SsaBasicBlock *dead = ssa_create_block(ctx->cur_func, "dead_after_break");
				ctx->cur_block = dead;
			}
		}
		else if(ISW("continue")) {
			if (ctx->loop_depth > 0 && ctx->loop_cont_stack[ctx->loop_depth - 1]) {
				ssa_emit_jmp(ctx->cur_block, ctx->loop_cont_stack[ctx->loop_depth - 1]);
				SsaBasicBlock *dead = ssa_create_block(ctx->cur_func, "dead_after_cont");
				ctx->cur_block = dead;
			}
		}
		else if(ISW("return")) {
			// Basic return handling 
			if (ctx->cur_func->return_type != SSA_TYPE_VOID && ctx->vstack_depth > 0) {
				VReg ret_val = pop_vreg(ctx);
				SsaInst *i = alloc_inst(ctx->cur_block, SSA_OP_RET, SSA_TYPE_VOID, 0);
				i->op1 = make_vreg_opnd(ret_val);
				i->operand_count = 1;
			} else {
				alloc_inst(ctx->cur_block, SSA_OP_RET, SSA_TYPE_VOID, 0);
			}
			SsaBasicBlock *dead = ssa_create_block(ctx->cur_func, "dead_after_ret");
			ctx->cur_block = dead;
		}
		else if(ISW("catch")) {
			// Drop error from stack
			if(ctx->vstack_depth > 0) pop_vreg(ctx);
		}
		else if(ISW("dup")) {
			if(ctx->vstack_depth > 0) {
				VReg top = ctx->vstack[ctx->vstack_depth - 1];
				push_vreg(ctx, top);
			}
		}
		else if(ISW("drop")) {
			if(ctx->vstack_depth > 0) pop_vreg(ctx);
		}
		else if(ISW("swap")) {
			if(ctx->vstack_depth >= 2) {
				VReg a = pop_vreg(ctx);
				VReg b = pop_vreg(ctx);
				push_vreg(ctx, a);
				push_vreg(ctx, b);
			}
		}
		else if(ISW("over")) {
			if(ctx->vstack_depth >= 2) {
				VReg second = ctx->vstack[ctx->vstack_depth - 2];
				push_vreg(ctx, second);
			}
		}
		else if(ISW("nip")) {
			if(ctx->vstack_depth >= 2) {
				VReg top = pop_vreg(ctx);
				pop_vreg(ctx); /* drop second */
				push_vreg(ctx, top);
			}
		}
		else if(ISW("rot")) {
			if(ctx->vstack_depth >= 3) {
				VReg c = pop_vreg(ctx);
				VReg b = pop_vreg(ctx);
				VReg a = pop_vreg(ctx);
				push_vreg(ctx, b);
				push_vreg(ctx, c);
				push_vreg(ctx, a);
			}
		}
		else if(ISW("print")) {
			VReg v = pop_vreg(ctx);
			SsaType t = SSA_TYPE_INT; // default TOS_INT=0
			if (v > 0 && v < ctx->cur_func->next_vreg && ctx->cur_func->vreg_defs && ctx->cur_func->vreg_defs[v]) {
				t = ctx->cur_func->vreg_defs[v]->type;
			}
			int tos_type = (t == SSA_TYPE_PTR) ? 1 : ((t == SSA_TYPE_FLOAT) ? 2 : 0);
			
			SsaInst *i = alloc_inst(ctx->cur_block, SSA_OP_CALL, SSA_TYPE_VOID, 0);
			i->operand_cap = 3; // sym, arg1 (type), arg2 (value)
			i->operands = malloc(sizeof(SsaOperand) * 3);
			i->operands[0].kind = SSA_OPND_SYM;
			i->operands[0].u.sym = strdup("mira_print");
			i->operands[1].kind = SSA_OPND_IMM;
			i->operands[1].u.imm = tos_type; 
			i->operands[2].kind = SSA_OPND_VREG;
			i->operands[2].u.vreg = v;
			i->operand_count = 3;
		}
		else if(sym[0] == 'e' && sym[1] == 'x' && sym[2] == 'e' && sym[3] == 'c' && sym[4] >= '0' && sym[4] <= '4' && sym[5] == '\0') {
			int nparams = sym[4] - '0';
			VReg dst2 = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
			SsaInst *ci = alloc_inst(ctx->cur_block, SSA_OP_ICALL, SSA_TYPE_INT, dst2);
			ci->operand_cap = nparams + 1;
			ci->operands = malloc(sizeof(SsaOperand) * ci->operand_cap);
			
			/* First pop is the function pointer */
			VReg fptr = pop_vreg(ctx);
			ci->operands[0] = make_vreg_opnd(fptr);
			ci->operand_count = 1;
			
			/* Now pop arguments in reverse order */
			if (nparams > 0) {
				VReg *args = malloc(sizeof(VReg) * nparams);
				for (int a = 0; a < nparams; a++) {
					args[a] = pop_vreg(ctx);
				}
				for (int a = nparams - 1; a >= 0; a--) {
					ci->operands[ci->operand_count++] = make_vreg_opnd(args[a]);
				}
				free(args);
			}
			push_vreg(ctx, dst2);
		}
		else {
			/* 妫€鏌ユ槸鍚︽槸褰撳墠鍑芥暟鐨勫弬鏁?*/
			int pidx = -1;
			if (ctx->cur_def) {
				for (int j = 0; j < ctx->cur_def->param_count; j++) {
					if (ctx->cur_def->param_lens[j] == slen && memcmp(ctx->cur_def->params[j], sym, slen) == 0) {
						pidx = j; break;
					}
				}
			}

			if (pidx >= 0) {
				/* `param @` 与 IR_VAR 一致：@ 是 fetch（取参数值），吸收之，
				   而不是当成指针 deref */
				if (next && next->kind == IR_WORD && next->u.word.len == 1 &&
				    next->u.word.name[0] == '@')
					o->next = next->next; /* consume the '@' token */
				VReg dst = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
				SsaInst *i = alloc_inst(ctx->cur_block, SSA_OP_LOAD_PARAM, SSA_TYPE_INT, dst);
				i->op1.kind = SSA_OPND_IMM;
				i->op1.u.imm = pidx;
				i->operand_count = 1;
				push_vreg(ctx, dst);
				break; /* done with IR_WORD */
			}

			bool found = false;
			const StdlibBuiltin *bw = stdlib_legacy_builtin_lookup(sym, strlen(sym));
			if (bw) {
					
					VReg dst = 0;
					if (bw->result_count) {
						dst = ssa_new_vreg(ctx->cur_func, bw->legacy_result_type);
					}
					
					SsaInst *i = alloc_inst(ctx->cur_block, SSA_OP_CALL, bw->result_count ? bw->legacy_result_type : SSA_TYPE_VOID, dst);
					i->operand_cap = bw->arity + 1; // sym + args
					i->operands = malloc(sizeof(SsaOperand) * i->operand_cap);
					
					i->operands[0].kind = SSA_OPND_SYM;
					i->operands[0].u.sym = strdup(bw->legacy_runtime_symbol);
					i->operand_count = 1;
					
					// Pop arguments in reverse order (stack semantics)
					if (bw->arity > 0) {
						VReg *args = malloc(sizeof(VReg) * bw->arity);
						for (int a = 0; a < bw->arity; a++) {
							args[a] = pop_vreg(ctx);
						}
						// First pop is lIR argument
						for (int a = bw->arity - 1; a >= 0; a--) {
							i->operands[i->operand_count++] = make_vreg_opnd(args[a]);
						}
						free(args);
					}
					
					if (bw->result_count) {
						push_vreg(ctx, dst);

						/* === 闈欐€佸紩鐢ㄦ墍鏈夋潈鏍囪 (Static Reference Ownership Tagging) === */
						/* 濡傛灉杩欎釜 builtin 浜х敓鍫嗗唴瀛樻寚閽堬紝鏍囪鑷姩閲婃斁 */
						if (strcmp(bw->legacy_runtime_symbol, "mem_alloc") == 0) {
							i->needs_free = 1;
							i->free_func_name = "mem_free";
						} else if (strcmp(bw->legacy_runtime_symbol, "mira_list_new") == 0 ||
								   strcmp(bw->legacy_runtime_symbol, "mira_list_push") == 0) {
							i->needs_free = 1;
							i->free_func_name = "mira_list_free";
						} else if (strcmp(bw->legacy_runtime_symbol, "mira_str_concat") == 0 ||
								   strcmp(bw->legacy_runtime_symbol, "mira_to_str") == 0 ||
								   strcmp(bw->legacy_runtime_symbol, "mira_str_substr") == 0) {
							i->needs_free = 1;
							i->free_func_name = "mem_free";
						}
					}
					found = true;
				break;
			}
			
			/* === 鍔ㄦ€佹敞鍐岃〃鏌ヨ锛坉ll-map JSON锛?=== */
			if (!found) {
				// 妫ｆ牕鍘涘Λ鈧弻銉︽Ц閸氾附妲?Lambda 閻ㄥ嫬寮弫?
				bool is_lambda_param = false;
				int param_idx = -1;
				if (ctx->current_lambda) {
					char **pnames = ctx->current_lambda->u.lambda.params;
					size_t *plens = ctx->current_lambda->u.lambda.param_lens;
					int pcount = ctx->current_lambda->u.lambda.param_count;
				for (int p = 0; p < pcount; p++) {
					if (plens[p] == slen && memcmp(pnames[p], n, slen) == 0) {
							is_lambda_param = true;
							param_idx = p;
							break;
						}
					}
				} else {
					/* not inside a lambda */
				}

				if (is_lambda_param) {
					/* `param @` 同 word 参数：吸收 @（fetch 语义） */
					if (next && next->kind == IR_WORD && next->u.word.len == 1 &&
					    next->u.word.name[0] == '@')
						o->next = next->next; /* consume the '@' token */
					VReg dst = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
					SsaInst *i = alloc_inst(ctx->cur_block, SSA_OP_LOAD_PARAM, SSA_TYPE_INT, dst);
					i->op1.kind = SSA_OPND_IMM;
					i->op1.u.imm = param_idx;
					i->operand_count = 1;
					push_vreg(ctx, dst);
				} else {
					// 閺屻儲澹橀悽銊﹀煕閼奉亜鐣炬稊澶婂毐閺佸府绱濋懢宄板絿閸欏倹鏆熸稉顏呮殶楠炶泛鍤弽?
					int nparams = 0;
					bool is_ext = false;
					bool found_def = false;
					for (Def *dd = ctx->prog->defs; dd; dd = dd->next) {
						if (dd->name_len == slen && memcmp(dd->name, n, slen) == 0) {
							nparams = dd->param_count;
							is_ext = dd->is_extern;
							found_def = true;
							break;
						}
					}
					
					/* Check if word matches a global variable slot (could be a lambda pointer) */
					int var_slot = -1;
					for (int v = 0; v < ctx->prog->var_count; v++) {
						if (ctx->prog->var_lens[v] == slen && memcmp(ctx->prog->var_names[v], n, slen) == 0) {
							var_slot = v;
							break;
						}
					}
					
					if (!found_def && var_slot >= 0) {
						/* Variable load: simply push to stack! */
						VReg fptr = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
						SsaInst *load_i = alloc_inst(ctx->cur_block, SSA_OP_LOAD_VAR, SSA_TYPE_INT, fptr);
						load_i->op1.kind = SSA_OPND_IMM;
						load_i->op1.u.imm = var_slot;
						load_i->operand_count = 1;
						push_vreg(ctx, fptr);
					} else {
						/* 参数猜测启发式仅用于未登记定义的 C word（参数表缺失的符号）：
						 * 用户函数 found_def 已给出确定参数个数，绝不猜测，
						 * 否则调用点栈上已有的值会被误当参数 pop 掉（SSA underflow ICE）。 */
						if (!found_def && nparams == 0 && ctx->vstack_depth >= 1 && !is_ext) {
							nparams = 1;
						}

						const StdlibBuiltin *runtime_builtin = is_ext ? stdlib_builtin_lookup(n, slen) : NULL;
						if (runtime_builtin && !stdlib_builtin_is_available(runtime_builtin)) {
							fprintf(stderr, "error: builtin '%.*s' is unavailable on this platform\n", (int)slen, n);
							exit(1);
						}
						if (runtime_builtin && nparams != runtime_builtin->arity) {
							fprintf(stderr, "error: builtin '%.*s' declares %d parameters; expected %d\n",
								(int)slen, n, nparams, runtime_builtin->arity);
							exit(1);
						}
						if (runtime_builtin) nparams = runtime_builtin->arity;

						SsaType call_type = runtime_builtin
							? (runtime_builtin->result_count ? runtime_builtin->result_type : SSA_TYPE_VOID)
							: SSA_TYPE_INT;
						if (!runtime_builtin) {
							for (int fi = 0; fi < ctx->mod->func_count; ++fi) {
								SsaFunction *known = ctx->mod->functions[fi];
								if (known && strcmp(known->name, sym) == 0) {
									call_type = known->return_type;
									break;
								}
							}
						}
						int has_result = runtime_builtin ? runtime_builtin->result_count : call_type != SSA_TYPE_VOID;
						VReg dst = has_result ? ssa_new_vreg(ctx->cur_func, call_type) : 0;
						SsaInst *i = alloc_inst(ctx->cur_block, SSA_OP_CALL, call_type, dst);
						i->operand_cap = nparams + 1;
						i->operands = malloc(sizeof(SsaOperand) * i->operand_cap);

						// Sanitization for standard Mira words calling C externals
						for(size_t j=0; j<slen; j++) if(sym[j] == '-') sym[j] = '_';

						i->operands[0].kind = SSA_OPND_SYM;
						i->operands[0].u.sym = strdup(runtime_builtin ? runtime_builtin->runtime_symbol : sym);
						i->operand_count = 1;

						// Pop arguments in reverse order (stack semantics)
						if (nparams > 0) {
							VReg *args = malloc(sizeof(VReg) * nparams);
							for (int a = 0; a < nparams; a++) args[a] = pop_vreg(ctx);
							for (int a = nparams - 1; a >= 0; a--)
								i->operands[i->operand_count++] = make_vreg_opnd(args[a]);
							free(args);
						}

						if (has_result) {
							apply_builtin_ownership(i, runtime_builtin);
							push_vreg(ctx, dst);
						}
					}
				}
			}
		}
		#undef ISW
		break;
	}
	case IR_IF: {
		/* C-style: cond ops are in o->u.iff.cond, execute them first */
		if (o->u.iff.cond) {
			build_ops(ctx, o->u.iff.cond);
		}
		VReg cond = pop_vreg(ctx);
		int entry_depth = ctx->vstack_depth;
		VReg entry_stack[1024];
		memcpy(entry_stack, ctx->vstack, entry_depth * sizeof(VReg));
		SsaBasicBlock *then_b = ssa_create_block(ctx->cur_func, "if_then");
		SsaBasicBlock *else_b = o->u.iff.else_b ? ssa_create_block(ctx->cur_func, "if_else") : NULL;
		SsaBasicBlock *end_b = ssa_create_block(ctx->cur_func, "if_end");
		
		ssa_emit_br(ctx->cur_block, cond, then_b, else_b ? else_b : end_b);
		
		ctx->cur_block = then_b;
		if (o->u.iff.then_b && o->u.iff.then_b->kind == IR_BLOCK)
			build_ops(ctx, o->u.iff.then_b->u.block);
		else if (o->u.iff.then_b)
			build_ops(ctx, o->u.iff.then_b);
		int then_depth = ctx->vstack_depth;
		VReg then_stack[1024];
		memcpy(then_stack, ctx->vstack, then_depth * sizeof(VReg));
		SsaBasicBlock *then_exit = ctx->cur_block;
		/* `return` continues builder traversal in a deliberately unreachable
		 * dead block.  Such a block must not acquire a synthetic edge to the
		 * merge, otherwise terminated branches corrupt stack/PHI accounting. */
		bool then_live = then_exit->pred_count > 0;
		if (then_live) ssa_emit_jmp(then_exit, end_b);
		
		if (else_b) {
			/* Each branch starts with the same virtual stack.  Sharing the
			 * builder stack here used to make the else result overwrite the
			 * then result regardless of which edge executed. */
			ctx->vstack_depth = entry_depth;
			memcpy(ctx->vstack, entry_stack, entry_depth * sizeof(VReg));
			ctx->cur_block = else_b;
			if (o->u.iff.else_b->kind == IR_BLOCK)
				build_ops(ctx, o->u.iff.else_b->u.block);
			else
				build_ops(ctx, o->u.iff.else_b);
			int else_depth = ctx->vstack_depth;
			VReg else_stack[1024];
			memcpy(else_stack, ctx->vstack, else_depth * sizeof(VReg));
			SsaBasicBlock *else_exit = ctx->cur_block;
			bool else_live = else_exit->pred_count > 0;
			if (else_live) ssa_emit_jmp(else_exit, end_b);

			ctx->cur_block = end_b;
			ctx->vstack_depth = 0;
			if (then_live && else_live && then_depth != else_depth) {
				fprintf(stderr,
				        "error: SSA Builder: if branches leave different stack depths in func '%s'\n",
				        ctx->cur_func ? ctx->cur_func->name : "none");
				exit(1);
			}
			if (then_live && !else_live) {
				memcpy(ctx->vstack, then_stack, then_depth * sizeof(VReg));
				ctx->vstack_depth = then_depth;
			} else if (!then_live && else_live) {
				memcpy(ctx->vstack, else_stack, else_depth * sizeof(VReg));
				ctx->vstack_depth = else_depth;
			} else if (!then_live && !else_live) {
				memcpy(ctx->vstack, entry_stack, entry_depth * sizeof(VReg));
				ctx->vstack_depth = entry_depth;
			}
			for (int slot = 0; then_live && else_live && slot < then_depth; slot++) {
				if (then_stack[slot] == else_stack[slot]) {
					push_vreg(ctx, then_stack[slot]);
					continue;
				}
				VReg merged = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
				SsaInst *phi = alloc_inst(end_b, SSA_OP_PHI, SSA_TYPE_INT, merged);
				phi->operand_cap = 4;
				phi->operand_count = 4;
				phi->operands = calloc(4, sizeof(SsaOperand));
				phi->operands[0] = make_vreg_opnd(then_stack[slot]);
				phi->operands[1].kind = SSA_OPND_BLOCK;
				phi->operands[1].u.block = then_exit;
				phi->operands[2] = make_vreg_opnd(else_stack[slot]);
				phi->operands[3].kind = SSA_OPND_BLOCK;
				phi->operands[3].u.block = else_exit;
				push_vreg(ctx, merged);
			}
		} else {
			/* A one-armed if is a statement.  Values produced only on the
			 * taken edge cannot exist on the untaken edge, so they must not
			 * leak into the continuation's virtual stack. */
			ctx->vstack_depth = entry_depth;
			memcpy(ctx->vstack, entry_stack, entry_depth * sizeof(VReg));
			ctx->cur_block = end_b;
		}
		break;
	}
	case IR_WHILE_COND: {
		/* {cond} {body} while */
		SsaBasicBlock *cond_b = ssa_create_block(ctx->cur_func, "while_cond");
		SsaBasicBlock *body_b = ssa_create_block(ctx->cur_func, "while_body");
		SsaBasicBlock *end_b  = ssa_create_block(ctx->cur_func, "while_end");

		ssa_emit_jmp(ctx->cur_block, cond_b);

		/* cond block: execute condition expression, test result */
		ctx->cur_block = cond_b;
		if (o->u.while_cond.cond && o->u.while_cond.cond->kind == IR_BLOCK)
			build_ops(ctx, o->u.while_cond.cond->u.block);
		else if (o->u.while_cond.cond)
			build_ops(ctx, o->u.while_cond.cond);
		VReg cond_v = pop_vreg(ctx);
		ssa_emit_br(ctx->cur_block, cond_v, body_b, end_b);

		/* loop stack mapping: cond_b is continue target, end_b is break target */
		if (ctx->loop_depth < 32) {
			ctx->loop_cont_stack[ctx->loop_depth] = cond_b;
			ctx->loop_end_stack[ctx->loop_depth] = end_b;
			ctx->loop_depth++;
		}

		/* body block */
		ctx->cur_block = body_b;
		if (o->u.while_cond.body && o->u.while_cond.body->kind == IR_BLOCK)
			build_ops(ctx, o->u.while_cond.body->u.block);
		else if (o->u.while_cond.body)
			build_ops(ctx, o->u.while_cond.body);
		ssa_emit_jmp(ctx->cur_block, cond_b);

		if (ctx->loop_depth > 0) ctx->loop_depth--;
		/* 瀵邦亞骞嗙紒鎾存将 */
		ctx->cur_block = end_b;
		break;
	}
	case IR_WHILE_INF: {
		SsaBasicBlock *loop_b = ssa_create_block(ctx->cur_func, "while_inf");
		SsaBasicBlock *end_b = ssa_create_block(ctx->cur_func, "while_inf_end");
		
		if (ctx->loop_depth < 32) {
			ctx->loop_cont_stack[ctx->loop_depth] = loop_b;
			ctx->loop_end_stack[ctx->loop_depth] = end_b;
			ctx->loop_depth++;
		}
		
		ssa_emit_jmp(ctx->cur_block, loop_b);
		ctx->cur_block = loop_b;
		build_ops(ctx, o->u.while_inf.body->u.block);
		ssa_emit_jmp(ctx->cur_block, loop_b);
		
		if (ctx->loop_depth > 0) ctx->loop_depth--;
		
		ctx->cur_block = end_b;
		break;
	}
	case IR_FOR_CSTYLE: {
		/* 娣囶喖顦? 韫囧懘銆忛崗鍫滅矤瑜版挸澧犻崸?entry)鐠哄疇娴嗛崚?for_init閿涘苯鎯侀崚?CFG 閺傤參鎽肩€佃壈鍤?mem2reg 婢惰鲸鏅?*/
		SsaBasicBlock *init_b = ssa_create_block(ctx->cur_func, "for_init");
		ssa_emit_jmp(ctx->cur_block, init_b);  /* entry -> for_init */
		ctx->cur_block = init_b;
		if (o->u.for_cstyle.init) build_ops(ctx, o->u.for_cstyle.init->u.block);
		
		SsaBasicBlock *cond_b = ssa_create_block(ctx->cur_func, "for_cond");
		SsaBasicBlock *body_b = ssa_create_block(ctx->cur_func, "for_body");
		SsaBasicBlock *step_b = ssa_create_block(ctx->cur_func, "for_step");
		SsaBasicBlock *end_b  = ssa_create_block(ctx->cur_func, "for_end");

		ssa_emit_jmp(ctx->cur_block, cond_b); /* for_init -> for_cond */
		
		ctx->cur_block = cond_b;
		if (o->u.for_cstyle.cond) {
			build_ops(ctx, o->u.for_cstyle.cond->u.block);
			VReg cond_v = pop_vreg(ctx);
			ssa_emit_br(ctx->cur_block, cond_v, body_b, end_b);
		} else {
			ssa_emit_jmp(ctx->cur_block, body_b);
		}

		if (ctx->loop_depth < 32) {
			ctx->loop_cont_stack[ctx->loop_depth] = step_b;
			ctx->loop_end_stack[ctx->loop_depth] = end_b;
			ctx->loop_depth++;
		}

		ctx->cur_block = body_b;
		if (o->u.for_cstyle.body) build_ops(ctx, o->u.for_cstyle.body->u.block);
		ssa_emit_jmp(ctx->cur_block, step_b);
		
		if (ctx->loop_depth > 0) ctx->loop_depth--;

		ctx->cur_block = step_b;
		if (o->u.for_cstyle.step) build_ops(ctx, o->u.for_cstyle.step->u.block);
		ssa_emit_jmp(ctx->cur_block, cond_b);

		ctx->cur_block = end_b;
		break;
	}
	case IR_FOR_RANGE: {
		int i_slot = o->u.for_range.var_slot;
		if (i_slot >= ctx->local_var_count) ctx->local_var_count = i_slot + 1;
		
		/* i = start */
		VReg start_v = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
		{
			SsaInst *si = alloc_inst(ctx->cur_block, SSA_OP_IMM, SSA_TYPE_INT, start_v);
			si->op1.kind = SSA_OPND_IMM; si->op1.u.imm = o->u.for_range.start;
			si->operand_count = 1;
		}
		{
			SsaInst *si = alloc_inst(ctx->cur_block, SSA_OP_STORE_VAR, SSA_TYPE_VOID, 0);
			si->op1 = make_vreg_opnd(start_v);
			si->op2.kind = SSA_OPND_IMM; si->op2.u.imm = i_slot;
			si->operand_count = 2;
		}
		
		SsaBasicBlock *cond_b = ssa_create_block(ctx->cur_func, "forrange_cond");
		SsaBasicBlock *body_b = ssa_create_block(ctx->cur_func, "forrange_body");
		SsaBasicBlock *step_b = ssa_create_block(ctx->cur_func, "forrange_step");
		SsaBasicBlock *end_b  = ssa_create_block(ctx->cur_func, "forrange_end");
		
		ssa_emit_jmp(ctx->cur_block, cond_b);
		
		/* Cond */
		ctx->cur_block = cond_b;
		VReg cur_i = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
		{
			SsaInst *li = alloc_inst(ctx->cur_block, SSA_OP_LOAD_VAR, SSA_TYPE_INT, cur_i);
			li->op1.kind = SSA_OPND_IMM; li->op1.u.imm = i_slot;
			li->operand_count = 1;
		}
		VReg end_v = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
		{
			SsaInst *si = alloc_inst(ctx->cur_block, SSA_OP_IMM, SSA_TYPE_INT, end_v);
			si->op1.kind = SSA_OPND_IMM; si->op1.u.imm = o->u.for_range.end;
			si->operand_count = 1;
		}
		VReg cmp_v = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
		ssa_emit_binop(ctx->cur_block, SSA_OP_CMP_LT, SSA_TYPE_INT, cmp_v, make_vreg_opnd(cur_i), make_vreg_opnd(end_v));
		ssa_emit_br(ctx->cur_block, cmp_v, body_b, end_b);
		
		if (ctx->loop_depth < 32) {
			ctx->loop_cont_stack[ctx->loop_depth] = step_b;
			ctx->loop_end_stack[ctx->loop_depth] = end_b;
			ctx->loop_depth++;
		}
		
		/* Body */
		ctx->cur_block = body_b;
		if (o->u.for_range.body) {
			if (o->u.for_range.body->kind == IR_BLOCK) build_ops(ctx, o->u.for_range.body->u.block);
			else build_ops(ctx, o->u.for_range.body);
		}
		ssa_emit_jmp(ctx->cur_block, step_b);
		
		if (ctx->loop_depth > 0) ctx->loop_depth--;
		
		/* Step */
		ctx->cur_block = step_b;
		VReg i_stepped = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
		VReg cur_i2 = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
		{
			SsaInst *li = alloc_inst(ctx->cur_block, SSA_OP_LOAD_VAR, SSA_TYPE_INT, cur_i2);
			li->op1.kind = SSA_OPND_IMM; li->op1.u.imm = i_slot;
			li->operand_count = 1;
		}
		VReg one_v = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
		{
			SsaInst *oi = alloc_inst(ctx->cur_block, SSA_OP_IMM, SSA_TYPE_INT, one_v);
			oi->op1.kind = SSA_OPND_IMM; oi->op1.u.imm = 1;
			oi->operand_count = 1;
		}
		ssa_emit_binop(ctx->cur_block, SSA_OP_ADD, SSA_TYPE_INT, i_stepped, make_vreg_opnd(cur_i2), make_vreg_opnd(one_v));
		{
			SsaInst *si = alloc_inst(ctx->cur_block, SSA_OP_STORE_VAR, SSA_TYPE_VOID, 0);
			si->op1 = make_vreg_opnd(i_stepped);
			si->op2.kind = SSA_OPND_IMM; si->op2.u.imm = i_slot;
			si->operand_count = 2;
		}
		ssa_emit_jmp(ctx->cur_block, cond_b);
		
		ctx->cur_block = end_b;
		break;
	}
	case IR_SWITCH: {
		/* Build the switch expression and pop its value */
		if (o->u.switch_.value) build_ops(ctx, o->u.switch_.value);
		VReg switch_val = pop_vreg(ctx);
		SsaBasicBlock *end_b = ssa_create_block(ctx->cur_func, "switch_end");
		
		IrNode *c = o->u.switch_.cases;
		SsaBasicBlock *curr_cond_b = ctx->cur_block;
		
		while (c) {
			IrNode *pattern = c;
			IrNode *block = c->next;
			if (!block) break;
			
			SsaBasicBlock *eval_b = ssa_create_block(ctx->cur_func, "switch_case_eval");
			SsaBasicBlock *body_b = ssa_create_block(ctx->cur_func, "switch_case_body");
			SsaBasicBlock *next_cond_b = ssa_create_block(ctx->cur_func, "switch_next");
			
			ssa_emit_jmp(curr_cond_b, eval_b);
			ctx->cur_block = eval_b;
			
			/* Isolate pattern node 閳?don't let build_ops follow ->next into body */
			IrNode *saved_next = pattern->next;
			pattern->next = NULL;
			build_ops(ctx, pattern); 
			pattern->next = saved_next;
			VReg pat_v = pop_vreg(ctx);
			
			VReg cmp_res = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
			ssa_emit_binop(ctx->cur_block, SSA_OP_CMP_EQ, SSA_TYPE_INT, cmp_res, make_vreg_opnd(switch_val), make_vreg_opnd(pat_v));
			
			ssa_emit_br(ctx->cur_block, cmp_res, body_b, next_cond_b);
			
			ctx->cur_block = body_b;
			if (block->kind == IR_BLOCK) build_ops(ctx, block->u.block);
			else build_ops(ctx, block); 
			
			ssa_emit_jmp(ctx->cur_block, end_b);
			
			curr_cond_b = next_cond_b;
			c = block->next;
		}
		
		ctx->cur_block = curr_cond_b;
		if (o->u.switch_.default_block) {
			IrNode *def = o->u.switch_.default_block;
			if (def->kind == IR_BLOCK) build_ops(ctx, def->u.block);
			else build_ops(ctx, def);
		}
		ssa_emit_jmp(ctx->cur_block, end_b);
		
		ctx->cur_block = end_b;
		break;
	}
	case IR_TRY: {
		SsaBasicBlock *eval_b = ssa_create_block(ctx->cur_func, "try_eval");
		SsaBasicBlock *body_b = ssa_create_block(ctx->cur_func, "try_body");
		SsaBasicBlock *err_b  = ssa_create_block(ctx->cur_func, "try_error");
		SsaBasicBlock *end_b  = ssa_create_block(ctx->cur_func, "try_end");

		ssa_emit_jmp(ctx->cur_block, eval_b);
		ctx->cur_block = eval_b;

		VReg jmp_buf = ssa_new_vreg(ctx->cur_func, SSA_TYPE_PTR);
		{
			SsaInst *i = alloc_inst(ctx->cur_block, SSA_OP_CALL, SSA_TYPE_PTR, jmp_buf);
			i->operand_cap = 1; i->operands = malloc(sizeof(SsaOperand));
			i->operands[0].kind = SSA_OPND_SYM; i->operands[0].u.sym = strdup("mira_try_begin");
			i->operand_count = 1;
		}

		VReg setjmp_res = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
		{
			SsaInst *i = alloc_inst(ctx->cur_block, SSA_OP_CALL, SSA_TYPE_INT, setjmp_res);
			i->operand_cap = 2; i->operands = malloc(sizeof(SsaOperand) * 2);
			i->operands[0].kind = SSA_OPND_SYM; i->operands[0].u.sym = strdup("_setjmp");
			i->operands[1] = make_vreg_opnd(jmp_buf);
			i->operand_count = 2;
		}

		VReg cmp_res = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
		VReg zero_v = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
		{
			SsaInst *i = alloc_inst(ctx->cur_block, SSA_OP_IMM, SSA_TYPE_INT, zero_v);
			i->op1.kind = SSA_OPND_IMM; i->op1.u.imm = 0;
			i->operand_count = 1;
		}
		ssa_emit_binop(ctx->cur_block, SSA_OP_CMP_EQ, SSA_TYPE_INT, cmp_res, make_vreg_opnd(setjmp_res), make_vreg_opnd(zero_v));
		ssa_emit_br(ctx->cur_block, cmp_res, body_b, err_b);

		ctx->cur_block = body_b;
		IrNode *body_block = o->u.try_block.body;
		if (body_block) {
			if (body_block->kind == IR_BLOCK) build_ops(ctx, body_block->u.block);
			else build_ops(ctx, body_block);
		}
		
		{
			SsaInst *i = alloc_inst(ctx->cur_block, SSA_OP_CALL, SSA_TYPE_VOID, 0);
			i->operand_cap = 1; i->operands = malloc(sizeof(SsaOperand));
			i->operands[0].kind = SSA_OPND_SYM; i->operands[0].u.sym = strdup("mira_try_end");
			i->operand_count = 1;
		}
		
		if (!o->u.try_block.catch_body) {
			VReg succ_v = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
			SsaInst *i = alloc_inst(ctx->cur_block, SSA_OP_IMM, SSA_TYPE_INT, succ_v);
			i->op1.kind = SSA_OPND_IMM; i->op1.u.imm = 1;
			i->operand_count = 1;
			push_vreg(ctx, succ_v);
		}
		ssa_emit_jmp(ctx->cur_block, end_b);

		ctx->cur_block = err_b;
		VReg err_v = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
		{
			SsaInst *i = alloc_inst(ctx->cur_block, SSA_OP_CALL, SSA_TYPE_INT, err_v);
			i->operand_cap = 1; i->operands = malloc(sizeof(SsaOperand));
			i->operands[0].kind = SSA_OPND_SYM; i->operands[0].u.sym = strdup("mira_get_error");
			i->operand_count = 1;
		}
		if (o->u.try_block.catch_body) {
			if (o->u.try_block.error_slot >= 0) {
				SsaInst *store = alloc_inst(ctx->cur_block, SSA_OP_STORE_VAR,
					SSA_TYPE_VOID, 0);
				store->op1 = make_vreg_opnd(err_v);
				store->op2.kind = SSA_OPND_IMM;
				store->op2.u.imm = o->u.try_block.error_slot;
				store->operand_count = 2;
			}
			IrNode *catch_block = o->u.try_block.catch_body;
			if (catch_block->kind == IR_BLOCK) build_ops(ctx, catch_block->u.block);
			else build_ops(ctx, catch_block);
		} else {
			push_vreg(ctx, err_v);
			VReg fail_v = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
			SsaInst *i = alloc_inst(ctx->cur_block, SSA_OP_IMM, SSA_TYPE_INT, fail_v);
			i->op1.kind = SSA_OPND_IMM; i->op1.u.imm = 0;
			i->operand_count = 1;
			push_vreg(ctx, fail_v);
		}
		ssa_emit_jmp(ctx->cur_block, end_b);

		ctx->cur_block = end_b;
		break;
	}
	case IR_EACH: {
		/* each (list_expr) { body }
		 * Translates to:
		 *   list_ptr = eval(list_expr)
		 *   len = mira_list_len(list_ptr)
		 *   idx = 0
		 *   loop: if idx >= len goto end
		 *         elem = mira_list_get(list_ptr, idx)
		 *         push elem; body; idx++; goto loop
		 *   end:
		 */
		if (o->u.each.list) build_ops(ctx, o->u.each.list);
		VReg list_v = pop_vreg(ctx);
		
		/* Allocate hidden variable slots for loop state */
		int list_slot = ctx->prog->var_count++;
		int len_slot  = ctx->prog->var_count++;
		int idx_slot  = ctx->prog->var_count++;
		ctx->local_var_count = ctx->prog->var_count;
		
		/* Store list pointer */
		{
			SsaInst *si = alloc_inst(ctx->cur_block, SSA_OP_STORE_VAR, SSA_TYPE_VOID, 0);
			si->op1.kind = SSA_OPND_VREG; si->op1.u.vreg = list_v;
			si->op2.kind = SSA_OPND_IMM;  si->op2.u.imm  = list_slot;
			si->operand_count = 2;
		}
		
		/* len = mira_list_len(list_ptr) */
		VReg len_v = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
		{
			SsaInst *ci = alloc_inst(ctx->cur_block, SSA_OP_CALL, SSA_TYPE_INT, len_v);
			ci->operand_cap = 2; ci->operands = malloc(sizeof(SsaOperand) * 2);
			ci->operands[0].kind = SSA_OPND_SYM; ci->operands[0].u.sym = strdup("mira_list_len");
			ci->operands[1] = make_vreg_opnd(list_v);
			ci->operand_count = 2;
		}
		{
			SsaInst *si = alloc_inst(ctx->cur_block, SSA_OP_STORE_VAR, SSA_TYPE_VOID, 0);
			si->op1.kind = SSA_OPND_VREG; si->op1.u.vreg = len_v;
			si->op2.kind = SSA_OPND_IMM;  si->op2.u.imm  = len_slot;
			si->operand_count = 2;
		}
		
		/* idx = 0 */
		VReg zero_v = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
		{
			SsaInst *zi = alloc_inst(ctx->cur_block, SSA_OP_IMM, SSA_TYPE_INT, zero_v);
			zi->operand_cap = 1; zi->operands = malloc(sizeof(SsaOperand));
			zi->operands[0].kind = SSA_OPND_IMM; zi->operands[0].u.imm = 0;
			zi->operand_count = 1;
		}
		{
			SsaInst *si = alloc_inst(ctx->cur_block, SSA_OP_STORE_VAR, SSA_TYPE_VOID, 0);
			si->op1.kind = SSA_OPND_VREG; si->op1.u.vreg = zero_v;
			si->op2.kind = SSA_OPND_IMM;  si->op2.u.imm  = idx_slot;
			si->operand_count = 2;
		}
		
		/* Create loop blocks */
		SsaBasicBlock *cond_b = ssa_create_block(ctx->cur_func, "each_cond");
		SsaBasicBlock *body_b = ssa_create_block(ctx->cur_func, "each_body");
		SsaBasicBlock *end_b  = ssa_create_block(ctx->cur_func, "each_end");
		
		ssa_emit_jmp(ctx->cur_block, cond_b);
		
		/* Cond block: idx < len ? */
		ctx->cur_block = cond_b;
		VReg cur_idx = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
		{
			SsaInst *li = alloc_inst(ctx->cur_block, SSA_OP_LOAD_VAR, SSA_TYPE_INT, cur_idx);
			li->op1.kind = SSA_OPND_IMM; li->op1.u.imm = idx_slot;
			li->operand_count = 1;
		}
		VReg cur_len = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
		{
			SsaInst *li = alloc_inst(ctx->cur_block, SSA_OP_LOAD_VAR, SSA_TYPE_INT, cur_len);
			li->op1.kind = SSA_OPND_IMM; li->op1.u.imm = len_slot;
			li->operand_count = 1;
		}
		VReg cmp_v = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
		ssa_emit_binop(ctx->cur_block, SSA_OP_CMP_LT, SSA_TYPE_INT, cmp_v,
			make_vreg_opnd(cur_idx), make_vreg_opnd(cur_len));
		ssa_emit_br(ctx->cur_block, cmp_v, body_b, end_b);
		
		/* Body block: elem = list_get(list, idx); push elem; run body; idx++ */
		ctx->cur_block = body_b;
		VReg lp = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
		{
			SsaInst *li = alloc_inst(ctx->cur_block, SSA_OP_LOAD_VAR, SSA_TYPE_INT, lp);
			li->op1.kind = SSA_OPND_IMM; li->op1.u.imm = list_slot;
			li->operand_count = 1;
		}
		VReg bi = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
		{
			SsaInst *li = alloc_inst(ctx->cur_block, SSA_OP_LOAD_VAR, SSA_TYPE_INT, bi);
			li->op1.kind = SSA_OPND_IMM; li->op1.u.imm = idx_slot;
			li->operand_count = 1;
		}
		VReg elem_v = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
		{
			SsaInst *ci = alloc_inst(ctx->cur_block, SSA_OP_CALL, SSA_TYPE_INT, elem_v);
			ci->operand_cap = 3; ci->operands = malloc(sizeof(SsaOperand) * 3);
			ci->operands[0].kind = SSA_OPND_SYM; ci->operands[0].u.sym = strdup("mira_list_get");
			ci->operands[1] = make_vreg_opnd(lp);
			ci->operands[2] = make_vreg_opnd(bi);
			ci->operand_count = 3;
		}
		push_vreg(ctx, elem_v);
		
		if (ctx->loop_depth < 32) {
			ctx->loop_cont_stack[ctx->loop_depth] = cond_b;
			ctx->loop_end_stack[ctx->loop_depth] = end_b;
			ctx->loop_depth++;
		}

		/* Execute body */
		if (o->u.each.body) {
			if (o->u.each.body->kind == IR_BLOCK) build_ops(ctx, o->u.each.body->u.block);
			else build_ops(ctx, o->u.each.body);
		}
		
		if (ctx->loop_depth > 0) ctx->loop_depth--;
		
		/* If body didn't consume the element, pop it to keep stack balanced */
		/* (In practice, the body should use/consume the value) */
		
		/* idx++ */
		VReg old_idx = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
		{
			SsaInst *li = alloc_inst(ctx->cur_block, SSA_OP_LOAD_VAR, SSA_TYPE_INT, old_idx);
			li->op1.kind = SSA_OPND_IMM; li->op1.u.imm = idx_slot;
			li->operand_count = 1;
		}
		VReg one_v = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
		{
			SsaInst *oi = alloc_inst(ctx->cur_block, SSA_OP_IMM, SSA_TYPE_INT, one_v);
			oi->operand_cap = 1; oi->operands = malloc(sizeof(SsaOperand));
			oi->operands[0].kind = SSA_OPND_IMM; oi->operands[0].u.imm = 1;
			oi->operand_count = 1;
		}
		VReg new_idx = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
		ssa_emit_binop(ctx->cur_block, SSA_OP_ADD, SSA_TYPE_INT, new_idx,
			make_vreg_opnd(old_idx), make_vreg_opnd(one_v));
		{
			SsaInst *si = alloc_inst(ctx->cur_block, SSA_OP_STORE_VAR, SSA_TYPE_VOID, 0);
			si->op1.kind = SSA_OPND_VREG; si->op1.u.vreg = new_idx;
			si->op2.kind = SSA_OPND_IMM;  si->op2.u.imm  = idx_slot;
			si->operand_count = 2;
		}
		ssa_emit_jmp(ctx->cur_block, cond_b);
		
		ctx->cur_block = end_b;
		break;
	}
	case IR_LIST_LITERAL: {
		int size = o->u.list_literal.size;
		
		/* Call mira_list_new(size) */
		VReg size_v = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
		{
			SsaInst *si = alloc_inst(ctx->cur_block, SSA_OP_IMM, SSA_TYPE_INT, size_v);
			si->op1.kind = SSA_OPND_IMM; si->op1.u.imm = size;
			si->operand_count = 1;
		}
		VReg list_ptr = ssa_new_vreg(ctx->cur_func, SSA_TYPE_PTR);
		{
			SsaInst *ci = alloc_inst(ctx->cur_block, SSA_OP_CALL, SSA_TYPE_PTR, list_ptr);
			ci->operand_cap = 2; ci->operands = malloc(sizeof(SsaOperand) * 2);
			ci->operands[0].kind = SSA_OPND_SYM; ci->operands[0].u.sym = strdup("mira_list_new");
			ci->operands[1] = make_vreg_opnd(size_v);
			ci->operand_count = 2;
		}
		
		/* Build each element and call mira_list_set(list, index, value) */
		IrNode *elem = o->u.list_literal.elements;
		for (int idx = 0; elem && idx < size; idx++) {
			IrNode *saved = elem->next;
			elem->next = NULL;
			build_ops(ctx, elem);
			elem->next = saved;
			VReg val = pop_vreg(ctx);
			
			VReg idx_v = ssa_new_vreg(ctx->cur_func, SSA_TYPE_INT);
			{
				SsaInst *ii = alloc_inst(ctx->cur_block, SSA_OP_IMM, SSA_TYPE_INT, idx_v);
				ii->op1.kind = SSA_OPND_IMM; ii->op1.u.imm = idx;
				ii->operand_count = 1;
			}
			{
				SsaInst *ci = alloc_inst(ctx->cur_block, SSA_OP_CALL, SSA_TYPE_VOID, 0);
				ci->operand_cap = 4; ci->operands = malloc(sizeof(SsaOperand) * 4);
				ci->operands[0].kind = SSA_OPND_SYM; ci->operands[0].u.sym = strdup("mira_list_set");
				ci->operands[1] = make_vreg_opnd(list_ptr);
				ci->operands[2] = make_vreg_opnd(idx_v);
				ci->operands[3] = make_vreg_opnd(val);
				ci->operand_count = 4;
			}
			elem = saved;
		}
		
		/* Store list pointer to temp_slot and push to vstack */
		{
			SsaInst *si = alloc_inst(ctx->cur_block, SSA_OP_STORE_VAR, SSA_TYPE_VOID, 0);
			si->op1.kind = SSA_OPND_VREG; si->op1.u.vreg = list_ptr;
			si->op2.kind = SSA_OPND_IMM;  si->op2.u.imm  = o->u.list_literal.temp_slot;
			si->operand_count = 2;
		}
		push_vreg(ctx, list_ptr);
		break;
	}
	case IR_LAMBDA: {
		/* 1. Create a unique name for this lambda */
		static int lambda_id_counter = 0;
		char l_name[64];
		snprintf(l_name, sizeof(l_name), "__lambda_%d", lambda_id_counter++);
		
		/* 2. Save current Builder Context state */
		SsaFunction *saved_func = ctx->cur_func;
		SsaBasicBlock *saved_block = ctx->cur_block;
		IrNode *saved_lambda = ctx->current_lambda;
		int saved_vstack_depth = ctx->vstack_depth;
		
		/* 3. Create the new SsaFunction for the Lambda */
		SsaFunction *lam_func = ssa_create_function(ctx->mod, l_name, SSA_TYPE_INT);
		lam_func->param_count = o->u.lambda.param_count;
		
		ctx->cur_func = lam_func;
		ctx->cur_block = ssa_create_block(lam_func, "entry");
		ctx->current_lambda = o; /* Store so IR_WORD can find params */
		ctx->vstack_depth = 0;
		
		/* 4. Inside the Lambda function body, we just build ops.
		   The parameter resolution is handled dynamically by IR_WORD falling back 
		   to scanning ctx->current_lambda->params and emitting SSA_OP_LOAD_PARAM.
		*/
		
		build_ops(ctx, o->u.lambda.body);
		
		if (!ctx->cur_block->inst_tail || ctx->cur_block->inst_tail->IrNode != SSA_OP_RET) {
			if (ctx->vstack_depth > 0) {
				VReg ret_v = pop_vreg(ctx);
				SsaInst *i = alloc_inst(ctx->cur_block, SSA_OP_RET, SSA_TYPE_INT, 0);
				i->op1.kind = SSA_OPND_VREG;  i->op1.u.vreg = ret_v;
				i->operand_count = 1;
			} else {
				alloc_inst(ctx->cur_block, SSA_OP_RET, SSA_TYPE_VOID, 0);
			}
		}
		
		/* 5. Restore Builder Context */
		ctx->cur_func = saved_func;
		ctx->cur_block = saved_block;
		ctx->current_lambda = saved_lambda;
		ctx->vstack_depth = saved_vstack_depth;
		
		/* 6. Push Lambda function pointer onto current stack */
		VReg fptr = ssa_new_vreg(ctx->cur_func, SSA_TYPE_PTR);
		{
			SsaInst *si = alloc_inst(ctx->cur_block, SSA_OP_LEA_FUNC, SSA_TYPE_PTR, fptr);
			si->op1.kind = SSA_OPND_SYM;
			si->op1.u.sym = strdup(l_name);
			si->operand_count = 1;
		}
		push_vreg(ctx, fptr);
		break;
	}
	default:
		// Unsupported block or IrNode
		break;
	}
}

void ssa_build_function(SsaBuilderCtx *ctx, Def *d) {
	char sym[256];
	size_t slen = d->name_len < 255 ? d->name_len : 255;
	for (size_t i = 0; i < slen; i++) sym[i] = (d->name[i] == '-') ? '_' : d->name[i];
	sym[slen] = '\0';

	ctx->cur_func = ssa_create_function(ctx->mod,
		(slen == 4 && memcmp(sym, "main", 4) == 0) ? "mira_main" : sym,
		SSA_TYPE_INT);
	ctx->cur_func->param_count = d->param_count;
	ctx->cur_def = d;
	ctx->cur_block = ssa_create_block(ctx->cur_func, "entry");
	ctx->vstack_depth = 0;

	/* 
	 * 1. 閸?entry block 閻㈢喐鍨氶悽銊ょ艾鐏炩偓闁劌褰夐柌蹇曟畱 Alloca
	 * 閸ョ姳璐熼幋鎴滄粦闂団偓鐟曚礁鐨?IR 閸╄桨绨弽鍫濆綁闁插繗娴嗛幑顫礋 SSA閵?
	 * 閸︺劍顒濇稊瀣閸忓牏鏁?mem2reg 閸撳秴顨旈敍姘湰闁劌褰夐柌蹇庣稊娑撶儤瀵氶柦鍫濈摠閸?
	 */
	int var_count = ctx->prog->var_count; 
	// 濞夈劍鍓伴敍灞藉弿鐏炩偓閻?var_count 鐎甸€涚艾閸戣姤鏆熸担婊呮暏閸╃喎褰查懗钘変焊婢堆勫灗閸嬪繐鐨敍灞界杽闂勫懎绨茶ぐ鎾舵暏閸戣姤鏆熼崘鍛瀻閺嬫劘绻冮惃鍕担?
	/* 閸欐﹢鍣洪悳鏉挎躬閻╁瓨甯撮柅姘崇箖 mira_vars[slot] 鐠佸潡妫堕敍鍦玈A_OP_LOAD_VAR / STORE_VAR閿涘绱濇稉宥夋付鐟?alloca */
	ctx->local_vars = NULL; /* 娑撳秴鍟€娴ｈ法鏁?*/
	ctx->local_var_count = var_count;
	free(ctx->var_types);
	ctx->var_types_cap = var_count;
	ctx->var_types = var_count > 0 ? calloc((size_t)var_count, sizeof(SsaType)) : NULL;


	build_ops(ctx, d->body);
	
	// 婵″倹鐏夌紒鎾崇啲濞屸剝婀?RET 閹稿洣鎶ら敍灞筋杻閸旂姳绔存稉?
	if(!ctx->cur_block->inst_tail || ctx->cur_block->inst_tail->IrNode != SSA_OP_RET) {
		if (ctx->vstack_depth > 0) {
			VReg ret_v = pop_vreg(ctx);
			if (ret_v > 0 && ret_v < ctx->cur_func->next_vreg &&
			    ctx->cur_func->vreg_defs && ctx->cur_func->vreg_defs[ret_v])
				ctx->cur_func->return_type = ctx->cur_func->vreg_defs[ret_v]->type;
			SsaInst *i = alloc_inst(ctx->cur_block, SSA_OP_RET, SSA_TYPE_INT, 0);
			i->op1.kind = SSA_OPND_VREG;
			i->op1.u.vreg = ret_v;
			i->operand_count = 1;
		} else {
			alloc_inst(ctx->cur_block, SSA_OP_RET, SSA_TYPE_VOID, 0);
		}
	}
	free(ctx->var_types);
	ctx->var_types = NULL;
	ctx->var_types_cap = 0;
}

static SsaType infer_value_type(SsaFunction *f, VReg v) {
	if (!f || v == 0 || v >= f->next_vreg || !f->vreg_defs || !f->vreg_defs[v])
		return SSA_TYPE_INT;
	SsaInst *def = f->vreg_defs[v];
	if (def->type == SSA_TYPE_FLOAT) return SSA_TYPE_FLOAT;
	if (def->IrNode == SSA_OP_LOAD_VAR && def->op1.kind == SSA_OPND_IMM) {
		int slot = (int)def->op1.u.imm;
		for (int bi = 0; bi < f->block_count; ++bi)
			for (SsaInst *i = f->blocks[bi]->inst_head; i; i = i->next)
				if (i->IrNode == SSA_OP_STORE_VAR && i->op2.kind == SSA_OPND_IMM &&
				    (int)i->op2.u.imm == slot && i->op1.kind == SSA_OPND_VREG) {
					VReg sv = i->op1.u.vreg;
					if (sv > 0 && sv < f->next_vreg && f->vreg_defs[sv] &&
					    f->vreg_defs[sv]->type == SSA_TYPE_FLOAT)
						return SSA_TYPE_FLOAT;
				}
	}
	return def->type;
}

void ssa_build_program(Program *prog, SsaModule *out_mod) {
	SsaBuilderCtx ctx = {0};
	ctx.mod = out_mod;
	ctx.prog = prog;
	ctx.loop_cap = 32;
	ctx.loop_end_stack = malloc(sizeof(SsaBasicBlock *) * ctx.loop_cap);
	ctx.loop_cont_stack = malloc(sizeof(SsaBasicBlock *) * ctx.loop_cap);
	ctx.loop_depth = 0;
	
	for(Def *d = prog->defs; d; d = d->next) {
		if(!d->is_extern) {
			ssa_build_function(&ctx, d);
		}
	}

	/* Function definitions may be stored in reverse source order.  Resolve
	 * return types after every body exists, then repair direct call and print
	 * types from those signatures. */
	for (int fi = 0; fi < out_mod->func_count; ++fi) {
		SsaFunction *f = out_mod->functions[fi];
		for (int bi = 0; bi < f->block_count; ++bi) {
			for (SsaInst *i = f->blocks[bi]->inst_head; i; i = i->next) {
				if (i->IrNode != SSA_OP_RET || i->op1.kind != SSA_OPND_VREG) continue;
				VReg v = i->op1.u.vreg;
				f->return_type = infer_value_type(f, v);
			}
		}
	}
	ssa_function_index_rebuild(out_mod);
	for (int fi = 0; fi < out_mod->func_count; ++fi) {
		SsaFunction *f = out_mod->functions[fi];
		for (int bi = 0; bi < f->block_count; ++bi) {
			for (SsaInst *i = f->blocks[bi]->inst_head; i; i = i->next) {
				if (i->IrNode != SSA_OP_CALL || !i->operands || i->operand_count < 1 ||
				    i->operands[0].kind != SSA_OPND_SYM) continue;
				const char *callee = i->operands[0].u.sym;
				SsaFunction *cf = ssa_function_index_find(out_mod, callee);
				if (cf) i->type = cf->return_type;
				if (strcmp(callee, "mira_print") == 0 && i->operand_count >= 3 &&
				    i->operands[2].kind == SSA_OPND_VREG) {
					VReg v = i->operands[2].u.vreg;
					SsaType t = infer_value_type(f, v);
					i->operands[1].kind = SSA_OPND_IMM;
					i->operands[1].u.imm = (t == SSA_TYPE_PTR) ? 1 : (t == SSA_TYPE_FLOAT ? 2 : 0);
				}
			}
		}
	}
	
	/* Check if a user-defined 'main' Def was already compiled as 'mira_main' */
	bool has_user_main = false;
	for (Def *d = prog->defs; d; d = d->next) {
		if (!d->is_extern && d->name_len == 4 && memcmp(d->name, "main", 4) == 0) {
			has_user_main = true;
			break;
		}
	}
	
	if (prog->main_block && !has_user_main) {
		ctx.cur_func = ssa_create_function(ctx.mod, "mira_main", SSA_TYPE_INT);
		ctx.cur_block = ssa_create_block(ctx.cur_func, "entry");
		
		int var_count = prog->var_count;
		ctx.local_vars = NULL;
		ctx.local_var_count = var_count;
		ctx.vstack_depth = 0;

		build_ops(&ctx, prog->main_block);
		
		if(!ctx.cur_block->inst_tail || ctx.cur_block->inst_tail->IrNode != SSA_OP_RET) {
			alloc_inst(ctx.cur_block, SSA_OP_RET, SSA_TYPE_VOID, 0);
		}
	}
	ssa_function_index_rebuild(out_mod);

	/* The top-level main block is materialized after named definitions, so do
	 * one final call/print type repair over the complete module. */
	for (int fi = 0; fi < out_mod->func_count; ++fi) {
		SsaFunction *f = out_mod->functions[fi];
		for (int bi = 0; bi < f->block_count; ++bi) {
			for (SsaInst *i = f->blocks[bi]->inst_head; i; i = i->next) {
				if (i->IrNode != SSA_OP_CALL || !i->operands || i->operand_count < 1 ||
				    i->operands[0].kind != SSA_OPND_SYM) continue;
				const char *callee = i->operands[0].u.sym;
				SsaFunction *cf = ssa_function_index_find(out_mod, callee);
				if (cf) i->type = cf->return_type;
				if (strcmp(callee, "mira_print") == 0 && i->operand_count >= 3 &&
				    i->operands[2].kind == SSA_OPND_VREG) {
					SsaType t = infer_value_type(f, i->operands[2].u.vreg);
					i->operands[1].kind = SSA_OPND_IMM;
					i->operands[1].u.imm = (t == SSA_TYPE_PTR) ? 1 : (t == SSA_TYPE_FLOAT ? 2 : 0);
				}
			}
		}
	}
}

