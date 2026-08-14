/* ir.c 閳?IR 閸欐垵鐨犻崙鑺ユ殶鐎圭偟骞?
 *
 * 閹碘偓閺?ir_emit_* 閸戣姤鏆熺亸?IrInst 鏉╄棄濮為崚?IrBuffer 閻╃绨插▓鍏歌厬閵?
 */
#include "ir.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ========== 閸愬懘鍎村銉ュ徔鐎?========== */

#define GROW(arr, count, cap, type) do { \
	if ((count) >= (cap)) { \
		(cap) = (cap) ? (cap) * 2 : 256; \
		(arr) = (type *)realloc((arr), (cap) * sizeof(type)); \
	} \
} while(0)

static void push_text(IrBuffer *ir, IrInst inst) {
	GROW(ir->text, ir->text_count, ir->text_cap, IrInst);
	ir->text[ir->text_count++] = inst;
}

static void push_data(IrBuffer *ir, IrInst inst) {
	GROW(ir->data, ir->data_count, ir->data_cap, IrInst);
	ir->data[ir->data_count++] = inst;
}
static void push_bss(IrBuffer *ir, IrInst inst) {
	GROW(ir->bss, ir->bss_count, ir->bss_cap, IrInst);
	ir->bss[ir->bss_count++] = inst;
}

/* ========== Init / Free ========== */

void ir_init(IrBuffer *ir) {
	memset(ir, 0, sizeof(*ir));
}

void ir_free(IrBuffer *ir) {
	/* data bytes 闇€瑕侀噴鏀?*/
	for (int i = 0; i < ir->data_count; i++) {
		if (ir->data[i].IrNode == IR_DATA_BYTES && ir->data[i].data)
			free(ir->data[i].data);
		if (ir->data[i].sym_name)
			free(ir->data[i].sym_name);
	}
	for (int i = 0; i < ir->text_count; i++) {
		if (ir->text[i].sym_name)
			free(ir->text[i].sym_name);
	}
	for (int i = 0; i < ir->bss_count; i++) {
		if (ir->bss[i].sym_name)
			free(ir->bss[i].sym_name);
	}
	free(ir->text);
	free(ir->data);
	free(ir->bss);
	for (int i = 0; i < ir->extern_count; i++) free(ir->externs[i]);
	free(ir->externs);
	for (int i = 0; i < ir->global_count; i++) free(ir->globals[i]);
	free(ir->globals);
	memset(ir, 0, sizeof(*ir));
}

/* 闁氨鏁?*/
void ir_emit_raw(IrBuffer *ir, IrInst inst) {
	push_text(ir, inst);
}

/* ========== .text 娈靛彂灏?========== */

/* --- 閺佺増宓佺粔璇插З --- */

void ir_mov_reg_reg(IrBuffer *ir, IrReg dst, IrReg src) {
	IrInst i = {0};
	i.IrNode = IR_MOV_REG_REG;
	i.dst = dst; i.src = src;
	push_text(ir, i);
}

void ir_mov_reg_imm(IrBuffer *ir, IrReg dst, int64_t imm) {
	IrInst i = {0};
	i.IrNode = IR_MOV_REG_IMM;
	i.dst = dst; i.imm = imm;
	push_text(ir, i);
}

void ir_mov_reg_mem(IrBuffer *ir, IrReg dst, IrReg base, int64_t disp) {
	IrInst i = {0};
	i.IrNode = IR_MOV_REG_MEM;
	i.dst = dst; i.src = base; i.imm = disp;
	push_text(ir, i);
}

void ir_mov_mem_reg(IrBuffer *ir, IrReg base, int64_t disp, IrReg src) {
	IrInst i = {0};
	i.IrNode = IR_MOV_MEM_REG;
	i.dst = base; i.src = src; i.imm = disp;
	push_text(ir, i);
}

void ir_mov_mem_imm(IrBuffer *ir, IrReg base, int64_t disp, int64_t imm) {
	IrInst i = {0};
	i.IrNode = IR_MOV_MEM_IMM;
	i.dst = base; i.imm = disp; i.extra_imm = imm;
	push_text(ir, i);
}

void ir_mov_mem8_reg(IrBuffer *ir, IrReg base, int64_t disp, IrReg src8) {
	IrInst i = {0};
	i.IrNode = IR_MOV_MEM8_REG;
	i.dst = base; i.src = src8; i.imm = disp;
	push_text(ir, i);
}

void ir_movzx_reg_mem8(IrBuffer *ir, IrReg dst, IrReg base, int64_t disp) {
	IrInst i = {0};
	i.IrNode = IR_MOVZX_REG_MEM8;
	i.dst = dst; i.src = base; i.imm = disp;
	push_text(ir, i);
}

void ir_lea(IrBuffer *ir, IrReg dst, IrReg base, int64_t disp) {
	IrInst i = {0};
	i.IrNode = IR_LEA;
	i.dst = dst; i.src = base; i.imm = disp;
	push_text(ir, i);
}

void ir_lea_idx(IrBuffer *ir, IrReg dst, IrReg base, IrReg index) {
	IrInst i = {0};
	i.IrNode = IR_LEA_IDX;
	i.dst = dst; i.src = base; i.imm = (int64_t)index;
	push_text(ir, i);
}

void ir_lea_rip(IrBuffer *ir, IrReg dst, const char *sym) {
	IrInst i = {0};
	i.IrNode = IR_LEA_RIP;
	i.dst = dst; i.sym_name = strdup(sym);
	push_text(ir, i);
}

void ir_lea_rip_label(IrBuffer *ir, IrReg dst, int label_id) {
	IrInst i = {0};
	i.IrNode = IR_LEA_RIP;
	i.dst = dst; i.label_id = label_id; i.sym_name = NULL;
	push_text(ir, i);
}

/* --- 绾兛娆㈤弽?--- */

void ir_push(IrBuffer *ir, IrReg reg) {
	IrInst i = {0};
	i.IrNode = IR_PUSH_REG;
	i.dst = reg;
	push_text(ir, i);
}

void ir_pop(IrBuffer *ir, IrReg reg) {
	IrInst i = {0};
	i.IrNode = IR_POP_REG;
	i.dst = reg;
	push_text(ir, i);
}

/* --- 缁犳婀?--- */

void ir_add_reg_reg(IrBuffer *ir, IrReg dst, IrReg src) {
	IrInst i = {0};
	i.IrNode = IR_ADD_REG_REG;
	i.dst = dst; i.src = src;
	push_text(ir, i);
}

void ir_add_reg_imm(IrBuffer *ir, IrReg dst, int64_t imm) {
	IrInst i = {0};
	i.IrNode = IR_ADD_REG_IMM;
	i.dst = dst; i.imm = imm;
	push_text(ir, i);
}

void ir_add_mem_imm(IrBuffer *ir, IrReg base, int64_t disp, int64_t val) {
	IrInst i = {0};
	i.IrNode = IR_ADD_MEM_IMM;
	i.dst = base; i.imm = disp; i.extra_imm = val;
	push_text(ir, i);
}

void ir_sub_reg_reg(IrBuffer *ir, IrReg dst, IrReg src) {
	IrInst i = {0};
	i.IrNode = IR_SUB_REG_REG;
	i.dst = dst; i.src = src;
	push_text(ir, i);
}

void ir_sub_reg_imm(IrBuffer *ir, IrReg dst, int64_t imm) {
	IrInst i = {0};
	i.IrNode = IR_SUB_REG_IMM;
	i.dst = dst; i.imm = imm;
	push_text(ir, i);
}

void ir_imul_reg_reg(IrBuffer *ir, IrReg dst, IrReg src) {
	IrInst i = {0};
	i.IrNode = IR_IMUL_REG_REG;
	i.dst = dst; i.src = src;
	push_text(ir, i);
}

void ir_idiv(IrBuffer *ir, IrReg divisor) {
	IrInst i = {0};
	i.IrNode = IR_IDIV_REG;
	i.src = divisor;
	push_text(ir, i);
}

void ir_inc_reg(IrBuffer *ir, IrReg reg) {
	IrInst i = {0};
	i.IrNode = IR_INC_REG;
	i.dst = reg;
	push_text(ir, i);
}

void ir_inc_mem(IrBuffer *ir, IrReg base, int64_t disp) {
	IrInst i = {0};
	i.IrNode = IR_INC_MEM;
	i.dst = base; i.imm = disp;
	push_text(ir, i);
}

void ir_neg(IrBuffer *ir, IrReg reg) {
	IrInst i = {0};
	i.IrNode = IR_NEG_REG;
	i.dst = reg;
	push_text(ir, i);
}

void ir_cqo(IrBuffer *ir) {
	IrInst i = {0};
	i.IrNode = IR_CQO;
	push_text(ir, i);
}

/* --- 娴ｅ秷绻嶇粻?--- */

void ir_xor_reg_reg(IrBuffer *ir, IrReg dst, IrReg src) {
	IrInst i = {0};
	i.IrNode = IR_XOR_REG_REG;
	i.dst = dst; i.src = src;
	push_text(ir, i);
}

void ir_xor_reg_imm(IrBuffer *ir, IrReg dst, int64_t imm) {
	IrInst i = {0};
	i.IrNode = IR_XOR_REG_IMM;
	i.dst = dst; i.imm = imm;
	push_text(ir, i);
}

void ir_and_reg_reg(IrBuffer *ir, IrReg dst, IrReg src) {
	IrInst i = {0};
	i.IrNode = IR_AND_REG_REG;
	i.dst = dst; i.src = src;
	push_text(ir, i);
}

void ir_or_reg_reg(IrBuffer *ir, IrReg dst, IrReg src) {
	IrInst i = {0};
	i.IrNode = IR_OR_REG_REG;
	i.dst = dst; i.src = src;
	push_text(ir, i);
}

void ir_shl_reg_imm(IrBuffer *ir, IrReg dst, int imm) {
	IrInst i = {0};
	i.IrNode = IR_SHL_REG_IMM;
	i.dst = dst; i.imm = imm;
	push_text(ir, i);
}

void ir_shr_reg_imm(IrBuffer *ir, IrReg dst, int imm) {
	IrInst i = {0};
	i.IrNode = IR_SHR_REG_IMM;
	i.dst = dst; i.imm = imm;
	push_text(ir, i);
}

static void ir_shift_reg_cl(IrBuffer *ir, IrOpcode opcode, IrReg dst) {
	IrInst i = {0};
	i.IrNode = opcode;
	i.dst = dst;
	push_text(ir, i);
}

void ir_shl_reg_cl(IrBuffer *ir, IrReg dst) { ir_shift_reg_cl(ir, IR_SHL_REG_CL, dst); }
void ir_shr_reg_cl(IrBuffer *ir, IrReg dst) { ir_shift_reg_cl(ir, IR_SHR_REG_CL, dst); }
void ir_sar_reg_cl(IrBuffer *ir, IrReg dst) { ir_shift_reg_cl(ir, IR_SAR_REG_CL, dst); }

void ir_not_reg(IrBuffer *ir, IrReg reg) {
	IrInst i = {0};
	i.IrNode = IR_NOT_REG;
	i.dst = reg;
	push_text(ir, i);
}

/* --- 濮ｆ棁绶?--- */

void ir_cmp_reg_reg(IrBuffer *ir, IrReg dst, IrReg src) {
	IrInst i = {0};
	i.IrNode = IR_CMP_REG_REG;
	i.dst = dst; i.src = src;
	push_text(ir, i);
}

void ir_cmp_reg_imm(IrBuffer *ir, IrReg dst, int64_t imm) {
	IrInst i = {0};
	i.IrNode = IR_CMP_REG_IMM;
	i.dst = dst; i.imm = imm;
	push_text(ir, i);
}

void ir_test_reg_reg(IrBuffer *ir, IrReg dst, IrReg src) {
	IrInst i = {0};
	i.IrNode = IR_TEST_REG_REG;
	i.dst = dst; i.src = src;
	push_text(ir, i);
}

/* --- 閺夆€叉鐎涙濡拋鍓х枂 --- */

void ir_setcc(IrBuffer *ir, IrOpcode setop, IrReg dst8) {
	IrInst i = {0};
	i.IrNode = setop;
	i.dst = dst8;
	push_text(ir, i);
}

void ir_movzx_reg8(IrBuffer *ir, IrReg dst32, IrReg src8) {
	IrInst i = {0};
	i.IrNode = IR_MOVZX_REG8;
	i.dst = dst32; i.src = src8;
	push_text(ir, i);
}

/* --- 鐠哄疇娴?--- */

void ir_jmp(IrBuffer *ir, int label_id) {
	IrInst i = {0};
	i.IrNode = IR_JMP;
	i.label_id = label_id;
	push_text(ir, i);
}

void ir_sar_reg_imm(IrBuffer *ir, IrReg dst, int imm) {
	IrInst i = {0};
	i.IrNode = IR_SAR_REG_IMM;
	i.dst = dst; i.imm = imm;
	push_text(ir, i);
}

void ir_jmp_extern(IrBuffer *ir, const char *sym) {
	IrInst i = {0};
	i.IrNode = IR_JMP_EXTERN;
	i.sym_name = strdup(sym);
	push_text(ir, i);
}

void ir_jcc(IrBuffer *ir, IrOpcode jop, int label_id) {
	IrInst i = {0};
	i.IrNode = jop;
	i.label_id = label_id;
	push_text(ir, i);
}

/* --- 鐠嬪啰鏁?--- */

void ir_call_label(IrBuffer *ir, int label_id) {
	IrInst i = {0};
	i.IrNode = IR_CALL_LABEL;
	i.label_id = label_id;
	push_text(ir, i);
}

void ir_call_extern(IrBuffer *ir, const char *sym) {
	IrInst i = {0};
	i.IrNode = IR_CALL_EXTERN;
	i.sym_name = strdup(sym);
	push_text(ir, i);
}

void ir_call_reg(IrBuffer *ir, IrReg reg) {
	IrInst i = {0};
	i.IrNode = IR_CALL_REG;
	i.dst = reg;
	push_text(ir, i);
}

void ir_ret(IrBuffer *ir) {
	IrInst i = {0};
	i.IrNode = IR_RET;
	push_text(ir, i);
}

/* --- 閺嶅洨顒?--- */

void ir_label(IrBuffer *ir, int label_id) {
	IrInst i = {0};
	i.IrNode = IR_LABEL;
	i.label_id = label_id;
	push_text(ir, i);
}

void ir_label_named(IrBuffer *ir, const char *name) {
	IrInst i = {0};
	i.IrNode = IR_LABEL_NAMED;
	i.sym_name = strdup(name);
	push_text(ir, i);
}

/* --- 绗﹀彿澹版槑 --- */

void ir_extern(IrBuffer *ir, const char *name) {
	/* 閸樺鍣?*/
	for (int j = 0; j < ir->extern_count; j++)
		if (strcmp(ir->externs[j], name) == 0) return;
	GROW(ir->externs, ir->extern_count, ir->extern_cap, char *);
	ir->externs[ir->extern_count++] = strdup(name);
}

void ir_global(IrBuffer *ir, const char *name) {
	for (int j = 0; j < ir->global_count; j++)
		if (strcmp(ir->globals[j], name) == 0) return;
	GROW(ir->globals, ir->global_count, ir->global_cap, char *);
	ir->globals[ir->global_count++] = strdup(name);
}

/* --- SSE 濞搭喚鍋?--- */

void ir_movq_xmm_reg(IrBuffer *ir, IrReg xmm, IrReg reg) {
	IrInst i = {0};
	i.IrNode = IR_MOVQ_XMM_REG;
	i.dst = xmm; i.src = reg;
	push_text(ir, i);
}

void ir_movq_reg_xmm(IrBuffer *ir, IrReg reg, IrReg xmm) {
	IrInst i = {0};
	i.IrNode = IR_MOVQ_REG_XMM;
	i.dst = reg; i.src = xmm;
	push_text(ir, i);
}

void ir_movsd_xmm_rip(IrBuffer *ir, IrReg xmm, const char *sym) {
	IrInst i = {0};
	i.IrNode = IR_MOVSD_XMM_MEM;
	i.dst = xmm; i.sym_name = strdup(sym);
	push_text(ir, i);
}

void ir_cvtsi2sd(IrBuffer *ir, IrReg xmm, IrReg reg) {
	IrInst i = {0};
	i.IrNode = IR_CVTSI2SD;
	i.dst = xmm; i.src = reg;
	push_text(ir, i);
}

void ir_cvttsd2si(IrBuffer *ir, IrReg reg, IrReg xmm) {
	IrInst i = {0};
	i.IrNode = IR_CVTTSD2SI;
	i.dst = reg; i.src = xmm;
	push_text(ir, i);
}

void ir_addsd(IrBuffer *ir, IrReg dst, IrReg src) {
	IrInst i = {0};
	i.IrNode = IR_ADDSD; i.dst = dst; i.src = src;
	push_text(ir, i);
}

void ir_subsd(IrBuffer *ir, IrReg dst, IrReg src) {
	IrInst i = {0};
	i.IrNode = IR_SUBSD; i.dst = dst; i.src = src;
	push_text(ir, i);
}

void ir_mulsd(IrBuffer *ir, IrReg dst, IrReg src) {
	IrInst i = {0};
	i.IrNode = IR_MULSD; i.dst = dst; i.src = src;
	push_text(ir, i);
}

void ir_divsd(IrBuffer *ir, IrReg dst, IrReg src) {
	IrInst i = {0};
	i.IrNode = IR_DIVSD; i.dst = dst; i.src = src;
	push_text(ir, i);
}

void ir_ucomisd(IrBuffer *ir, IrReg dst, IrReg src) {
	IrInst i = {0};
	i.IrNode = IR_UCOMISD; i.dst = dst; i.src = src;
	push_text(ir, i);
}

void ir_and_reg_imm(IrBuffer *ir, IrReg dst, int64_t imm) {
	IrInst i = {0};
	i.IrNode = IR_AND_REG_IMM;
	i.dst = dst; i.imm = imm;
	push_text(ir, i);
}

void ir_cmovcc(IrBuffer *ir, IrOpcode cmovop, IrReg dst, IrReg src) {
	IrInst i = {0};
	i.IrNode = cmovop;
	i.dst = dst; i.src = src;
	push_text(ir, i);
}

void ir_vfmadd132sd(IrBuffer *ir, IrReg dst, IrReg add, IrReg mul) {
	IrInst i = {0};
	i.IrNode = IR_VFMADD132SD; i.dst = dst; i.src = add; i.src2 = mul;
	push_text(ir, i);
}

void ir_imul_reg_imm(IrBuffer *ir, IrReg dst, IrReg src, int64_t imm) {
	IrInst i = {0};
	i.IrNode = IR_IMUL_REG_IMM;
	i.dst = dst; i.src = src; i.imm = imm;
	push_text(ir, i);
}

static void ir_emit_vec3(IrBuffer *ir, IrOpcode op, IrReg dst, IrReg src1, IrReg src2) {
	IrInst i = {0};
	i.IrNode = op; i.dst = dst; i.src = src1; i.src2 = src2;
	push_text(ir, i);
}

void ir_vpaddq(IrBuffer *ir, IrReg dst, IrReg src1, IrReg src2) {
	ir_emit_vec3(ir, IR_VPADDQ, dst, src1, src2);
}

void ir_vpsubq(IrBuffer *ir, IrReg dst, IrReg src1, IrReg src2) {
	ir_emit_vec3(ir, IR_VPSUBQ, dst, src1, src2);
}
void ir_vpxor(IrBuffer *ir, IrReg dst, IrReg src1, IrReg src2) {
	ir_emit_vec3(ir, IR_VPXOR, dst, src1, src2);
}
void ir_vpand(IrBuffer *ir, IrReg dst, IrReg src1, IrReg src2) {
	ir_emit_vec3(ir, IR_VPAND, dst, src1, src2);
}
void ir_vpor(IrBuffer *ir, IrReg dst, IrReg src1, IrReg src2) {
	ir_emit_vec3(ir, IR_VPOR, dst, src1, src2);
}

void ir_vpmulld(IrBuffer *ir, IrReg dst, IrReg src1, IrReg src2) {
	ir_emit_vec3(ir, IR_VPMULLD, dst, src1, src2);
}

void ir_vpcmpeqq(IrBuffer *ir, IrReg dst, IrReg src1, IrReg src2) {
	ir_emit_vec3(ir, IR_VPCMPEQQ, dst, src1, src2);
}

void ir_imul_wide(IrBuffer *ir, IrReg src) {
	IrInst i = {0};
	i.IrNode = IR_IMUL_WIDE_REG;
	i.src = src;
	push_text(ir, i);
}

void ir_mul_wide(IrBuffer *ir, IrReg src) {
	IrInst i = {0};
	i.IrNode = IR_MUL_WIDE_REG;
	i.src = src;
	push_text(ir, i);
}

void ir_vpbroadcastq(IrBuffer *ir, IrReg dst, IrReg src) {
	IrInst i = {0}; i.IrNode = IR_VPBROADCASTQ; i.dst = dst; i.src = src;
	push_text(ir, i);
}

void ir_vmovdqu_load(IrBuffer *ir, IrReg dst, IrReg base, int64_t disp) {
	IrInst i = {0}; i.IrNode = IR_VMOVDQU_LOAD; i.dst = dst; i.src = base; i.imm = disp;
	push_text(ir, i);
}

void ir_vmovdqu_store(IrBuffer *ir, IrReg base, int64_t disp, IrReg src) {
	IrInst i = {0}; i.IrNode = IR_VMOVDQU_STORE; i.dst = base; i.src = src; i.imm = disp;
	push_text(ir, i);
}

void ir_vzeroupper(IrBuffer *ir) {
	IrInst i = {0};
	i.IrNode = IR_VZEROUPPER;
	push_text(ir, i);
}

/* ========== .data 娈靛彂灏?========== */

void ir_data_label(IrBuffer *ir, const char *name) {
	IrInst i = {0};
	i.IrNode = IR_DATA_LABEL;
	i.sym_name = strdup(name);
	push_data(ir, i);
}

void ir_data_label_id(IrBuffer *ir, const char *prefix, int id) {
	char buf[128];
	snprintf(buf, sizeof(buf), "%s.%d", prefix, id);
	ir_data_label(ir, buf);
}

void ir_data_bytes(IrBuffer *ir, const uint8_t *bytes, int len) {
	IrInst i = {0};
	i.IrNode = IR_DATA_BYTES;
	i.data = (uint8_t *)malloc(len);
	memcpy(i.data, bytes, len);
	i.data_len = len;
	push_data(ir, i);
}

void ir_data_qword(IrBuffer *ir, int64_t val) {
	IrInst i = {0};
	i.IrNode = IR_DATA_QWORD;
	i.imm = val;
	push_data(ir, i);
}

void ir_data_qword_dbl(IrBuffer *ir, double val) {
	IrInst i = {0};
	i.IrNode = IR_DATA_QWORD;
	/* 鐏?double 娴ｅ秵膩瀵繐鐡ㄦ稉?int64 */
	memcpy(&i.imm, &val, sizeof(double));
	push_data(ir, i);
}

void ir_data_qword_sym(IrBuffer *ir, const char *sym) {
	IrInst i = {0};
	i.IrNode = IR_DATA_QWORD_SYM;
	i.sym_name = strdup(sym);
	push_data(ir, i);
}

/* ========== .bss 娈靛彂灏?========== */

void ir_bss_label(IrBuffer *ir, const char *name) {
	IrInst i = {0};
	i.IrNode = IR_BSS_LABEL;
	i.sym_name = strdup(name);
	push_bss(ir, i);
}

void ir_bss_resq(IrBuffer *ir, int count) {
	IrInst i = {0};
	i.IrNode = IR_BSS_RESQ;
	i.imm = count;
	push_bss(ir, i);
}

