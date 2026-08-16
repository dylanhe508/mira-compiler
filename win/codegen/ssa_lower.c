/* ssa_lower.c 闁?SSA 闁?x86-64 IR (Instruction Selection) 闂傚嫬绉舵?
 *
 * 閻庨潧瀚悺銊╁闯閵娿儱鐎婚梺鏉跨С缁狅綁宕ユ惔顖滅婵絽绻嬮柌?Virtual Register 闂侇喛濮ょ€氥垽寮垫径澶屽晩閻庣數鎳撶花鏌ユ儍閸曨厼鈷栭柣鐐叉閻﹀海鈧稒锚濞呮帡濡?
 */
#include "ir_ssa.h"
#include "ir.h"
#include "codegen.h"
#include "abi.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern CodegenState *cg; // 閸忚京鏁ら崢鐔告拱閻?CodegenState閿涘奔濞囬悽銊ョ暊閻?cg->ir 缂傛挸鍟?

// 閺屻儴銆冪亸?Ssa閻ㄥ嫮澧块悶鍡楃槑鐎涙ê娅掔紓鏍у娇閺勭姴鐨犻崚?x86-64 IrReg
// 婵炲鍔嶉崜? R12 闁?Mira 闁轰胶澧楀畵渚€寮介崼鐔风樄闂佽棄鐗炵槐婕?3 濞ｅ洦绻勯弳鈧柨娑橆劏BP/RSP 濞戞挸绉村顒佺▔鎼粹€崇€婚梺?
/* 閸斻劍鈧礁褰夐柌蹇撶槑鐎涙ê娅掗弰鐘茬殸閿涙氨鏁遍悜顓炲閸掑棙鐎界紒鎾寸亯閸愬啿鐣?slot -> 閻椻晝鎮婄€靛嫬鐡ㄩ崳?*/
static SsaFunction *_cur_lower_func = NULL;
static IrReg map_phys_reg(int phys_reg);
#define VAR_REG_REMAT (-2)
static const int *gs_sort_hotness = NULL;

static int cmp_hotness_desc(const void *a, const void *b) {
	const int *hotness = gs_sort_hotness;
	if (!hotness) return 0;
	int ha = hotness[*(const int *)a], hb = hotness[*(const int *)b];
	return (ha > hb) ? -1 : (ha < hb) ? 1 : 0;
}

static IrReg fIR_var_reg(int slot) {
	if (_cur_lower_func && _cur_lower_func->var_reg_map &&
	    slot >= 0 && slot < _cur_lower_func->var_count) {
		int mapped = _cur_lower_func->var_reg_map[slot];
		return mapped >= 0 ? (IrReg)mapped : REG_NONE;
	}
	return REG_NONE;
}

/* 可用的快速变量寄存器池 —— 来自 abi.h,按目标平台切换。
 * Win64: 前 7 个 callee-saved(R13/R14/R15/RBX/RDI/RSI/R12,在 Win64
 *        下 RDI/RSI 也是非易失),后 2 个 caller-saved(R8/R9,需在 CALL
 *        前后 PUSH/POP 保护)。
 * SysV:  前 5 个 callee-saved(RBX/R12/R13/R14/R15,SysV 下 RDI/RSI 是
 *        参数寄存器,不能进池),后 2 个 caller-saved(R8/R9)。
 * 因 ABI 在运行时确定,改用下方访问器从 abi.h 取池指针与边界。 */
static const IrReg *fir_pool(int *count) {
	return mira_abi_fir_pool(count);
}
static int fir_volatile_start(void) {
	return mira_abi_fir_volatile_start();
}

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

static int affine_param_vreg(SsaFunction *func, VReg vreg, int depth,
	                         int *param, int64_t *mul, int64_t *add) {
	if (!func || !func->vreg_defs || vreg == 0 ||
	    vreg >= (VReg)func->vreg_defs_cap || depth > 12) return 0;
	SsaInst *def = func->vreg_defs[vreg];
	if (!def) return 0;
	if (def->IrNode == SSA_OP_COPY && def->op1.kind == SSA_OPND_VREG)
		return affine_param_vreg(func, def->op1.u.vreg, depth + 1, param, mul, add);
	if (def->IrNode == SSA_OP_LOAD_PARAM && def->op1.kind == SSA_OPND_IMM) {
		*param = (int)def->op1.u.imm; *mul = 1; *add = 0; return 1;
	}
	if (def->IrNode == SSA_OP_IMM && def->op1.kind == SSA_OPND_IMM) {
		*param = -1; *mul = 0; *add = def->op1.u.imm; return 1;
	}
	if (def->IrNode != SSA_OP_ADD && def->IrNode != SSA_OP_SUB &&
	    def->IrNode != SSA_OP_MUL) return 0;
	int pa, pb; int64_t ma, mb, aa, ab;
	if (def->op1.kind == SSA_OPND_IMM) {
		pa = -1; ma = 0; aa = def->op1.u.imm;
	} else if (def->op1.kind == SSA_OPND_VREG) {
		if (!affine_param_vreg(func, def->op1.u.vreg, depth + 1, &pa, &ma, &aa)) return 0;
	} else return 0;
	if (def->op2.kind == SSA_OPND_IMM) {
		pb = -1; mb = 0; ab = def->op2.u.imm;
	} else if (def->op2.kind == SSA_OPND_VREG) {
		if (!affine_param_vreg(func, def->op2.u.vreg, depth + 1, &pb, &mb, &ab)) return 0;
	} else return 0;
	if (pa >= 0 && pb >= 0 && pa != pb) return 0;
	int p = pa >= 0 ? pa : pb;
	__int128 m = 0, a = 0;
	if (def->IrNode == SSA_OP_ADD) {
		m = (__int128)ma + mb; a = (__int128)aa + ab;
	} else if (def->IrNode == SSA_OP_SUB) {
		m = (__int128)ma - mb; a = (__int128)aa - ab;
	} else {
		if (ma != 0 && mb != 0) return 0;
		if (ma != 0) { m = (__int128)ma * ab; a = (__int128)aa * ab; }
		else if (mb != 0) { m = (__int128)mb * aa; a = (__int128)ab * aa; }
		else { m = 0; a = (__int128)aa * ab; }
	}
	if (m < INT64_MIN || m > INT64_MAX || a < INT64_MIN || a > INT64_MAX) return 0;
	*param = p; *mul = (int64_t)m; *add = (int64_t)a; return 1;
}

static int affine_param_slot(SsaFunction *func, int slot,
	                         int *param, int64_t *mul, int64_t *add) {
	int found = 0;
	for (int bi = 0; bi < func->block_count; ++bi)
		for (SsaInst *inst = func->blocks[bi]->inst_head; inst; inst = inst->next) {
			if (inst->IrNode != SSA_OP_STORE_VAR || inst->op2.kind != SSA_OPND_IMM ||
			    inst->op2.u.imm != slot) continue;
			if (inst->op1.kind != SSA_OPND_VREG) return 0;
			int p; int64_t m, a;
			if (!affine_param_vreg(func, inst->op1.u.vreg, 0, &p, &m, &a) || p < 0)
				return 0;
			if (!found) { *param = p; *mul = m; *add = a; found = 1; }
			else if (*param != p || *mul != m || *add != a) return 0;
		}
	return found;
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
	int pool_count = 0;
	const IrReg *pool = fir_pool(&pool_count);
	int nvol = fir_volatile_start();
	int count = 0;
	for (int i = 0; i < nvol && i < pool_count; i++)
		if (function_uses_nonvolatile(func, pool[i])) count++;
	return count;
}

/* SysV 下前 6 个参数通过寄存器传递,没有调用方影子区。为了让
 * LOAD_PARAM 用统一的栈偏移读取参数(与 Win64 读 [rbp+16..] 对称),
 * prologue 把这些寄存器参数保存到本地帧顶部的"寄存器参数归宿区"。
 * 返回需要保存的寄存器参数个数(min(param_count, 6)),Win64 返回 0。
 * 归宿区布局:本地帧最底部 [rbp - frame + i*8],i 从 0 开始。 */
static int sysv_param_save_count(SsaFunction *func) {
	if (mira_target_abi != MIRA_ABI_SYSV || !func) return 0;
	int nreg = mira_abi_int_arg_reg_count();   /* SysV = 6 */
	int np = func->param_count;
	return np < nreg ? np : nreg;
}

static int param_save_size(SsaFunction *func) {
	return sysv_param_save_count(func) * 8;
}

static int local_frame_size(SsaFunction *func) {
	int p = nonvolatile_fir_count(func);
	int base = func->spill_size + param_save_size(func);
	if (mira_target_abi != MIRA_ABI_SYSV)
		return (p & 1) ? 8 + base : base;   /* Win64 原逻辑,零回归 */
	/* SysV 硬性约束:call 前 rsp ≡ 0 (mod 16)(libc 内 movaps 等 16 字节
	 * 存取在错位 8 时 #GP)。入口 rsp ≡ 8,push rbp 后 ≡ 0,再 push p 个
	 * 非易失寄存器后 ≡ -8p,故帧大小必须满足 frame ≡ -8p (mod 16)。
	 * 旧逻辑只按 p 奇偶补 8,未补偿 base 的 mod 16 残差(参数归宿区
	 * param_save_size = 前 6 参 × 8,参数个数为奇数时残差 8 → 函数内
	 * 所有调用错位)。调用点临时帧的对齐((frame+n_vol*8)%16)在
	 * frame ≡ -8p 成立时自动正确:调用点 rsp ≡ -8p-frame-8·n_vol,
	 * -8p-frame ≡ 0 抵消,余量仅剩 n_vol,由调用点逻辑补齐。
	 * 栈槽分区:spill 区(spill_offset)从 -(saves+alignment+8) 起向下
	 * 扩展 8·spill_slots 字节,参数归宿区贴帧底向上 param_save_size
	 * 字节,两区间距 = pad - alignment - 8。pad 只按 16 对齐时可能
	 * 过小:内层循环临时槽落入外层参数槽,循环上限被覆盖,轮数错乱
	 * (wh5 中 cols 副本写入 rounds 槽,30 轮变 25 轮;更甚者读到
	 * 垃圾上限 → 死循环)。故 pad 必须 ≥ alignment+8 = (p&1)?16:8,
	 * 不足时加 16(16 的倍数,不破坏 16 对齐)。 */
	int pad = (int)((16 - ((8 * p + base) & 15)) & 15);
	int need = (p & 1) ? 16 : 8;   /* 即 spill_offset 的 alignment+8 */
	if (pad < need) pad += 16;
	return base + pad;
}

/* SysV 寄存器参数归宿区中第 pidx 个参数相对于 RBP 的偏移(负值)。
 * 归宿区紧贴本地帧底部,但栈上还有 prologue 压栈的非易失寄存器区
 * ([rbp-8] 起,共 nonvolatile_fir_count*8 字节)。偏移必须跨过该区,
 * 否则与保存的调用者寄存器重叠(pop 时恢复出参数值)。
 * Win64 不使用此函数(参数直接在调用方影子区 [rbp+16..])。 */
static int sysv_param_home_offset(SsaFunction *func, int pidx) {
	int saves = nonvolatile_fir_count(func) * 8;
	return -(saves + local_frame_size(func)) + pidx * 8;
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

/* Unified Win64/SysV call-site frame: shadow space (Win64) or relay area
 * (SysV) plus stacked parameters, padded so (frame + spilled bytes) is
 * 16-aligned right before the call.  Kept in one place so the hoist scan
 * and the lowering emit identical sizes. */
static int call_site_frame(SsaFunction *func, int nparams, int is_icall) {
	int n_vol = count_volatile_fIR_vars(func);
	int nreg = mira_abi_int_arg_reg_count();
	int extra = nparams > nreg ? nparams - nreg : 0;
	int frame = (mira_target_abi == MIRA_ABI_WIN64)
		? mira_abi_shadow_space() + extra * 8
		: nreg * 8 + extra * 8;
	if ((frame + (n_vol + (is_icall ? 1 : 0)) * 8) % 16 != 0) frame += 8;
	return frame;
}

/* hoist 每个直接调用的 call-site 栈帧到 prologue:prologue 一次性 sub 最大
 * 帧,所有 call 点的 RSP 对齐与逐点分配逐字节一致,省掉每个 call 点的
 * sub/add。Win64(影子空间)与 SysV(relay 中转区)共用:
 *   Win64 frame = 32 + extra*8;  SysV frame = 48 + extra*8;
 * pad 使 (frame + n_vol*8) ≡ 0 (mod 16),故 maxf + n_vol*8 保持 16 对齐。
 * ICALL 额外 push 函数指针会破坏对齐,含 ICALL 的函数不 hoist。 */
static int hoist_call_frame(SsaFunction *func) {
	int maxf = 0, any = 0;
	for (int bi = 0; bi < func->block_count; ++bi)
		for (SsaInst *si = func->blocks[bi]->inst_head; si; si = si->next) {
			if (si->IrNode == SSA_OP_ICALL) return 0;
			if (si->IrNode != SSA_OP_CALL) continue;
			any = 1;
			int f = call_site_frame(func, si->operand_count - 1, 0);
			if (f > maxf) maxf = f;
		}
	/* 在最大 call 帧之上再预留 volatile 变量 spill 槽。hoist 后 call 点
	 * 不再 sub frame,若沿用原 push 方案,spill 数据会落在 [rsp+0..n_vol*8),
	 * 与 Win64 影子区 / SysV 中转区([rsp+0..31]/[rsp+0..47])重叠——装载
	 * 会覆盖保存值。因此 spill 改为帧内固定偏移 store/load,槽位在
	 * [rsp+maxf .. rsp+maxf+n_vol*8),恰为旧布局(push 后 sub frame)中
	 * push 数据所在的位置。 */
	return any ? maxf + count_volatile_fIR_vars(func) * 8 : 0;
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

/* hoist 变体:volatile 变量存到帧内固定槽 [rsp+hoist_frame-n_vol*8+i*8],
 * 即 [rsp+maxf .. rsp+maxf+n_vol*8)。该区在最大 call 帧(maxf)之上,不受
 * 影子区/中转区([rsp+0..maxf))装载影响,与旧布局(push 后 sub frame)中
 * push 数据所在的位置一致。Win64/SysV 共用(槽位只依赖 maxf)。 */
static void spill_volatile_hoisted(IrBuffer *ir, SsaFunction *func, int hoist_frame) {
	if (!func || !func->var_reg_map) return;
	int n_vol = count_volatile_fIR_vars(func);
	int nv = 0;
	for (int i = 0; i < func->var_count; i++) {
		IrReg r = (IrReg)func->var_reg_map[i];
		if (r == REG_R8 || r == REG_R9)
			ir_mov_mem_reg(ir, REG_RSP, hoist_frame - n_vol * 8 + (nv++) * 8, r);
	}
}

static void fill_volatile_hoisted(IrBuffer *ir, SsaFunction *func, int hoist_frame) {
	if (!func || !func->var_reg_map) return;
	/* 倒序 load,与 store 顺序对称 */
	int n_vol = count_volatile_fIR_vars(func);
	int nv = n_vol - 1;
	for (int i = func->var_count - 1; i >= 0; i--) {
		IrReg r = (IrReg)func->var_reg_map[i];
		if (r == REG_R8 || r == REG_R9)
			ir_mov_reg_mem(ir, r, REG_RSP, hoist_frame - n_vol * 8 + (nv--) * 8);
	}
}

/* 鐑害鍒嗘瀽锛氱粺璁℃瘡涓?var slot 鍦ㄥ惊鐜腑鐨勪娇鐢ㄩ鐜囷紝鏈€鐑殑 7 涓嬁瀵勫瓨鍣?*/
static void compute_var_reg_map(SsaFunction *func, int var_count,
                                const bool *shared_slots) {
	if (var_count <= 0) { func->var_reg_map = NULL; func->var_count = 0; return; }

	func->var_count = var_count;
	func->var_reg_map = (int *)calloc(var_count, sizeof(int));
	func->var_remat_param = (int *)malloc((size_t)var_count * sizeof(int));
	func->var_remat_mul = (int64_t *)calloc((size_t)var_count, sizeof(int64_t));
	func->var_remat_add = (int64_t *)calloc((size_t)var_count, sizeof(int64_t));
	bool *remat_bad = (bool *)calloc((size_t)var_count, sizeof(bool));
	for (int i = 0; i < var_count; i++) {
		func->var_reg_map[i] = REG_NONE;
		func->var_remat_param[i] = -1;
	}

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
			if (inst->IrNode == SSA_OP_STORE_VAR && inst->op1.kind == SSA_OPND_VREG &&
			    inst->op2.kind == SSA_OPND_IMM) {
				int slot = (int)inst->op2.u.imm, p; int64_t m, a;
				if (slot >= 0 && slot < var_count) {
					if (!affine_param_vreg(func, inst->op1.u.vreg, 0, &p, &m, &a) || p < 0)
						remat_bad[slot] = true;
					else if (func->var_remat_param[slot] < 0) {
						func->var_remat_param[slot] = p;
						func->var_remat_mul[slot] = m;
						func->var_remat_add[slot] = a;
					} else if (func->var_remat_param[slot] != p ||
					           func->var_remat_mul[slot] != m ||
					           func->var_remat_add[slot] != a)
						remat_bad[slot] = true;
				}
			}
		}
	}

	int *sorted = (int *)malloc(var_count * sizeof(int));
	for (int i = 0; i < var_count; i++) sorted[i] = i;
	/* Sort slots by descending hotness.  The previous bubble-selection
	 * loop was O(var_count^2) per function with var_count being the
	 * module-wide slot count, so it exploded on multi-function files. */
	gs_sort_hotness = hotness;
	qsort(sorted, (size_t)var_count, sizeof(int), cmp_hotness_desc);
	gs_sort_hotness = NULL;

	int pool_size = 0;
	const IrReg *pool = fir_pool(&pool_size);
	int assigned = 0;
	for (int i = 0; i < var_count && assigned < pool_size; i++) {
		int slot = sorted[i];
		if (hotness[slot] > 0 && (!shared_slots || !shared_slots[slot])) {
			func->var_reg_map[slot] = (int)pool[assigned];
			assigned++;
		}
	}
	/* Prefer recomputing a private parameter-derived affine value after a
	 * call over storing it in mira_vars when the fixed register pool is full. */
	for (int i = 0; i < var_count; ++i) {
		if (func->var_reg_map[i] == REG_NONE &&
		    (!shared_slots || !shared_slots[i]) &&
		    !remat_bad[i] && func->var_remat_param[i] >= 0)
			func->var_reg_map[i] = VAR_REG_REMAT;
	}

	free(remat_bad);
	free(hotness);
	free(sorted);
}

/* 闁绘せ鏅濋幃濠勨偓闈涘閻°劑宕抽妸锔叫侀悘? 濞?7 濞戞搩浜顏呯┍濠靛牊娈岄悗闈涘閻°劑宕抽妸銉ユ濞戞挸闄eg闁告帒妫濋崢?*/
static IrReg map_phys_reg(int phys_reg) {
	return mira_abi_map_phys_reg(phys_reg);
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
static bool resolve_shift_immediate(const SsaFunction *func, SsaOperand operand,
	int *amount) {
	if (!amount) return false;
	if (operand.kind == SSA_OPND_IMM) {
		*amount = (int)(operand.u.imm & 63);
		return true;
	}
	if (!func || operand.kind != SSA_OPND_VREG || operand.u.vreg == 0 ||
		operand.u.vreg >= (VReg)func->vreg_defs_cap)
		return false;
	SsaInst *def = func->vreg_defs[operand.u.vreg];
	if (!def || def->IrNode != SSA_OP_IMM || def->type != SSA_TYPE_INT ||
		def->op1.kind != SSA_OPND_IMM)
		return false;
	*amount = (int)(def->op1.u.imm & 63);
	return true;
}

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

/* Lower constants whose multiplication is cheaper as a move, negate, or
 * shift. Unsigned magnitude also handles INT64_MIN without signed overflow. */
static int try_lower_mul_imm(IrBuffer *ir, IrReg dst, IrReg src, int64_t imm) {
	if (imm == 0) {
		ir_mov_reg_imm(ir, dst, 0);
		return 1;
	}
	if (imm == 1) {
		if (dst != src) ir_mov_reg_reg(ir, dst, src);
		return 1;
	}
	if (imm == -1) {
		if (dst != src) ir_mov_reg_reg(ir, dst, src);
		ir_neg(ir, dst);
		return 1;
	}
	if (imm == 2) {
		/* lea dst, [src+src]: one ALU op beats mov+shl, dst==src safe */
		ir_lea_idx(ir, dst, src, src);
		return 1;
	}
	uint64_t magnitude = imm < 0
		? (uint64_t)0 - (uint64_t)imm : (uint64_t)imm;
	if ((magnitude & (magnitude - 1)) != 0) return 0;
	if (dst != src) ir_mov_reg_reg(ir, dst, src);
	int shift = 0;
	while ((UINT64_C(1) << shift) != magnitude) ++shift;
	ir_shl_reg_imm(ir, dst, shift);
	if (imm < 0) ir_neg(ir, dst);
	return 1;
}


void ssa_lower_function(SsaFunction *func, IrBuffer *ir) {
	_cur_lower_func = func;
	int hoist_frame = hoist_call_frame(func);
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
	/* 目标 ABI 要求被调用者保存这些寄存器。fIR 把热点语言变量放在里面,
	 * 因此每个生成的函数都保存实际用到的非易失池成员。压栈数量随 ABI
	 * 不同(Win64 最多 7 个,SysV 最多 5 个),需据此补齐 RSP 的 16 字节
	 * 对齐(见 local_frame_size)。 */
	{
		int pool_size = 0;
		const IrReg *pool = fir_pool(&pool_size);
		int nvol = fir_volatile_start();
		for (int ri = 0; ri < nvol && ri < pool_size; ri++)
			if (function_uses_nonvolatile(func, pool[ri]))
				ir_push(ir, pool[ri]);
	}
	if (local_frame_size(func) > 0)
		ir_sub_reg_imm(ir, REG_RSP, local_frame_size(func));
	if (hoist_frame > 0)
		ir_sub_reg_imm(ir, REG_RSP, hoist_frame);

	/* SysV: 前 6 个参数在寄存器(RDI/RSI/RDX/RCX/R8/R9),无调用方影子区。
	 * 把它们保存到本地帧顶部的归宿区,使 LOAD_PARAM 能用统一栈偏移读取
	 * (与 Win64 读 [rbp+16..] 对称)。Win64 跳过此步(参数已在影子区)。 */
	{
		int nsav = sysv_param_save_count(func);
		if (nsav > 0) {
			IrReg sv[6];
			sv[0] = mira_abi_int_arg_reg(0);  /* RDI */
			sv[1] = mira_abi_int_arg_reg(1);  /* RSI */
			sv[2] = mira_abi_int_arg_reg(2);  /* RDX */
			sv[3] = mira_abi_int_arg_reg(3);  /* RCX */
			sv[4] = mira_abi_int_arg_reg(4);  /* R8  */
			sv[5] = mira_abi_int_arg_reg(5);  /* R9  */
			for (int i = 0; i < nsav && i < 6; i++)
				ir_mov_mem_reg(ir, REG_RBP, sysv_param_home_offset(func, i), sv[i]);
		}
	}

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
						if (try_lower_mul_imm(ir, REG_R10, r1, imm)) {
							/* result is already in the spill scratch register */
						} else if (imm >= INT32_MIN && imm <= INT32_MAX) {
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
					if (try_lower_mul_imm(ir, dst_reg, r1, imm)) {
						/* selected without a multiply */
					} else if (imm >= INT32_MIN && imm <= INT32_MAX) {
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
				if (inst->IrNode == SSA_OP_FCMP_EQ ||
					inst->IrNode == SSA_OP_FCMP_LT ||
					inst->IrNode == SSA_OP_FCMP_LE) {
					IrReg ordered = dst_reg == REG_R10 ? REG_R11 : REG_R10;
					ir_setcc(ir, IR_SETNP, ordered);
					ir_movzx_reg8(ir, ordered, ordered);
					ir_and_reg_reg(ir, dst_reg, ordered);
				} else if (inst->IrNode == SSA_OP_FCMP_NE) {
					IrReg ordered = dst_reg == REG_R10 ? REG_R11 : REG_R10;
					ir_setcc(ir, IR_SETNP, ordered);
					ir_movzx_reg8(ir, ordered, ordered);
					ir_xor_reg_imm(ir, ordered, 1);
					ir_or_reg_reg(ir, dst_reg, ordered);
				}
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
				IrReg r1 = load_operand(ir, func, inst->op1, REG_R10);
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
				IrReg scratch  = REG_R10;
				IrReg scratch2 = REG_R11;
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
				/* RAX/RCX/RDX/R8/R9 are allocator-owned.  Literal and spill
				 * materialization may only use the reserved R10/R11 pair. */
				IrReg scratch = REG_R10;
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
					IrReg scratch2 = REG_R11;
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
			case SSA_OP_SHL:
			case SSA_OP_ASHR:
			case SSA_OP_LSHR: {
				/* x86 variable shifts require CL.  Preserve both operands in the
				 * dedicated lowering scratch registers before touching RCX, because
				 * the allocator may assign RCX to either input or the destination. */
				IrReg value = load_operand(ir, func, inst->op1, REG_R10);
				if (value != REG_R10) ir_mov_reg_reg(ir, REG_R10, value);
				int amount = 0;
				if (resolve_shift_immediate(func, inst->op2, &amount)) {
					if (inst->IrNode == SSA_OP_SHL) ir_shl_reg_imm(ir, REG_R10, amount);
					else if (inst->IrNode == SSA_OP_ASHR) ir_sar_reg_imm(ir, REG_R10, amount);
					else ir_shr_reg_imm(ir, REG_R10, amount);
				} else {
					/* RCX is an allocator-owned register outside this instruction.
					 * CL is only a temporary architectural constraint, so preserve
					 * the old live value across the shift. */
					ir_push(ir, REG_RCX);
					IrReg amount = load_operand(ir, func, inst->op2, REG_R11);
					if (amount != REG_RCX) ir_mov_reg_reg(ir, REG_RCX, amount);
					if (inst->IrNode == SSA_OP_SHL) ir_shl_reg_cl(ir, REG_R10);
					else if (inst->IrNode == SSA_OP_ASHR) ir_sar_reg_cl(ir, REG_R10);
					else ir_shr_reg_cl(ir, REG_R10);
					ir_pop(ir, REG_RCX);
				}
				if (dst_reg != REG_R10) ir_mov_reg_reg(ir, dst_reg, REG_R10);
				break;
			}
			case SSA_OP_OR: {
				bool op1_spilled = inst->op1.kind == SSA_OPND_VREG &&
					is_spilled(func, inst->op1.u.vreg);
				bool op2_spilled = inst->op2.kind == SSA_OPND_VREG &&
					is_spilled(func, inst->op2.u.vreg);
				IrReg r1 = load_operand(ir, func, inst->op1, REG_R10);
				IrReg r2 = load_operand(ir, func, inst->op2, REG_R11);
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
				IrReg r1 = load_operand(ir, func, inst->op1, REG_R10);
				IrReg r2 = load_operand(ir, func, inst->op2, REG_R11);
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
				IrReg r1 = load_operand(ir, func, inst->op1, REG_R10);
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
					/* A branch condition can be spilled, especially after PHI
					 * destruction creates edge-local definitions.  Reading only
					 * the register map turns REG_NONE into a bogus condition. */
					IrReg cond = load_operand(ir, func, inst->op1, REG_R10);
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
				int nreg_params_tail = mira_abi_int_arg_reg_count();
				/* tail-call 仅在 Win64 启用:Win64 有调用方影子区可在撤栈前快照
				 * 参数值。SysV 无影子区,撤栈后无法安全重装寄存器参数,因此
				 * 退化为普通 CALL(仅损失一个尾调用优化,不影响正确性)。 */
				int is_tail = mira_target_abi == MIRA_ABI_WIN64 &&
					nparams <= nreg_params_tail && inst->dst > 0 && inst->next &&
					inst->next->IrNode == SSA_OP_RET &&
					inst->next->op1.kind == SSA_OPND_VREG &&
					inst->next->op1.u.vreg == inst->dst &&
					inst->operands[0].kind == SSA_OPND_SYM;
					if (is_tail) {
						/* Win64 tail-call:把参数值先快照到调用方影子区 [rbp+16..],
						 * 撤栈后从那里装载到 RCX/RDX/R8/R9 再 jmp。(is_tail 已保证
						 * 当前为 Win64,SysV 不走此路径。)
						 * 注意:装载必须发生在 pop rbp 之前——pop 之后 rbp 已恢复
						 * 为调用者的帧,再读 [rbp+16..] 会读到调用者栈(越界崩)。 */
						for (int ai = 0; ai < nparams; ai++) {
							SsaOperand arg = inst->operands[ai + 1];
							IrReg src;
							if (arg.kind == SSA_OPND_IMM) {
								ir_mov_reg_imm(ir, REG_R10, arg.u.imm);
								src = REG_R10;
							} else src = load_operand(ir, func, arg, REG_R10);
							ir_mov_mem_reg(ir, REG_RBP, 16 + ai * 8, src);
						}
						if (local_frame_size(func) > 0)
							ir_add_reg_imm(ir, REG_RSP, local_frame_size(func));
						if (hoist_frame > 0)
							ir_add_reg_imm(ir, REG_RSP, hoist_frame);
						{
							int _ps = 0;
							const IrReg *_pl = fir_pool(&_ps);
							int _nv = fir_volatile_start();
							for (int ri = _nv - 1; ri >= 0; ri--)
								if (function_uses_nonvolatile(func, _pl[ri]))
									ir_pop(ir, _pl[ri]);
						}
						if (nparams >= 4) ir_mov_reg_mem(ir, REG_R9,  REG_RBP, 40);
						if (nparams >= 3) ir_mov_reg_mem(ir, REG_R8,  REG_RBP, 32);
						if (nparams >= 2) ir_mov_reg_mem(ir, REG_RDX, REG_RBP, 24);
						if (nparams >= 1) ir_mov_reg_mem(ir, REG_RCX, REG_RBP, 16);
						ir_pop(ir, REG_RBP);
						ir_jmp_extern(ir, inst->operands[0].u.sym);
						inst = inst->next; /* skip the RET consumed by the tail jump */
						break;
					}
				int frame = call_site_frame(func, nparams, 0);
				int nreg_params = mira_abi_int_arg_reg_count();

					if (mira_target_abi == MIRA_ABI_WIN64) {
						/* Win64: 32 字节影子空间 + 溢出参数。前 4 参通过影子空间
						 * [rsp+0..23] 中转后再装载到 RCX/RDX/R8/R9,溢出参数落在
						 * [rsp+32..]。此路径保持与抽象前逐字节一致。 */
						if (hoist_frame) {
							/* hoist:spill 槽固定为 [rsp+maxf+i*8],在影子区
							 * ([rsp+0..31]) 与溢出参数之上,不受装载影响 */
							spill_volatile_hoisted(ir, func, hoist_frame);
						} else {
							spill_volatile_fIR_vars(ir, func);
						}
						if (!hoist_frame)
							ir_sub_reg_imm(ir, REG_RSP, frame);

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
				} else {
					/* SysV: 无影子空间。前 6 参装载到 RDI/RSI/RDX/RCX/R8/R9,
					 * 溢出参数放在 [rsp+relay..]。为避免装载顺序冲突(某参数当前
					 * 恰好就在目标寄存器里),前 6 参先统一存到一个栈中转区,
					 * 再装车;溢出参数单独布置在中转区之后。
					 *   frame = 6*8(中转) + extra*8(溢出) + 对齐 */
					int relay = nreg_params * 8;

					/* hoist 下与 Win64 相同:spill 槽在 [rsp+maxf+i*8],
					 * 位于中转区 ([rsp+0..47]) 与溢出参数之上 */
					if (hoist_frame) {
						spill_volatile_hoisted(ir, func, hoist_frame);
					} else {
						spill_volatile_fIR_vars(ir, func);
					}
					if (!hoist_frame)
						ir_sub_reg_imm(ir, REG_RSP, frame);

					#define SLOAD_ARG_TO_RELAY(idx, opnd) \
						if((opnd).kind == SSA_OPND_VREG) { \
							IrReg ar = load_operand(ir, func, (opnd), REG_R10); \
							ir_mov_mem_reg(ir, REG_RSP, (idx)*8, ar); \
						} else if((opnd).kind == SSA_OPND_IMM) { \
							ir_mov_reg_imm(ir, REG_RAX, (opnd).u.imm); \
							ir_mov_mem_reg(ir, REG_RSP, (idx)*8, REG_RAX); \
						}
					int top = nparams < nreg_params ? nparams : nreg_params;
					/* 倒序装载(与 Win64 影子区装载顺序一致):IMM 参数经 RAX 中转,
					 * 正序会先写坏 RAX,覆盖后面尚未存栈的 VREG 参数。 */
					for (int ai = top - 1; ai >= 0; ai--)
						SLOAD_ARG_TO_RELAY(ai, inst->operands[ai + 1]);
					#undef SLOAD_ARG_TO_RELAY

					#define SLOAD_ARG_TO_STACK(idx, opnd) \
						if((opnd).kind == SSA_OPND_VREG) { \
							IrReg ar = load_operand(ir, func, (opnd), REG_R10); \
							ir_mov_mem_reg(ir, REG_RSP, relay + (idx)*8, ar); \
						} else if((opnd).kind == SSA_OPND_IMM) { \
							ir_mov_reg_imm(ir, REG_RAX, (opnd).u.imm); \
							ir_mov_mem_reg(ir, REG_RSP, relay + (idx)*8, REG_RAX); \
						}
					for (int ai = nparams - 1; ai >= nreg_params; ai--)
						SLOAD_ARG_TO_STACK(ai - nreg_params, inst->operands[ai + 1]);
					#undef SLOAD_ARG_TO_STACK

					/* 从中转区统一装载到 RDI/RSI/RDX/RCX/R8/R9 */
					if (top >= 6) ir_mov_reg_mem(ir, REG_R9,  REG_RSP, 40);
					if (top >= 5) ir_mov_reg_mem(ir, REG_R8,  REG_RSP, 32);
					if (top >= 4) ir_mov_reg_mem(ir, REG_RCX, REG_RSP, 24);
					if (top >= 3) ir_mov_reg_mem(ir, REG_RDX, REG_RSP, 16);
					if (top >= 2) ir_mov_reg_mem(ir, REG_RSI, REG_RSP, 8);
					if (top >= 1) ir_mov_reg_mem(ir, REG_RDI, REG_RSP, 0);
				}

				if(inst->operands[0].kind == SSA_OPND_SYM) {
					ir_call_extern(ir, inst->operands[0].u.sym);
				}
				
				if (!hoist_frame)
					ir_add_reg_imm(ir, REG_RSP, frame);
				if (hoist_frame) {
					/* hoist:从固定槽倒序 load,与 store 顺序对称 */
					fill_volatile_hoisted(ir, func, hoist_frame);
				} else {
					fill_volatile_fIR_vars(ir, func);
				}

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
				int n_vol = count_volatile_fIR_vars(func);
				int nreg_params = mira_abi_int_arg_reg_count();
				int shadow = mira_abi_shadow_space();
				int extra = nparams > nreg_params ? nparams - nreg_params : 0;
				int frame;
				/* ICALL 额外有一个 fptr 的 PUSH,所以对齐计算 +1 */

				/* === Spill: 保护 volatile 快变量 === */
				spill_volatile_fIR_vars(ir, func);

				/* 先取出函数指针并压栈,防止后续参数装载覆盖它 */
				IrReg fptr_reg = get_ssa_reg(func, inst->operands[0].u.vreg);
				ir_push(ir, fptr_reg);

				if (mira_target_abi == MIRA_ABI_WIN64) {
					/* Win64: 32 影子空间 + 溢出参数,前 4 参走影子中转。
					 * 此路径保持与抽象前逐字节一致。 */
					frame = shadow + extra * 8;
					if ((frame + (n_vol + 1) * 8) % 16 != 0) frame += 8;
					ir_sub_reg_imm(ir, REG_RSP, frame);

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
				} else {
					/* SysV: 前 6 参装载到 RDI/RSI/RDX/RCX/R8/R9,溢出参数在
					 * [rsp+relay..]。前 6 参先存栈中转区再装车,避免顺序冲突。
					 *   frame = 6*8(中转) + extra*8(溢出) + 对齐 */
					int relay = nreg_params * 8;
					frame = relay + extra * 8;
					if ((frame + (n_vol + 1) * 8) % 16 != 0) frame += 8;
					ir_sub_reg_imm(ir, REG_RSP, frame);

					#define SLOAD_ARG_TO_RELAY(idx, opnd) \
						if((opnd).kind == SSA_OPND_VREG) { \
							IrReg ar = load_operand(ir, func, (opnd), REG_R10); \
							ir_mov_mem_reg(ir, REG_RSP, (idx)*8, ar); \
						} else if((opnd).kind == SSA_OPND_IMM) { \
							ir_mov_reg_imm(ir, REG_RAX, (opnd).u.imm); \
							ir_mov_mem_reg(ir, REG_RSP, (idx)*8, REG_RAX); \
						}
					int top = nparams < nreg_params ? nparams : nreg_params;
					/* 倒序装载:与 CALL 路径一致,IMM 经 RAX 中转不能覆盖
					 * 尚未存栈的 VREG 参数。 */
					for (int ai = top - 1; ai >= 0; ai--)
						SLOAD_ARG_TO_RELAY(ai, inst->operands[ai + 1]);
					#undef SLOAD_ARG_TO_RELAY

					#define SLOAD_ARG_TO_STACK(idx, opnd) \
						if((opnd).kind == SSA_OPND_VREG) { \
							IrReg ar = load_operand(ir, func, (opnd), REG_R10); \
							ir_mov_mem_reg(ir, REG_RSP, relay + (idx)*8, ar); \
						} else if((opnd).kind == SSA_OPND_IMM) { \
							ir_mov_reg_imm(ir, REG_RAX, (opnd).u.imm); \
							ir_mov_mem_reg(ir, REG_RSP, relay + (idx)*8, REG_RAX); \
						}
					for (int ai = nparams - 1; ai >= nreg_params; ai--)
						SLOAD_ARG_TO_STACK(ai - nreg_params, inst->operands[ai + 1]);
					#undef SLOAD_ARG_TO_STACK

					if (top >= 6) ir_mov_reg_mem(ir, REG_R9,  REG_RSP, 40);
					if (top >= 5) ir_mov_reg_mem(ir, REG_R8,  REG_RSP, 32);
					if (top >= 4) ir_mov_reg_mem(ir, REG_RCX, REG_RSP, 24);
					if (top >= 3) ir_mov_reg_mem(ir, REG_RDX, REG_RSP, 16);
					if (top >= 2) ir_mov_reg_mem(ir, REG_RSI, REG_RSP, 8);
					if (top >= 1) ir_mov_reg_mem(ir, REG_RDI, REG_RSP, 0);
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
				{
					int frame_adj = local_frame_size(func) + hoist_frame;
					if (frame_adj > 0)
						ir_add_reg_imm(ir, REG_RSP, frame_adj);
				}
				{
					int _ps = 0;
					const IrReg *_pl = fir_pool(&_ps);
					int _nv = fir_volatile_start();
					for (int ri = _nv - 1; ri >= 0; ri--)
						if (function_uses_nonvolatile(func, _pl[ri]))
							ir_pop(ir, _pl[ri]);
				}
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
				int remat = func->var_reg_map && slot >= 0 && slot < func->var_count &&
					func->var_reg_map[slot] == VAR_REG_REMAT;
				if (remat && dst_reg != REG_NONE) {
					int p = func->var_remat_param[slot];
					int64_t m = func->var_remat_mul[slot];
					int64_t a = func->var_remat_add[slot];
					if (mira_target_abi == MIRA_ABI_SYSV && mira_abi_param_in_reg(p))
						ir_mov_reg_mem(ir, dst_reg, REG_RBP, sysv_param_home_offset(func, p));
					else if (mira_target_abi == MIRA_ABI_SYSV)
						ir_mov_reg_mem(ir, dst_reg, REG_RBP,
						               16 + mira_abi_int_arg_reg_count() * 8 +
						               (p - mira_abi_int_arg_reg_count()) * 8);
					else
						ir_mov_reg_mem(ir, dst_reg, REG_RBP, 16 + p * 8);
					if (m != 1) ir_imul_reg_imm(ir, dst_reg, dst_reg, m);
					if (a != 0) ir_add_reg_imm(ir, dst_reg, a);
					break;
				}
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
				if (func->var_reg_map && slot >= 0 && slot < func->var_count &&
				    func->var_reg_map[slot] == VAR_REG_REMAT)
					break;
				
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
				/* dst = param[index]
				 * Win64: 读调用方影子区 [rbp + 16 + index*8](调用方为每个参数
				 *        都预留了 home space,前 4 个寄存器参数也被调用方铺到那)。
				 * SysV: 前 6 个参数在寄存器,prologue 已把它们存到本地帧归宿区
				 *        [rbp + sysv_param_home_offset(index)];第 7 个起在调用方
				 *        栈,从 [rbp + 16 + (index-6)*8] 读。 */
				int pidx = (int)inst->op1.u.imm;
				if (dst_float_reg != REG_NONE) {
					if (mira_abi_param_in_reg(pidx) && mira_target_abi == MIRA_ABI_SYSV) {
						ir_mov_reg_mem(ir, REG_R10, REG_RBP,
						                   sysv_param_home_offset(func, pidx));
					} else if (mira_target_abi == MIRA_ABI_SYSV) {
						int nreg = mira_abi_int_arg_reg_count();
						ir_mov_reg_mem(ir, REG_R10, REG_RBP,
						                   16 + nreg * 8 + (pidx - nreg) * 8);
					} else {
						ir_mov_reg_mem(ir, REG_R10, REG_RBP, 16 + pidx * 8);
					}
					ir_movq_xmm_reg(ir, dst_float_reg, REG_R10);
				} else if (dst_reg != REG_NONE) {
					if (mira_abi_param_in_reg(pidx) && mira_target_abi == MIRA_ABI_SYSV) {
						ir_mov_reg_mem(ir, dst_reg, REG_RBP,
						               sysv_param_home_offset(func, pidx));
					} else if (mira_target_abi == MIRA_ABI_SYSV) {
						/* 第 7 参起在调用方栈。调用方把溢出参数放在
						 * [rsp + relay + k*8](relay = 寄存器参数中转区 6*8),
						 * 这里必须加回 relay 偏移。 */
						int nreg = mira_abi_int_arg_reg_count();
						ir_mov_reg_mem(ir, dst_reg, REG_RBP,
						               16 + nreg * 8 + (pidx - nreg) * 8);
					} else {
						ir_mov_reg_mem(ir, dst_reg, REG_RBP, 16 + pidx * 8);
					}
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
	int *func_var_counts = mod->func_count > 0
		? calloc((size_t)mod->func_count, sizeof(*func_var_counts)) : NULL;
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
				if (func_var_counts && slot >= func_var_counts[fi])
					func_var_counts[fi] = slot + 1;
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

	/* A function-private slot that is never loaded cannot communicate a value
	 * to either this function or another function.  Remove these stores after
	 * module ownership is known; doing it earlier is unsafe because a callee
	 * may own a load of a genuinely shared slot. */
	if (shared_slots) {
		for (int fi = 0; fi < mod->func_count; ++fi) {
			SsaFunction *func = mod->functions[fi];
			bool *loaded = calloc((size_t)var_count, sizeof(bool));
			if (!loaded) continue;
			for (int bi = 0; bi < func->block_count; ++bi)
				for (SsaInst *inst = func->blocks[bi]->inst_head; inst; inst = inst->next)
					if (inst->IrNode == SSA_OP_LOAD_VAR && inst->op1.kind == SSA_OPND_IMM &&
					    inst->op1.u.imm >= 0 && inst->op1.u.imm < var_count)
						loaded[inst->op1.u.imm] = true;
			for (int bi = 0; bi < func->block_count; ++bi) {
				SsaBasicBlock *block = func->blocks[bi];
				for (SsaInst *inst = block->inst_head; inst; ) {
					SsaInst *next = inst->next;
					if (inst->IrNode == SSA_OP_STORE_VAR && inst->op2.kind == SSA_OPND_IMM) {
						int slot = (int)inst->op2.u.imm;
						if (slot >= 0 && slot < var_count && !shared_slots[slot] && !loaded[slot]) {
							if (inst->prev) inst->prev->next = inst->next;
							else block->inst_head = inst->next;
							if (inst->next) inst->next->prev = inst->prev;
							else block->inst_tail = inst->prev;
						}
					}
					inst = next;
				}
			}
			free(loaded);
		}
	}

	for (int i = 0; i < mod->func_count; i++)
		compute_var_reg_map(mod->functions[i],
			func_var_counts ? func_var_counts[i] : var_count, shared_slots);
	free(func_var_counts);
	free(first_owner);
	free(shared_slots);
}


