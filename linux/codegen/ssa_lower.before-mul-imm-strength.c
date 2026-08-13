/* ssa_lower.c 闁?SSA 闁?x86-64 IR (Instruction Selection) 闂傚嫬绉舵?
 *
 * 閻庨潧瀚悺銊╁闯閵娿儱鐎婚梺鏉跨С缁狅綁宕ユ惔顖滅婵絽绻嬮柌?Virtual Register 闂侇喛濮ょ€氥垽寮垫径澶屽晩閻庣數鎳撶花鏌ユ儍閸曨厼鈷栭柣鐐叉閻﹀海鈧稒锚濞呮帡濡?
 */
#include "ir_ssa.h"
#include "ir.h"
#include "codegen.h"
#include <stdlib.h>
#include <string.h>

extern CodegenState *cg; // 閸忚京鏁ら崢鐔告拱閻?CodegenState閿涘奔濞囬悽銊ョ暊閻?cg->ir 缂傛挸鍟?

// 閺屻儴銆冪亸?Ssa閻ㄥ嫮澧块悶鍡楃槑鐎涙ê娅掔紓鏍у娇閺勭姴鐨犻崚?x86-64 IrReg
// 婵炲鍔嶉崜? R12 闁?Mira 闁轰胶澧楀畵渚€寮介崼鐔风樄闂佽棄鐗炵槐婕?3 濞ｅ洦绻勯弳鈧柨娑橆劏BP/RSP 濞戞挸绉村顒佺▔鎼粹€崇€婚梺?
/* 閸斻劍鈧礁褰夐柌蹇撶槑鐎涙ê娅掗弰鐘茬殸閿涙氨鏁遍悜顓炲閸掑棙鐎界紒鎾寸亯閸愬啿鐣?slot -> 閻椻晝鎮婄€靛嫬鐡ㄩ崳?*/
static SsaFunction *_cur_lower_func = NULL;
static IrReg map_phys_reg(int phys_reg);

static IrReg fIR_var_reg(int slot) {
	if (_cur_lower_func && _cur_lower_func->var_reg_map &&
	    slot >= 0 && slot < _cur_lower_func->var_count) {
		return (IrReg)_cur_lower_func->var_reg_map[slot];
	}
	return REG_NONE;
}

/* 鍙敤鐨勫揩閫熷彉閲忓瘎瀛樺櫒姹?
 * 鍓?7 涓?= callee-saved锛堟案杩滃畨鍏級
 * 鍚?3 涓?= caller-saved / volatile锛堥渶瑕佸湪 CALL 鍓嶅悗 PUSH/POP 淇濇姢锛?
 * VOLATILE_FIR_START 鏍囪 volatile 鍖哄煙鐨勮捣濮嬬储寮?*/
static const IrReg fIR_reg_pool[] = {
	REG_R13, REG_R14, REG_R15, REG_RBX, REG_RDI, REG_RSI, REG_R12,
	REG_R8, REG_R9
};
#define FIR_REG_POOL_SIZE 9
#define VOLATILE_FIR_START 7  /* fIR_reg_pool[7..8] are volatile */

static int function_uses_nonvolatile_fir(SsaFunction *func, IrReg reg) {
	if (!func || !func->var_reg_map) return 0;
	for (int bi = 0; bi < func->block_count; bi++) {
		for (SsaInst *inst = func->blocks[bi]->inst_head; inst; inst = inst->next) {
			if ((inst->IrNode == SSA_OP_LOAD_VAR &&
			     inst->op1.kind == SSA_OPND_IMM) ||
			    (inst->IrNode == SSA_OP_STORE_VAR &&
			     inst->op2.kind == SSA_OPND_IMM)) {
				int slot = inst->IrNode == SSA_OP_LOAD_VAR
					? (int)inst->op1.u.imm : (int)inst->op2.u.imm;
				if (slot >= 0 && slot < func->var_count &&
				    (IrReg)func->var_reg_map[slot] == reg) return 1;
			}
		}
	}
	return 0;
}

static int function_uses_nonvolatile(SsaFunction *func, IrReg reg) {
	if (function_uses_nonvolatile_fir(func, reg)) return 1;
	if (func && func->vreg_phys_map) {
		for (VReg v = 1; v < func->next_vreg; ++v)
			if (map_phys_reg(func->vreg_phys_map[v]) == reg) return 1;
	}
	return 0;
}

static int nonvolatile_fir_count(SsaFunction *func) {
	int count = 0;
	for (int i = 0; i < VOLATILE_FIR_START; i++)
		if (function_uses_nonvolatile(func, fIR_reg_pool[i])) count++;
	return count;
}

static int local_frame_size(SsaFunction *func) {
	int alignment = (nonvolatile_fir_count(func) & 1) ? 8 : 0;
	return alignment + func->spill_size;
}

/* === Caller-Saved 蹇€熷彉閲忕殑 Spill & Fill ===
 * 鍦ㄦ瘡娆?CALL/ICALL 涔嬪墠锛屾妸鍒嗛厤鍒?volatile 瀵勫瓨鍣ㄧ殑 fIR_var 鍘嬫爤淇濇姢锛?
 * CALL 杩斿洖鍚庣珛鍒诲脊鏍堟仮澶嶃€備繚鎶ら『搴忥細姝ｅ簭 PUSH锛岄€嗗簭 POP銆?*/

/* 缁熻褰撳墠鍑芥暟涓湁澶氬皯涓?fIR_var 琚垎閰嶅埌浜?volatile 瀵勫瓨鍣?*/
static int count_volatile_fIR_vars(SsaFunction *func) {
	if (!func || !func->var_reg_map) return 0;
	int count = 0;
	for (int i = 0; i < func->var_count; i++) {
		IrReg r = (IrReg)func->var_reg_map[i];
		if (r == REG_R8 || r == REG_R9) count++;
	}
	return count;
}

static void spill_volatile_fIR_vars(IrBuffer *ir, SsaFunction *func) {
	if (!func || !func->var_reg_map) return;
	for (int i = 0; i < func->var_count; i++) {
		IrReg r = (IrReg)func->var_reg_map[i];
		if (r == REG_R8 || r == REG_R9) {
			ir_push(ir, r);
		}
	}
}

static void fill_volatile_fIR_vars(IrBuffer *ir, SsaFunction *func) {
	if (!func || !func->var_reg_map) return;
	/* 閫嗗簭 POP锛屼笌 spill 鐨?PUSH 椤哄簭瀵圭О */
	for (int i = func->var_count - 1; i >= 0; i--) {
		IrReg r = (IrReg)func->var_reg_map[i];
		if (r == REG_R8 || r == REG_R9) {
			ir_pop(ir, r);
		}
	}
}

/* 鐑害鍒嗘瀽锛氱粺璁℃瘡涓?var slot 鍦ㄥ惊鐜腑鐨勪娇鐢ㄩ鐜囷紝鏈€鐑殑 7 涓嬁瀵勫瓨鍣?*/
static void compute_var_reg_map(SsaFunction *func, int var_count,
                                const bool *shared_slots) {
	if (var_count <= 0) { func->var_reg_map = NULL; func->var_count = 0; return; }

	func->var_count = var_count;
	func->var_reg_map = (int *)calloc(var_count, sizeof(int));
	for (int i = 0; i < var_count; i++) func->var_reg_map[i] = REG_NONE;

	int *hotness = (int *)calloc(var_count, sizeof(int));

	for (int bi = 0; bi < func->block_count; bi++) {
		SsaBasicBlock *b = func->blocks[bi];
		if (!b) continue;

		/* detect loop headers and bodies */


		int is_loop_header = 0;
		int is_loop_body = 0;
		for (int s = 0; s < b->succ_count; s++)
			if (b->succs[s]->id <= b->id) is_loop_body = 1;
		for (int p = 0; p < b->pred_count; p++)
			if (b->preds[p]->id > b->id) is_loop_header = 1;

		/* 寰幆澶寸殑鍙橀噺鏉冮噸 x300锛堟潯浠舵鏌ユ瘡杩唬蹇呰蛋锛夛紝寰幆浣?x100锛屽惊鐜 x1 */
		int weight = is_loop_header ? 300 : (is_loop_body ? 100 : 1);

		for (SsaInst *inst = b->inst_head; inst; inst = inst->next) {
			if (inst->IrNode == SSA_OP_LOAD_VAR || inst->IrNode == SSA_OP_STORE_VAR) {
				int slot = -1;
				if (inst->IrNode == SSA_OP_LOAD_VAR) slot = (int)inst->op1.u.imm;
				else slot = (int)inst->op2.u.imm;
				if (slot >= 0 && slot < var_count) hotness[slot] += weight;
			}
		}
	}

	int *sorted = (int *)malloc(var_count * sizeof(int));
	for (int i = 0; i < var_count; i++) sorted[i] = i;
	for (int i = 0; i < var_count - 1; i++) {
		for (int j = i + 1; j < var_count; j++) {
			if (hotness[sorted[j]] > hotness[sorted[i]]) {
				int tmp = sorted[i]; sorted[i] = sorted[j]; sorted[j] = tmp;
			}
		}
	}

	int assigned = 0;
	for (int i = 0; i < var_count && assigned < FIR_REG_POOL_SIZE; i++) {
		int slot = sorted[i];
		if (hotness[slot] > 0 && (!shared_slots || !shared_slots[slot])) {
			func->var_reg_map[slot] = (int)fIR_reg_pool[assigned];
			assigned++;
		}
	}

	free(hotness);
	free(sorted);
}

/* 闁绘せ鏅濋幃濠勨偓闈涘閻°劑宕抽妸锔叫侀悘? 濞?7 濞戞搩浜顏呯┍濠靛牊娈岄悗闈涘閻°劑宕抽妸銉ユ濞戞挸闄eg闁告帒妫濋崢?*/
static IrReg map_phys_reg(int phys_reg) {
	switch(phys_reg) {
	case 0: return REG_RAX;
	case 1: return REG_RCX;
	case 2: return REG_RDX;
	case 3: return REG_R8;
	case 4: return REG_R9;
	case 5: return REG_R13;
	case 6: return REG_R14;
	case 7: return REG_R15;
	case 8: return REG_RBX;
	case 9: return REG_RDI;
	case 10: return REG_RSI;
	case 11: return REG_R12;
	default: return REG_NONE;
	}
}

static IrReg get_ssa_reg(SsaFunction *f, VReg vreg) {
	if(vreg == 0 || vreg >= f->next_vreg) return REG_NONE;
	/* 闁繋绱舵导妯哄閿涙艾顩ч弸?vreg 閺夈儴鍤?LOAD_VAR 娑?slot 閺?fIR_var_reg閿涘瞼娲块幒銉ㄧ箲閸ョ偟澧块悶鍡楃槑鐎涙ê娅?*/
	if (f->vreg_defs && f->vreg_defs[vreg]) {
		SsaInst *def = f->vreg_defs[vreg];
		if (def->IrNode == SSA_OP_LOAD_VAR) {
			int slot = (int)def->op1.u.imm;
			IrReg fr = fIR_var_reg(slot);
			if (fr != REG_NONE) return fr;
		}
	}
	int phys = f->vreg_phys_map[vreg];
	if (phys == -1) {
		return REG_NONE;
	}
	return map_phys_reg(phys);
}

static IrReg get_ssa_vec_reg(SsaFunction *f, VReg vreg) {
	if (!f->vreg_vec_phys_map || vreg == 0 || vreg >= f->next_vreg) return REG_NONE;
	int phys = f->vreg_vec_phys_map[vreg];
	return (phys >= 0 && phys < 16) ? (IrReg)(REG_YMM0 + phys) : REG_NONE;
}

static IrReg get_ssa_float_reg(SsaFunction *f, VReg vreg) {
	if (!f->vreg_float_phys_map || vreg == 0 || vreg >= f->next_vreg) return REG_NONE;
	int phys = f->vreg_float_phys_map[vreg];
	return (phys >= 0 && phys < 3) ? (IrReg)(REG_XMM3 + phys) : REG_NONE;
}

typedef struct {
	int64_t multiplier;
	int shift;
} SignedDivMagic;

/* Hacker's Delight signed-division magic for a positive divisor > 1. */
static SignedDivMagic signed_div_magic(int64_t divisor) {
	uint64_t ad = (uint64_t)divisor;
	uint64_t two63 = UINT64_C(1) << 63;
	uint64_t anc = two63 - 1 - (two63 % ad);
	uint64_t q1 = two63 / anc, r1 = two63 - q1 * anc;
	uint64_t q2 = two63 / ad,  r2 = two63 - q2 * ad;
	uint64_t delta;
	int p = 63;
	do {
		p++;
		q1 <<= 1; r1 <<= 1;
		if (r1 >= anc) { q1++; r1 -= anc; }
		q2 <<= 1; r2 <<= 1;
		if (r2 >= ad) { q2++; r2 -= ad; }
		delta = ad - r2;
	} while (q1 < delta || (q1 == delta && r1 == 0));
	SignedDivMagic magic = { (int64_t)(q2 + 1), p - 64 };
	return magic;
}

/* Emit truncating signed division by a positive compile-time constant.
 * R10 keeps the dividend, R11 holds the magic or correction temporary. */
static void lower_sdiv_positive_const(IrBuffer *ir, IrReg dividend,
	                                  int64_t divisor, IrReg dst,
	                                  int want_remainder) {
	/* Signed division by a positive power of two can preserve truncation
	 * toward zero without the general magic-multiply sequence:
	 *   q = (x + ((x >> 63) & (d - 1))) >> log2(d)
	 * This is valid for every signed input, not merely non-negative ranges. */
	if (!want_remainder && divisor > 1 && divisor <= INT64_C(2147483648) &&
	    ((uint64_t)divisor & ((uint64_t)divisor - 1)) == 0) {
		int shift = 0;
		for (uint64_t d = (uint64_t)divisor; d > 1; d >>= 1) shift++;
		IrReg correction = dst != REG_R11 ? REG_R11 : REG_R10;
		if (dst != dividend) ir_mov_reg_reg(ir, dst, dividend);
		ir_mov_reg_reg(ir, correction, dst);
		ir_sar_reg_imm(ir, correction, 63);
		ir_and_reg_imm(ir, correction, divisor - 1);
		ir_add_reg_reg(ir, dst, correction);
		ir_sar_reg_imm(ir, dst, shift);
		return;
	}
	SignedDivMagic magic = signed_div_magic(divisor);
	if (dividend != REG_R10) ir_mov_reg_reg(ir, REG_R10, dividend);
	ir_mov_reg_reg(ir, REG_RAX, REG_R10);
	ir_mov_reg_imm(ir, REG_R11, magic.multiplier);
	ir_imul_wide(ir, REG_R11);
	if (magic.multiplier < 0) ir_add_reg_reg(ir, REG_RDX, REG_R10);
	if (magic.shift) ir_sar_reg_imm(ir, REG_RDX, magic.shift);
	ir_mov_reg_reg(ir, REG_R11, REG_RDX);
	ir_shr_reg_imm(ir, REG_R11, 63);
	ir_add_reg_reg(ir, REG_RDX, REG_R11);
	if (!want_remainder) {
		if (dst != REG_RDX) ir_mov_reg_reg(ir, dst, REG_RDX);
		return;
	}
	ir_imul_reg_imm(ir, REG_R11, REG_RDX, divisor);
	if (dst != REG_R10) ir_mov_reg_reg(ir, dst, REG_R10);
	ir_sub_reg_reg(ir, dst, REG_R11);
}

static int spill_offset(SsaFunction *f, VReg vreg) {
	if (!f->vreg_spill_map || vreg == 0 || vreg >= f->next_vreg) return 0;
	int slot = f->vreg_spill_map[vreg];
	int saves = nonvolatile_fir_count(f) * 8;
	int alignment = (nonvolatile_fir_count(f) & 1) ? 8 : 0;
	return slot >= 0 ? -(saves + alignment + 8 + slot * 8) : 0;
}

static int is_spilled(SsaFunction *f, VReg vreg) {
	return spill_offset(f, vreg) != 0;
}

/* 濞寸姴绨肩粩瀛樼▔?SSA 闁瑰灝绉崇紞鏃堝极妫颁浇鍘柟缁樺姇瑜板洭宕愰悡搴＄厒閻庨潧瀚悺銊╁闯閵婏絺鍋?
 * 閼汇儲鎼锋担婊勬殶閺?VREG閿涘瞼娲块幒銉ㄧ箲閸ョ偛鍙鹃悧鈺冩倞鐎靛嫬鐡ㄩ崳銊ｂ偓?
 * 闁兼眹鍎查幖閿嬫媴濠婂嫭娈堕柡?IMM闁挎稑鑻ぐ鍌滀焊?mov scratch_reg, imm 妤犵偞鍎肩换鎴﹀炊?scratch_reg闁?
 * scratch_reg 闁哄嫷鍨伴ˇ顒勬偨閵娿儳妲戦悗娑櫭▍鎺楁晬閸粎鐟濋柤瀹犳閹?dst_reg 闁告劘灏欓悰濠囨晬婢跺牃鍋?
 */
static IrReg load_operand(IrBuffer *ir, SsaFunction *func, SsaOperand opnd, IrReg scratch) {
	if (opnd.kind == SSA_OPND_VREG) {
		IrReg xr = get_ssa_float_reg(func, opnd.u.vreg);
		if (xr != REG_NONE) {
			ir_movq_reg_xmm(ir, scratch, xr);
			return scratch;
		}
		/* 闁繋绱舵导妯哄閿涙艾顩ч弸?vreg 閻ㄥ嫬鐣炬稊澶嬫Ц LOAD_VAR 娑?slot 閺堝鎻╅柅鐔风槑鐎涙ê娅掗敍宀€娲块幒銉ㄧ箲閸?fIR_var_reg閿?
		 * 鐠哄疇绻?vreg 閳?RAX/RCX 閻ㄥ嫪鑵戦梻?mov */
		VReg v = opnd.u.vreg;
		if (v > 0 && v < func->next_vreg && func->vreg_defs && func->vreg_defs[v]) {
			SsaInst *def = func->vreg_defs[v];
			if (def->IrNode == SSA_OP_LOAD_VAR) {
				int slot = (int)def->op1.u.imm;
				IrReg fr = fIR_var_reg(slot);
				if (fr != REG_NONE) return fr;
			}
		}
		IrReg r = get_ssa_reg(func, opnd.u.vreg);
		if (r != REG_NONE) return r;
		int off = spill_offset(func, opnd.u.vreg);
		if (off) {
			ir_mov_reg_mem(ir, scratch, REG_RBP, off);
			return scratch;
		}
		return REG_NONE;
	} else if (opnd.kind == SSA_OPND_IMM) {
		ir_mov_reg_imm(ir, scratch, opnd.u.imm);
		return scratch;
	}
	return REG_NONE;
}


void ssa_lower_function(SsaFunction *func, IrBuffer *ir) {
	_cur_lower_func = func;
	bool function_uses_avx2 = false;
	for (int bi = 0; bi < func->block_count; ++bi)
		for (SsaInst *si = func->blocks[bi]->inst_head; si; si = si->next)
			if (si->type == SSA_TYPE_V4I64 ||
			    (si->IrNode >= SSA_OP_VEC_LOAD && si->IrNode <= SSA_OP_VEC_CMPEQ))
				function_uses_avx2 = true;
	ir_label_named(ir, func->name);
	
	// Prologue
	ir_push(ir, REG_RBP);
	ir_mov_reg_reg(ir, REG_RBP, REG_RSP);
	/* Windows x64 requires callees to preserve these registers.  fIR keeps
	 * hot language variables in them, so every generated function saves the
	 * complete pool.  Seven pushes invert stack alignment; reserve one more
	 * slot so RSP is 16-byte aligned at every call site. */
	for (int ri = 0; ri < VOLATILE_FIR_START; ri++)
		if (function_uses_nonvolatile(func, fIR_reg_pool[ri]))
			ir_push(ir, fIR_reg_pool[ri]);
	if (local_frame_size(func) > 0)
		ir_sub_reg_imm(ir, REG_RSP, local_frame_size(func));

	/* 娣囨繂鐡ㄩ柅姘崇箖鐎靛嫬鐡ㄩ崳銊ょ炊閸忋儳娈戦崣鍌涙殶閸掓澘濂栫€涙劗鈹栭梻?Shadow Space)閿涘奔绶?SSA_OP_LOAD_PARAM 鐠囪褰?*/
	/* Internal Mira callers already materialize every argument in the Windows
	 * home/stack area. LOAD_PARAM reads that stable copy, so rewriting the first
	 * four homes from RCX/RDX/R8/R9 here is redundant. */

	/* 闁圭顦靛〒鍫曞礆濠靛棭娼楅柛鏍ㄧ墪閹烩晠鏌呴悢宄扮秮闂佹彃绻愰惁搴ｂ偓娑櫭▍鎺撶▔?0 (slots 0-6 闁?R13/R14/R15/RBX/RDI/RSI/R12)
	 * 濞村吋锚鐎? 闁兼眹鍎茶潕闁革负鍔岄崣鍡涘矗閿濆懏鍋ュΛ锝嗙墬椤愬吋鎷呯捄銊︽殢闁告挸绉撮崙锛勬偖?STORE_VAR 閻熸洖妫涘ú濠囨晬鐏炶棄鐏熼柡鍐█濞撳爼宕氬┑鍡╂綏闁告牗鐗旂拹?0 */
	{
		bool needs_init[7] = {true,true,true,true,true,true,true};
		/* 閹殿偅寮块崗銉ュ經閸ф绱濋幍鎯у毉閸濐亙绨?slot 閸忓牐顫﹂崘?(STORE_VAR) 閸愬秷顫︾拠?(LOAD_VAR) */
		if (func->entry_block) {
			bool slot_written[7] = {false};
			for (SsaInst *i = func->entry_block->inst_head; i; i = i->next) {
				if (i->IrNode == SSA_OP_STORE_VAR) {
					int slot = (int)i->op2.u.imm;
					if (slot >= 0 && slot < 7) slot_written[slot] = true;
				} else if (i->IrNode == SSA_OP_LOAD_VAR) {
					int slot = (int)i->op1.u.imm;
					if (slot >= 0 && slot < 7 && slot_written[slot]) {
						/* Read after write in entry block 闁?no init needed */
						needs_init[slot] = false;
					}
				}
			}
			/* If STORE_VAR appears before any LOAD_VAR for a slot, no init needed */
			bool slot_stored_first[7] = {false};
			bool slot_read_before_store[7] = {false};
			for (SsaInst *i = func->entry_block->inst_head; i; i = i->next) {
				if (i->IrNode == SSA_OP_STORE_VAR) {
					int slot = (int)i->op2.u.imm;
					if (slot >= 0 && slot < 7 && !slot_read_before_store[slot])
						slot_stored_first[slot] = true;
				} else if (i->IrNode == SSA_OP_LOAD_VAR) {
					int slot = (int)i->op1.u.imm;
					if (slot >= 0 && slot < 7 && !slot_stored_first[slot])
						slot_read_before_store[slot] = true;
				}
			}
			for (int s = 0; s < 7; s++) {
				if (slot_stored_first[s] && !slot_read_before_store[s])
					needs_init[s] = false;
			}
		}
		for (int s = 0; s < 7; s++) {
			if (needs_init[s]) {
				IrReg fr = fIR_var_reg(s);
				if (fr != REG_NONE &&
				    (fr == REG_R8 || fr == REG_R9 || fr == REG_R10 ||
				     function_uses_nonvolatile_fir(func, fr)))
					ir_mov_reg_imm(ir, fr, 0);
			}
		}
	}
	
	// 鏉╂瑩鍣烽崣顖濆厴鏉╂﹢娓剁憰浣稿瀻闁?spills 閺嶅牏鈹栭梻?
	// ir_sub_reg_imm(ir, REG_RSP, func->spill_size);
	
	for(int b_idx=0; b_idx < func->block_count; b_idx++) {
		SsaBasicBlock *b = func->blocks[b_idx];
		ir_label(ir, b->id); // 婵絽绻嬮柌?BasicBlock 閻庣數鎳撶花鍙夌▔閳ь剚绋?Target Label
		int next_block_id = (b_idx + 1 < func->block_count) ? func->blocks[b_idx + 1]->id : -1;
		int _store_sunk = 0;
		for(SsaInst *inst = b->inst_head; inst; inst = inst->next) {
			
			IrReg dst_reg = REG_NONE;
			IrReg dst_float_reg = inst->dst > 0 ? get_ssa_float_reg(func, inst->dst) : REG_NONE;
			if(inst->dst > 0) {
				dst_reg = get_ssa_reg(func, inst->dst);
				if (dst_reg == REG_NONE && is_spilled(func, inst->dst)) dst_reg = REG_R10;
				/* 鐎瑰鍙?Store Sinking: 婵″倹鐏夐張顒冪箥缁犳瀵氭禒銈囨畱缂佹挻鐏夌槐褎甯?STORE_VAR 閸?fIR slot,
				 * 娑撴棁顕?vreg 閸氬海鐢诲▽鈩冩箒閸忔湹绮☉鍫ｅ瀭閼? 閻╁瓨甯撮悽?fIR_var_reg 娴ｆ粈璐熸潻鎰暬閻╊喗鐖? */
				if (dst_float_reg == REG_NONE && !is_spilled(func, inst->dst) &&
				    inst->IrNode != SSA_OP_LOAD_VAR &&
				    inst->next && inst->next->IrNode == SSA_OP_STORE_VAR &&
				    inst->next->op1.kind == SSA_OPND_VREG &&
				    inst->next->op1.u.vreg == inst->dst) {
					int ss = (int)inst->next->op2.u.imm;
					IrReg sfr = fIR_var_reg(ss);
					if (sfr != REG_NONE) {
						/* 閸氭垵澧犻幍顐ｅ伎: 绾喛顓?vreg 濞屸剝婀佺悮?STORE_VAR 娑斿鎮楅惃鍕瘹娴犮倕绱╅悽?*/
						VReg target_vreg = inst->dst;
						int safe = 1;
						for (SsaInst *scan = inst->next->next; scan; scan = scan->next) {
							/* 濡偓閺屻儲澧嶉張澶嬫惙娴ｆ粍鏆?*/
							if ((scan->op1.kind == SSA_OPND_VREG && scan->op1.u.vreg == target_vreg) ||
							    (scan->op2.kind == SSA_OPND_VREG && scan->op2.u.vreg == target_vreg)) {
								safe = 0; break;
							}
							/* 濡偓閺屻儲澧跨仦鏇熸惙娴ｆ粍鏆?*/
							if (scan->operands) for (int oi = 0; oi < scan->operand_count; oi++) {
								if (scan->operands[oi].kind == SSA_OPND_VREG && 
								    scan->operands[oi].u.vreg == target_vreg) {
									safe = 0; break;
								}
							}
							if (!safe) break;
						}
						/* The destination may be consumed in a successor block (notably
						 * by an inlined parameter COPY).  Redirecting this definition to
						 * the variable's fixed register would then leave the allocator's
						 * assigned vreg register uninitialized. */
						for (int ub = 0; safe && ub < func->block_count; ub++) {
							SsaBasicBlock *use_bb = func->blocks[ub];
							if (use_bb == b) continue;
							for (SsaInst *scan = use_bb->inst_head; scan; scan = scan->next) {
								if ((scan->op1.kind == SSA_OPND_VREG && scan->op1.u.vreg == target_vreg) ||
								    (scan->op2.kind == SSA_OPND_VREG && scan->op2.u.vreg == target_vreg)) {
									safe = 0; break;
								}
								for (int oi = 0; scan->operands && oi < scan->operand_count; oi++) {
									if (scan->operands[oi].kind == SSA_OPND_VREG &&
									    scan->operands[oi].u.vreg == target_vreg) {
										safe = 0; break;
									}
								}
								if (!safe) break;
							}
						}
						if (safe) {
							dst_reg = sfr;
							_store_sunk = 1;
						}
					}
				}
			}

			switch(inst->IrNode) {
			case SSA_OP_VEC_LOAD: {
				IrReg vd = get_ssa_vec_reg(func, inst->dst);
				IrReg base = load_operand(ir, func, inst->op1, REG_RAX);
				if (vd != REG_NONE && base != REG_NONE) ir_vmovdqu_load(ir, vd, base, 0);
				break;
			}
			case SSA_OP_VEC_STORE: {
				IrReg vs = get_ssa_vec_reg(func, inst->op1.u.vreg);
				IrReg base = load_operand(ir, func, inst->op2, REG_RAX);
				if (vs != REG_NONE && base != REG_NONE) ir_vmovdqu_store(ir, base, 0, vs);
				break;
			}
			case SSA_OP_VEC_ADD: case SSA_OP_VEC_SUB: case SSA_OP_VEC_XOR:
			case SSA_OP_VEC_AND: case SSA_OP_VEC_OR: case SSA_OP_VEC_MULLD:
			case SSA_OP_VEC_CMPEQ: {
				IrReg vd = get_ssa_vec_reg(func, inst->dst);
				IrReg va = get_ssa_vec_reg(func, inst->op1.u.vreg);
				IrReg vb = get_ssa_vec_reg(func, inst->op2.u.vreg);
				if (vd == REG_NONE || va == REG_NONE || vb == REG_NONE) break;
				if (inst->IrNode == SSA_OP_VEC_ADD) ir_vpaddq(ir, vd, va, vb);
				else if (inst->IrNode == SSA_OP_VEC_SUB) ir_vpsubq(ir, vd, va, vb);
				else if (inst->IrNode == SSA_OP_VEC_XOR) ir_vpxor(ir, vd, va, vb);
				else if (inst->IrNode == SSA_OP_VEC_AND) ir_vpand(ir, vd, va, vb);
				else if (inst->IrNode == SSA_OP_VEC_OR) ir_vpor(ir, vd, va, vb);
				else if (inst->IrNode == SSA_OP_VEC_MULLD) ir_vpmulld(ir, vd, va, vb);
				else ir_vpcmpeqq(ir, vd, va, vb);
				break;
			}
			case SSA_OP_IMM: {
				if(inst->op1.kind == SSA_OPND_IMM) {
					ir_mov_reg_imm(ir, dst_reg, inst->op1.u.imm);
				} else if(inst->op1.kind == SSA_OPND_FIMM) {
					union { double d; int64_t i; } u;
					u.d = inst->op1.u.fimm;
					if (dst_float_reg != REG_NONE) {
						ir_mov_reg_imm(ir, REG_R10, u.i);
						ir_movq_xmm_reg(ir, dst_float_reg, REG_R10);
					} else ir_mov_reg_imm(ir, dst_reg, u.i);
				} else if(inst->op1.kind == SSA_OPND_STRING) {
					static int string_id = 0;
					int sid = ++string_id;
					char label[64];
					snprintf(label, sizeof(label), ".Lstr%d", sid);
					ir_data_label(ir, label);
					ir_data_bytes(ir, (const uint8_t*)inst->op1.u.string.str, inst->op1.u.string.len + 1);
					ir_lea_rip(ir, dst_reg, label);
				}
				break;
			}
			case SSA_OP_ADD: {
				if (is_spilled(func, inst->dst)) {
					IrReg r1 = load_operand(ir, func, inst->op1, REG_R10);
					if (r1 != REG_R10) ir_mov_reg_reg(ir, REG_R10, r1);
					if (inst->op2.kind == SSA_OPND_IMM) {
						int64_t imm = inst->op2.u.imm;
						if (imm >= INT32_MIN && imm <= INT32_MAX)
							ir_add_reg_imm(ir, REG_R10, imm);
						else {
							ir_mov_reg_imm(ir, REG_R11, imm);
							ir_add_reg_reg(ir, REG_R10, REG_R11);
						}
					}
					else { IrReg r2 = load_operand(ir, func, inst->op2, REG_R11); ir_add_reg_reg(ir, REG_R10, r2); }
					break;
				}
				IrReg scratch  = REG_R10;
				IrReg scratch2 = REG_R11;
				IrReg r1 = load_operand(ir, func, inst->op1, scratch);
				if (inst->op2.kind == SSA_OPND_IMM) {
					if (dst_reg != r1) ir_mov_reg_reg(ir, dst_reg, r1);
					int64_t imm = inst->op2.u.imm;
					if (imm >= INT32_MIN && imm <= INT32_MAX)
						ir_add_reg_imm(ir, dst_reg, imm);
					else {
						ir_mov_reg_imm(ir, REG_R11, imm);
						ir_add_reg_reg(ir, dst_reg, REG_R11);
					}
				} else {
					IrReg r2 = load_operand(ir, func, inst->op2, scratch2);
					/* 交换律允许目标直接复用任一输入，避免先复制时覆盖右操作数。 */
					if (dst_reg != r1 && dst_reg != r2 && r1 != REG_RSP && r2 != REG_RSP) {
						ir_lea_idx(ir, dst_reg, r1, r2);
					} else if (dst_reg == r2 && dst_reg != r1) {
						ir_add_reg_reg(ir, dst_reg, r1);
					} else {
						if (dst_reg != r1) ir_mov_reg_reg(ir, dst_reg, r1);
						ir_add_reg_reg(ir, dst_reg, r2);
					}
				}
				break;
			}
			case SSA_OP_SUB: {
				if (is_spilled(func, inst->dst)) {
					IrReg r1 = load_operand(ir, func, inst->op1, REG_R10);
					if (r1 != REG_R10) ir_mov_reg_reg(ir, REG_R10, r1);
					if (inst->op2.kind == SSA_OPND_IMM) {
						int64_t imm = inst->op2.u.imm;
						if (imm >= INT32_MIN && imm <= INT32_MAX)
							ir_sub_reg_imm(ir, REG_R10, imm);
						else {
							ir_mov_reg_imm(ir, REG_R11, imm);
							ir_sub_reg_reg(ir, REG_R10, REG_R11);
						}
					}
					else { IrReg r2 = load_operand(ir, func, inst->op2, REG_R11); ir_sub_reg_reg(ir, REG_R10, r2); }
					break;
				}
				/* SUB+IMM 閻楃懓瀵? sub x, 1 閳?dec x */
				if (inst->op2.kind == SSA_OPND_IMM && inst->op2.u.imm == 1) {
					IrReg r1 = load_operand(ir, func, inst->op1, REG_R10);
					if (dst_reg != r1) ir_mov_reg_reg(ir, dst_reg, r1);
					ir_sub_reg_imm(ir, dst_reg, 1);
				} else if (inst->op2.kind == SSA_OPND_IMM) {
					IrReg r1 = load_operand(ir, func, inst->op1, REG_R10);
					if (dst_reg != r1) ir_mov_reg_reg(ir, dst_reg, r1);
					int64_t imm = inst->op2.u.imm;
					if (imm >= INT32_MIN && imm <= INT32_MAX)
						ir_sub_reg_imm(ir, dst_reg, imm);
					else {
						ir_mov_reg_imm(ir, REG_R11, imm);
						ir_sub_reg_reg(ir, dst_reg, REG_R11);
					}
				} else {
					IrReg scratch = REG_R10;
					IrReg r1 = load_operand(ir, func, inst->op1, scratch);
					IrReg scratch2 = REG_R11;
					IrReg r2 = load_operand(ir, func, inst->op2, scratch2);
					if (dst_reg == r2 && dst_reg != r1) {
						ir_mov_reg_reg(ir, REG_R11, r2);
						r2 = REG_R11;
					}
					if(dst_reg != r1) ir_mov_reg_reg(ir, dst_reg, r1);
					ir_sub_reg_reg(ir, dst_reg, r2);
				}
				break;
			}
			case SSA_OP_MUL: {
				if (is_spilled(func, inst->dst)) {
					IrReg r1 = load_operand(ir, func, inst->op1, REG_R10);
					if (inst->op2.kind == SSA_OPND_IMM) {
						int64_t imm = inst->op2.u.imm;
						if (imm >= INT32_MIN && imm <= INT32_MAX) {
							ir_imul_reg_imm(ir, REG_R10, r1, imm);
						} else {
							if (r1 != REG_R10) ir_mov_reg_reg(ir, REG_R10, r1);
							ir_mov_reg_imm(ir, REG_R11, imm);
							ir_imul_reg_reg(ir, REG_R10, REG_R11);
						}
					}
					else { IrReg r2 = load_operand(ir, func, inst->op2, REG_R11); if (r1 != REG_R10) ir_mov_reg_reg(ir, REG_R10, r1); ir_imul_reg_reg(ir, REG_R10, r2); }
					break;
				}
				IrReg scratch = REG_R10;
				IrReg r1 = load_operand(ir, func, inst->op1, scratch);
				if (inst->op2.kind == SSA_OPND_IMM) {
					int64_t imm = inst->op2.u.imm;
					if (imm >= INT32_MIN && imm <= INT32_MAX) {
						ir_imul_reg_imm(ir, dst_reg, r1, imm);
					} else {
						if (dst_reg != r1) ir_mov_reg_reg(ir, dst_reg, r1);
						ir_mov_reg_imm(ir, REG_R11, imm);
						ir_imul_reg_reg(ir, dst_reg, REG_R11);
					}
				} else {
					IrReg scratch2 = REG_R11;
					IrReg r2 = load_operand(ir, func, inst->op2, scratch2);
					if (dst_reg == r2 && dst_reg != r1) {
						ir_imul_reg_reg(ir, dst_reg, r1);
					} else {
						if(dst_reg != r1) ir_mov_reg_reg(ir, dst_reg, r1);
						ir_imul_reg_reg(ir, dst_reg, r2);
					}
				}
				break;
			}
			case SSA_OP_FADD:
			case SSA_OP_FSUB:
			case SSA_OP_FMUL:
			case SSA_OP_FDIV: {
				IrReg x1 = inst->op1.kind == SSA_OPND_VREG ? get_ssa_float_reg(func, inst->op1.u.vreg) : REG_NONE;
				IrReg x2 = inst->op2.kind == SSA_OPND_VREG ? get_ssa_float_reg(func, inst->op2.u.vreg) : REG_NONE;
				if (x1 == REG_NONE) { IrReg r = load_operand(ir, func, inst->op1, REG_R10); ir_movq_xmm_reg(ir, REG_XMM0, r); x1 = REG_XMM0; }
				if (x2 == REG_NONE) { IrReg r = load_operand(ir, func, inst->op2, REG_R11); ir_movq_xmm_reg(ir, REG_XMM1, r); x2 = REG_XMM1; }
				IrReg xd = dst_float_reg != REG_NONE ? dst_float_reg : REG_XMM0;
				/* Preserve the right operand when the allocator reuses its register
				 * for the destination of a non-commutative operation. */
				if (xd == x2 && xd != x1) { ir_movq_reg_xmm(ir, REG_R11, x2); ir_movq_xmm_reg(ir, REG_XMM1, REG_R11); x2 = REG_XMM1; }
				if (xd != x1) { ir_movq_reg_xmm(ir, REG_R10, x1); ir_movq_xmm_reg(ir, xd, REG_R10); }
				if (inst->IrNode == SSA_OP_FADD) ir_addsd(ir, xd, x2);
				else if (inst->IrNode == SSA_OP_FSUB) ir_subsd(ir, xd, x2);
				else if (inst->IrNode == SSA_OP_FMUL) ir_mulsd(ir, xd, x2);
				else ir_divsd(ir, xd, x2);
				if (dst_float_reg == REG_NONE) ir_movq_reg_xmm(ir, dst_reg, xd);
				break;
			}
			case SSA_OP_SITOFP: {
				IrReg r1 = load_operand(ir, func, inst->op1, REG_R10);
				IrReg xd = dst_float_reg != REG_NONE ? dst_float_reg : REG_XMM0;
				ir_cvtsi2sd(ir, xd, r1);
				if (dst_float_reg == REG_NONE) ir_movq_reg_xmm(ir, dst_reg, xd);
				break;
			}
			case SSA_OP_FPTOSI: {
				IrReg r1 = load_operand(ir, func, inst->op1, REG_R10);
				ir_movq_xmm_reg(ir, REG_XMM0, r1);
				ir_cvttsd2si(ir, dst_reg, REG_XMM0);
				break;
			}
			case SSA_OP_FCMP_EQ:
			case SSA_OP_FCMP_NE:
			case SSA_OP_FCMP_LT:
			case SSA_OP_FCMP_LE:
			case SSA_OP_FCMP_GT:
			case SSA_OP_FCMP_GE: {
				IrReg r1 = load_operand(ir, func, inst->op1, REG_R10);
				IrReg r2 = load_operand(ir, func, inst->op2, REG_R11);
				ir_movq_xmm_reg(ir, REG_XMM0, r1);
				ir_movq_xmm_reg(ir, REG_XMM1, r2);
				ir_ucomisd(ir, REG_XMM0, REG_XMM1);
				IrOpcode setop = IR_SETE;
				if (inst->IrNode == SSA_OP_FCMP_NE) setop = IR_SETNE;
				else if (inst->IrNode == SSA_OP_FCMP_LT) setop = IR_SETB;
				else if (inst->IrNode == SSA_OP_FCMP_LE) setop = IR_SETBE;
				else if (inst->IrNode == SSA_OP_FCMP_GT) setop = IR_SETA;
				else if (inst->IrNode == SSA_OP_FCMP_GE) setop = IR_SETAE;
				ir_setcc(ir, setop, dst_reg);
				ir_movzx_reg8(ir, dst_reg, dst_reg);
				break;
			}
			case SSA_OP_SDIV: {
				if (inst->op2.kind == SSA_OPND_IMM && inst->op2.u.imm > 1) {
					IrReg dividend = load_operand(ir, func, inst->op1, REG_R10);
					lower_sdiv_positive_const(ir, dividend, inst->op2.u.imm,
					                          dst_reg, 0);
					break;
				}
				/* x86-64 idiv: rax = rax / r2, rdx = rax % r2
				 * idiv 闂傚懏鍔曠槐鈩冩媴鐠恒劍鏆?RAX:RDX闁挎稑鏈晶宥嗙?scratch 闁告瑯浜ｉ崗姗€鏌?RCX/R8 */
				IrReg r1 = load_operand(ir, func, inst->op1, REG_R10);
				IrReg divisor_scratch = REG_R11;
				IrReg r2 = load_operand(ir, func, inst->op2, divisor_scratch);
				if (r2 == REG_RAX || r2 == REG_RDX) {
					ir_mov_reg_reg(ir, REG_R11, r2);
					r2 = REG_R11;
				}
				ir_mov_reg_reg(ir, REG_RAX, r1);
				ir_cqo(ir);
				/* 濠碘€冲€归悘?r2 闁告帗鑹鹃妶浠嬪及?RAX 闁?RDX(閻?cqo 閻熸洖妫涘ú?闁挎稑鐭傚〒鍓佹啺娴ｇ儤鏆?scratch */
				ir_idiv(ir, r2);
				if (dst_reg != REG_RAX) ir_mov_reg_reg(ir, dst_reg, REG_RAX);
				break;
			}
			case SSA_OP_SREM: {
				if (inst->op2.kind == SSA_OPND_IMM && inst->op2.u.imm > 1 &&
				    inst->op2.u.imm <= INT32_MAX) {
					IrReg dividend = load_operand(ir, func, inst->op1, REG_R10);
					lower_sdiv_positive_const(ir, dividend, inst->op2.u.imm,
					                          dst_reg, 1);
					break;
				}
				/* x86-64 idiv: rax = rax / r2, RDX = rax % r2 */
				IrReg r1 = load_operand(ir, func, inst->op1, REG_R10);
				IrReg divisor_scratch = REG_R11;
				IrReg r2 = load_operand(ir, func, inst->op2, divisor_scratch);
				if (r2 == REG_RAX || r2 == REG_RDX) {
					ir_mov_reg_reg(ir, REG_R11, r2);
					r2 = REG_R11;
				}
				ir_mov_reg_reg(ir, REG_RAX, r1);
				ir_cqo(ir);
				ir_idiv(ir, r2);
				if (dst_reg != REG_RDX) ir_mov_reg_reg(ir, dst_reg, REG_RDX);
				break;
			}
			case SSA_OP_NEG: {
				IrReg r1 = load_operand(ir, func, inst->op1, REG_RCX);
				if(dst_reg != r1) ir_mov_reg_reg(ir, dst_reg, r1);
				ir_neg(ir, dst_reg);
				break;
			}
			case SSA_OP_CMP_LT:
			case SSA_OP_CMP_GT:
			case SSA_OP_CMP_GE:
			case SSA_OP_CMP_LE:
			case SSA_OP_CMP_EQ:
			case SSA_OP_CMP_NE: {
				/* 婵″倹鐏夋稉瀣╃閺夆剝瀵氭禒銈嗘Ц BR 娑撴柨绱╅悽銊ょ啊鐠?CMP 閻?dst vreg, 
				 * 閸?BR 娴兼俺鐎洪崥鍫燁劃 CMP閿涘本妫ら棁鈧悽鐔稿灇 setcc+movzx 閻ㄥ嫭顒存禒锝囩垳 */
				if (inst->next && inst->next->IrNode == SSA_OP_BR && 
				    inst->next->op1.kind == SSA_OPND_VREG &&
				    inst->next->op1.u.vreg == inst->dst) {
					break; /* 鐠哄疇绻冮敍涓匯 閾诲秴鎮庢导姘跺櫢閺傛壆鏁撻幋?cmp */
				}
				IrReg scratch  = (dst_reg == REG_RCX) ? REG_R8 : REG_RCX;
				IrReg scratch2 = (dst_reg == REG_RDX || scratch == REG_RDX) ? REG_R9 : REG_RDX;
				IrReg r1 = load_operand(ir, func, inst->op1, scratch);
				if (inst->op2.kind == SSA_OPND_IMM && inst->op2.u.imm >= INT32_MIN && inst->op2.u.imm <= INT32_MAX) {
					ir_cmp_reg_imm(ir, r1, inst->op2.u.imm);
				} else {
					IrReg r2 = load_operand(ir, func, inst->op2, scratch2);
					ir_cmp_reg_reg(ir, r1, r2);
				}
				IrOpcode setop;
				switch(inst->IrNode) {
					case SSA_OP_CMP_LT: setop = IR_SETL;  break;
					case SSA_OP_CMP_GT: setop = IR_SETG;  break;
					case SSA_OP_CMP_GE: setop = IR_SETGE; break;
					case SSA_OP_CMP_LE: setop = IR_SETLE; break;
					case SSA_OP_CMP_EQ: setop = IR_SETE;  break;
					default:            setop = IR_SETNE; break;
				}
				ir_setcc(ir, setop, dst_reg);
				ir_movzx_reg8(ir, dst_reg, dst_reg);
				break;
			}
			case SSA_OP_AND: {
				bool op1_spilled = inst->op1.kind == SSA_OPND_VREG &&
					is_spilled(func, inst->op1.u.vreg);
				IrReg scratch = op1_spilled ? REG_R10 :
					((dst_reg == REG_RCX) ? REG_RDX : REG_RCX);
				IrReg r1 = load_operand(ir, func, inst->op1, scratch);
				SsaOperand rhs = inst->op2;
				if (rhs.kind == SSA_OPND_VREG && rhs.u.vreg < (VReg)func->vreg_defs_cap) {
					SsaInst *def = func->vreg_defs[rhs.u.vreg];
					if (def && def->IrNode == SSA_OP_IMM && def->op1.kind == SSA_OPND_IMM)
						rhs = def->op1;
				}
				if (rhs.kind == SSA_OPND_IMM && rhs.u.imm >= INT32_MIN && rhs.u.imm <= INT32_MAX) {
					if(dst_reg != r1) ir_mov_reg_reg(ir, dst_reg, r1);
					ir_and_reg_imm(ir, dst_reg, rhs.u.imm);
				} else {
					bool rhs_spilled = rhs.kind == SSA_OPND_VREG &&
						is_spilled(func, rhs.u.vreg);
					IrReg scratch2 = rhs_spilled ? REG_R11 :
						((dst_reg == REG_RDX || r1 == REG_RDX) ? REG_R8 : REG_RDX);
					IrReg r2 = load_operand(ir, func, rhs, scratch2);
					if (dst_reg == r2 && dst_reg != r1) {
						ir_mov_reg_reg(ir, REG_R11, r2);
						r2 = REG_R11;
					}
					if(dst_reg != r1) ir_mov_reg_reg(ir, dst_reg, r1);
					ir_and_reg_reg(ir, dst_reg, r2);
				}
				break;
			}
			case SSA_OP_OR: {
				bool op1_spilled = inst->op1.kind == SSA_OPND_VREG &&
					is_spilled(func, inst->op1.u.vreg);
				bool op2_spilled = inst->op2.kind == SSA_OPND_VREG &&
					is_spilled(func, inst->op2.u.vreg);
				IrReg r1 = load_operand(ir, func, inst->op1, op1_spilled ? REG_R10 : REG_RCX);
				IrReg r2 = load_operand(ir, func, inst->op2, op2_spilled ? REG_R11 : REG_RDX);
				if (dst_reg == r2 && dst_reg != r1) {
					ir_mov_reg_reg(ir, REG_R11, r2);
					r2 = REG_R11;
				}
				if(dst_reg != r1) ir_mov_reg_reg(ir, dst_reg, r1);
				ir_or_reg_reg(ir, dst_reg, r2);
				break;
			}
			case SSA_OP_XOR: {
				bool op1_spilled = inst->op1.kind == SSA_OPND_VREG &&
					is_spilled(func, inst->op1.u.vreg);
				bool op2_spilled = inst->op2.kind == SSA_OPND_VREG &&
					is_spilled(func, inst->op2.u.vreg);
				IrReg r1 = load_operand(ir, func, inst->op1, op1_spilled ? REG_R10 : REG_RCX);
				IrReg r2 = load_operand(ir, func, inst->op2, op2_spilled ? REG_R11 : REG_RDX);
				if (dst_reg == r2 && dst_reg != r1) {
					ir_mov_reg_reg(ir, REG_R11, r2);
					r2 = REG_R11;
				}
				if(dst_reg != r1) ir_mov_reg_reg(ir, dst_reg, r1);
				ir_xor_reg_reg(ir, dst_reg, r2);
				break;
			}
			case SSA_OP_NOT: {
				bool op1_spilled = inst->op1.kind == SSA_OPND_VREG &&
					is_spilled(func, inst->op1.u.vreg);
				IrReg r1 = load_operand(ir, func, inst->op1, op1_spilled ? REG_R10 : REG_RCX);
				if(dst_reg != r1) ir_mov_reg_reg(ir, dst_reg, r1);
				ir_not_reg(ir, dst_reg);
				break;
			}
			case SSA_OP_LOAD: {
				IrReg ptr = load_operand(ir, func, inst->op1, REG_R11);
				if (ptr == REG_NONE || dst_reg == REG_NONE) break; /* REG_NONE guard */
				ir_mov_reg_mem(ir, dst_reg, ptr, 0);
				break;
			}
			case SSA_OP_STORE: {
				IrReg ptr = load_operand(ir, func, inst->op2, REG_R11);
				IrReg val = load_operand(ir, func, inst->op1, REG_R10);
				if (ptr == REG_NONE || val == REG_NONE) break; /* REG_NONE guard */
				ir_mov_mem_reg(ir, ptr, 0, val);
				break;
			}
			case SSA_OP_LOAD8: {
				IrReg ptr = load_operand(ir, func, inst->op1, REG_R11);
				ir_movzx_reg_mem8(ir, dst_reg, ptr, 0);
				break;
			}
			case SSA_OP_STORE8: {
				IrReg ptr = load_operand(ir, func, inst->op2, REG_R11);
				IrReg val = load_operand(ir, func, inst->op1, REG_R10);
				ir_mov_mem8_reg(ir, ptr, 0, val);
				break;
			}
			case SSA_OP_JMP: {
				ir_jmp(ir, inst->op1.u.block->id);
				break;
			}
			case SSA_OP_BR: {
				// === CMP+BR 閾诲秴鎮庢导妯哄 ===
				// 婵″倹鐏?cond vreg 閺勵垳鏁?CMP 閹稿洣鎶ょ€规矮绠熼惃鍕剁礉閻╁瓨甯撮崣鎴濈殸 cmp + jcc閿?
				// 鐠哄疇绻?setcc + movzx + test + jz 閻ㄥ嫯鍟懗鈧惔蹇撳灙閵?
				VReg cond_vreg = inst->op1.u.vreg;
				SsaInst *cond_def = NULL;
				if (cond_vreg > 0 && cond_vreg < func->next_vreg)
					cond_def = func->vreg_defs ? func->vreg_defs[cond_vreg] : NULL;
				
				int true_id  = inst->operands[0].u.block->id;
				int false_id = inst->operands[1].u.block->id;
				
				if (cond_def && cond_def->IrNode >= SSA_OP_CMP_EQ && cond_def->IrNode <= SSA_OP_CMP_GE) {
					/* 閾诲秴鎮? 閻╁瓨甯?cmp + jcc */
					IrReg scratch  = REG_RCX;
					IrReg scratch2 = REG_RDX;
					IrReg r1 = load_operand(ir, func, cond_def->op1, scratch);
					if (cond_def->op2.kind == SSA_OPND_IMM && cond_def->op2.u.imm >= INT32_MIN && cond_def->op2.u.imm <= INT32_MAX) {
						ir_cmp_reg_imm(ir, r1, cond_def->op2.u.imm);
					} else {
						if (r1 == scratch2) scratch2 = REG_R8;
						IrReg r2 = load_operand(ir, func, cond_def->op2, scratch2);
						ir_cmp_reg_reg(ir, r1, r2);
					}
					
					/* 閺嶈宓佸В鏃囩窛缁鐎烽柅澶嬪鐠哄疇娴嗛弶鈥叉閿涘牐鐑﹂崥?true_block閿?*/
					IrOpcode jcc_op;
					switch(cond_def->IrNode) {
						case SSA_OP_CMP_LT: jcc_op = IR_JL;  break;
						case SSA_OP_CMP_GT: jcc_op = IR_JG;  break;
						case SSA_OP_CMP_GE: jcc_op = IR_JGE; break;
						case SSA_OP_CMP_LE: jcc_op = IR_JLE; break;
						case SSA_OP_CMP_EQ: jcc_op = IR_JE;  break;
						default:            jcc_op = IR_JNE; break;
					}
					/* Branch fallthrough: if true_block is next, invert condition and jcc to false */
					if (true_id == next_block_id) {
						IrOpcode inv_op;
						switch(jcc_op) {
							case IR_JL:  inv_op = IR_JGE; break;
							case IR_JG:  inv_op = IR_JLE; break;
							case IR_JGE: inv_op = IR_JL;  break;
							case IR_JLE: inv_op = IR_JG;  break;
							case IR_JE:  inv_op = IR_JNE; break;
							case IR_JNE: inv_op = IR_JE;  break;
							default:     inv_op = jcc_op; break;
						}
						ir_jcc(ir, inv_op, false_id);
						ir->text[ir->text_count - 1].branch_policy = inst->branch_policy;
						/* fallthrough to true_block */
					} else if (false_id == next_block_id) {
						ir_jcc(ir, jcc_op, true_id);
						ir->text[ir->text_count - 1].branch_policy = inst->branch_policy;
						/* fallthrough to false_block */
					} else {
						ir_jcc(ir, jcc_op, true_id);
						ir->text[ir->text_count - 1].branch_policy = inst->branch_policy;
						ir_jmp(ir, false_id);
					}
				} else {
					/* 鍥為€€: test + jz */
					IrReg cond = get_ssa_reg(func, cond_vreg);
					ir_test_reg_reg(ir, cond, cond);
					ir_jcc(ir, IR_JZ, false_id);
					ir->text[ir->text_count - 1].branch_policy = inst->branch_policy;
					ir_jmp(ir, true_id);
				}
				break;
			}
			case SSA_OP_CALL: {
				if (function_uses_avx2) ir_vzeroupper(ir);
				int nparams = inst->operand_count - 1; 
				int is_tail = nparams <= 4 && inst->dst > 0 && inst->next &&
					inst->next->IrNode == SSA_OP_RET &&
					inst->next->op1.kind == SSA_OPND_VREG &&
					inst->next->op1.u.vreg == inst->dst &&
					inst->operands[0].kind == SSA_OPND_SYM;
				if (is_tail) {
					/* Preserve all argument values in the original caller-owned home
					 * area before restoring this frame. */
					for (int ai = 0; ai < nparams; ai++) {
						SsaOperand arg = inst->operands[ai + 1];
						IrReg src;
						if (arg.kind == SSA_OPND_IMM) {
							ir_mov_reg_imm(ir, REG_R10, arg.u.imm);
							src = REG_R10;
						} else src = load_operand(ir, func, arg, REG_R10);
						ir_mov_mem_reg(ir, REG_RBP, 16 + ai * 8, src);
					}
					if (nparams >= 4) ir_mov_reg_mem(ir, REG_R9,  REG_RBP, 40);
					if (nparams >= 3) ir_mov_reg_mem(ir, REG_R8,  REG_RBP, 32);
					if (nparams >= 2) ir_mov_reg_mem(ir, REG_RDX, REG_RBP, 24);
					if (nparams >= 1) ir_mov_reg_mem(ir, REG_RCX, REG_RBP, 16);
					if (local_frame_size(func) > 0)
						ir_add_reg_imm(ir, REG_RSP, local_frame_size(func));
					for (int ri = VOLATILE_FIR_START - 1; ri >= 0; ri--)
						if (function_uses_nonvolatile(func, fIR_reg_pool[ri]))
							ir_pop(ir, fIR_reg_pool[ri]);
					ir_pop(ir, REG_RBP);
					ir_jmp_extern(ir, inst->operands[0].u.sym);
					inst = inst->next; /* skip the RET consumed by the tail jump */
					break;
				}
				int extra = nparams > 4 ? nparams - 4 : 0;
				int n_vol = count_volatile_fIR_vars(func);
				int frame = 32 + extra * 8;
				/* 16 瀛楄妭瀵归綈锛氶渶鑰冭檻 n_vol 涓?PUSH锛堝悇 8 瀛楄妭锛夊 RSP 鐨勫奖鍝?*/
				if ((frame + n_vol * 8) % 16 != 0) frame += 8;

				/* === Spill: 淇濇姢 volatile 蹇€熷彉閲?=== */
				spill_volatile_fIR_vars(ir, func);

				ir_sub_reg_imm(ir, REG_RSP, frame);

				// 閲囩敤褰卞瓙绌洪棿瑁呰浇鍙傛暟锛氱敱浜?r8/r9 绛夊悓鏃跺吋鑱屼簡鍙傛暟瀵勫瓨鍣ㄥ拰蹇€熷彉閲忥紝
				// 濡傛灉鐩存帴浠庡揩鍙橀噺瑁呭～鍙傛暟鍙兘浼氬嚭鐜板惊鐜鐩栵紙濡傚厛瑁?r9=r8, 鍐嶈 r8=xxx 鏃讹紝r8鍘熷€煎凡缁忚鐮村潖锛夈€?
				// 瑙ｅ喅鏂规锛氭墍鏈夊墠 4 鍙傛暟鍏堢粺涓€鏄犲皠鍒?rsp 褰卞瓙绌洪棿 ([rsp], [rsp+8], [rsp+16], [rsp+24])锛?
				// 瑁呭～瀹屾瘯鍚庡啀缁熶竴 `mov r*, [rsp+...]` 瑁呰溅銆?
				
				#define LOAD_ARG_TO_SHADOW(idx, opnd) \
					if((opnd).kind == SSA_OPND_VREG) { \
						IrReg arg_reg = load_operand(ir, func, (opnd), REG_R10); \
						ir_mov_mem_reg(ir, REG_RSP, (idx)*8, arg_reg); \
					} else if((opnd).kind == SSA_OPND_IMM) { \
						ir_mov_reg_imm(ir, REG_RAX, (opnd).u.imm); \
						ir_mov_mem_reg(ir, REG_RSP, (idx)*8, REG_RAX); \
					}

				if (nparams >= 4) { LOAD_ARG_TO_SHADOW(3, inst->operands[4]); }
				if (nparams >= 3) { LOAD_ARG_TO_SHADOW(2, inst->operands[3]); }
				if (nparams >= 2) { LOAD_ARG_TO_SHADOW(1, inst->operands[2]); }
				if (nparams >= 1) { LOAD_ARG_TO_SHADOW(0, inst->operands[1]); }
				for (int ai = 4; ai < nparams; ai++) {
					LOAD_ARG_TO_SHADOW(ai, inst->operands[ai + 1]);
				}
				#undef LOAD_ARG_TO_SHADOW

				// 缁熶竴灏嗗奖瀛愮┖闂磋杞﹀彂寰€ ABI 瀵勫瓨鍣?
				if (nparams == 1) {
					SsaOperand arg = inst->operands[1];
					if (arg.kind == SSA_OPND_IMM) ir_mov_reg_imm(ir, REG_RCX, arg.u.imm);
					else {
						IrReg src = load_operand(ir, func, arg, REG_R10);
						if (src != REG_RCX) ir_mov_reg_reg(ir, REG_RCX, src);
					}
				} else {
					if (nparams >= 4) ir_mov_reg_mem(ir, REG_R9,  REG_RSP, 24);
					if (nparams >= 3) ir_mov_reg_mem(ir, REG_R8,  REG_RSP, 16);
					if (nparams >= 2) ir_mov_reg_mem(ir, REG_RDX, REG_RSP, 8);
					if (nparams >= 1) ir_mov_reg_mem(ir, REG_RCX, REG_RSP, 0);
				}

				if(inst->operands[0].kind == SSA_OPND_SYM) {
					ir_call_extern(ir, inst->operands[0].u.sym);
				}
				
				ir_add_reg_imm(ir, REG_RSP, frame);
				fill_volatile_fIR_vars(ir, func);

				if (dst_float_reg != REG_NONE) {
					ir_movq_xmm_reg(ir, dst_float_reg, REG_RAX);
				} else if(dst_reg != REG_NONE) {
					ir_mov_reg_reg(ir, dst_reg, REG_RAX);
				}

				/* === Fill: 鎭㈠ volatile 蹇€熷彉閲?=== */
				break;
			}
			case SSA_OP_ICALL: {
				if (function_uses_avx2) ir_vzeroupper(ir);
				/* Indirect call via function pointer VReg.
				 * operands[0] = VReg holding function pointer
				 * operands[1..n] = arguments
				 */
				int nparams = inst->operand_count - 1;
				int extra = nparams > 4 ? nparams - 4 : 0;
				int n_vol = count_volatile_fIR_vars(func);
				/* ICALL 棰濆鏈変竴涓?fptr 鐨?PUSH锛屾墍浠?+1 */
				int frame = 32 + extra * 8;
				if ((frame + (n_vol + 1) * 8) % 16 != 0) frame += 8;

				/* === Spill: 淇濇姢 volatile 蹇€熷彉閲?=== */
				spill_volatile_fIR_vars(ir, func);

				/* Load function pointer then save to stack to prevent it from being clobbered by arg loading */
				IrReg fptr_reg = get_ssa_reg(func, inst->operands[0].u.vreg);
				ir_push(ir, fptr_reg);

				ir_sub_reg_imm(ir, REG_RSP, frame);

				/* Load arguments into Windows ABI registers via shadow space */
				#define ILOAD_ARG_TO_SHADOW(idx, opnd) \
					if((opnd).kind == SSA_OPND_VREG) { \
						IrReg ar = load_operand(ir, func, (opnd), REG_R10); \
						ir_mov_mem_reg(ir, REG_RSP, (idx)*8, ar); \
					} else if((opnd).kind == SSA_OPND_IMM) { \
						ir_mov_reg_imm(ir, REG_RAX, (opnd).u.imm); \
						ir_mov_mem_reg(ir, REG_RSP, (idx)*8, REG_RAX); \
					}

				if (nparams >= 4) { ILOAD_ARG_TO_SHADOW(3, inst->operands[4]); }
				if (nparams >= 3) { ILOAD_ARG_TO_SHADOW(2, inst->operands[3]); }
				if (nparams >= 2) { ILOAD_ARG_TO_SHADOW(1, inst->operands[2]); }
				if (nparams >= 1) { ILOAD_ARG_TO_SHADOW(0, inst->operands[1]); }
				for (int ai = 4; ai < nparams; ai++) {
					ILOAD_ARG_TO_SHADOW(ai, inst->operands[ai + 1]);
				}
				#undef ILOAD_ARG_TO_SHADOW

				// 缁熶竴灏嗗奖瀛愮┖闂磋杞﹀彂寰€ ABI 瀵勫瓨鍣?
				if (nparams == 1) {
					SsaOperand arg = inst->operands[1];
					if (arg.kind == SSA_OPND_IMM) ir_mov_reg_imm(ir, REG_RCX, arg.u.imm);
					else {
						IrReg src = load_operand(ir, func, arg, REG_R10);
						if (src != REG_RCX) ir_mov_reg_reg(ir, REG_RCX, src);
					}
				} else {
					if (nparams >= 4) ir_mov_reg_mem(ir, REG_R9,  REG_RSP, 24);
					if (nparams >= 3) ir_mov_reg_mem(ir, REG_R8,  REG_RSP, 16);
					if (nparams >= 2) ir_mov_reg_mem(ir, REG_RDX, REG_RSP, 8);
					if (nparams >= 1) ir_mov_reg_mem(ir, REG_RCX, REG_RSP, 0);
				}

				/* Restore fptr into RAX (now at [RSP + frame]) */
				ir_mov_reg_mem(ir, REG_RAX, REG_RSP, frame);
				ir_call_reg(ir, REG_RAX); /* call rax */
				
				ir_add_reg_imm(ir, REG_RSP, frame);
				/* Cleanup fptr pop */
				ir_add_reg_imm(ir, REG_RSP, 8);
				fill_volatile_fIR_vars(ir, func);

				if (dst_float_reg != REG_NONE) {
					ir_movq_xmm_reg(ir, dst_float_reg, REG_RAX);
				} else if (dst_reg != REG_NONE) {
					ir_mov_reg_reg(ir, dst_reg, REG_RAX);
				}

				/* === Fill: 鎭㈠ volatile 蹇€熷彉閲?=== */
				break;
			}
			case SSA_OP_COPY: {
				if (dst_float_reg != REG_NONE && inst->op1.kind != SSA_OPND_NONE) {
					IrReg xs = get_ssa_float_reg(func, inst->op1.u.vreg);
					if (xs != REG_NONE && xs != dst_float_reg) {
						ir_movq_reg_xmm(ir, REG_R10, xs);
						ir_movq_xmm_reg(ir, dst_float_reg, REG_R10);
					} else if (xs == REG_NONE) {
						IrReg src = load_operand(ir, func, inst->op1, REG_R10);
						ir_movq_xmm_reg(ir, dst_float_reg, src);
					}
				} else if(dst_reg != REG_NONE && inst->op1.kind != SSA_OPND_NONE) {
					IrReg src = load_operand(ir, func, inst->op1, REG_R11);
					if(src != REG_NONE && src != dst_reg) {
						ir_mov_reg_reg(ir, dst_reg, src);
					}
				}
				break;
			}
			case SSA_OP_RET: {
				if(inst->operand_count > 0 && inst->op1.kind == SSA_OPND_VREG) {
					IrReg ret_val = load_operand(ir, func, inst->op1, REG_R10);
					if(ret_val != REG_RAX) ir_mov_reg_reg(ir, REG_RAX, ret_val);
				}
				/* 婵絽绻嬮柌?RET 闂侇喛妫勭换鈧銈堫嚙閸炴挳鎳曢弮鍌涙櫢闁?epilogue闁挎稑鑻幆渚€宕氬▎搴℃暕闁活喕妞掔槐鎵矚閸фせ鍋撹箛鎾崇厒濞戞挸顑勭粩瀛樼▔椤忓嫮鍞ㄩ柡鍫墮濞?*/
				if (function_uses_avx2) ir_vzeroupper(ir);
				if (local_frame_size(func) > 0)
					ir_add_reg_imm(ir, REG_RSP, local_frame_size(func));
				for (int ri = VOLATILE_FIR_START - 1; ri >= 0; ri--)
					if (function_uses_nonvolatile(func, fIR_reg_pool[ri]))
						ir_pop(ir, fIR_reg_pool[ri]);
				ir_pop(ir, REG_RBP);
				ir_ret(ir);
				break;
			}
			case SSA_OP_ALLOCA: {
				/* 闁告瑦锕㈤崳鍝勵啅閸欏鏆☉鎾存そ閳ь剚淇虹换?mira_vars[slot] 闁烩晛鐡ㄧ敮瀵告媼閸ф锛栭柨娑樼搼LLOCA 闁圭娲ｉ幎銈堢疀閻ｅ本娈?*/
				break;
			}
			case SSA_OP_LOAD_VAR: {
				/* dst = mira_vars[slot] */
				int slot = (int)inst->op1.u.imm;
				IrReg fr = fIR_var_reg(slot);
				if (fr != REG_NONE) {
					/* 闂嗚泛绱戦柨鈧捄顖氱窞閿涙et_ssa_reg 闁繋绱?+ load_operand 闁繋绱?
					 * 瀹歌尙绮＄拋鈺傚閺堝绗呭〒鍛婂瘹娴犮倗娲块幒銉嚢 fIR_var_reg閿?
					 * LOAD_VAR 閼奉亣闊╂稉宥夋付鐟曚胶鏁撻幋鎰崲娴ｆ洑鍞惍渚婄磼 */
					if (dst_float_reg != REG_NONE) ir_movq_xmm_reg(ir, dst_float_reg, fr);
				} else if (dst_float_reg != REG_NONE) {
					ir_lea_rip(ir, REG_R10, "mira_vars");
					ir_mov_reg_mem(ir, REG_R10, REG_R10, slot * 8);
					ir_movq_xmm_reg(ir, dst_float_reg, REG_R10);
				} else if (dst_reg != REG_NONE) {
					IrReg scratch = (dst_reg == REG_RAX) ? REG_RCX : REG_RAX;
					ir_lea_rip(ir, dst_reg, "mira_vars");
					ir_mov_reg_mem(ir, dst_reg, dst_reg, slot * 8);
				}
				break;
			}
			case SSA_OP_STORE_VAR: {
				/* mira_vars[slot] = val_reg */
				int slot = (int)inst->op2.u.imm;
				IrReg fr = fIR_var_reg(slot);
				
				if (fr != REG_NONE) {
					/* Store Sinking: 娑撳﹣绔撮弶鈥冲嚒閻╁瓨甯撮崘娆忓弳 fr? 鐠哄疇绻? */
					if (_store_sunk) { _store_sunk = 0; break; }
					/* 閫忎紶鐩存帴锛歡et_ssa_reg 鐜板湪浼氳繑鍥炴簮鍙橀噺鐨?fIR_var_reg */
				IrReg val_reg = load_operand(ir, func, inst->op1, REG_R10);
				if (val_reg == REG_NONE) break;
				/* LOAD_VAR can disappear after value forwarding while its users
				 * still read the slot's fixed register through the original
				 * vreg definition.  Therefore absence of a remaining LOAD_VAR
				 * is not a valid dead-store proof at lowering time.  Adjacent
				 * producer/store pairs are already handled by store sinking. */
				if (fr != val_reg) ir_mov_reg_reg(ir, fr, val_reg);
				} else {
					IrReg val_reg = load_operand(ir, func, inst->op1, REG_R10);
					if (val_reg == REG_NONE) break;
					IrReg scratch = (val_reg == REG_RAX) ? REG_RCX : REG_RAX;
					ir_push(ir, scratch);
					ir_lea_rip(ir, scratch, "mira_vars");
					ir_mov_mem_reg(ir, scratch, slot * 8, val_reg);
					ir_pop(ir, scratch);
				}
				break;
			}
			case SSA_OP_LOAD_PARAM: {
				/* dst = param[index], read from [RBP + 16 + index * 8] (Windows ABI shadow space starts at +16) */
				int pidx = (int)inst->op1.u.imm;
				if (dst_reg != REG_NONE) {
					ir_mov_reg_mem(ir, dst_reg, REG_RBP, 16 + pidx * 8);
				}
				break;
			}
			case SSA_OP_LEA_FUNC: {
				/* dst = address of function */
				if (dst_reg != REG_NONE) {
					ir_lea_rip(ir, dst_reg, inst->op1.u.sym);
				}
				break;
			}
			default:
				// 閸忔湹绮張顏囶潶缂堟槒鐦ч崚鏉跨俺鐏炲倻娈?SSA 閸樼喎鐎烽幐鍥︽姢娣囨繃瀵旈棃娆撶帛閹存牗濮ら柨?
				break;
			}
			if (inst->dst > 0 && is_spilled(func, inst->dst))
				ir_mov_mem_reg(ir, REG_RBP, spill_offset(func, inst->dst), REG_R10);
		}
	}
	
	/* Every reachable function exit is represented by SSA_OP_RET and lowered
	 * above.  Appending another epilogue here creates unreachable duplicate
	 * RET sequences and hides malformed control flow. */
}

void ssa_lower_module(SsaModule *mod, IrBuffer *ir) {
	for(int i=0; i<mod->func_count; i++) {
		ssa_lower_function(mod->functions[i], ir);
	}
}

/* 棰勫鐞嗭細涓烘墍鏈夊嚱鏁拌绠楀姩鎬佸彉閲忓瘎瀛樺櫒鏄犲皠 */
void ssa_compute_var_reg_maps(SsaModule *mod, int var_count) {
	/* SSA optimizations may consume slots from the 16-entry internal tail
	 * reserved by program.c.  Include every referenced internal slot in
	 * register mapping and shared-slot analysis. */
	for (int fi = 0; fi < mod->func_count; ++fi)
		for (int bi = 0; bi < mod->functions[fi]->block_count; ++bi)
			for (SsaInst *inst = mod->functions[fi]->blocks[bi]->inst_head;
			     inst; inst = inst->next) {
				int slot = -1;
				if (inst->IrNode == SSA_OP_LOAD_VAR && inst->op1.kind == SSA_OPND_IMM)
					slot = (int)inst->op1.u.imm;
				else if (inst->IrNode == SSA_OP_STORE_VAR && inst->op2.kind == SSA_OPND_IMM)
					slot = (int)inst->op2.u.imm;
				if (slot >= var_count) var_count = slot + 1;
			}
	bool *shared_slots = var_count > 0 ? calloc((size_t)var_count, sizeof(bool)) : NULL;
	int *first_owner = var_count > 0 ? malloc((size_t)var_count * sizeof(int)) : NULL;
	if (first_owner)
		for (int slot = 0; slot < var_count; ++slot) first_owner[slot] = -1;

	if (shared_slots && first_owner) {
		for (int fi = 0; fi < mod->func_count; ++fi) {
			SsaFunction *func = mod->functions[fi];
			bool *seen = calloc((size_t)var_count, sizeof(bool));
			if (!seen) continue;
			for (int bi = 0; bi < func->block_count; ++bi) {
				for (SsaInst *inst = func->blocks[bi]->inst_head; inst; inst = inst->next) {
					int slot = -1;
					if (inst->IrNode == SSA_OP_LOAD_VAR && inst->op1.kind == SSA_OPND_IMM)
						slot = (int)inst->op1.u.imm;
					else if (inst->IrNode == SSA_OP_STORE_VAR && inst->op2.kind == SSA_OPND_IMM)
						slot = (int)inst->op2.u.imm;
					if (slot < 0 || slot >= var_count || seen[slot]) continue;
					seen[slot] = true;
					if (first_owner[slot] < 0) first_owner[slot] = fi;
					else if (first_owner[slot] != fi) shared_slots[slot] = true;
				}
			}
			free(seen);
		}
	}

	for (int i = 0; i < mod->func_count; i++)
		compute_var_reg_map(mod->functions[i], var_count, shared_slots);
	free(first_owner);
	free(shared_slots);
}


