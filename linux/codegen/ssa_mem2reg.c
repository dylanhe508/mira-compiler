/* ssa_mem2reg.c 鈥?mem2reg / SSA 鏋勫缓 
 * 
 * 鎶婃垜浠墠闈骇鐢熺殑 ALLOCA, LOAD, STORE 鎸囦护杞崲涓虹湡姝ｇ殑 SSA 褰㈠紡銆?
 * 鍩烘湰鎬濊矾:
 * 1. 鎵惧埌鎵€鏈夎 ALLOCA 鍒嗛厤鐨?鍙橀噺"銆?
 * 2. 鎵弿瀵规瘡涓彉閲忕殑 STORE 鍙戠敓鎵€鍦ㄧ殑 BasicBlock銆?
 * 3. 浣跨敤鏀厤杈圭晫 DF 杩唬璁＄畻搴旇鍦ㄥ摢浜涘潡鎻掑叆 PHI 鑺傜偣銆?
 * 4. 鍙橀噺閲嶅懡鍚?(Renaming): 娣卞害浼樺厛閬嶅巻鏀厤鏍戯紝缁存姢姣忎釜鍙橀噺鐨勫綋鍓嶅€?褰撳墠璧嬪€肩殑 VReg)锛?
 *    鐒跺悗鍦?LOAD 澶勭洿鎺ュ紩鐢ㄣ€?
 * 5. Phi Destruction: 灏?Phi 鑺傜偣杞负鍓嶉┍鍧楀熬閮ㄧ殑 COPY 鎸囦护銆?
 */
#include "ir_ssa.h"
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

void ssa_compute_dom_info(SsaFunction *func);

/* ========== 鏁版嵁缁撴瀯 ========== */

typedef struct {
	VReg alloc_reg;        /* 鍘熷 ALLOCA 鐨勭洰鏍囧瘎瀛樺櫒 */
	int  phi_var_tag;      /* 鏍囪瘑绗︼紝鐢ㄤ簬 Phi 鑺傜偣璇嗗埆 */
	SsaBasicBlock **def_blocks;
	int def_count;
	int def_cap;
} AllocaVar;

/* 鍙橀噺閲嶅懡鍚嶇敤鐨勬爤 */
#define RENAME_STACK_MAX 256
typedef struct {
	VReg values[RENAME_STACK_MAX];
	int  depth;
} RenameStack;

static void add_def_block(AllocaVar *v, SsaBasicBlock *b) {
	for(int i=0; i<v->def_count; i++) {
		if(v->def_blocks[i] == b) return;
	}
	if(v->def_count >= v->def_cap) {
		v->def_cap = v->def_cap ? v->def_cap*2 : 4;
		v->def_blocks = realloc(v->def_blocks, v->def_cap * sizeof(SsaBasicBlock*));
	}
	v->def_blocks[v->def_count++] = b;
}

/* ========== Phase 1: 鏀堕泦 ALLOCA 鍙橀噺 ========== */

static AllocaVar *collect_allocas(SsaFunction *func, int *out_count) {
	AllocaVar *vars = NULL;
	int count = 0, cap = 0;

	for(int i=0; i<func->block_count; i++) {
		SsaBasicBlock *b = func->blocks[i];
		for(SsaInst *inst = b->inst_head; inst; inst = inst->next) {
			if(inst->IrNode == SSA_OP_ALLOCA) {
				if(count >= cap) {
					cap = cap ? cap*2 : 16;
					vars = realloc(vars, cap * sizeof(AllocaVar));
				}
				vars[count].alloc_reg = inst->dst;
				vars[count].phi_var_tag = count; 
				vars[count].def_blocks = NULL;
				vars[count].def_count = 0;
				vars[count].def_cap = 0;
				count++;
			}
		}
	}
	*out_count = count;
	return vars;
}

/* 鎵惧埌鍙橀噺绱㈠紩 */
static int find_var_idx(AllocaVar *vars, int var_count, VReg alloc_reg) {
	for(int v=0; v<var_count; v++) {
		if(vars[v].alloc_reg == alloc_reg) return v;
	}
	return -1;
}

/* ========== Phase 2: 鏀堕泦 STORE 鐨勫畾涔夊潡 ========== */

static void collect_stores(SsaFunction *func, AllocaVar *vars, int var_count) {
	for(int i=0; i<func->block_count; i++) {
		SsaBasicBlock *b = func->blocks[i];
		for(SsaInst *inst = b->inst_head; inst; inst = inst->next) {
			if(inst->IrNode == SSA_OP_STORE) {
				VReg ptr = inst->op2.u.vreg;
				int v = find_var_idx(vars, var_count, ptr);
				if(v >= 0) add_def_block(&vars[v], b);
			}
		}
	}
}

/* ========== Phase 3: 鎻掑叆 Phi 鑺傜偣 ========== */

static void insert_phis(SsaFunction *func, AllocaVar *vars, int var_count) {
	int n = func->block_count;
	bool *in_w = malloc(n * sizeof(bool));
	bool *has_phi = malloc(n * sizeof(bool));
	SsaBasicBlock **W = malloc(n * sizeof(SsaBasicBlock*));

	for(int v=0; v<var_count; v++) {
		int w_count = 0;
		for(int k=0; k<n; k++) { in_w[k] = false; has_phi[k] = false; }
		
		for(int i=0; i<vars[v].def_count; i++) {
			SsaBasicBlock *d = vars[v].def_blocks[i];
			W[w_count++] = d;
			in_w[d->id] = true;
		}

		while(w_count > 0) {
			SsaBasicBlock *x = W[--w_count];
			in_w[x->id] = false;

			for(int i=0; i<x->df_count; i++) {
				SsaBasicBlock *y = x->df[i];
				if(!has_phi[y->id]) {
					/* 鍦ㄦ鎻掑叆 Phi */
					SsaInst *phi = calloc(1, sizeof(SsaInst));
					phi->IrNode = SSA_OP_PHI;
					phi->type = SSA_TYPE_INT;
					phi->dst = ssa_new_vreg(func, phi->type);
					phi->parent = y;
					
					/* 棰勫垎閰嶆搷浣滄暟绌洪棿: 姣忎釜鍓嶉┍ 2 涓?(vreg, block) */
					phi->operand_cap = y->pred_count * 2;
					phi->operands = calloc(phi->operand_cap, sizeof(SsaOperand));
					phi->operand_count = 0;
					
					/* 鐢?op2.u.imm 瀛樺偍鍙橀噺绱㈠紩 (var_tag) */
					phi->op2.kind = SSA_OPND_IMM;
					phi->op2.u.imm = v;

					/* 鎻掑叆鍒?basic block 澶撮儴 */
					if(!y->inst_head) {
						y->inst_head = y->inst_tail = phi;
					} else {
						phi->next = y->inst_head;
						y->inst_head->prev = phi;
						y->inst_head = phi;
					}

					has_phi[y->id] = true;
					if(!in_w[y->id]) {
						in_w[y->id] = true;
						W[w_count++] = y;
					}
				}
			}
		}
	}

	free(in_w);
	free(has_phi);
	free(W);
}

/* ========== Phase 4: 鍙橀噺閲嶅懡鍚?(Renaming) ========== */

static void rename_block(SsaFunction *func, SsaBasicBlock *b,
                         RenameStack *stacks, int var_count, AllocaVar *vars) {
	/* 淇濆瓨褰撳墠鏍堟繁搴︼紝鐢ㄤ簬閫€鍑烘椂鍥炴函 */
	int *saved_depths = malloc(var_count * sizeof(int));
	for(int i=0; i<var_count; i++) saved_depths[i] = stacks[i].depth;

	/* (a) 澶勭悊褰撳墠鍧楀唴鎸囦护 */
	SsaInst *curr = b->inst_head;
	while(curr) {
		SsaInst *next_inst = curr->next;
		
		if(curr->IrNode == SSA_OP_PHI) {
			int v_idx = (int)curr->op2.u.imm;
			/* Phi 鐨?dst 鏄璇ュ彉閲忕殑鏂板畾涔?*/
			if(v_idx >= 0 && v_idx < var_count && stacks[v_idx].depth < RENAME_STACK_MAX) {
				stacks[v_idx].values[stacks[v_idx].depth++] = curr->dst;
			}
		}
		else if(curr->IrNode == SSA_OP_LOAD) {
			VReg ptr = curr->op1.u.vreg;
			int v_idx = find_var_idx(vars, var_count, ptr);
			if(v_idx >= 0 && stacks[v_idx].depth > 0) {
				/* 鏇挎崲 LOAD 涓?COPY锛岀洿鎺ュ紩鐢ㄥ綋鍓嶆椿璺冪殑 VReg */
				VReg val = stacks[v_idx].values[stacks[v_idx].depth - 1];
				curr->IrNode = SSA_OP_COPY;
				curr->op1.kind = SSA_OPND_VREG;
				curr->op1.u.vreg = val;
				curr->operand_count = 1;
			}
		}
		else if(curr->IrNode == SSA_OP_STORE) {
			VReg val = curr->op1.u.vreg; /* 鍊?*/
			VReg ptr = curr->op2.u.vreg; /* 鍦板潃 */
			int v_idx = find_var_idx(vars, var_count, ptr);
			if(v_idx >= 0) {
				/* 鍘嬪叆鏂板畾涔?*/
				if(stacks[v_idx].depth < RENAME_STACK_MAX) {
					stacks[v_idx].values[stacks[v_idx].depth++] = val;
				}
				/* 鍒犻櫎 STORE 鎸囦护 */
				if(curr->prev) curr->prev->next = curr->next;
				else b->inst_head = curr->next;
				if(curr->next) curr->next->prev = curr->prev;
				else b->inst_tail = curr->prev;
			}
		}
		/* 鍏朵粬鎸囦护淇濇寔涓嶅姩 */
		
		curr = next_inst;
	}

	/* (b) 濉厖鍚庣户鍧?Phi 鑺傜偣鐨勬搷浣滄暟 */
	for(int s=0; s<b->succ_count; s++) {
		SsaBasicBlock *succ = b->succs[s];
		for(SsaInst *phi = succ->inst_head; phi && phi->IrNode == SSA_OP_PHI; phi = phi->next) {
			int v_idx = (int)phi->op2.u.imm;
			if(v_idx >= 0 && v_idx < var_count && stacks[v_idx].depth > 0) {
				VReg val = stacks[v_idx].values[stacks[v_idx].depth - 1];
				/* 杩藉姞 (vreg, block) 瀵?*/
				int oc = phi->operand_count;
				if(oc + 2 <= phi->operand_cap) {
					phi->operands[oc].kind = SSA_OPND_VREG;
					phi->operands[oc].u.vreg = val;
					phi->operands[oc+1].kind = SSA_OPND_BLOCK;
					phi->operands[oc+1].u.block = b;
					phi->operand_count = oc + 2;
				}
			}
		}
	}

	/* (c) 閫掑綊杩涘叆鏀厤鏍戝瓙鑺傜偣 */
	for(int i=0; i<b->dom_child_count; i++) {
		rename_block(func, b->dom_children[i], stacks, var_count, vars);
	}

	/* (d) 鎭㈠鏍堟繁搴?*/
	for(int i=0; i<var_count; i++) stacks[i].depth = saved_depths[i];
	free(saved_depths);
}

/* ========== Phase 5: 鍒犻櫎 ALLOCA 鎸囦护 ========== */

static void remove_allocas(SsaFunction *func, AllocaVar *vars, int var_count) {
	for(int i=0; i<func->block_count; i++) {
		SsaBasicBlock *b = func->blocks[i];
		SsaInst *curr = b->inst_head;
		while(curr) {
			SsaInst *next_inst = curr->next;
			if(curr->IrNode == SSA_OP_ALLOCA) {
				int v = find_var_idx(vars, var_count, curr->dst);
				if(v >= 0) {
					/* 浠庨摼琛ㄤ腑鍒犻櫎杩欐潯 ALLOCA */
					if(curr->prev) curr->prev->next = curr->next;
					else b->inst_head = curr->next;
					if(curr->next) curr->next->prev = curr->prev;
					else b->inst_tail = curr->prev;
				}
			}
			curr = next_inst;
		}
	}
}

/* ========== Phase 6: Phi Destruction (Phi 鈫?COPY) ========== */

static void destroy_phis(SsaFunction *func) {
	for(int i=0; i<func->block_count; i++) {
		SsaBasicBlock *b = func->blocks[i];
			SsaInst *curr = b->inst_head;
			int phi_count = 0;
			while(curr && curr->IrNode == SSA_OP_PHI) { phi_count++; curr = curr->next; }
			if(phi_count == 0) continue;
			if (!ssa_phi_prefix_is_valid(func, b)) continue;
			
			SsaInst **phis = malloc(phi_count * sizeof(SsaInst*));
			curr = b->inst_head;
			for(int k=0; k<phi_count; k++) { phis[k] = curr; curr = curr->next; }
			
			/* 瀵规瘡涓墠椹卞潡鍒嗗埆澶勭悊骞惰鎷疯礉 */
			for(int p=0; p<b->pred_count; p++) {
				SsaBasicBlock *pred = b->preds[p];
				VReg *tmps = calloc(phi_count, sizeof(VReg));
				
				/* Pass 1: tmp = COPY src */
				for(int k=0; k<phi_count; k++) {
					SsaInst *phi = phis[k];
					VReg src = 0;
					for(int j=0; j+1 < phi->operand_count; j+=2) {
						if(phi->operands[j+1].u.block == pred) {
							src = phi->operands[j].u.vreg; break;
						}
					}
					if(src > 0) {
						VReg tmp = func->next_vreg++;
						if (func->next_vreg > func->vreg_defs_cap) {
							func->vreg_defs_cap = func->next_vreg * 2;
							func->vreg_defs = realloc(func->vreg_defs, func->vreg_defs_cap * sizeof(SsaInst*));
						}
						tmps[k] = tmp;
						
						SsaInst *copy = calloc(1, sizeof(SsaInst));
						copy->IrNode = SSA_OP_COPY;
						copy->type = SSA_TYPE_INT;
						copy->dst = tmp;
						copy->op1.kind = SSA_OPND_VREG;
						copy->op1.u.vreg = src;
						copy->operand_count = 1;
						copy->parent = pred;
						
						SsaInst *term = pred->inst_tail;
						if(term && (term->IrNode == SSA_OP_JMP || term->IrNode == SSA_OP_BR || term->IrNode == SSA_OP_RET)) {
							copy->next = term; copy->prev = term->prev;
							if(term->prev) term->prev->next = copy; else pred->inst_head = copy;
							term->prev = copy;
						} else {
							if(!pred->inst_tail) pred->inst_head = pred->inst_tail = copy;
							else { pred->inst_tail->next = copy; copy->prev = pred->inst_tail; pred->inst_tail = copy; }
						}
						func->vreg_defs[tmp] = copy;
					}
				}
				
				/* Pass 2: dst = COPY tmp */
				for(int k=0; k<phi_count; k++) {
					if(tmps[k] > 0) {
						SsaInst *copy = calloc(1, sizeof(SsaInst));
						copy->IrNode = SSA_OP_COPY;
						copy->type = SSA_TYPE_INT;
						copy->dst = phis[k]->dst;
						copy->op1.kind = SSA_OPND_VREG;
						copy->op1.u.vreg = tmps[k];
						copy->operand_count = 1;
						copy->parent = pred;
						
						SsaInst *term = pred->inst_tail;
						if(term && (term->IrNode == SSA_OP_JMP || term->IrNode == SSA_OP_BR || term->IrNode == SSA_OP_RET)) {
							copy->next = term; copy->prev = term->prev;
							if(term->prev) term->prev->next = copy; else pred->inst_head = copy;
							term->prev = copy;
						} else {
							if(!pred->inst_tail) pred->inst_head = pred->inst_tail = copy;
							else { pred->inst_tail->next = copy; copy->prev = pred->inst_tail; pred->inst_tail = copy; }
						}
						func->vreg_defs[copy->dst] = copy;
					}
				}
				free(tmps);
			}
			
			/* 鍒犻櫎鍧楀唴 Phi 鎸囦护 */
			for(int k=0; k<phi_count; k++) {
				SsaInst *phi = phis[k];
				if(phi->prev) phi->prev->next = phi->next; else b->inst_head = phi->next;
				if(phi->next) phi->next->prev = phi->prev; else b->inst_tail = phi->prev;
			}
			free(phis);
	}
}

void ssa_destroy_phis_module(SsaModule *mod) {
	if (!mod) return;
	for (int f = 0; f < mod->func_count; f++) {
		destroy_phis(mod->functions[f]);
	}
}

/* ========== 鍏ュ彛锛歋SA 鏋勫缓涓绘祦绋?========== */

void ssa_build(SsaModule *mod) {
	for(int f=0; f < mod->func_count; f++) {
		SsaFunction *func = mod->functions[f];
		
		/* Step 0: 璁＄畻鏀厤鏍戝拰鏀厤杈圭晫 */
		ssa_compute_dom_info(func);
		
		/* Step 1: 鏀堕泦 ALLOCA 鍙橀噺 */
		int var_count = 0;
		AllocaVar *vars = collect_allocas(func, &var_count);
		
		if (var_count > 0) {
			/* Step 2: 鏀堕泦 STORE 瀹氫箟鍧?*/
			collect_stores(func, vars, var_count);
			
			/* Step 3: 鎻掑叆 Phi 鑺傜偣 */
			insert_phis(func, vars, var_count);
			
			/* Step 4: 鍙橀噺閲嶅懡鍚?*/
			RenameStack *stacks = calloc(var_count, sizeof(RenameStack));
			for(int v=0; v<var_count; v++) {
				VReg init = ssa_new_vreg(func, SSA_TYPE_INT);
				stacks[v].values[0] = init;
				stacks[v].depth = 1;
			}
			if(func->entry_block) {
				rename_block(func, func->entry_block, stacks, var_count, vars);
			}
			free(stacks);
			
			/* Step 5: 鍒犻櫎 ALLOCA 鎸囦护骞舵竻鐞?*/
			remove_allocas(func, vars, var_count);
			
			for(int v=0; v<var_count; v++) {
				if(vars[v].def_blocks) free(vars[v].def_blocks);
			}
			free(vars);
		}

	}
}
