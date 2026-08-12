/* ir_opt.c 鈥?IR 浼樺寲 passes
 *
 * 1. 甯搁噺鎶樺彔锛氳繛缁?push_imm + push_imm + IrNode 鈫?push_imm result
 * 2. 绐ュ瓟浼樺寲锛氭秷闄ゅ啑浣?push/pop 瀵圭瓑
 *
 * 鍦?encoder 涔嬪墠銆乮r_dump 涔嬪墠璋冪敤銆?
 */
#include "ir.h"
#include "decision.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ========== 甯搁噺鎶樺彔 ========== */
/*
 * 妯″紡锛?
 *   IR_MOV_MEM_IMM [r12+0], A     (push_imm A)
 *   IR_ADD_REG_IMM r12, 8
 *   IR_MOV_MEM_IMM [r12+0], B     (push_imm B)
 *   IR_ADD_REG_IMM r12, 8
 *   IR_SUB_REG_IMM r12, 8         (pop_rax)
 *   IR_MOV_REG_MEM rax, [r12]
 *   IR_MOV_REG_REG rcx, rax
 *   IR_SUB_REG_IMM r12, 8         (pop_rax)
 *   IR_MOV_REG_MEM rax, [r12]
 *   IR_ADD_REG_REG rax, rcx       (or sub/imul)
 *   IR_MOV_MEM_REG [r12], rax     (push_rax)
 *   IR_ADD_REG_IMM r12, 8
 *
 * 鎶樺彔涓猴細
 *   IR_MOV_MEM_IMM [r12+0], (A IrNode B)
 *   IR_ADD_REG_IMM r12, 8
 */

/* 妫€娴?push_imm 妯″紡: MOV_MEM_IMM [r12,0] + ADD_REG_IMM r12,8 */
static bool is_push_imm(IrInst *a, IrInst *b, int64_t *val) {
	if (a->IrNode == IR_MOV_MEM_IMM && a->dst == REG_R12 && a->imm == 0 &&
	    b->IrNode == IR_ADD_REG_IMM && b->dst == REG_R12 && b->imm == 8) {
		*val = a->extra_imm;
		return true;
	}
	return false;
}

/* 妫€娴?pop_rax 妯″紡: SUB_REG_IMM r12,8 + MOV_REG_MEM rax,[r12] */
static bool is_pop_rax(IrInst *a, IrInst *b) {
	return a->IrNode == IR_SUB_REG_IMM && a->dst == REG_R12 && a->imm == 8 &&
	       b->IrNode == IR_MOV_REG_MEM && b->dst == REG_RAX && b->src == REG_R12 && b->imm == 0;
}

/* 妫€娴?push_rax 妯″紡: MOV_MEM_REG [r12],rax + ADD_REG_IMM r12,8 */
static bool is_push_rax(IrInst *a, IrInst *b) {
	return a->IrNode == IR_MOV_MEM_REG && a->dst == REG_R12 && a->imm == 0 && a->src == REG_RAX &&
	       b->IrNode == IR_ADD_REG_IMM && b->dst == REG_R12 && b->imm == 8;
}

void ir_opt_constant_fold(IrBuffer *ir) {
	if (!ir || !ir->text || ir->text_count < 12) return;
	IrInst *t = ir->text;
	int n = ir->text_count;

	/* 鏍囪瑕佸垹闄ょ殑鎸囦护 */
	bool *del = (bool *)calloc(n, sizeof(bool));

	for (int i = 0; i + 11 < n; i++) {
		if (del[i]) continue;

		int64_t a_val, b_val;
		/* push_imm A (2 insts) */
		if (!is_push_imm(&t[i], &t[i+1], &a_val)) continue;
		/* push_imm B (2 insts) */
		if (!is_push_imm(&t[i+2], &t[i+3], &b_val)) continue;
		/* pop_rax (2 insts) */
		if (!is_pop_rax(&t[i+4], &t[i+5])) continue;
		/* mov rcx, rax */
		if (t[i+6].IrNode != IR_MOV_REG_REG || t[i+6].dst != REG_RCX || t[i+6].src != REG_RAX) continue;
		/* pop_rax (2 insts) */
		if (!is_pop_rax(&t[i+7], &t[i+8])) continue;

		/* 绠楁湳鎿嶄綔 */
		int64_t result;
		bool matched = false;
		if (t[i+9].IrNode == IR_ADD_REG_REG && t[i+9].dst == REG_RAX && t[i+9].src == REG_RCX) {
			result = a_val + b_val; matched = true;
		} else if (t[i+9].IrNode == IR_SUB_REG_REG && t[i+9].dst == REG_RAX && t[i+9].src == REG_RCX) {
			result = a_val - b_val; matched = true;
		} else if (t[i+9].IrNode == IR_IMUL_REG_REG && t[i+9].dst == REG_RAX && t[i+9].src == REG_RCX) {
			result = a_val * b_val; matched = true;
		}

		if (!matched) continue;
		/* push_rax (2 insts) */
		if (!is_push_rax(&t[i+10], &t[i+11])) continue;

		/* 折叠！替换为 push_imm result */
		t[i].extra_imm = result;   /* 淇濈暀 push_imm 浣嗘敼鍊?*/
		/* t[i+1] 淇濈暀 (add r12, 8) */
		/* 鍒犻櫎 i+2 鍒?i+11 */
		for (int j = i + 2; j <= i + 11; j++) del[j] = true;
	}

	/* 鍘嬬缉鍒犻櫎 */
	int w = 0;
	for (int r = 0; r < n; r++) {
		if (!del[r]) {
			if (w != r) t[w] = t[r];
			w++;
		}
	}
	ir->text_count = w;
	free(del);
}

/* ========== 绐ュ瓟浼樺寲 ========== */

void ir_opt_peephole(IrBuffer *ir) {
	if (!ir || !ir->text || ir->text_count < 4) return;
	IrInst *t = ir->text;
	int n = ir->text_count;

	bool *del = (bool *)calloc(n, sizeof(bool));

	for (int i = 0; i + 3 < n; i++) {
		if (del[i]) continue;

		/* 妯″紡1: push_rax 绱ц窡 pop_rax 鈫?鍒犻櫎涓ゅ */
		if (i + 3 < n &&
		    is_push_rax(&t[i], &t[i+1]) &&
		    is_pop_rax(&t[i+2], &t[i+3])) {
			del[i] = del[i+1] = del[i+2] = del[i+3] = true;
			continue;
		}

		/* 妯″紡2: push_imm X + pop_rax 鈫?mov rax, X */
		if (i + 3 < n) {
			int64_t val;
			if (is_push_imm(&t[i], &t[i+1], &val) &&
			    is_pop_rax(&t[i+2], &t[i+3])) {
				/* 鏇挎崲涓?mov rax, imm */
				t[i].IrNode = IR_MOV_REG_IMM;
				t[i].dst = REG_RAX;
				t[i].src = REG_NONE;
				t[i].imm = val;
				t[i].extra_imm = 0;
				del[i+1] = del[i+2] = del[i+3] = true;
				continue;
			}
		}

		/* 妯″紡3: add/sub reg, 0 鈫?鍒犻櫎 */
		if ((t[i].IrNode == IR_ADD_REG_IMM || t[i].IrNode == IR_SUB_REG_IMM) && t[i].imm == 0) {
			del[i] = true;
			continue;
		}

		/* 妯″紡4: mov reg, reg (same) 鈫?鍒犻櫎 */
		if (t[i].IrNode == IR_MOV_REG_REG && t[i].dst == t[i].src) {
			del[i] = true;
			continue;
		}

		/* 妯″紡5: add r12, N 绱ц窡 sub r12, N 鈫?鍒犻櫎涓ゆ潯 */
		if (i + 1 < n &&
		    t[i].IrNode == IR_ADD_REG_IMM && t[i].dst == REG_R12 &&
		    t[i+1].IrNode == IR_SUB_REG_IMM && t[i+1].dst == REG_R12 &&
		    t[i].imm == t[i+1].imm) {
			del[i] = del[i+1] = true;
			continue;
		}

		/* 妯″紡6: sub r12, N 绱ц窡 add r12, N 鈫?鍒犻櫎涓ゆ潯 */
		if (i + 1 < n &&
		    t[i].IrNode == IR_SUB_REG_IMM && t[i].dst == REG_R12 &&
		    t[i+1].IrNode == IR_ADD_REG_IMM && t[i+1].dst == REG_R12 &&
		    t[i].imm == t[i+1].imm) {
			del[i] = del[i+1] = true;
			continue;
		}

		/* 妯″紡7: push_imm 0 浼樺寲 鈥?宸茬Щ闄わ紙鍘熷疄鐜版湁 bug锛屼細鐮村潖 IR 鐘舵€侊級 */
	}

	/* 涔熸鏌ユ渶鍚庡嚑鏉?*/
	for (int i = n - 1; i >= 0; i--) {
		if (del[i]) continue;
		if ((t[i].IrNode == IR_ADD_REG_IMM || t[i].IrNode == IR_SUB_REG_IMM) && t[i].imm == 0)
			del[i] = true;
		if (t[i].IrNode == IR_MOV_REG_REG && t[i].dst == t[i].src)
			del[i] = true;
	}

	/* 鍘嬬缉 */
	int w = 0;
	for (int r = 0; r < n; r++) {
		if (!del[r]) {
			if (w != r) t[w] = t[r];
			w++;
		}
	}
	ir->text_count = w;
	free(del);
}

/* ========== 寮哄害鍓婂噺 ========== */
/* 灏嗕箻浠?鐨勫箓杞崲涓哄乏绉伙紝涔樹互0鈫抶or锛屼箻浠?鈫掑垹闄?*/

static int is_power_of_2(int64_t v) {
	if (v <= 0) return -1;
	int shift = 0;
	while (v > 1) {
		if (v & 1) return -1;
		v >>= 1;
		shift++;
	}
	return shift;
}

void ir_opt_strength_reduce(IrBuffer *ir) {
	if (!ir || !ir->text || ir->text_count < 12) return;
	IrInst *t = ir->text;
	int n = ir->text_count;

	bool *del = (bool *)calloc(n, sizeof(bool));

	for (int i = 0; i + 11 < n; i++) {
		if (del[i]) continue;

		/* 鏌ユ壘: push_imm A, push_imm B, pop, mov rcx=rax, pop, imul rax*rcx, push */
		int64_t a_val, b_val;
		if (!is_push_imm(&t[i], &t[i+1], &a_val)) continue;
		if (!is_push_imm(&t[i+2], &t[i+3], &b_val)) continue;
		if (!is_pop_rax(&t[i+4], &t[i+5])) continue;
		if (t[i+6].IrNode != IR_MOV_REG_REG || t[i+6].dst != REG_RCX || t[i+6].src != REG_RAX) continue;
		if (!is_pop_rax(&t[i+7], &t[i+8])) continue;
		if (t[i+9].IrNode != IR_IMUL_REG_REG || t[i+9].dst != REG_RAX || t[i+9].src != REG_RCX) continue;
		if (!is_push_rax(&t[i+10], &t[i+11])) continue;

		/* a_val * b_val: 濡傛灉鍏朵腑涓€涓槸2鐨勫箓锛屽彲浠ヤ紭鍖?*/
		int shift = is_power_of_2(b_val);
		if (shift > 0) {
			/* 鏇挎崲涓?push_imm(a_val) + pop + shl rax, shift + push */
			/* 绠€鍖栵細鐩存帴鎶樺彔涓?push_imm(a_val << shift) */
			t[i].extra_imm = a_val << shift;
			for (int j = i + 2; j <= i + 11; j++) del[j] = true;
			continue;
		}
		shift = is_power_of_2(a_val);
		if (shift > 0) {
			t[i].extra_imm = b_val << shift;
			for (int j = i + 2; j <= i + 11; j++) del[j] = true;
			continue;
		}
	}

	/* 鍗曠嫭妫€鏌? imul rax, rcx 涓斾箣鍓嶆湁 mov rcx, imm 鈫?鍙互鍋氬己搴﹀墛鍑?*/
	for (int i = 0; i < n; i++) {
		if (del[i]) continue;
		if (t[i].IrNode != IR_IMUL_REG_REG || t[i].dst != REG_RAX || t[i].src != REG_RCX) continue;

		/* 鍚戝墠鎼滅储鏈€杩戠殑 mov rcx, rax (鏉ヨ嚜 emit_pop_rax 鍚? */
		/* 杩欑鎯呭喌鍦ㄥ父閲忔姌鍙犳病鍖归厤鍒版椂涓嶅お鍙兘鍑虹幇锛岃烦杩?*/
	}

	/* 鍘嬬缉 */
		/* SSA-era multiply: imul dst, src, c  ->  lea dst, [src + src*(c-1)]
	 * for c in {2,3,5,9} (SIB scale 1/2/4/8). LEA is a pure ALU op:
	 * imul latency 3 on the multiply port drops to latency 1 on the
	 * general ALU ports, and dst == src is safe (the address is computed
	 * before the destination is written). */
	for (int i = 0; i < n; i++) {
		if (del[i]) continue;
		IrInst *m = &t[i];
		if (m->IrNode != IR_IMUL_REG_IMM) continue;
		int scale = 0;
		switch (m->imm) {
		case 2: scale = 1; break;
		case 3: scale = 2; break;
		case 5: scale = 4; break;
		case 9: scale = 8; break;
		default: continue;
		}
		if (m->dst == REG_NONE || m->src == REG_NONE) continue;
		m->IrNode = IR_LEA_IDX;
		m->imm = (int64_t)m->src;   /* index = src (multiplier in the SIB scale) */
		m->extra_imm = scale;       /* SIB scale: 1/2/4/8 */
	}

int w = 0;
	for (int r = 0; r < n; r++) {
		if (!del[r]) { if (w != r) t[w] = t[r]; w++; }
	}
	ir->text_count = w;
	free(del);
}

/* ========== 鍐椾綑鍔犺浇娑堥櫎 ========== */
/* 妯″紡: mov [r12], rax; add r12,8; sub r12,8; mov rax,[r12] 鈫?鍒犻櫎鍚庝袱鏉?*/

void ir_opt_redundant_load(IrBuffer *ir) {
	if (!ir || !ir->text || ir->text_count < 4) return;
	IrInst *t = ir->text;
	int n = ir->text_count;

	bool *del = (bool *)calloc(n, sizeof(bool));

	for (int i = 0; i + 3 < n; i++) {
		if (del[i]) continue;

		/* push_rax 鍚庣揣璺?pop_rax锛氬凡鍦?peephole 澶勭悊锛岃繖閲屽鐞嗘洿瀹芥澗鐨勬儏鍐?*/
		/* mov [r12+off], rax 鈫?... 鈫?mov rax, [r12+off] (same offset, no r12 change between) */

		/* 鍏蜂綋妯″紡: store to [r12+0], add r12,8, sub r12,8, load from [r12+0] */
		if (t[i].IrNode == IR_MOV_MEM_REG && t[i].dst == REG_R12 && t[i].src == REG_RAX &&
		    t[i+1].IrNode == IR_ADD_REG_IMM && t[i+1].dst == REG_R12 && t[i+1].imm == 8 &&
		    t[i+2].IrNode == IR_SUB_REG_IMM && t[i+2].dst == REG_R12 && t[i+2].imm == 8 &&
		    t[i+3].IrNode == IR_MOV_REG_MEM && t[i+3].dst == REG_RAX && t[i+3].src == REG_R12 &&
		    t[i].imm == t[i+3].imm) {
			/* add+sub 互相抵消，load 可以省略（rax 值未变） */
			del[i+1] = del[i+2] = del[i+3] = true;
			/* 淇濈暀 store锛屼絾涔熶笉闇€瑕佷簡锛堝€煎凡鍦?rax 涓級锛屼絾涓哄畨鍏ㄨ捣瑙佷繚鐣?*/
		}
	}

	/* SSA regalloc spills emit an adjacent store->load pair:
	 *   mov [base+off], reg          (IR_MOV_MEM_REG)
	 *   mov reg', [base+off]         (IR_MOV_REG_MEM)
	 * The value never left the register, so the load is redundant.
	 * When reg' == reg the store is dead too unless another read of
	 * the slot exists in the function (single-reader spill slot);
	 * otherwise the pair degenerates to a register copy.  The reader
	 * scan covers 64-bit/8-bit loads and LEA uses, matching the
	 * convention of ir_opt_remove_dead_stack_stores.  In SSA a spill
	 * def dominates every use, so no reachable reader can precede
	 * the pair in linear order and this forward scan is sufficient. */
	for (int i = 0; i + 1 < n; i++) {
		if (del[i]) continue;
		IrInst *s = &t[i], *l = &t[i+1];
		if (s->IrNode != IR_MOV_MEM_REG || l->IrNode != IR_MOV_REG_MEM) continue;
		if (l->src != s->dst || l->imm != s->imm) continue;
		if (s->src == REG_NONE || l->dst == REG_NONE) continue;

		int fe = i + 2;
		while (fe < n && t[fe].IrNode != IR_LABEL_NAMED) fe++;
		int other_reader = 0;
		for (int q = i + 2; q < fe; q++) {
			IrInst *x = &t[q];
			if ((x->IrNode == IR_MOV_REG_MEM || x->IrNode == IR_MOVZX_REG_MEM8) &&
			    x->src == s->dst && x->imm == s->imm) { other_reader = 1; break; }
			if (x->IrNode == IR_LEA && x->src == s->dst && x->imm == s->imm) { other_reader = 1; break; }
		}

		if (l->dst == s->src) {
			del[i+1] = true;           /* reload into same reg: value already there */
			if (!other_reader) del[i] = true;  /* store dead when no other reader */
		} else {
			l->IrNode = IR_MOV_REG_REG;        /* degrade to a register copy */
			l->src = s->src;
			if (!other_reader) del[i] = true;
		}
	}

	int w = 0;
	for (int r = 0; r < n; r++) {
		if (!del[r]) { if (w != r) t[w] = t[r]; w++; }
	}
	ir->text_count = w;
	free(del);
}

/* ========== 扩展常量折叠（除法） ========== */
/* 妯″紡鍜?ir_opt_constant_fold 绫讳技锛屼絾澶勭悊 idiv */

void ir_opt_const_fold_div(IrBuffer *ir) {
	if (!ir || !ir->text || ir->text_count < 12) return;
	IrInst *t = ir->text;
	int n = ir->text_count;

	bool *del = (bool *)calloc(n, sizeof(bool));

	for (int i = 0; i + 11 < n; i++) {
		if (del[i]) continue;

		int64_t a_val, b_val;
		if (!is_push_imm(&t[i], &t[i+1], &a_val)) continue;
		if (!is_push_imm(&t[i+2], &t[i+3], &b_val)) continue;
		if (!is_pop_rax(&t[i+4], &t[i+5])) continue;
		if (t[i+6].IrNode != IR_MOV_REG_REG || t[i+6].dst != REG_RCX || t[i+6].src != REG_RAX) continue;
		if (!is_pop_rax(&t[i+7], &t[i+8])) continue;

		/* 闄ゆ硶妯″紡: cqo + idiv rcx (3鏉￠澶栨寚浠? */
		/* 瀹為檯 IR 鍙兘鏄? xor rdx,rdx (鎴?cqo) + idiv rcx */
		/* 杩欎釜妯″紡姣?add/sub 澶氭寚浠わ紝鏆備笖鍙姌鍙犵函闄ゆ硶 */
		if (b_val != 0 && i + 12 < n && t[i+9].IrNode == IR_IDIV_REG && t[i+9].src == REG_RCX) {
			int64_t result = a_val / b_val;
			if (is_push_rax(&t[i+10], &t[i+11])) {
				t[i].extra_imm = result;
				for (int j = i + 2; j <= i + 11; j++) del[j] = true;
			}
		}
	}

	int w = 0;
	for (int r = 0; r < n; r++) {
		if (!del[r]) { if (w != r) t[w] = t[r]; w++; }
	}
	ir->text_count = w;
	free(del);
}


/* ========== Canonical register-loop unroll with scalar remainder ========== */

static bool unroll_body_inst_ok(const IrInst *i) {
	if (i->sym_name || i->data) return false;
	if (i->dst == REG_R11 || i->src == REG_R11 || i->src2 == REG_R11) return false;
	switch (i->IrNode) {
	case IR_MOV_REG_REG: case IR_MOV_REG_IMM:
	case IR_LEA: case IR_LEA_IDX:
	case IR_ADD_REG_REG: case IR_ADD_REG_IMM:
	case IR_SUB_REG_REG: case IR_SUB_REG_IMM:
	case IR_IMUL_REG_REG: case IR_IMUL_REG_IMM:
	case IR_XOR_REG_REG: case IR_XOR_REG_IMM:
	case IR_AND_REG_REG: case IR_AND_REG_IMM: case IR_OR_REG_REG:
	case IR_SHL_REG_IMM: case IR_SHR_REG_IMM: case IR_SAR_REG_IMM:
	case IR_INC_REG: case IR_NEG_REG: case IR_NOT_REG:
		return true;
	default:
		return false;
	}
}

static void append_ir_inst(IrInst **out, int *count, int *cap, IrInst i) {
	if (*count >= *cap) {
		*cap = *cap ? *cap * 2 : 256;
		*out = (IrInst *)realloc(*out, (size_t)*cap * sizeof(**out));
	}
	(*out)[(*count)++] = i;
}

static IrInst make_ir(IrOpcode op, IrReg dst, IrReg src, int64_t imm, int label) {
	IrInst i;
	memset(&i, 0, sizeof(i));
	i.IrNode = op; i.dst = dst; i.src = src; i.imm = imm; i.label_id = label;
	return i;
}

static IrInst make_vec_ir(IrOpcode op, IrReg dst, IrReg src, IrReg src2) {
	IrInst i = make_ir(op, dst, src, 0, 0);
	i.src2 = src2;
	return i;
}

static IrInst make_vec_mem_ir(IrOpcode op, IrReg dst, IrReg src, int64_t displacement) {
	IrInst i = make_vec_ir(op, dst, src, REG_NONE);
	i.imm = displacement;
	return i;
}

/* 将标量二元运算映射到等价的四路 i64 AVX2 运算。乘法没有 packed i64
 * AVX2 指令，不能用只计算四个 i32 的 vpmulld 冒充。 */
static IrOpcode vector_i64_binop(IrOpcode scalar) {
	switch (scalar) {
	case IR_ADD_REG_REG: return IR_VPADDQ;
	case IR_SUB_REG_REG: return IR_VPSUBQ;
	case IR_XOR_REG_REG: return IR_VPXOR;
	case IR_AND_REG_REG: return IR_VPAND;
	case IR_OR_REG_REG:  return IR_VPOR;
	default: return IR_OPCODE_COUNT;
	}
}

static bool vector_reg_mentioned(const IrInst *i, IrReg reg);
static bool vector_copy_result_unused(const IrInst *body, int at, int count);

/* Recognize the canonical i64 pattern produced for
 * dst[i] = left[i] + right[i].  Raw pointer parameters are versioned with
 * runtime overlap checks; the original scalar loop remains the fallback. */
void ir_opt_auto_vectorize(IrBuffer *ir) {
	extern int mira_target_avx2;
	if (!mira_target_avx2 || !ir || !ir->text || ir->text_count < 20) return;
	IrInst *t = ir->text;
	int n = ir->text_count;
	int max_label = 0;
	for (int i = 0; i < n; ++i)
		if (t[i].IrNode == IR_LABEL && t[i].label_id > max_label) max_label = t[i].label_id;

	IrInst *out = NULL;
	int out_count = 0, out_cap = 0;
	for (int p = 0; p < n; ) {
		bool matched = false;
		bool explicit_entry = out_count > 0 && out[out_count - 1].IrNode == IR_JMP &&
			out[out_count - 1].label_id == t[p].label_id;
		bool fallthrough_entry = out_count > 0 &&
			out[out_count - 1].IrNode != IR_JMP &&
			out[out_count - 1].IrNode != IR_RET;
		if (p + 18 < n && t[p].IrNode == IR_LABEL &&
		    t[p + 1].IrNode == IR_CMP_REG_REG && t[p + 2].IrNode == IR_JGE &&
		    (explicit_entry || fallthrough_entry)) {
			int h = t[p].label_id;
			IrReg induction = t[p + 1].dst;
			IrReg bound = t[p + 1].src;
			int exit_label = t[p + 2].label_id;
			int bound_slot = 0;
			int bound_load = explicit_entry ? out_count - 2 : out_count - 1;
			if (bound_load >= 0 && out[bound_load].IrNode == IR_MOV_REG_MEM &&
			    out[bound_load].dst == bound && out[bound_load].src == REG_RBP)
				bound_slot = (int)out[bound_load].imm;
			int bs = p + 3;
			int scalar_body_label = t[bs].IrNode == IR_LABEL ? t[bs].label_id : -1;
			if (t[bs].IrNode == IR_LABEL) bs++;
			int step = -1;
			bool rotated_latch = false;
			for (int q = bs; q + 1 < n; ++q) {
				if (t[q].IrNode == IR_ADD_REG_IMM && t[q].dst == induction && t[q].imm == 1 &&
				    t[q + 1].IrNode == IR_JMP && t[q + 1].label_id == h) { step = q; break; }
				if (q + 2 < n && t[q].IrNode == IR_ADD_REG_IMM &&
				    t[q].dst == induction && t[q].imm == 1 &&
				    t[q + 1].IrNode == IR_CMP_REG_REG &&
				    t[q + 1].dst == induction && t[q + 1].src == bound &&
				    t[q + 2].IrNode == IR_JL &&
				    t[q + 2].label_id == scalar_body_label) {
					step = q;
					rotated_latch = true;
					break;
				}
				if (t[q].IrNode == IR_LABEL || t[q].IrNode == IR_RET) break;
			}
			if (step > bs) {
				IrInst compact_body[64];
				int body_count = 0;
				for (int q = bs; q < step && body_count < 64; ++q) {
					if (vector_copy_result_unused(&t[bs], q - bs, step - bs)) continue;
					compact_body[body_count++] = t[q];
				}
				IrInst *b = compact_body;
				bool compact_scale = b[0].IrNode == IR_IMUL_REG_IMM;
				bool shift_scale = body_count > 1 && b[0].IrNode == IR_MOV_REG_REG &&
					b[0].src == induction && b[1].IrNode == IR_SHL_REG_IMM &&
					b[1].dst == b[0].dst && b[1].imm == 3;
				int lb = compact_scale ? 1 : (shift_scale ? 2 : 3), la = lb + 1;
				int ll = la + 1, rb = ll + 1, ra = rb + 1, rl = ra + 1;
				int op_begin = rl + 1, op_end = op_begin;
				IrOpcode vector_op = IR_OPCODE_COUNT;
				IrReg scalar_result = REG_NONE;
				if (op_begin < body_count && b[op_begin].IrNode == IR_LEA_IDX &&
				    b[op_begin].src == b[ll].dst && (IrReg)b[op_begin].imm == b[rl].dst) {
					vector_op = IR_VPADDQ;
					scalar_result = b[op_begin].dst;
				} else if (op_begin < body_count &&
				           b[op_begin].dst == b[ll].dst &&
				           b[op_begin].src == b[rl].dst &&
				           vector_i64_binop(b[op_begin].IrNode) != IR_OPCODE_COUNT) {
					vector_op = vector_i64_binop(b[op_begin].IrNode);
					scalar_result = b[op_begin].dst;
				} else if (op_begin + 1 < body_count &&
				           b[op_begin].IrNode == IR_MOV_REG_REG && b[op_begin].src == b[ll].dst &&
				           b[op_begin + 1].dst == b[op_begin].dst && b[op_begin + 1].src == b[rl].dst) {
					vector_op = vector_i64_binop(b[op_begin + 1].IrNode);
					scalar_result = b[op_begin + 1].dst;
					op_end++;
				}
				int db = op_end + 1, da = db + 1, st = da + 1;
				IrReg offset_reg = (compact_scale || shift_scale) ? b[0].dst : b[1].dst;
				bool scale_shape = compact_scale
					? (b[0].IrNode == IR_IMUL_REG_IMM && b[0].src == induction && b[0].imm == 8)
					: shift_scale ? true : (b[0].IrNode == IR_MOV_REG_IMM && b[0].imm == 8 &&
					b[1].IrNode == IR_MOV_REG_REG && b[1].src == induction &&
					b[2].IrNode == IR_IMUL_REG_REG && b[2].dst == b[1].dst && b[2].src == b[0].dst);
				bool shape = scale_shape &&
					b[lb].IrNode == IR_MOV_REG_MEM && b[lb].src == REG_RBP &&
					((b[la].IrNode == IR_LEA_IDX && b[la].src == b[lb].dst && (IrReg)b[la].imm == offset_reg) ||
					 (b[la].IrNode == IR_ADD_REG_REG && b[la].dst == b[lb].dst && b[la].src == offset_reg)) &&
					b[ll].IrNode == IR_MOV_REG_MEM && b[ll].src == b[la].dst &&
					b[rb].IrNode == IR_MOV_REG_MEM && b[rb].src == REG_RBP &&
					((b[ra].IrNode == IR_LEA_IDX && b[ra].src == b[rb].dst && (IrReg)b[ra].imm == offset_reg) ||
					 (b[ra].IrNode == IR_ADD_REG_REG && b[ra].dst == b[rb].dst && b[ra].src == offset_reg)) &&
					b[rl].IrNode == IR_MOV_REG_MEM && b[rl].src == b[ra].dst &&
					vector_op != IR_OPCODE_COUNT && st + 1 == body_count &&
					b[db].IrNode == IR_MOV_REG_MEM && b[db].src == REG_RBP &&
					((b[da].IrNode == IR_LEA_IDX && b[da].src == b[db].dst && (IrReg)b[da].imm == offset_reg) ||
					 (b[da].IrNode == IR_ADD_REG_REG && b[da].dst == b[db].dst && b[da].src == offset_reg)) &&
					b[st].IrNode == IR_MOV_MEM_REG && b[st].dst == b[da].dst && b[st].src == scalar_result &&
					induction != REG_R9 && induction != REG_R10 && induction != REG_R11 &&
					induction != REG_R8 &&
					bound != REG_R9 && bound != REG_R10 && bound != REG_R11 &&
					bound != REG_R8 &&
					bound_slot != 0;
				if (getenv("MIRA_VECTOR_DEBUG")) {
					fprintf(stderr,
						"vector-match header=%d entry=%s bound_slot=%d body=%d compact=%d scale=%d op=%d shape=%d "
						"lb=%d la=%d ll=%d rb=%d ra=%d rl=%d db=%d da=%d st=%d\n",
						h, explicit_entry ? "jump" : "fallthrough", bound_slot,
						body_count, compact_scale, scale_shape, (int)vector_op, shape,
						(int)b[lb].IrNode, (int)b[la].IrNode, (int)b[ll].IrNode,
						(int)b[rb].IrNode, (int)b[ra].IrNode, (int)b[rl].IrNode,
						(int)b[db].IrNode, (int)b[da].IrNode, (int)b[st].IrNode);
				}
				DecisionResult vector_decision;
				DecisionKind vector_kind = shape
					? decision_choose_loop(UINT64_MAX, (uint32_t)body_count, 1, 1,
						4, 8, 128, &vector_decision)
					: DECISION_KEEP;
				if (shape && vector_kind == DECISION_VECTORIZE) {
					/* Numeric labels are historically reused by separately lowered
					 * functions.  Once this pass adds out-of-line dispatch code, a
					 * fallback jump must not resolve to a same-numbered label in the
					 * next function.  Give the preserved scalar loop a module-unique
					 * header and exit, and retarget all of its local edges. */
					int scalar_h = ++max_label, scalar_exit = ++max_label;
					int dispatch = ++max_label, left_ok = ++max_label, right_ok = ++max_label;
					int vector_fast = ++max_label, vector_h = ++max_label;
					int vector_tail = ++max_label, vector_done = ++max_label;
					t[p].label_id = scalar_h;
					t[p + 2].label_id = scalar_exit;
					if (!rotated_latch)
						t[step + 1].label_id = scalar_h;
					if (step + 2 < n && t[step + 2].IrNode == IR_LABEL &&
					    t[step + 2].label_id == exit_label)
						t[step + 2].label_id = scalar_exit;
					if (explicit_entry) {
						out[out_count - 1].label_id = dispatch;
					} else {
						append_ir_inst(&out, &out_count, &out_cap,
							make_ir(IR_JMP, REG_NONE, REG_NONE, 0, dispatch));
					}
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_LABEL, REG_NONE, REG_NONE, 0, dispatch));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_CMP_REG_IMM, bound, REG_NONE, 4, 0));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_JL, REG_NONE, REG_NONE, 0, scalar_h));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_MOV_REG_REG, REG_R11, bound, 0, 0));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_SHL_REG_IMM, REG_R11, REG_NONE, 3, 0));

					/* dst versus left: exact alias or disjoint ranges */
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_MOV_REG_MEM, REG_R9, REG_RBP, b[db].imm, 0));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_MOV_REG_MEM, REG_R10, REG_RBP, b[lb].imm, 0));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_CMP_REG_REG, REG_R9, REG_R10, 0, 0));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_JE, REG_NONE, REG_NONE, 0, left_ok));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_MOV_REG_REG, REG_RCX, REG_R9, 0, 0));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_ADD_REG_REG, REG_RCX, REG_R11, 0, 0));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_JB, REG_NONE, REG_NONE, 0, scalar_h));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_CMP_REG_REG, REG_RCX, REG_R10, 0, 0));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_JBE, REG_NONE, REG_NONE, 0, left_ok));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_MOV_REG_REG, REG_RDX, REG_R10, 0, 0));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_ADD_REG_REG, REG_RDX, REG_R11, 0, 0));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_JB, REG_NONE, REG_NONE, 0, scalar_h));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_CMP_REG_REG, REG_RDX, REG_R9, 0, 0));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_JA, REG_NONE, REG_NONE, 0, scalar_h));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_LABEL, REG_NONE, REG_NONE, 0, left_ok));

					/* dst versus right */
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_MOV_REG_MEM, REG_R10, REG_RBP, b[rb].imm, 0));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_CMP_REG_REG, REG_R9, REG_R10, 0, 0));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_JE, REG_NONE, REG_NONE, 0, right_ok));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_MOV_REG_REG, REG_RCX, REG_R9, 0, 0));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_ADD_REG_REG, REG_RCX, REG_R11, 0, 0));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_JB, REG_NONE, REG_NONE, 0, scalar_h));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_CMP_REG_REG, REG_RCX, REG_R10, 0, 0));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_JBE, REG_NONE, REG_NONE, 0, right_ok));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_MOV_REG_REG, REG_RDX, REG_R10, 0, 0));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_ADD_REG_REG, REG_RDX, REG_R11, 0, 0));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_JB, REG_NONE, REG_NONE, 0, scalar_h));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_CMP_REG_REG, REG_RDX, REG_R9, 0, 0));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_JA, REG_NONE, REG_NONE, 0, scalar_h));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_LABEL, REG_NONE, REG_NONE, 0, right_ok));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_JMP, REG_NONE, REG_NONE, 0, vector_fast));

					/* Process sixteen i64 elements per fast iteration.  The
					 * four-lane loop below handles the remaining complete vector
					 * chunks before the scalar tail. */
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_LABEL, REG_NONE, REG_NONE, 0, vector_fast));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_MOV_REG_MEM, bound, REG_RBP, bound_slot, 0));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_MOV_REG_REG, REG_R11, bound, 0, 0));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_SUB_REG_REG, REG_R11, induction, 0, 0));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_CMP_REG_IMM, REG_R11, REG_NONE, 16, 0));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_JL, REG_NONE, REG_NONE, 0, vector_h));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_MOV_REG_REG, REG_R8, induction, 0, 0));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_SHL_REG_IMM, REG_R8, REG_NONE, 3, 0));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_MOV_REG_MEM, REG_R9, REG_RBP, b[lb].imm, 0));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_ADD_REG_REG, REG_R9, REG_R8, 0, 0));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_MOV_REG_MEM, REG_R10, REG_RBP, b[rb].imm, 0));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_ADD_REG_REG, REG_R10, REG_R8, 0, 0));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_MOV_REG_MEM, REG_R11, REG_RBP, b[db].imm, 0));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_ADD_REG_REG, REG_R11, REG_R8, 0, 0));
					for (int lane = 0; lane < 4; ++lane) {
						int64_t displacement = (int64_t)lane * 32;
						append_ir_inst(&out, &out_count, &out_cap,
							make_vec_mem_ir(IR_VMOVDQU_LOAD, REG_YMM0, REG_R9, displacement));
						append_ir_inst(&out, &out_count, &out_cap,
							make_vec_mem_ir(IR_VMOVDQU_LOAD, REG_YMM1, REG_R10, displacement));
						append_ir_inst(&out, &out_count, &out_cap,
							make_vec_ir(vector_op, REG_YMM2, REG_YMM0, REG_YMM1));
						append_ir_inst(&out, &out_count, &out_cap,
							make_vec_mem_ir(IR_VMOVDQU_STORE, REG_R11, REG_YMM2, displacement));
					}
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_ADD_REG_IMM, induction, REG_NONE, 16, 0));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_JMP, REG_NONE, REG_NONE, 0, vector_fast));

					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_LABEL, REG_NONE, REG_NONE, 0, vector_h));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_MOV_REG_MEM, bound, REG_RBP, bound_slot, 0));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_MOV_REG_REG, REG_R11, bound, 0, 0));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_SUB_REG_REG, REG_R11, induction, 0, 0));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_CMP_REG_IMM, REG_R11, REG_NONE, 4, 0));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_JL, REG_NONE, REG_NONE, 0, vector_tail));
					for (int q = 0; q <= la; ++q) append_ir_inst(&out, &out_count, &out_cap, b[q]);
					append_ir_inst(&out, &out_count, &out_cap, make_vec_ir(IR_VMOVDQU_LOAD, REG_YMM0, b[la].dst, REG_NONE));
					append_ir_inst(&out, &out_count, &out_cap, b[rb]); append_ir_inst(&out, &out_count, &out_cap, b[ra]);
					append_ir_inst(&out, &out_count, &out_cap, make_vec_ir(IR_VMOVDQU_LOAD, REG_YMM1, b[ra].dst, REG_NONE));
					append_ir_inst(&out, &out_count, &out_cap, make_vec_ir(vector_op, REG_YMM2, REG_YMM0, REG_YMM1));
					append_ir_inst(&out, &out_count, &out_cap, b[db]); append_ir_inst(&out, &out_count, &out_cap, b[da]);
					append_ir_inst(&out, &out_count, &out_cap, make_vec_ir(IR_VMOVDQU_STORE, b[da].dst, REG_YMM2, REG_NONE));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_ADD_REG_IMM, induction, REG_NONE, 4, 0));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_JMP, REG_NONE, REG_NONE, 0, vector_h));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_LABEL, REG_NONE, REG_NONE, 0, vector_tail));
					append_ir_inst(&out, &out_count, &out_cap, make_vec_ir(IR_VZEROUPPER, REG_NONE, REG_NONE, REG_NONE));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_JMP, REG_NONE, REG_NONE, 0, scalar_h));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_LABEL, REG_NONE, REG_NONE, 0, vector_done));
					append_ir_inst(&out, &out_count, &out_cap, make_vec_ir(IR_VZEROUPPER, REG_NONE, REG_NONE, REG_NONE));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_JMP, REG_NONE, REG_NONE, 0, scalar_exit));
					/* The legacy physical allocator may reuse bound's register in the
					 * body.  Reload it at every scalar test as well. */
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_LABEL, REG_NONE, REG_NONE, 0, scalar_h));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_MOV_REG_MEM, bound, REG_RBP, bound_slot, 0));
					t[p].label_id = ++max_label;
					matched = true;
				}
			}
		}
		append_ir_inst(&out, &out_count, &out_cap, t[p++]);
		(void)matched;
	}
	free(ir->text);
	ir->text = out; ir->text_count = out_count; ir->text_cap = out_cap;
}

/* Vectorize a canonical contiguous i64 sum after loop rotation.  A scalar
 * tail preserves arbitrary non-multiple-of-four bounds; the original
 * accumulator may contain a nonzero value and is updated only at reduction. */
void ir_opt_vectorize_i64_reductions(IrBuffer *ir) {
	extern int mira_target_avx2;
	if (!mira_target_avx2 || !ir || !ir->text || ir->text_count < 12) return;
	IrInst *t = ir->text;
	int n = ir->text_count, max_label = 0;
	for (int i = 0; i < n; ++i)
		if (t[i].IrNode == IR_LABEL && t[i].label_id > max_label) max_label = t[i].label_id;
	IrInst *out = NULL;
	int out_count = 0, out_cap = 0;
	for (int p = 0; p < n; ) {
		int matched = 0;
		/* Canonical unsigned-byte equality count:
		 *   count += (*(uint8_t *)(base + i) == immediate)
		 * Convert the 0xff compare mask to byte ones before VPSADBW so
		 * the qword lanes contain exact match counts. */
		if (p > 0 && p + 13 < n &&
			t[p - 1].IrNode == IR_MOV_REG_IMM &&
			t[p].IrNode == IR_LABEL &&
			t[p + 1].IrNode == IR_CMP_REG_IMM && t[p + 2].IrNode == IR_JGE &&
			t[p + 3].IrNode == IR_LABEL &&
			t[p + 4].IrNode == IR_LEA_IDX &&
			t[p + 5].IrNode == IR_MOVZX_REG_MEM8 &&
			t[p + 6].IrNode == IR_CMP_REG_IMM &&
			t[p + 7].IrNode == IR_SETE &&
			t[p + 8].IrNode == IR_MOVZX_REG8 &&
			t[p + 9].IrNode == IR_ADD_REG_REG &&
			t[p + 10].IrNode == IR_ADD_REG_IMM && t[p + 10].imm == 1 &&
			t[p + 11].IrNode == IR_CMP_REG_IMM &&
			(t[p + 12].IrNode == IR_JNE || t[p + 12].IrNode == IR_JL) &&
			t[p + 12].label_id == t[p + 3].label_id &&
			((t[p + 13].IrNode == IR_LABEL &&
			  t[p + 13].label_id == t[p + 2].label_id) ||
			 (p + 14 < n && t[p + 13].IrNode == IR_JMP &&
			  t[p + 13].label_id == t[p + 2].label_id &&
			  t[p + 14].IrNode == IR_LABEL &&
			  t[p + 14].label_id == t[p + 2].label_id))) {
			int exit_offset = t[p + 13].IrNode == IR_JMP ? 14 : 13;
			IrReg ind = t[p + 1].dst, address = t[p + 4].dst;
			IrReg base = t[p + 4].src, value = t[p + 5].dst;
			IrReg boolean = t[p + 7].dst, acc = t[p + 9].dst;
			int64_t start = t[p - 1].imm, bound = t[p + 1].imm;
			int64_t needle = t[p + 6].imm;
			IrReg roles[5] = { ind, address, base, value, acc };
			bool safe = t[p - 1].dst == ind &&
				(IrReg)t[p + 4].imm == ind && t[p + 5].src == address &&
				t[p + 6].dst == value &&
				t[p + 8].dst == boolean && t[p + 8].src == boolean &&
				t[p + 9].src == boolean && t[p + 10].dst == ind &&
				t[p + 11].dst == ind && t[p + 11].imm == bound &&
				needle >= 0 && needle <= 255 && bound > start &&
				boolean >= REG_RAX && boolean <= REG_R15 &&
				boolean != REG_R10 && boolean != REG_R11 &&
				boolean != REG_RSP && boolean != ind && boolean != base &&
				boolean != value && boolean != acc;
			for (int a = 0; a < 5; ++a) {
				if (roles[a] < REG_RAX || roles[a] > REG_R15 ||
					roles[a] == REG_R10 || roles[a] == REG_R11 ||
					roles[a] == REG_RSP)
					safe = false;
				for (int b = a + 1; b < 5; ++b)
					if (roles[a] == roles[b]) safe = false;
			}
			uint64_t trip_count = safe
				? (uint64_t)bound - (uint64_t)start : 0;
			DecisionResult vector_decision;
			if (safe && decision_choose_loop(trip_count, 8, 1, 1, 32, 5,
				64, &vector_decision) == DECISION_VECTORIZE) {
				int64_t vector_end = bound - (int64_t)(trip_count & UINT64_C(31));
				int vbody = ++max_label, tail = ++max_label;
				append_ir_inst(&out, &out_count, &out_cap, t[p]);
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_CMP_REG_IMM, ind, REG_NONE, vector_end, 0));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_JGE, REG_NONE, REG_NONE, 0, tail));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_MOV_REG_IMM, value, REG_NONE, needle, 0));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_MOVQ_XMM_REG, REG_XMM0, value, 0, 0));
				append_ir_inst(&out, &out_count, &out_cap,
					make_vec_ir(IR_VPBROADCASTB, REG_YMM3, REG_XMM0, REG_NONE));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_MOV_REG_IMM, value, REG_NONE, 1, 0));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_MOVQ_XMM_REG, REG_XMM0, value, 0, 0));
				append_ir_inst(&out, &out_count, &out_cap,
					make_vec_ir(IR_VPBROADCASTB, REG_YMM4, REG_XMM0, REG_NONE));
				append_ir_inst(&out, &out_count, &out_cap,
					make_vec_ir(IR_VPXOR, REG_YMM0, REG_YMM0, REG_YMM0));
				append_ir_inst(&out, &out_count, &out_cap,
					make_vec_ir(IR_VPXOR, REG_YMM2, REG_YMM2, REG_YMM2));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_LABEL, REG_NONE, REG_NONE, 0, vbody));
				append_ir_inst(&out, &out_count, &out_cap, t[p + 4]);
				append_ir_inst(&out, &out_count, &out_cap,
					make_vec_ir(IR_VMOVDQU_LOAD, REG_YMM1, address, REG_NONE));
				append_ir_inst(&out, &out_count, &out_cap,
					make_vec_ir(IR_VPCMPEQB, REG_YMM1, REG_YMM1, REG_YMM3));
				append_ir_inst(&out, &out_count, &out_cap,
					make_vec_ir(IR_VPAND, REG_YMM1, REG_YMM1, REG_YMM4));
				append_ir_inst(&out, &out_count, &out_cap,
					make_vec_ir(IR_VPSADBW, REG_YMM1, REG_YMM1, REG_YMM2));
				append_ir_inst(&out, &out_count, &out_cap,
					make_vec_ir(IR_VPADDQ, REG_YMM0, REG_YMM0, REG_YMM1));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_ADD_REG_IMM, ind, REG_NONE, 32, 0));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_CMP_REG_IMM, ind, REG_NONE, vector_end, 0));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_JNE, REG_NONE, REG_NONE, 0, vbody));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_SUB_REG_IMM, REG_RSP, REG_NONE, 32, 0));
				append_ir_inst(&out, &out_count, &out_cap,
					make_vec_ir(IR_VMOVDQU_STORE, REG_RSP, REG_YMM0, REG_NONE));
				for (int off = 0; off < 32; off += 8) {
					append_ir_inst(&out, &out_count, &out_cap,
						make_ir(IR_MOV_REG_MEM, value, REG_RSP, off, 0));
					append_ir_inst(&out, &out_count, &out_cap,
						make_ir(IR_ADD_REG_REG, acc, value, 0, 0));
				}
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_ADD_REG_IMM, REG_RSP, REG_NONE, 32, 0));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_VZEROUPPER, REG_NONE, REG_NONE, 0, 0));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_LABEL, REG_NONE, REG_NONE, 0, tail));
				for (int q = 1; q <= 12; ++q)
					append_ir_inst(&out, &out_count, &out_cap, t[p + q]);
				for (int q = 13; q <= exit_offset; ++q)
					append_ir_inst(&out, &out_count, &out_cap, t[p + q]);
				p += exit_offset + 1;
				matched = 1;
			}
		}
		if (matched) continue;
		/* Canonical unsigned-byte sum:
		 *   sum += *(uint8_t *)(base + i)
		 * VPSADBW against zero widens and sums 32 bytes into four qwords;
		 * the existing qword reduction epilogue then folds those lanes. */
		if (p > 0 && p + 10 < n &&
			t[p - 1].IrNode == IR_MOV_REG_IMM &&
			t[p].IrNode == IR_LABEL &&
			t[p + 1].IrNode == IR_CMP_REG_IMM && t[p + 2].IrNode == IR_JGE &&
			t[p + 3].IrNode == IR_LABEL &&
			t[p + 4].IrNode == IR_LEA_IDX &&
			t[p + 5].IrNode == IR_MOVZX_REG_MEM8 &&
			t[p + 6].IrNode == IR_ADD_REG_REG &&
			t[p + 7].IrNode == IR_ADD_REG_IMM && t[p + 7].imm == 1 &&
			t[p + 8].IrNode == IR_CMP_REG_IMM &&
			(t[p + 9].IrNode == IR_JNE || t[p + 9].IrNode == IR_JL) &&
			t[p + 9].label_id == t[p + 3].label_id &&
			((t[p + 10].IrNode == IR_LABEL &&
			  t[p + 10].label_id == t[p + 2].label_id) ||
			 (p + 11 < n && t[p + 10].IrNode == IR_JMP &&
			  t[p + 10].label_id == t[p + 2].label_id &&
			  t[p + 11].IrNode == IR_LABEL &&
			  t[p + 11].label_id == t[p + 2].label_id))) {
			int exit_offset = t[p + 10].IrNode == IR_JMP ? 11 : 10;
			IrReg ind = t[p + 1].dst, address = t[p + 4].dst;
			IrReg base = t[p + 4].src, value = t[p + 5].dst;
			IrReg acc = t[p + 6].dst;
			int64_t start = t[p - 1].imm, bound = t[p + 1].imm;
			IrReg roles[5] = { ind, address, base, value, acc };
			bool safe = t[p - 1].dst == ind &&
				(IrReg)t[p + 4].imm == ind && t[p + 5].src == address &&
				t[p + 6].src == value && t[p + 7].dst == ind &&
				t[p + 8].dst == ind && t[p + 8].imm == bound &&
				bound > start;
			for (int a = 0; a < 5; ++a) {
				if (roles[a] < REG_RAX || roles[a] > REG_R15 ||
					roles[a] == REG_R10 || roles[a] == REG_R11 ||
					roles[a] == REG_RSP)
					safe = false;
				for (int b = a + 1; b < 5; ++b)
					if (roles[a] == roles[b]) safe = false;
			}
			uint64_t trip_count = safe
				? (uint64_t)bound - (uint64_t)start : 0;
			DecisionResult vector_decision;
			if (safe && decision_choose_loop(trip_count, 5, 1, 1, 32, 4,
				64, &vector_decision) == DECISION_VECTORIZE) {
				int64_t vector_end = bound - (int64_t)(trip_count & UINT64_C(31));
				int vbody = ++max_label, tail = ++max_label;
				append_ir_inst(&out, &out_count, &out_cap, t[p]);
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_CMP_REG_IMM, ind, REG_NONE, vector_end, 0));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_JGE, REG_NONE, REG_NONE, 0, tail));
				append_ir_inst(&out, &out_count, &out_cap,
					make_vec_ir(IR_VPXOR, REG_YMM0, REG_YMM0, REG_YMM0));
				append_ir_inst(&out, &out_count, &out_cap,
					make_vec_ir(IR_VPXOR, REG_YMM2, REG_YMM2, REG_YMM2));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_LABEL, REG_NONE, REG_NONE, 0, vbody));
				append_ir_inst(&out, &out_count, &out_cap, t[p + 4]);
				append_ir_inst(&out, &out_count, &out_cap,
					make_vec_ir(IR_VMOVDQU_LOAD, REG_YMM1, address, REG_NONE));
				append_ir_inst(&out, &out_count, &out_cap,
					make_vec_ir(IR_VPSADBW, REG_YMM1, REG_YMM1, REG_YMM2));
				append_ir_inst(&out, &out_count, &out_cap,
					make_vec_ir(IR_VPADDQ, REG_YMM0, REG_YMM0, REG_YMM1));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_ADD_REG_IMM, ind, REG_NONE, 32, 0));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_CMP_REG_IMM, ind, REG_NONE, vector_end, 0));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_JNE, REG_NONE, REG_NONE, 0, vbody));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_SUB_REG_IMM, REG_RSP, REG_NONE, 32, 0));
				append_ir_inst(&out, &out_count, &out_cap,
					make_vec_ir(IR_VMOVDQU_STORE, REG_RSP, REG_YMM0, REG_NONE));
				for (int off = 0; off < 32; off += 8) {
					append_ir_inst(&out, &out_count, &out_cap,
						make_ir(IR_MOV_REG_MEM, value, REG_RSP, off, 0));
					append_ir_inst(&out, &out_count, &out_cap,
						make_ir(IR_ADD_REG_REG, acc, value, 0, 0));
				}
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_ADD_REG_IMM, REG_RSP, REG_NONE, 32, 0));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_VZEROUPPER, REG_NONE, REG_NONE, 0, 0));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_LABEL, REG_NONE, REG_NONE, 0, tail));
				for (int q = 1; q <= 9; ++q)
					append_ir_inst(&out, &out_count, &out_cap, t[p + q]);
				for (int q = 10; q <= exit_offset; ++q)
					append_ir_inst(&out, &out_count, &out_cap, t[p + q]);
				p += exit_offset + 1;
				matched = 1;
			}
		}
		/* Canonical variable-bound reduction produced for
		 * sum += load_i64(base + i * 8) & 255.  Accumulate four qwords
		 * per iteration and retain the complete scalar loop as the tail. */
		if (p + 14 < n &&
			t[p].IrNode == IR_LABEL &&
			t[p + 1].IrNode == IR_CMP_REG_REG && t[p + 2].IrNode == IR_JGE &&
			t[p + 3].IrNode == IR_LABEL &&
			t[p + 4].IrNode == IR_MOV_REG_REG &&
			t[p + 5].IrNode == IR_SHL_REG_IMM && t[p + 5].imm == 3 &&
			t[p + 6].IrNode == IR_LEA_IDX && (IrReg)t[p + 6].imm == t[p + 4].dst &&
			t[p + 7].IrNode == IR_MOV_REG_MEM && t[p + 7].src == t[p + 6].dst &&
			t[p + 8].IrNode == IR_AND_REG_IMM && t[p + 8].imm == 255 &&
			t[p + 9].IrNode == IR_ADD_REG_REG && t[p + 9].src == t[p + 7].dst &&
			t[p + 10].IrNode == IR_ADD_REG_IMM && t[p + 10].imm == 1 &&
			t[p + 11].IrNode == IR_CMP_REG_REG &&
			t[p + 12].IrNode == IR_JL && t[p + 12].label_id == t[p + 3].label_id &&
			((t[p + 13].IrNode == IR_LABEL &&
			  t[p + 13].label_id == t[p + 2].label_id) ||
			 (t[p + 13].IrNode == IR_JMP &&
			  t[p + 13].label_id == t[p + 2].label_id &&
			  t[p + 14].IrNode == IR_LABEL &&
			  t[p + 14].label_id == t[p + 2].label_id))) {
			IrReg ind = t[p + 1].dst, bound = t[p + 1].src;
			IrReg scale = t[p + 4].dst, address = t[p + 6].dst;
			IrReg base = t[p + 6].src, value = t[p + 7].dst;
			IrReg acc = t[p + 9].dst;
			IrReg roles[] = { ind, bound, scale, address, base, value, acc };
			bool safe_roles = t[p + 4].src == ind && t[p + 5].dst == scale &&
				t[p + 10].dst == ind && t[p + 11].dst == ind &&
				t[p + 11].src == bound && t[p + 8].dst == value &&
				acc != REG_R10 && acc != REG_R11;
			for (size_t ri = 0; ri < sizeof(roles) / sizeof(roles[0]); ++ri)
				if (roles[ri] == REG_RSP || roles[ri] == REG_R10 ||
					roles[ri] == REG_R11 || roles[ri] < REG_RAX ||
					roles[ri] > REG_R15)
					safe_roles = false;
				else
					for (size_t rj = ri + 1;
						rj < sizeof(roles) / sizeof(roles[0]); ++rj)
						if (roles[ri] == roles[rj]) safe_roles = false;
			DecisionResult vector_decision;
			if (safe_roles && decision_choose_loop(UINT64_MAX, 10, 1, 1, 4, 4,
				64, &vector_decision) == DECISION_VECTORIZE) {
				int dispatch = t[p].label_id;
				int vector_loop = ++max_label, reduce = ++max_label;
				int scalar_h = ++max_label;
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_LABEL, REG_NONE, REG_NONE, 0, dispatch));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_MOV_REG_REG, REG_R11, bound, 0, 0));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_SUB_REG_REG, REG_R11, ind, 0, 0));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_CMP_REG_IMM, REG_R11, REG_NONE, 4, 0));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_JL, REG_NONE, REG_NONE, 0, scalar_h));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_SUB_REG_IMM, REG_RSP, REG_NONE, 32, 0));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_MOV_REG_IMM, REG_R10, REG_NONE, 255, 0));
				for (int off = 0; off < 32; off += 8)
					append_ir_inst(&out, &out_count, &out_cap,
						make_ir(IR_MOV_MEM_REG, REG_RSP, REG_R10, off, 0));
				append_ir_inst(&out, &out_count, &out_cap,
					make_vec_mem_ir(IR_VMOVDQU_LOAD, REG_YMM2, REG_RSP, 0));
				append_ir_inst(&out, &out_count, &out_cap,
					make_vec_ir(IR_VPXOR, REG_YMM0, REG_YMM0, REG_YMM0));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_LABEL, REG_NONE, REG_NONE, 0, vector_loop));
				append_ir_inst(&out, &out_count, &out_cap, t[p + 4]);
				append_ir_inst(&out, &out_count, &out_cap, t[p + 5]);
				append_ir_inst(&out, &out_count, &out_cap, t[p + 6]);
				append_ir_inst(&out, &out_count, &out_cap,
					make_vec_mem_ir(IR_VMOVDQU_LOAD, REG_YMM1, address, 0));
				append_ir_inst(&out, &out_count, &out_cap,
					make_vec_ir(IR_VPAND, REG_YMM1, REG_YMM1, REG_YMM2));
				append_ir_inst(&out, &out_count, &out_cap,
					make_vec_ir(IR_VPADDQ, REG_YMM0, REG_YMM0, REG_YMM1));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_ADD_REG_IMM, ind, REG_NONE, 4, 0));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_MOV_REG_REG, REG_R11, bound, 0, 0));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_SUB_REG_REG, REG_R11, ind, 0, 0));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_CMP_REG_IMM, REG_R11, REG_NONE, 4, 0));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_JGE, REG_NONE, REG_NONE, 0, vector_loop));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_JMP, REG_NONE, REG_NONE, 0, reduce));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_LABEL, REG_NONE, REG_NONE, 0, reduce));
				append_ir_inst(&out, &out_count, &out_cap,
					make_vec_mem_ir(IR_VMOVDQU_STORE, REG_RSP, REG_YMM0, 0));
				for (int off = 0; off < 32; off += 8) {
					append_ir_inst(&out, &out_count, &out_cap,
						make_ir(IR_MOV_REG_MEM, REG_R10, REG_RSP, off, 0));
					append_ir_inst(&out, &out_count, &out_cap,
						make_ir(IR_ADD_REG_REG, acc, REG_R10, 0, 0));
				}
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_ADD_REG_IMM, REG_RSP, REG_NONE, 32, 0));
				append_ir_inst(&out, &out_count, &out_cap,
					make_vec_ir(IR_VZEROUPPER, REG_NONE, REG_NONE, REG_NONE));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_JMP, REG_NONE, REG_NONE, 0, scalar_h));
				t[p].label_id = scalar_h;
				int exit_offset = t[p + 13].IrNode == IR_JMP ? 14 : 13;
				for (int q = 0; q <= exit_offset; ++q)
					append_ir_inst(&out, &out_count, &out_cap, t[p + q]);
				p += exit_offset + 1;
				matched = 1;
			}
		}
		/* The SSA affine/induction pipeline can canonicalize the same
		 * contiguous reduction as
		 *
		 *   address = index; address <<= 3; address += base;
		 *   accumulator += *address;
		 *
		 * instead of retaining LEA_IDX.  Keep this shape in the same
		 * bounded AVX2 reduction path; otherwise a harmless address-form
		 * choice silently drops a proven sequential scan back to scalar. */
		if (!matched && p > 0 && p + 12 < n &&
			t[p - 1].IrNode == IR_MOV_REG_IMM &&
			t[p].IrNode == IR_LABEL &&
			t[p + 1].IrNode == IR_CMP_REG_IMM && t[p + 2].IrNode == IR_JGE &&
			t[p + 3].IrNode == IR_LABEL &&
			t[p + 4].IrNode == IR_MOV_REG_REG &&
			t[p + 5].IrNode == IR_SHL_REG_IMM && t[p + 5].imm == 3 &&
			t[p + 6].IrNode == IR_ADD_REG_REG &&
			t[p + 7].IrNode == IR_MOV_REG_MEM &&
			t[p + 8].IrNode == IR_ADD_REG_REG &&
			t[p + 9].IrNode == IR_ADD_REG_IMM && t[p + 9].imm == 1 &&
			t[p + 10].IrNode == IR_CMP_REG_IMM &&
			(t[p + 11].IrNode == IR_JNE || t[p + 11].IrNode == IR_JL) &&
			t[p + 11].label_id == t[p + 3].label_id &&
			((t[p + 12].IrNode == IR_LABEL &&
			  t[p + 12].label_id == t[p + 2].label_id) ||
			 (p + 13 < n && t[p + 12].IrNode == IR_JMP &&
			  t[p + 12].label_id == t[p + 2].label_id &&
			  t[p + 13].IrNode == IR_LABEL &&
			  t[p + 13].label_id == t[p + 2].label_id))) {
			int exit_offset = t[p + 12].IrNode == IR_JMP ? 13 : 12;
			IrReg ind = t[p + 1].dst;
			int64_t start = t[p - 1].imm, bound = t[p + 1].imm;
			IrReg address = t[p + 4].dst, base = t[p + 6].src;
			IrReg value = t[p + 7].dst, acc = t[p + 8].dst;
			IrReg roles[5] = { ind, address, base, value, acc };
			bool roles_distinct = true;
			for (int a = 0; a < 5; ++a) {
				if (roles[a] < REG_RAX || roles[a] > REG_R15 ||
					roles[a] == REG_R10 || roles[a] == REG_R11 ||
					roles[a] == REG_RSP)
					roles_distinct = false;
				for (int b = a + 1; b < 5; ++b)
					if (roles[a] == roles[b]) roles_distinct = false;
			}
			bool shape_safe = t[p - 1].dst == ind &&
				t[p + 4].src == ind && t[p + 5].dst == address &&
				t[p + 6].dst == address && t[p + 7].src == address &&
				t[p + 8].src == value && t[p + 9].dst == ind &&
				t[p + 10].dst == ind && t[p + 10].imm == bound &&
				bound > start;
			uint64_t trip_count = shape_safe
				? (uint64_t)bound - (uint64_t)start : 0;
			DecisionResult vector_decision;
			if (shape_safe && roles_distinct &&
				decision_choose_loop(trip_count, 7, 1, 1, 4, 4, 64,
					&vector_decision) == DECISION_VECTORIZE) {
				int64_t vector_end = bound - (int64_t)(trip_count & UINT64_C(3));
				int vbody = ++max_label, tail = ++max_label;
				append_ir_inst(&out, &out_count, &out_cap, t[p]);
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_CMP_REG_IMM, ind, REG_NONE, vector_end, 0));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_JGE, REG_NONE, REG_NONE, 0, tail));
				append_ir_inst(&out, &out_count, &out_cap,
					make_vec_ir(IR_VPXOR, REG_YMM0, REG_YMM0, REG_YMM0));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_LABEL, REG_NONE, REG_NONE, 0, vbody));
				append_ir_inst(&out, &out_count, &out_cap, t[p + 4]);
				append_ir_inst(&out, &out_count, &out_cap, t[p + 5]);
				append_ir_inst(&out, &out_count, &out_cap, t[p + 6]);
				append_ir_inst(&out, &out_count, &out_cap,
					make_vec_ir(IR_VMOVDQU_LOAD, REG_YMM1, address, REG_NONE));
				append_ir_inst(&out, &out_count, &out_cap,
					make_vec_ir(IR_VPADDQ, REG_YMM0, REG_YMM0, REG_YMM1));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_ADD_REG_IMM, ind, REG_NONE, 4, 0));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_CMP_REG_IMM, ind, REG_NONE, vector_end, 0));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_JNE, REG_NONE, REG_NONE, 0, vbody));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_SUB_REG_IMM, REG_RSP, REG_NONE, 32, 0));
				append_ir_inst(&out, &out_count, &out_cap,
					make_vec_ir(IR_VMOVDQU_STORE, REG_RSP, REG_YMM0, REG_NONE));
				for (int off = 0; off < 32; off += 8) {
					append_ir_inst(&out, &out_count, &out_cap,
						make_ir(IR_MOV_REG_MEM, value, REG_RSP, off, 0));
					append_ir_inst(&out, &out_count, &out_cap,
						make_ir(IR_ADD_REG_REG, acc, value, 0, 0));
				}
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_ADD_REG_IMM, REG_RSP, REG_NONE, 32, 0));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_VZEROUPPER, REG_NONE, REG_NONE, 0, 0));
				append_ir_inst(&out, &out_count, &out_cap,
					make_ir(IR_LABEL, REG_NONE, REG_NONE, 0, tail));
				for (int q = 1; q <= 11; ++q)
					append_ir_inst(&out, &out_count, &out_cap, t[p + q]);
				for (int q = 12; q <= exit_offset; ++q)
					append_ir_inst(&out, &out_count, &out_cap, t[p + q]);
				p += exit_offset + 1;
				matched = 1;
			}
		}
		if (!matched && p > 0 && p + 11 < n &&
			t[p - 1].IrNode == IR_MOV_REG_IMM &&
			t[p].IrNode == IR_LABEL &&
			t[p + 1].IrNode == IR_CMP_REG_IMM && t[p + 2].IrNode == IR_JGE &&
			t[p + 3].IrNode == IR_LABEL &&
			t[p + 4].IrNode == IR_IMUL_REG_IMM && t[p + 4].imm == 8 &&
			t[p + 5].IrNode == IR_LEA_IDX && t[p + 5].src == t[p + 4].dst &&
			t[p + 6].IrNode == IR_MOV_REG_MEM && t[p + 6].src == t[p + 5].dst &&
			t[p + 7].IrNode == IR_ADD_REG_REG && t[p + 7].src == t[p + 6].dst &&
			t[p + 8].IrNode == IR_ADD_REG_IMM && t[p + 8].imm == 1 &&
			t[p + 9].IrNode == IR_CMP_REG_IMM &&
			(t[p + 10].IrNode == IR_JNE || t[p + 10].IrNode == IR_JL) &&
			t[p + 10].label_id == t[p + 3].label_id &&
			((t[p + 11].IrNode == IR_LABEL &&
			  t[p + 11].label_id == t[p + 2].label_id) ||
			 (p + 12 < n && t[p + 11].IrNode == IR_JMP &&
			  t[p + 11].label_id == t[p + 2].label_id &&
			  t[p + 12].IrNode == IR_LABEL &&
			  t[p + 12].label_id == t[p + 2].label_id))) {
			int exit_offset = t[p + 11].IrNode == IR_JMP ? 12 : 11;
			IrReg ind = t[p + 1].dst;
			int64_t start = t[p - 1].imm, bound = t[p + 1].imm;
			IrReg scale = t[p + 4].dst, address = t[p + 5].dst;
			IrReg base = (IrReg)t[p + 5].imm;
			IrReg value = t[p + 6].dst, acc = t[p + 7].dst;
			IrReg roles[6] = { ind, scale, address, base, value, acc };
			bool roles_distinct = true;
			for (int a = 0; a < 6; ++a) {
				if (roles[a] < REG_RAX || roles[a] > REG_R15) roles_distinct = false;
				for (int b = a + 1; b < 6; ++b)
					if (roles[a] == roles[b]) roles_distinct = false;
			}
			if (t[p - 1].dst == ind && t[p + 4].src == ind &&
				t[p + 8].dst == ind && t[p + 9].dst == ind &&
				t[p + 9].imm == bound && bound > start && roles_distinct) {
				uint64_t trip_count = (uint64_t)bound - (uint64_t)start;
				DecisionResult vector_decision;
				if (decision_choose_loop(trip_count, 7, 1, 1, 4, 4,
					64, &vector_decision) != DECISION_VECTORIZE) {
					append_ir_inst(&out, &out_count, &out_cap, t[p++]);
					continue;
				}
				int64_t vector_end = bound - (int64_t)(trip_count & UINT64_C(3));
				int vbody = ++max_label, tail = ++max_label;
				append_ir_inst(&out, &out_count, &out_cap, t[p]);
				append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_CMP_REG_IMM, ind, REG_NONE, vector_end, 0));
				append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_JGE, REG_NONE, REG_NONE, 0, tail));
				append_ir_inst(&out, &out_count, &out_cap, make_vec_ir(IR_VPXOR, REG_YMM0, REG_YMM0, REG_YMM0));
				append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_LABEL, REG_NONE, REG_NONE, 0, vbody));
				append_ir_inst(&out, &out_count, &out_cap, t[p + 4]);
				append_ir_inst(&out, &out_count, &out_cap, t[p + 5]);
				append_ir_inst(&out, &out_count, &out_cap, make_vec_ir(IR_VMOVDQU_LOAD, REG_YMM1, t[p + 5].dst, REG_NONE));
				append_ir_inst(&out, &out_count, &out_cap, make_vec_ir(IR_VPADDQ, REG_YMM0, REG_YMM0, REG_YMM1));
				append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_ADD_REG_IMM, ind, REG_NONE, 4, 0));
				append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_CMP_REG_IMM, ind, REG_NONE, vector_end, 0));
				append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_JNE, REG_NONE, REG_NONE, 0, vbody));
				append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_SUB_REG_IMM, REG_RSP, REG_NONE, 32, 0));
				append_ir_inst(&out, &out_count, &out_cap, make_vec_ir(IR_VMOVDQU_STORE, REG_RSP, REG_YMM0, REG_NONE));
				for (int off = 0; off < 32; off += 8) {
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_MOV_REG_MEM, value, REG_RSP, off, 0));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_ADD_REG_REG, acc, value, 0, 0));
				}
				append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_ADD_REG_IMM, REG_RSP, REG_NONE, 32, 0));
				append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_VZEROUPPER, REG_NONE, REG_NONE, 0, 0));
				append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_LABEL, REG_NONE, REG_NONE, 0, tail));
				append_ir_inst(&out, &out_count, &out_cap, t[p + 1]);
				append_ir_inst(&out, &out_count, &out_cap, t[p + 2]);
				append_ir_inst(&out, &out_count, &out_cap, t[p + 3]);
				for (int q = 4; q <= 10; ++q) append_ir_inst(&out, &out_count, &out_cap, t[p + q]);
				for (int q = 11; q <= exit_offset; ++q)
					append_ir_inst(&out, &out_count, &out_cap, t[p + q]);
				p += exit_offset + 1;
				matched = 1;
			}
		}
		if (!matched) append_ir_inst(&out, &out_count, &out_cap, t[p++]);
	}
	free(ir->text);
	ir->text = out; ir->text_count = out_count; ir->text_cap = out_cap;
}

/* Convert canonical i=0; i<bound; ++i loops to a compact countdown when i
 * is otherwise unused.  This is ordinary induction-variable elimination:
 * indexed loops and loops that observe i are deliberately left unchanged. */
void ir_opt_countdown_loops(IrBuffer *ir) {
	if (!ir || !ir->text || ir->text_count < 8) return;
	IrInst *t = ir->text;
	int n = ir->text_count, max_label = 0;
	for (int i = 0; i < n; ++i)
		if (t[i].IrNode == IR_LABEL && t[i].label_id > max_label) max_label = t[i].label_id;
	IrInst *out = NULL;
	int out_count = 0, out_cap = 0;
	for (int p = 0; p < n; ) {
		bool matched = false;
		if (p + 6 < n && t[p].IrNode == IR_LABEL &&
		    (t[p + 1].IrNode == IR_CMP_REG_REG ||
		     t[p + 1].IrNode == IR_CMP_REG_IMM) && t[p + 2].IrNode == IR_JGE &&
		    out_count > 0 && out[out_count - 1].IrNode == IR_JMP &&
		    out[out_count - 1].label_id == t[p].label_id) {
			IrReg induction = t[p + 1].dst;
			bool immediate_bound = t[p + 1].IrNode == IR_CMP_REG_IMM;
			IrReg bound = immediate_bound ? induction : t[p + 1].src;
			int64_t bound_imm = t[p + 1].imm;
			int header = t[p].label_id, exit_label = t[p + 2].label_id;
			bool initialized_zero = false;
			int init_index = -1;
			for (int q = out_count - 2; q >= 0 && out[q].IrNode != IR_LABEL &&
			     out[q].IrNode != IR_LABEL_NAMED; --q)
				if (out[q].IrNode == IR_MOV_REG_IMM && out[q].dst == induction && out[q].imm == 0) {
					initialized_zero = true;
					init_index = q;
				}
			int bs = p + 3;
			if (bs < n && t[bs].IrNode == IR_LABEL) bs++;
			int step = -1;
			for (int q = bs; q + 1 < n; ++q) {
				if (t[q].IrNode == IR_ADD_REG_IMM && t[q].dst == induction && t[q].imm == 1 &&
				    t[q + 1].IrNode == IR_JMP && t[q + 1].label_id == header) { step = q; break; }
				if (t[q].IrNode == IR_LABEL || t[q].IrNode == IR_LABEL_NAMED || t[q].IrNode == IR_RET) break;
			}
			bool legal = initialized_zero && step > bs &&
				(immediate_bound ? bound_imm > 0 : bound != induction);
			for (int q = bs; legal && q < step; ++q) {
				IrInst *i = &t[q];
				if (i->dst == induction || i->src == induction || i->src2 == induction ||
				    (i->IrNode == IR_LEA_IDX && (IrReg)i->imm == induction)) legal = false;
				switch (i->IrNode) {
				case IR_JMP: case IR_JE: case IR_JNE: case IR_JZ: case IR_JNZ:
				case IR_JG: case IR_JGE: case IR_JL: case IR_JLE:
				case IR_JA: case IR_JAE: case IR_JB: case IR_JBE:
				case IR_CALL_LABEL: case IR_CALL_EXTERN: case IR_CALL_REG:
				case IR_RET: case IR_LABEL: case IR_LABEL_NAMED:
					legal = false; break;
				default: break;
				}
			}
			if (legal && step + 2 < n && t[step + 2].IrNode == IR_LABEL &&
			    t[step + 2].label_id == exit_label) {
				if (immediate_bound) out[init_index].imm = bound_imm;
				int body_label = ++max_label;
				append_ir_inst(&out, &out_count, &out_cap, t[p]);
				append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_CMP_REG_IMM, bound, REG_NONE, 0, 0));
				append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_JLE, REG_NONE, REG_NONE, 0, exit_label));
				append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_LABEL, REG_NONE, REG_NONE, 0, body_label));
				for (int q = bs; q < step; ++q) append_ir_inst(&out, &out_count, &out_cap, t[q]);
				append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_SUB_REG_IMM, bound, REG_NONE, 1, 0));
				append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_JNE, REG_NONE, REG_NONE, 0, body_label));
				p = step + 2;
				matched = true;
			}
		}
		if (!matched) append_ir_inst(&out, &out_count, &out_cap, t[p++]);
	}
	free(ir->text);
	ir->text = out; ir->text_count = out_count; ir->text_cap = out_cap;
}

static bool unroll_inst_mentions_reg(const IrInst *i, IrReg reg) {
	if (i->dst == reg || i->src == reg || i->src2 == reg) return true;
	if (i->IrNode == IR_LEA_IDX && (IrReg)i->imm == reg) return true;
	return false;
}

/* SSA basic-block numbers are local to each function, while the encoder's
 * patch table is module-wide.  Rewrite every function-local numeric label to
 * a unique module label after all transformations have finished. */
void ir_opt_uniquify_labels(IrBuffer *ir) {
	if (!ir || !ir->text || ir->text_count == 0) return;
	int next_label = 0;
	for (int i = 0; i < ir->text_count; ++i)
		if (ir->text[i].IrNode == IR_LABEL && ir->text[i].label_id > next_label)
			next_label = ir->text[i].label_id;
	for (int start = 0; start < ir->text_count; ) {
		while (start < ir->text_count && ir->text[start].IrNode != IR_LABEL_NAMED) start++;
		if (start >= ir->text_count) break;
		int end = start + 1;
		while (end < ir->text_count && ir->text[end].IrNode != IR_LABEL_NAMED) end++;
		int max_old = 0;
		for (int i = start; i < end; ++i)
			if (ir->text[i].IrNode == IR_LABEL && ir->text[i].label_id > max_old)
				max_old = ir->text[i].label_id;
		if (max_old > 0) {
			int *map = (int *)calloc((size_t)max_old + 1, sizeof(int));
			for (int i = start; i < end; ++i) {
				IrInst *inst = &ir->text[i];
				if (inst->IrNode == IR_LABEL && inst->label_id > 0) {
					int old = inst->label_id;
					if (!map[old]) map[old] = ++next_label;
				}
			}
			for (int i = start; i < end; ++i) {
				IrInst *inst = &ir->text[i];
				int old = inst->label_id;
				if (old > 0 && old <= max_old && map[old]) inst->label_id = map[old];
			}
			free(map);
		}
		start = end;
	}
}

/* Repair a late physical-register hazard introduced when an immutable
 * parameter load is hoisted before a loop but the legacy lowering reuses the
 * same hardware register inside that loop.  Only affected canonical loop
 * bounds are reloaded; genuinely resident bounds keep the fast path. */
static bool repair_is_direct_branch(IrOpcode op) {
	return op == IR_JMP || (op >= IR_JE && op <= IR_JBE);
}

void ir_opt_repair_loop_bounds(IrBuffer *ir) {
	if (!ir || !ir->text || ir->text_count < 6) return;
	IrInst *t = ir->text;
	int n = ir->text_count;
	bool *reload = (bool *)calloc((size_t)n, sizeof(bool));
	int *slots = (int *)calloc((size_t)n, sizeof(int));
	IrReg *reload_regs = (IrReg *)calloc((size_t)n, sizeof(IrReg));
	int repairs = 0;
	for (int p = 2; p + 2 < n; ++p) {
		if (t[p].IrNode != IR_LABEL || t[p + 1].IrNode != IR_CMP_REG_REG ||
		    t[p + 2].IrNode != IR_JGE || t[p - 1].IrNode != IR_JMP ||
		    t[p - 2].IrNode != IR_MOV_REG_MEM || t[p - 2].src != REG_RBP ||
		    t[p - 2].dst != t[p + 1].src) continue;
		IrReg bound = t[p + 1].src;
		int exit_label = t[p + 2].label_id;
		bool clobbered = false;
		for (int q = p + 3; q < n; ++q) {
			if (t[q].IrNode == IR_LABEL && t[q].label_id == exit_label) break;
			if (t[q].IrNode == IR_RET || t[q].IrNode == IR_LABEL_NAMED) break;
			if (t[q].dst == bound && t[q].IrNode != IR_CMP_REG_REG &&
			    t[q].IrNode != IR_CMP_REG_IMM && t[q].IrNode != IR_TEST_REG_REG &&
			    t[q].IrNode != IR_MOV_MEM_REG && t[q].IrNode != IR_VMOVDQU_STORE) {
				clobbered = true; break;
			}
		}
		if (clobbered) {
			static const IrReg candidates[] = {
				REG_R10, REG_R9, REG_R8, REG_RCX, REG_RDX,
				REG_RBX, REG_RDI, REG_RSI, REG_R12, REG_R14, REG_R15
			};
			IrReg spare = REG_NONE;
			for (size_t ci = 0; ci < sizeof(candidates) / sizeof(candidates[0]); ++ci) {
				IrReg c = candidates[ci];
				bool used = (c == t[p + 1].dst);
				for (int q = p; !used && q < n; ++q) {
					if (q > p && t[q].IrNode == IR_LABEL && t[q].label_id == exit_label) break;
					if (t[q].IrNode == IR_RET || t[q].IrNode == IR_LABEL_NAMED) break;
					if (t[q].dst == c || t[q].src == c || t[q].src2 == c ||
					    (t[q].IrNode == IR_LEA_IDX && (IrReg)t[q].imm == c)) used = true;
				}
				if (!used) { spare = c; break; }
			}
			if (spare != REG_NONE) {
				t[p - 2].dst = spare;
				/* The unroller has already copied the old bound into its
				 * remaining-count and scalar-tail guards.  Retarget every
				 * structural guard, but not arbitrary body values that merely
				 * reuse the old hardware register. */
				for (int q = p; q < n; ++q) {
					if (q > p && t[q].IrNode == IR_LABEL && t[q].label_id == exit_label) break;
					if (t[q].IrNode == IR_RET || t[q].IrNode == IR_LABEL_NAMED) break;
					if (t[q].IrNode == IR_CMP_REG_REG && t[q].dst == t[p + 1].dst &&
					    t[q].src == bound)
						t[q].src = spare;
					if (t[q].IrNode == IR_MOV_REG_REG && t[q].dst == REG_R11 &&
					    t[q].src == bound)
						t[q].src = spare;
				}
			} else {
				reload[p] = true;
				slots[p] = (int)t[p - 2].imm;
				reload_regs[p] = t[p + 1].src;
				repairs++;
			}
		}
	}
	/* Inlined CFG loops do not necessarily have the legacy JMP/load/header
	 * prefix above. Trace a compare bound through nearby copies back to an RBP
	 * slot and reload it only when the body overwrites that physical register. */
	for (int p = 0; p < n; ++p) {
		if (t[p].IrNode != IR_LABEL || reload[p]) continue;
		int cmp = -1, back = -1;
		for (int q = p + 1; q < n && q <= p + 12; ++q)
			if (t[q].IrNode == IR_CMP_REG_REG) { cmp = q; break; }
		if (cmp < 0) continue;
		for (int q = cmp + 1; q < n; ++q) {
			/* A return terminates its block, not necessarily the function: cold
			 * loop arms may be laid out after an outlined return block. */
			if (t[q].IrNode == IR_LABEL_NAMED) break;
			if (repair_is_direct_branch(t[q].IrNode) && t[q].label_id == t[p].label_id) { back = q; break; }
		}
		if (back < 0) continue;
		IrReg bound = t[cmp].src, cursor = bound;
		int slot = 0, found_slot = 0;
		for (int q = p - 1; q >= 0 && q >= p - 16; --q) {
			if (t[q].IrNode == IR_MOV_REG_REG && t[q].dst == cursor) cursor = t[q].src;
			else if (t[q].IrNode == IR_MOV_REG_MEM && t[q].dst == cursor && t[q].src == REG_RBP) {
				slot = (int)t[q].imm; found_slot = 1; break;
			}
		}
		if (!found_slot) continue;
		int clobbered = 0;
		for (int q = cmp + 1; q < back; ++q)
			if (t[q].dst == bound && t[q].IrNode != IR_CMP_REG_REG &&
			    t[q].IrNode != IR_MOV_MEM_REG) { clobbered = 1; break; }
		if (clobbered) {
			reload[p] = true; slots[p] = slot; reload_regs[p] = bound; repairs++;
		}
	}
	if (repairs) {
		IrInst *out = (IrInst *)malloc((size_t)(n + repairs) * sizeof(*out));
		int w = 0;
		for (int p = 0; p < n; ++p) {
			out[w++] = t[p];
			if (reload[p]) out[w++] = make_ir(IR_MOV_REG_MEM, reload_regs[p], REG_RBP, slots[p], 0);
		}
		free(ir->text);
		ir->text = out; ir->text_count = w; ir->text_cap = n + repairs;
	}
	free(slots);
	free(reload_regs);
	free(reload);
}

void ir_opt_unroll4_remainder(IrBuffer *ir) {
	if (!ir || !ir->text || ir->text_count < 8) return;
	IrInst *t = ir->text;
	int n = ir->text_count;
	int max_label = 0;
	for (int i = 0; i < n; ++i)
		if (t[i].IrNode == IR_LABEL && t[i].label_id > max_label) max_label = t[i].label_id;

	IrInst *out = NULL;
	int out_count = 0, out_cap = 0;
	for (int p = 0; p < n; ) {
		bool matched = false;
		/* Canonical countdown loop produced by ir_opt_countdown_loops:
		 *
		 *   H: cmp count, 0; jle exit
		 *   B: body; sub count, 1; jne B
		 *
		 * Peel one iteration for odd counts, then run two body copies per
		 * back edge.  R10/R11 are used only when both are completely absent
		 * from the containing function, so no live physical value is stolen. */
		if (!matched && p + 7 < n && t[p].IrNode == IR_LABEL &&
		    t[p + 1].IrNode == IR_CMP_REG_IMM && t[p + 1].imm == 0 &&
		    t[p + 2].IrNode == IR_JLE && t[p + 3].IrNode == IR_LABEL) {
			IrReg count = t[p + 1].dst;
			int body_label = t[p + 3].label_id;
			int exit_label = t[p + 2].label_id;
			int step = -1;
			for (int q = p + 4; q + 1 < n; ++q) {
				if (t[q].IrNode == IR_SUB_REG_IMM && t[q].dst == count && t[q].imm == 1 &&
				    t[q + 1].IrNode == IR_JNE && t[q + 1].label_id == body_label) {
					step = q; break;
				}
				if (t[q].IrNode == IR_LABEL || t[q].IrNode == IR_LABEL_NAMED ||
				    t[q].IrNode == IR_RET) break;
			}
			if (step > p + 4 && step + 2 < n &&
			    t[step + 2].IrNode == IR_LABEL && t[step + 2].label_id == exit_label) {
				bool legal = count != REG_R10 && count != REG_R11;
				for (int q = p + 4; legal && q < step; ++q) {
					if (!unroll_body_inst_ok(&t[q]) || t[q].dst == count) legal = false;
				}
				/* Scratch registers must be absent from the entire function, not
				 * merely the loop, because values may be live across the loop. */
				int fs = p;
				while (fs > 0 && t[fs].IrNode != IR_LABEL_NAMED) --fs;
				int fe = step + 2;
				while (fe < n && t[fe].IrNode != IR_LABEL_NAMED) ++fe;
				for (int q = fs; legal && q < fe; ++q) {
					IrInst *x = &t[q];
					if (x->dst == REG_R10 || x->src == REG_R10 || x->src2 == REG_R10 ||
					    x->dst == REG_R11 || x->src == REG_R11 || x->src2 == REG_R11 ||
					    (x->IrNode == IR_LEA_IDX &&
					     ((IrReg)x->imm == REG_R10 || (IrReg)x->imm == REG_R11)))
						legal = false;
				}
				DecisionResult unroll_decision;
				DecisionKind unroll_kind = legal
					? decision_choose_loop(UINT64_MAX, (uint32_t)(step - (p + 4)),
						1, 0, 2, 6, 128, &unroll_decision)
					: DECISION_KEEP;
				if (legal && unroll_kind == DECISION_UNROLL && unroll_decision.parameter == 2) {
					int tail_label = ++max_label;
					append_ir_inst(&out, &out_count, &out_cap, t[p]);
					append_ir_inst(&out, &out_count, &out_cap, t[p + 1]);
					append_ir_inst(&out, &out_count, &out_cap, t[p + 2]);
					append_ir_inst(&out, &out_count, &out_cap,
					               make_ir(IR_MOV_REG_REG, REG_R11, count, 0, 0));
					append_ir_inst(&out, &out_count, &out_cap,
					               make_ir(IR_MOV_REG_IMM, REG_R10, REG_NONE, 1, 0));
					append_ir_inst(&out, &out_count, &out_cap,
					               make_ir(IR_AND_REG_REG, REG_R11, REG_R10, 0, 0));
					append_ir_inst(&out, &out_count, &out_cap,
					               make_ir(IR_TEST_REG_REG, REG_R11, REG_R11, 0, 0));
					append_ir_inst(&out, &out_count, &out_cap,
					               make_ir(IR_JNZ, REG_NONE, REG_NONE, 0, tail_label));
					append_ir_inst(&out, &out_count, &out_cap, t[p + 3]);
					for (int copy = 0; copy < 2; ++copy)
						for (int q = p + 4; q < step; ++q)
							append_ir_inst(&out, &out_count, &out_cap, t[q]);
					append_ir_inst(&out, &out_count, &out_cap,
					               make_ir(IR_SUB_REG_IMM, count, REG_NONE, 2, 0));
					append_ir_inst(&out, &out_count, &out_cap,
					               make_ir(IR_JNE, REG_NONE, REG_NONE, 0, body_label));
					append_ir_inst(&out, &out_count, &out_cap,
					               make_ir(IR_JMP, REG_NONE, REG_NONE, 0, exit_label));
					append_ir_inst(&out, &out_count, &out_cap,
					               make_ir(IR_LABEL, REG_NONE, REG_NONE, 0, tail_label));
					for (int q = p + 4; q < step; ++q)
						append_ir_inst(&out, &out_count, &out_cap, t[q]);
					append_ir_inst(&out, &out_count, &out_cap, t[step]);
					append_ir_inst(&out, &out_count, &out_cap,
					               make_ir(IR_JNE, REG_NONE, REG_NONE, 0, body_label));
					p = step + 2;
					matched = true;
				}
			}
		}
		if (!matched && p + 7 < n && t[p].IrNode == IR_LABEL &&
		    t[p + 1].IrNode == IR_CMP_REG_REG && t[p + 2].IrNode == IR_JGE) {
			int header_label = t[p].label_id;
			IrReg induction = t[p + 1].dst;
			IrReg bound = t[p + 1].src;
			int exit_label = t[p + 2].label_id;
			int body_start = p + 3;
			if (body_start < n && t[body_start].IrNode == IR_LABEL) body_start++;
			int step = -1;
			for (int q = body_start; q + 1 < n; ++q) {
				if (t[q].IrNode == IR_ADD_REG_IMM && t[q].dst == induction && t[q].imm == 1 &&
				    t[q + 1].IrNode == IR_JMP && t[q + 1].label_id == header_label) {
					step = q; break;
				}
				if (t[q].IrNode == IR_LABEL || t[q].IrNode == IR_LABEL_NAMED || t[q].IrNode == IR_RET) break;
			}
			if (step > body_start && step + 2 < n &&
			    t[step + 2].IrNode == IR_LABEL && t[step + 2].label_id == exit_label &&
			    induction != REG_R11 && bound != REG_R11) {
				bool legal = true;
				for (int q = body_start; q < step; ++q)
					if (!unroll_body_inst_ok(&t[q]) ||
					    unroll_inst_mentions_reg(&t[q], induction)) {
						legal = false; break;
					}
				DecisionResult unroll_decision;
				DecisionKind unroll_kind = legal
					? decision_choose_loop(UINT64_MAX, (uint32_t)(step - body_start),
						1, 0, 4, 6, 128, &unroll_decision)
					: DECISION_KEEP;
				if (legal && unroll_kind == DECISION_UNROLL && unroll_decision.parameter == 4) {
					int tail_label = ++max_label;
					append_ir_inst(&out, &out_count, &out_cap, t[p]);
					append_ir_inst(&out, &out_count, &out_cap, t[p + 1]);
					append_ir_inst(&out, &out_count, &out_cap, t[p + 2]);
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_MOV_REG_REG, REG_R11, bound, 0, 0));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_SUB_REG_REG, REG_R11, induction, 0, 0));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_CMP_REG_IMM, REG_R11, REG_NONE, 4, 0));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_JL, REG_NONE, REG_NONE, 0, tail_label));
					for (int copy = 0; copy < 4; ++copy)
						for (int q = body_start; q < step; ++q)
							append_ir_inst(&out, &out_count, &out_cap, t[q]);
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_ADD_REG_IMM, induction, REG_NONE, 4, 0));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_JMP, REG_NONE, REG_NONE, 0, header_label));
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_LABEL, REG_NONE, REG_NONE, 0, tail_label));
					append_ir_inst(&out, &out_count, &out_cap, t[p + 1]);
					append_ir_inst(&out, &out_count, &out_cap, t[p + 2]);
					for (int q = body_start; q < step; ++q)
						append_ir_inst(&out, &out_count, &out_cap, t[q]);
					append_ir_inst(&out, &out_count, &out_cap, t[step]);
					append_ir_inst(&out, &out_count, &out_cap, make_ir(IR_JMP, REG_NONE, REG_NONE, 0, tail_label));
					p = step + 2;
					matched = true;
				}
			}
		}
		if (!matched) append_ir_inst(&out, &out_count, &out_cap, t[p++]);
	}
	free(ir->text);
	ir->text = out; ir->text_count = out_count; ir->text_cap = out_cap;
}

/* ========== 战役四: ILP 指令并行调度 ========== */
/* Fold two adjacent three-register sum rotations after loop unrolling:
 *   t=a+b; a=b; b=t; t=a+b; a=b; b=t
 * into:
 *   a+=b; b+=a
 * This is a register data-flow identity and never examines source names.
 * ADD changes flags, so the following instruction must overwrite ZF. */
static bool overwrites_zf(const IrInst *i) {
	switch (i->IrNode) {
	case IR_ADD_REG_REG: case IR_ADD_REG_IMM:
	case IR_SUB_REG_REG: case IR_SUB_REG_IMM:
	case IR_XOR_REG_REG: case IR_XOR_REG_IMM:
	case IR_AND_REG_REG: case IR_AND_REG_IMM: case IR_OR_REG_REG:
	case IR_CMP_REG_REG: case IR_CMP_REG_IMM:
	case IR_TEST_REG_REG:
		return true;
	default:
		return false;
	}
}

static bool vector_reg_mentioned(const IrInst *i, IrReg reg) {
	return i && (i->dst == reg || i->src == reg || i->src2 == reg ||
		(IrReg)i->imm == reg);
}

static bool vector_copy_result_unused(const IrInst *body, int at, int count) {
	if (!body || at < 0 || at >= count || body[at].IrNode != IR_MOV_REG_REG)
		return false;
	IrReg dst = body[at].dst;
	for (int i = at + 1; i < count; ++i)
		if (vector_reg_mentioned(&body[i], dst)) return false;
	return true;
}

static bool rotation_reg_read(const IrInst *i, IrReg reg) {
	if (i->src == reg || i->src2 == reg ||
	    (i->IrNode == IR_LEA_IDX && (IrReg)i->imm == reg))
		return true;
	switch (i->IrNode) {
	case IR_ADD_REG_REG: case IR_ADD_REG_IMM:
	case IR_SUB_REG_REG: case IR_SUB_REG_IMM:
	case IR_IMUL_REG_REG: case IR_IMUL_REG_IMM:
	case IR_XOR_REG_REG: case IR_XOR_REG_IMM:
	case IR_AND_REG_REG: case IR_AND_REG_IMM: case IR_OR_REG_REG:
	case IR_SHL_REG_IMM: case IR_SHR_REG_IMM: case IR_SAR_REG_IMM:
	case IR_SHL_REG_CL: case IR_SHR_REG_CL: case IR_SAR_REG_CL:
	case IR_INC_REG: case IR_NEG_REG: case IR_NOT_REG:
	case IR_CMP_REG_REG: case IR_CMP_REG_IMM: case IR_TEST_REG_REG:
	case IR_PUSH_REG:
		return i->dst == reg;
	default:
		return false;
	}
}

static bool rotation_reg_overwritten(const IrInst *i, IrReg reg) {
	if (i->dst != reg) return false;
	switch (i->IrNode) {
	case IR_MOV_REG_REG: case IR_MOV_REG_IMM: case IR_MOV_REG_MEM:
	case IR_LEA: case IR_LEA_RIP: case IR_LEA_IDX:
	case IR_MOVZX_REG_MEM8: case IR_MOVZX_REG8:
		return true;
	default:
		return false;
	}
}

/* The lowering can retain a source-level assignment of the recurrence result:
 *   t=a+b; dead=t; a=b; b=t
 * It is removable only when its value is not observed before the next
 * overwrite (or function end).  This is deliberately source-agnostic. */
static bool rotation_dead_store(const IrInst *t, int n, int at, IrReg tmp) {
	if (at >= n || t[at].IrNode != IR_MOV_REG_REG || t[at].src != tmp)
		return false;
	IrReg dead = t[at].dst;
	if (dead == REG_NONE || dead == tmp) return false;
	for (int q = at + 1; q < n; ++q) {
		if (t[q].IrNode == IR_LABEL_NAMED || t[q].IrNode == IR_RET)
			return true;
		if (rotation_reg_read(&t[q], dead)) return false;
		if (rotation_reg_overwritten(&t[q], dead)) return true;
	}
	return true;
}

void ir_opt_register_rotation(IrBuffer *ir) {
	if (!ir || !ir->text || ir->text_count < 7) return;
	IrInst *t = ir->text;
	int n = ir->text_count;
	bool *del = (bool *)calloc((size_t)n, sizeof(bool));
	for (int i = 0; i + 6 < n; ++i) {
		/* Same identity with one provably dead result copy per rotation. */
		if (i + 8 < n) {
			IrInst *s1 = &t[i], *d1 = &t[i + 1], *m1 = &t[i + 2], *m2 = &t[i + 3];
			IrInst *s2 = &t[i + 4], *d2 = &t[i + 5], *m3 = &t[i + 6], *m4 = &t[i + 7];
			if (s1->IrNode == IR_LEA_IDX && s2->IrNode == IR_LEA_IDX &&
			    m1->IrNode == IR_MOV_REG_REG && m2->IrNode == IR_MOV_REG_REG &&
			    m3->IrNode == IR_MOV_REG_REG && m4->IrNode == IR_MOV_REG_REG &&
			    overwrites_zf(&t[i + 8])) {
				IrReg tmp = s1->dst, a = s1->src, b = (IrReg)s1->imm;
				if (tmp != REG_NONE && a != REG_NONE && b != REG_NONE &&
				    tmp != a && tmp != b && a != b &&
				    d1->dst != a && d1->dst != b &&
				    d2->dst != a && d2->dst != b &&
				    m1->dst == a && m1->src == b && m2->dst == b && m2->src == tmp &&
				    s2->dst == tmp && s2->src == a && (IrReg)s2->imm == b &&
				    m3->dst == a && m3->src == b && m4->dst == b && m4->src == tmp &&
				    rotation_dead_store(t, n, i + 1, tmp) &&
				    rotation_dead_store(t, n, i + 5, tmp)) {
					s1->IrNode = IR_ADD_REG_REG;
					s1->dst = a; s1->src = b; s1->src2 = REG_NONE; s1->imm = 0;
					s2->IrNode = IR_ADD_REG_REG;
					s2->dst = b; s2->src = a; s2->src2 = REG_NONE; s2->imm = 0;
					del[i + 1] = del[i + 2] = del[i + 3] = true;
					del[i + 5] = del[i + 6] = del[i + 7] = true;
					i += 7;
					continue;
				}
			}
		}
		IrInst *s1 = &t[i], *m1 = &t[i + 1], *m2 = &t[i + 2];
		IrInst *s2 = &t[i + 3], *m3 = &t[i + 4], *m4 = &t[i + 5];
		if (s1->IrNode != IR_LEA_IDX || s2->IrNode != IR_LEA_IDX ||
		    m1->IrNode != IR_MOV_REG_REG || m2->IrNode != IR_MOV_REG_REG ||
		    m3->IrNode != IR_MOV_REG_REG || m4->IrNode != IR_MOV_REG_REG ||
		    !overwrites_zf(&t[i + 6])) continue;
		IrReg tmp = s1->dst;
		IrReg a = s1->src;
		IrReg b = (IrReg)s1->imm;
		if (tmp == REG_NONE || a == REG_NONE || b == REG_NONE ||
		    tmp == a || tmp == b || a == b) continue;
		if (m1->dst != a || m1->src != b || m2->dst != b || m2->src != tmp)
			continue;
		if (s2->dst != tmp || s2->src != a || (IrReg)s2->imm != b ||
		    m3->dst != a || m3->src != b || m4->dst != b || m4->src != tmp)
			continue;

		s1->IrNode = IR_ADD_REG_REG;
		s1->dst = a; s1->src = b; s1->src2 = REG_NONE; s1->imm = 0;
		s2->IrNode = IR_ADD_REG_REG;
		s2->dst = b; s2->src = a; s2->src2 = REG_NONE; s2->imm = 0;
		del[i + 1] = del[i + 2] = del[i + 4] = del[i + 5] = true;
		i += 5;
	}
	int w = 0;
	for (int r = 0; r < n; ++r)
		if (!del[r]) t[w++] = t[r];
	ir->text_count = w;
	free(del);
}

static bool ilp_is_barrier(IrOpcode IrNode) {
	switch (IrNode) {
	case IR_LABEL: case IR_LABEL_NAMED:
	case IR_JMP: case IR_JE: case IR_JNE: case IR_JZ: case IR_JNZ:
	case IR_JG: case IR_JGE: case IR_JL: case IR_JLE:
	case IR_JA: case IR_JAE: case IR_JB: case IR_JBE:
	case IR_CALL_LABEL: case IR_CALL_EXTERN: case IR_CALL_REG:
	case IR_RET: case IR_PUSH_REG: case IR_POP_REG:
	case IR_DATA_LABEL: case IR_DATA_BYTES: case IR_DATA_QWORD:
	case IR_DATA_QWORD_SYM: case IR_BSS_LABEL: case IR_BSS_RESQ:
	case IR_EXTERN: case IR_GLOBAL:
	case IR_CQO: case IR_IDIV_REG: case IR_IMUL_WIDE_REG: case IR_MUL_WIDE_REG:
	/* 写内存仍是硬屏障。普通只读加载和地址计算可以在寄存器依赖约束下
	 * 穿插，以覆盖加载延迟；RIP 相对地址保留为重定位屏障。 */
	case IR_MOV_MEM_REG: case IR_MOV_MEM_IMM: case IR_MOV_MEM8_REG:
	case IR_ADD_MEM_IMM: case IR_INC_MEM:
	case IR_LEA_RIP:
	case IR_VPADDQ: case IR_VPSUBQ: case IR_VPSADBW: case IR_VPXOR:
	case IR_VPAND: case IR_VPOR: case IR_VPMULLD: case IR_VPCMPEQB:
	case IR_VPCMPEQQ: case IR_VPBROADCASTB: case IR_VPBROADCASTQ:
	case IR_VMOVDQU_LOAD: case IR_VMOVDQU_STORE: case IR_VZEROUPPER:
		return true;
	case IR_MOVQ_XMM_REG: case IR_MOVQ_REG_XMM: case IR_MOVSD_XMM_MEM:
	case IR_CVTSI2SD: case IR_CVTTSD2SI:
	case IR_ADDSD: case IR_SUBSD: case IR_MULSD: case IR_DIVSD:
	case IR_UCOMISD: case IR_VFMADD132SD:
		/* The scalar scheduler has a GPR dependency model.  Until XMM
		 * registers have a separate mask namespace, crossing one of these can
		 * move a result extraction before the arithmetic that defines it. */
		return true;
	default:
		return false;
	}
}

static int ilp_writes_reg(IrInst *inst) {
	switch (inst->IrNode) {
	case IR_MOV_REG_REG: case IR_MOV_REG_IMM: case IR_MOV_REG_MEM:
	case IR_LEA: case IR_LEA_RIP: case IR_LEA_IDX:
	case IR_ADD_REG_REG: case IR_ADD_REG_IMM:
	case IR_SUB_REG_REG: case IR_SUB_REG_IMM:
	case IR_IMUL_REG_REG: case IR_IMUL_REG_IMM:
	case IR_XOR_REG_REG: case IR_XOR_REG_IMM:
	case IR_AND_REG_REG: case IR_AND_REG_IMM: case IR_OR_REG_REG:
	case IR_SHL_REG_IMM: case IR_SHR_REG_IMM: case IR_SAR_REG_IMM:
	case IR_INC_REG: case IR_NEG_REG: case IR_NOT_REG:
	case IR_MOVZX_REG_MEM8: case IR_MOVZX_REG8:
	case IR_SETE: case IR_SETNE: case IR_SETL: case IR_SETLE:
	case IR_SETG: case IR_SETGE: case IR_SETA: case IR_SETAE:
	case IR_SETB: case IR_SETBE: case IR_SETZ: case IR_SETNP:
		return ir_reg_id(inst->dst);
	default:
		return -1;
	}
}

static uint32_t ilp_reads_mask(IrInst *inst) {
	uint32_t mask = 0;
	int src_id = ir_reg_id(inst->src);
	if (src_id >= 0 && src_id < 16) mask |= (1u << src_id);
	switch (inst->IrNode) {
	case IR_ADD_REG_REG: case IR_ADD_REG_IMM:
	case IR_SUB_REG_REG: case IR_SUB_REG_IMM:
	case IR_IMUL_REG_REG: case IR_IMUL_REG_IMM:
	case IR_XOR_REG_REG: case IR_XOR_REG_IMM:
	case IR_AND_REG_REG: case IR_AND_REG_IMM: case IR_OR_REG_REG:
	case IR_SHL_REG_IMM: case IR_SHR_REG_IMM: case IR_SAR_REG_IMM:
	case IR_INC_REG: case IR_NEG_REG: case IR_NOT_REG:
	case IR_CMP_REG_REG: case IR_CMP_REG_IMM:
	case IR_TEST_REG_REG: {
		int dst_id = ir_reg_id(inst->dst);
		if (dst_id >= 0 && dst_id < 16) mask |= (1u << dst_id);
		break;
	}
	case IR_SHL_REG_CL: case IR_SHR_REG_CL: case IR_SAR_REG_CL: {
		int dst_id = ir_reg_id(inst->dst);
		if (dst_id >= 0 && dst_id < 16) mask |= (1u << dst_id);
		mask |= (1u << ir_reg_id(REG_RCX));
		break;
	}
	case IR_MOV_MEM_REG: case IR_MOV_MEM_IMM: case IR_MOV_MEM8_REG:
	case IR_ADD_MEM_IMM: case IR_INC_MEM: {
		int dst_id = ir_reg_id(inst->dst);
		if (dst_id >= 0 && dst_id < 16) mask |= (1u << dst_id);
		break;
	}
	case IR_MOV_REG_MEM: case IR_MOVZX_REG_MEM8: {
		int s = ir_reg_id(inst->src);
		if (s >= 0 && s < 16) mask |= (1u << s);
		break;
	}
	case IR_LEA: {
		int s = ir_reg_id(inst->src);
		if (s >= 0 && s < 16) mask |= (1u << s);
		break;
	}
	case IR_LEA_IDX: {
		int s = ir_reg_id(inst->src);
		if (s >= 0 && s < 16) mask |= (1u << s);
		if (inst->imm >= 0 && inst->imm < 16) mask |= (1u << (int)inst->imm);
		break;
	}
	default: break;
	}
	return mask;
}

static bool ilp_writes_flags(const IrInst *inst) {
	switch (inst->IrNode) {
	case IR_ADD_REG_REG: case IR_ADD_REG_IMM:
	case IR_SUB_REG_REG: case IR_SUB_REG_IMM:
	case IR_IMUL_REG_REG: case IR_IMUL_REG_IMM:
	case IR_XOR_REG_REG: case IR_XOR_REG_IMM:
	case IR_AND_REG_REG: case IR_AND_REG_IMM: case IR_OR_REG_REG:
	case IR_SHL_REG_IMM: case IR_SHR_REG_IMM: case IR_SAR_REG_IMM:
	case IR_SHL_REG_CL: case IR_SHR_REG_CL: case IR_SAR_REG_CL:
	case IR_INC_REG: case IR_NEG_REG:
	case IR_CMP_REG_REG: case IR_CMP_REG_IMM: case IR_TEST_REG_REG:
		return true;
	default:
		return false;
	}
}

static bool ilp_reads_flags(const IrInst *inst) {
	switch (inst->IrNode) {
	case IR_SETE: case IR_SETNE: case IR_SETL: case IR_SETLE:
	case IR_SETG: case IR_SETGE: case IR_SETA: case IR_SETAE:
	case IR_SETB: case IR_SETBE: case IR_SETZ: case IR_SETNP:
		return true;
	default:
		return false;
	}
}

static bool ilp_has_dependency(IrInst *a, IrInst *b) {
	int a_wr = ilp_writes_reg(a);
	int b_wr = ilp_writes_reg(b);
	uint32_t a_rd = ilp_reads_mask(a);
	uint32_t b_rd = ilp_reads_mask(b);
	if (a_wr >= 0 && (b_rd & (1u << a_wr))) return true;
	if (b_wr >= 0 && (a_rd & (1u << b_wr))) return true;
	if (a_wr >= 0 && b_wr >= 0 && a_wr == b_wr) return true;
	/* 标志寄存器也有 RAW/WAR/WAW；否则算术可能越过 cmp/setcc 改坏布尔值。 */
	if (ilp_writes_flags(a) && (ilp_reads_flags(b) || ilp_writes_flags(b))) return true;
	if (ilp_reads_flags(a) && ilp_writes_flags(b)) return true;
	return false;
}

#define ILP_WINDOW 32

void ir_opt_ilp_schedule(IrBuffer *ir) {
	if (!ir || !ir->text || ir->text_count < 3) return;
	IrInst *t = ir->text;
	int n = ir->text_count;
	int block_start = 0;
	for (int i = 0; i <= n; i++) {
		bool at_boundary = (i == n) || ilp_is_barrier(t[i].IrNode);
		if (at_boundary) {
			int block_end = i;
			int block_len = block_end - block_start;
			if (block_len >= 3) {
				for (int j = block_start; j < block_end - 1; j++) {
					int j_wr = ilp_writes_reg(&t[j]);
					uint32_t j1_rd = ilp_reads_mask(&t[j + 1]);
					if (j_wr < 0) continue;
					if (!(j1_rd & (1u << j_wr))) continue;
					int window_start = j - 1;
					if (window_start < block_start) continue;
					int window_limit = j - ILP_WINDOW;
					if (window_limit < block_start) window_limit = block_start;
					for (int c = window_start; c >= window_limit; c--) {
						if (ilp_is_barrier(t[c].IrNode)) break;
						bool can_move = true;
						for (int k = c + 1; k <= j + 1 && can_move; k++) {
							if (ilp_has_dependency(&t[c], &t[k])) can_move = false;
						}
						if (can_move) {
							IrInst saved = t[c];
							for (int m = c; m < j; m++) t[m] = t[m + 1];
							t[j] = saved;
							break;
						}
					}
				}
			}
			block_start = i + 1;
		}
	}
}

