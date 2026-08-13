/* encoder.c 閳?x86-64 閺堝搫娅掗惍浣虹椽閻礁娅?
 *
 * 鐏?IrInst[] 缂傛牜鐖滄稉?x86-64 閺堝搫娅掗惍浣碘偓?
 * 娑撱倝浜舵径鍕倞閿涙氨顑囨稉鈧柆宥堫吀缁犳澧嶉張澶嬪瘹娴犮倕浜哥粔浼欑礉缁楊兛绨╅柆宥呮礀婵夘偉鐑︽潪?RIP-relative 閸嬪繒些閵?
 * 娴ｈ法鏁ら崫鍫濈瑖鐞涖劌鍨庨崣?IrOpcode 閳?缂傛牜鐖滈崙鑺ユ殶閵?
 */
#include "ir.h"
#include "../hash.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ========== 閸愬懘鍎村銉ュ徔 ========== */

typedef struct {
	uint8_t *buf;
	int      len;
	int      cap;
	/* 閺嶅洨顒烽崑蹇曅╃悰?*/
	int     *label_offsets;
	int      label_cap;
	/* 閸ョ偛锝炵悰?*/
	struct { int buf_off; int label_id; int inst_end; int size; /* 1 or 4 */ } *patches;
	int      patch_count;
	int      patch_cap;
	/* RIP-relative 閺佺増宓?缁楋箑褰块崶鐐诧綖 */
	IrReloc *relocs;
	int      reloc_count;
	int      reloc_cap;
	/* 缁楋箑褰?(named labels) */
	struct { char *name; uint32_t offset; } *syms;
	int      sym_count;
	int      sym_cap;
	int      current_inst;
	int     *inst_offsets;
	uint8_t *short_branch;
} EncoderCtx;

static void enc_init(EncoderCtx *e) {
	memset(e, 0, sizeof(*e));
	e->cap = 4096;
	e->buf = (uint8_t *)malloc(e->cap);
	e->label_cap = 1024;
	e->label_offsets = (int *)calloc(e->label_cap, sizeof(int));
	for (int i = 0; i < e->label_cap; i++) e->label_offsets[i] = -1;
}

static void enc_ensure(EncoderCtx *e, int need) {
	while (e->len + need > e->cap) {
		e->cap *= 2;
		e->buf = (uint8_t *)realloc(e->buf, e->cap);
	}
}

static void enc_byte(EncoderCtx *e, uint8_t b) {
	enc_ensure(e, 1);
	e->buf[e->len++] = b;
}

static void enc_bytes(EncoderCtx *e, const uint8_t *data, int n) {
	enc_ensure(e, n);
	memcpy(e->buf + e->len, data, n);
	e->len += n;
}

static void enc_u32(EncoderCtx *e, uint32_t v) {
	enc_ensure(e, 4);
	memcpy(e->buf + e->len, &v, 4);
	e->len += 4;
}

static void enc_u64(EncoderCtx *e, uint64_t v) {
	enc_ensure(e, 8);
	memcpy(e->buf + e->len, &v, 8);
	e->len += 8;
}

static void enc_i32(EncoderCtx *e, int32_t v) {
	enc_u32(e, (uint32_t)v);
}

static void enc_label_ensure(EncoderCtx *e, int id) {
	if (id >= e->label_cap) {
		int old = e->label_cap;
		while (id >= e->label_cap) e->label_cap *= 2;
		e->label_offsets = (int *)realloc(e->label_offsets, e->label_cap * sizeof(int));
		for (int i = old; i < e->label_cap; i++) e->label_offsets[i] = -1;
	}
}

static void enc_add_patch_size(EncoderCtx *e, int buf_off, int label_id, int inst_end, int size) {
	if (e->patch_count >= e->patch_cap) {
		e->patch_cap = e->patch_cap ? e->patch_cap * 2 : 256;
		e->patches = realloc(e->patches, e->patch_cap * sizeof(e->patches[0]));
	}
	e->patches[e->patch_count].buf_off = buf_off;
	e->patches[e->patch_count].label_id = label_id;
	e->patches[e->patch_count].inst_end = inst_end;
	e->patches[e->patch_count].size = size;
	e->patch_count++;
}

static void enc_add_patch(EncoderCtx *e, int buf_off, int label_id, int inst_end) {
	enc_add_patch_size(e, buf_off, label_id, inst_end, 4);
}

static void enc_add_reloc(EncoderCtx *e, uint32_t offset, const char *sym, uint16_t type, bool is_rip_data, int label_id) {
	if (e->reloc_count >= e->reloc_cap) {
		e->reloc_cap = e->reloc_cap ? e->reloc_cap * 2 : 64;
		e->relocs = realloc(e->relocs, e->reloc_cap * sizeof(IrReloc));
	}
	e->relocs[e->reloc_count].offset = offset;
	e->relocs[e->reloc_count].sym_name = sym ? strdup(sym) : NULL;
	e->relocs[e->reloc_count].type = type;
	e->relocs[e->reloc_count].is_rip_data = is_rip_data;
	e->relocs[e->reloc_count].label_id = label_id;
	e->reloc_count++;
}

static void enc_add_sym(EncoderCtx *e, const char *name, uint32_t offset) {
	if (e->sym_count >= e->sym_cap) {
		e->sym_cap = e->sym_cap ? e->sym_cap * 2 : 64;
		e->syms = realloc(e->syms, e->sym_cap * sizeof(e->syms[0]));
	}
	e->syms[e->sym_count].name = strdup(name);
	e->syms[e->sym_count].offset = offset;
	e->sym_count++;
}

/* ========== x86-64 缂傛牜鐖滃銉ュ徔 ========== */

/* REX prefix: W=64-bit, R=reg ext, X=SIB ext, B=rm ext */
static uint8_t rex(bool w, bool r, bool x, bool b) {
	return 0x40 | (w ? 8 : 0) | (r ? 4 : 0) | (x ? 2 : 0) | (b ? 1 : 0);
}

/* ModR/M byte */
static uint8_t modrm(int mod, int reg, int rm) {
	return (uint8_t)((mod << 6) | ((reg & 7) << 3) | (rm & 7));
}

static void enc_vex3_rrr(EncoderCtx *e, int map, int pp, bool w,
		uint8_t opcode, IrReg dst, IrReg src1, IrReg src2) {
	int d = ir_reg_id(dst), s1 = ir_reg_id(src1), s2 = ir_reg_id(src2);
	enc_byte(e, 0xC4);
	enc_byte(e, (uint8_t)(((d < 8) ? 0x80 : 0) | 0x40 |
		((s2 < 8) ? 0x20 : 0) | (map & 0x1F)));
	enc_byte(e, (uint8_t)((w ? 0x80 : 0) | (((~s1) & 15) << 3) |
		0x04 | (pp & 3)));
	enc_byte(e, opcode);
	enc_byte(e, modrm(3, d, s2));
}

static void enc_vex3_mem(EncoderCtx *e, int map, int pp, uint8_t opcode,
		IrReg vec, IrReg base, int64_t disp) {
	int v = ir_reg_id(vec), b = ir_reg_id(base);
	enc_byte(e, 0xC4);
	enc_byte(e, (uint8_t)(((v < 8) ? 0x80 : 0) | 0x40 |
		((b < 8) ? 0x20 : 0) | (map & 0x1F)));
	/* vvvv=1111 is reserved for VMOVDQU; L=1 selects YMM. */
	enc_byte(e, (uint8_t)(0x78 | 0x04 | (pp & 3)));
	enc_byte(e, opcode);
	bool sib = (b & 7) == 4;
	bool force_disp = (b & 7) == 5 && disp == 0;
	if (disp == 0 && !force_disp) {
		enc_byte(e, modrm(0, v, b));
		if (sib) enc_byte(e, 0x24);
	} else if (disp >= -128 && disp <= 127) {
		enc_byte(e, modrm(1, v, b));
		if (sib) enc_byte(e, 0x24);
		enc_byte(e, (uint8_t)(int8_t)disp);
	} else {
		enc_byte(e, modrm(2, v, b));
		if (sib) enc_byte(e, 0x24);
		enc_i32(e, (int32_t)disp);
	}
}

/* 缂傛牜鐖滅€靛嫬鐡ㄩ崳?鐎靛嫬鐡ㄩ崳銊﹀瘹娴?(闁氨鏁? */
static void enc_rr(EncoderCtx *e, uint8_t opcode, IrReg dst, IrReg src, bool w) {
	int d = ir_reg_id(dst), s = ir_reg_id(src);
	bool need_rex = w || d >= 8 || s >= 8;
	if (need_rex) enc_byte(e, rex(w, s >= 8, false, d >= 8));
	enc_byte(e, opcode);
	enc_byte(e, modrm(3, s & 7, d & 7));
}

/* 缂傛牜鐖?[base + disp32] 閸愬懎鐡ㄥ鏇犳暏 */
static void enc_mem_base_disp(EncoderCtx *e, uint8_t opcode, int reg_or_ext, IrReg base, int64_t disp, bool w, bool byte_op) {
	int b = ir_reg_id(base);
	bool need_rex = w || reg_or_ext >= 8 || b >= 8 || byte_op;
	if (need_rex) enc_byte(e, rex(w, reg_or_ext >= 8, false, b >= 8));
	enc_byte(e, opcode);

	/* RSP/R12 闂団偓鐟?SIB */
	bool need_sib = (b & 7) == 4;
	/* RBP/R13 with disp=0 闂団偓鐟?disp8 */
	bool force_disp = (b & 7) == 5 && disp == 0;

	if (disp == 0 && !force_disp) {
		enc_byte(e, modrm(0, reg_or_ext & 7, b & 7));
		if (need_sib) enc_byte(e, 0x24); /* SIB: scale=0, index=RSP(none), base=RSP */
	} else if (disp >= -128 && disp <= 127) {
		enc_byte(e, modrm(1, reg_or_ext & 7, b & 7));
		if (need_sib) enc_byte(e, 0x24);
		enc_byte(e, (uint8_t)(int8_t)disp);
	} else {
		enc_byte(e, modrm(2, reg_or_ext & 7, b & 7));
		if (need_sib) enc_byte(e, 0x24);
		enc_i32(e, (int32_t)disp);
	}
}

/* RIP-relative 閸愬懎鐡ㄥ鏇犳暏 [rip + disp32] */
static void enc_rip_rel(EncoderCtx *e, uint8_t prefix_byte, uint8_t opcode, int reg_field, const char *sym, int label_id) {
	if (prefix_byte) enc_byte(e, prefix_byte);
	enc_byte(e, rex(true, reg_field >= 8, false, false));
	enc_byte(e, opcode);
	enc_byte(e, modrm(0, reg_field & 7, 5)); /* mod=00, rm=101 = RIP-relative */
	int patch_off = e->len;
	enc_i32(e, 0); /* placeholder, will be patched */
	if (sym) {
		/* IMAGE_REL_AMD64_REL32 = 4 */
		enc_add_reloc(e, patch_off, sym, 4, true, label_id);
	} else if (label_id > 0) {
		enc_add_patch(e, patch_off, label_id, e->len);
	}
}

/* ========== 閹稿洣鎶ょ紓鏍垳閸戣姤鏆?========== */

static void encode_mov_reg_reg(EncoderCtx *e, IrInst *inst) {
	IrReg dst = inst->dst, src = inst->src;
	int db = ir_reg_bits(dst), sb = ir_reg_bits(src);
	if (db == 64 && sb == 64) {
		enc_rr(e, 0x89, dst, src, true);
	} else if (db == 32 && sb == 32) {
		/* 32-bit mov锛屼笉闇€瑕?REX.W */
		int d = ir_reg_id(dst), s = ir_reg_id(src);
		bool need_rex = d >= 8 || s >= 8;
		if (need_rex) enc_byte(e, rex(false, s >= 8, false, d >= 8));
		enc_byte(e, 0x89);
		enc_byte(e, modrm(3, s & 7, d & 7));
	}
}

static void encode_mov_reg_imm(EncoderCtx *e, IrInst *inst) {
	IrReg dst = inst->dst;
	int64_t imm = inst->imm;
	int d = ir_reg_id(dst);
	int bits = ir_reg_bits(dst);

	if (bits == 32) {
		/* mov eXX, imm32 */
		if (d >= 8) enc_byte(e, rex(false, false, false, true));
		enc_byte(e, 0xB8 + (d & 7));
		enc_u32(e, (uint32_t)imm);
	} else if (imm >= 0 && imm <= 0xFFFFFFFF) {
		/* mov eXX, imm32 (zero extends to 64-bit) */
		if (d >= 8) enc_byte(e, rex(false, false, false, true));
		enc_byte(e, 0xB8 + (d & 7));
		enc_u32(e, (uint32_t)imm);
	} else {
		/* mov rXX, imm64 */
		enc_byte(e, rex(true, false, false, d >= 8));
		enc_byte(e, 0xB8 + (d & 7));
		enc_u64(e, (uint64_t)imm);
	}
}

static void encode_mov_reg_mem(EncoderCtx *e, IrInst *inst) {
	int d = ir_reg_id(inst->dst);
	enc_mem_base_disp(e, 0x8B, d, inst->src, inst->imm, true, false);
}

static void encode_mov_mem_reg(EncoderCtx *e, IrInst *inst) {
	int s = ir_reg_id(inst->src);
	enc_mem_base_disp(e, 0x89, s, inst->dst, inst->imm, true, false);
}

static void encode_mov_mem_imm(EncoderCtx *e, IrInst *inst) {
	/* mov qword [base + disp], imm32 (sign-extended) */
	int b = ir_reg_id(inst->dst);
	enc_byte(e, rex(true, false, false, b >= 8));
	enc_byte(e, 0xC7);
	bool need_sib = (b & 7) == 4;
	int64_t disp = inst->imm;
	bool force_disp = (b & 7) == 5 && disp == 0;
	if (disp == 0 && !force_disp) {
		enc_byte(e, modrm(0, 0, b & 7));
		if (need_sib) enc_byte(e, 0x24);
	} else if (disp >= -128 && disp <= 127) {
		enc_byte(e, modrm(1, 0, b & 7));
		if (need_sib) enc_byte(e, 0x24);
		enc_byte(e, (uint8_t)(int8_t)disp);
	} else {
		enc_byte(e, modrm(2, 0, b & 7));
		if (need_sib) enc_byte(e, 0x24);
		enc_i32(e, (int32_t)disp);
	}
	enc_i32(e, (int32_t)inst->extra_imm);
}

static void encode_mov_mem8_reg(EncoderCtx *e, IrInst *inst) {
	/* mov byte [base + disp], src8 */
	int s = ir_reg_id(inst->src);
	int b = ir_reg_id(inst->dst);
	/* Need REX if using sil/dil/bpl/spl or extended regs */
	bool need_rex = s >= 4 || b >= 8 || s >= 8;
	if (need_rex) enc_byte(e, rex(false, s >= 8, false, b >= 8));
	enc_byte(e, 0x88);
	int64_t disp = inst->imm;
	bool need_sib = (b & 7) == 4;
	bool force_disp = (b & 7) == 5 && disp == 0;
	if (disp == 0 && !force_disp) {
		enc_byte(e, modrm(0, s & 7, b & 7));
		if (need_sib) enc_byte(e, 0x24);
	} else if (disp >= -128 && disp <= 127) {
		enc_byte(e, modrm(1, s & 7, b & 7));
		if (need_sib) enc_byte(e, 0x24);
		enc_byte(e, (uint8_t)(int8_t)disp);
	} else {
		enc_byte(e, modrm(2, s & 7, b & 7));
		if (need_sib) enc_byte(e, 0x24);
		enc_i32(e, (int32_t)disp);
	}
}

static void encode_movzx_reg_mem8(EncoderCtx *e, IrInst *inst) {
	/* movzx r32, byte [base + disp] */
	int d = ir_reg_id(inst->dst);
	int b = ir_reg_id(inst->src);
	bool need_rex = d >= 8 || b >= 8;
	if (need_rex) enc_byte(e, rex(false, d >= 8, false, b >= 8));
	enc_byte(e, 0x0F); enc_byte(e, 0xB6);
	int64_t disp = inst->imm;
	bool need_sib = (b & 7) == 4;
	bool force_disp = (b & 7) == 5 && disp == 0;
	if (disp == 0 && !force_disp) {
		enc_byte(e, modrm(0, d & 7, b & 7));
		if (need_sib) enc_byte(e, 0x24);
	} else if (disp >= -128 && disp <= 127) {
		enc_byte(e, modrm(1, d & 7, b & 7));
		if (need_sib) enc_byte(e, 0x24);
		enc_byte(e, (uint8_t)(int8_t)disp);
	} else {
		enc_byte(e, modrm(2, d & 7, b & 7));
		if (need_sib) enc_byte(e, 0x24);
		enc_i32(e, (int32_t)disp);
	}
}

static void encode_lea(EncoderCtx *e, IrInst *inst) {
	int d = ir_reg_id(inst->dst);
	enc_mem_base_disp(e, 0x8D, d, inst->src, inst->imm, true, false);
}

static void encode_lea_idx(EncoderCtx *e, IrInst *inst) {
	/* lea dst, [base + index*scale] 鈥?浣跨敤 SIB 缂栫爜 */
	int d = ir_reg_id(inst->dst);
	int b = ir_reg_id(inst->src);           /* base register */
	int x = ir_reg_id((IrReg)inst->imm);    /* index register (stored in imm) */
	/* scale from extra_imm: 0/1 -> 1x, 2 -> 2x, 4 -> 4x, 8 -> 8x.
	 * ir_opt_strength_reduce emits [src + src*scale] to replace imul by
	 * 2/3/5/9 (scale = multiplier - 1). */
	int sc = inst->extra_imm >= 8 ? 3 : inst->extra_imm >= 4 ? 2 : inst->extra_imm >= 2 ? 1 : 0;
	/* REX: W=1(64bit), R=dst_hi, X=index_hi, B=base_hi */
	enc_byte(e, rex(true, d >= 8, x >= 8, b >= 8));
	enc_byte(e, 0x8D); /* LEA opcode */
	/* ModRM: mod=00 (no disp), reg=dst, rm=100 (SIB escape) */
	/* 鐗规畵鎯呭喌: base 鏄?RBP/R13 (缂栧彿&7==5) 鏃?mod=00 鎰忓懗鐫€ [disp32+index]锛?
	 * 鎵€浠ュ繀椤讳娇鐢?mod=01 + disp8=0 鏉ヨ〃杈?[base+index+0] */
	if ((b & 7) == 5) {
		enc_byte(e, modrm(1, d & 7, 4)); /* mod=01, rm=100(SIB) */
		enc_byte(e, (uint8_t)((sc << 6) | ((x & 7) << 3) | (b & 7))); /* SIB: scale, index, base */
		enc_byte(e, 0); /* disp8 = 0 */
	} else {
		enc_byte(e, modrm(0, d & 7, 4)); /* mod=00, rm=100(SIB) */
		enc_byte(e, (uint8_t)((sc << 6) | ((x & 7) << 3) | (b & 7))); /* SIB: scale, index, base */
	}
}

static void encode_lea_rip(EncoderCtx *e, IrInst *inst) {
	/* lea dst, [rip + disp32] */
	int d = ir_reg_id(inst->dst);
	enc_byte(e, rex(true, d >= 8, false, false));
	enc_byte(e, 0x8D);
	enc_byte(e, modrm(0, d & 7, 5)); /* RIP-relative */
	int patch_off = e->len;
	enc_i32(e, 0);
	if (inst->sym_name) {
		enc_add_reloc(e, patch_off, inst->sym_name, 4, true, 0);
	} else if (inst->label_id > 0) {
		enc_add_patch(e, patch_off, inst->label_id, e->len);
	}
}

static void encode_push_reg(EncoderCtx *e, IrInst *inst) {
	int r = ir_reg_id(inst->dst);
	if (r >= 8) enc_byte(e, rex(false, false, false, true));
	enc_byte(e, 0x50 + (r & 7));
}

static void encode_pop_reg(EncoderCtx *e, IrInst *inst) {
	int r = ir_reg_id(inst->dst);
	if (r >= 8) enc_byte(e, rex(false, false, false, true));
	enc_byte(e, 0x58 + (r & 7));
}

/* ALU reg,reg 閫氱敤缂栫爜 */
static void encode_alu_rr(EncoderCtx *e, IrInst *inst, uint8_t opcode) {
	enc_rr(e, opcode, inst->dst, inst->src, true);
}

/* ALU reg,imm32 閫氱敤缂栫爜 */
static void encode_alu_ri(EncoderCtx *e, IrInst *inst, int ext, bool w) {
	int d = ir_reg_id(inst->dst);
	int bits = ir_reg_bits(inst->dst);
	if (bits <= 32) w = false;
	enc_byte(e, rex(w, false, false, d >= 8));
	int32_t imm = (int32_t)inst->imm;
	if (imm >= -128 && imm <= 127) {
		enc_byte(e, w ? 0x83 : 0x83);
		enc_byte(e, modrm(3, ext, d & 7));
		enc_byte(e, (uint8_t)(int8_t)imm);
	} else {
		enc_byte(e, 0x81);
		enc_byte(e, modrm(3, ext, d & 7));
		enc_i32(e, imm);
	}
}

static void encode_add_reg_reg(EncoderCtx *e, IrInst *inst) { encode_alu_rr(e, inst, 0x01); }
static void encode_sub_reg_reg(EncoderCtx *e, IrInst *inst) { encode_alu_rr(e, inst, 0x29); }
static void encode_and_reg_reg(EncoderCtx *e, IrInst *inst) { encode_alu_rr(e, inst, 0x21); }
static void encode_or_reg_reg(EncoderCtx *e, IrInst *inst)  { encode_alu_rr(e, inst, 0x09); }
static void encode_xor_reg_reg(EncoderCtx *e, IrInst *inst) { encode_alu_rr(e, inst, 0x31); }
static void encode_cmp_reg_reg(EncoderCtx *e, IrInst *inst) { encode_alu_rr(e, inst, 0x39); }

static void encode_add_reg_imm(EncoderCtx *e, IrInst *inst) { encode_alu_ri(e, inst, 0, true); }
static void encode_sub_reg_imm(EncoderCtx *e, IrInst *inst) { encode_alu_ri(e, inst, 5, true); }
static void encode_cmp_reg_imm(EncoderCtx *e, IrInst *inst) { encode_alu_ri(e, inst, 7, true); }
static void encode_xor_reg_imm(EncoderCtx *e, IrInst *inst) { encode_alu_ri(e, inst, 6, ir_reg_bits(inst->dst) == 64); }
static void encode_and_reg_imm(EncoderCtx *e, IrInst *inst) { encode_alu_ri(e, inst, 4, ir_reg_bits(inst->dst) == 64); }

static void encode_add_mem_imm(EncoderCtx *e, IrInst *inst) {
	/* add qword [base + disp], imm32 */
	int b = ir_reg_id(inst->dst);
	enc_byte(e, rex(true, false, false, b >= 8));
	int32_t val = (int32_t)inst->extra_imm;
	if (val >= -128 && val <= 127) {
		enc_byte(e, 0x83);
	} else {
		enc_byte(e, 0x81);
	}
	int64_t disp = inst->imm;
	bool need_sib = (b & 7) == 4;
	bool force_disp = (b & 7) == 5 && disp == 0;
	if (disp == 0 && !force_disp) {
		enc_byte(e, modrm(0, 0, b & 7));
		if (need_sib) enc_byte(e, 0x24);
	} else if (disp >= -128 && disp <= 127) {
		enc_byte(e, modrm(1, 0, b & 7));
		if (need_sib) enc_byte(e, 0x24);
		enc_byte(e, (uint8_t)(int8_t)disp);
	} else {
		enc_byte(e, modrm(2, 0, b & 7));
		if (need_sib) enc_byte(e, 0x24);
		enc_i32(e, (int32_t)disp);
	}
	if (val >= -128 && val <= 127) {
		enc_byte(e, (uint8_t)(int8_t)val);
	} else {
		enc_i32(e, val);
	}
}

static void encode_imul_reg_reg(EncoderCtx *e, IrInst *inst) {
	int d = ir_reg_id(inst->dst), s = ir_reg_id(inst->src);
	enc_byte(e, rex(true, d >= 8, false, s >= 8));
	enc_byte(e, 0x0F); enc_byte(e, 0xAF);
	enc_byte(e, modrm(3, d & 7, s & 7));
}

static void encode_imul_wide_reg(EncoderCtx *e, IrInst *inst) {
	int s = ir_reg_id(inst->src);
	enc_byte(e, rex(true, false, false, s >= 8));
	enc_byte(e, 0xF7);
	enc_byte(e, modrm(3, 5, s & 7));
}

static void encode_mul_wide_reg(EncoderCtx *e, IrInst *inst) {
	int s = ir_reg_id(inst->src);
	enc_byte(e, rex(true, false, false, s >= 8));
	enc_byte(e, 0xF7);
	enc_byte(e, modrm(3, 4, s & 7));
}

static void encode_align32(EncoderCtx *e, IrInst *inst) {
	(void)inst;
	while (e->len & 31) enc_byte(e, 0x90);
}

static void encode_idiv_reg(EncoderCtx *e, IrInst *inst) {
	int s = ir_reg_id(inst->src);
	enc_byte(e, rex(true, false, false, s >= 8));
	enc_byte(e, 0xF7);
	enc_byte(e, modrm(3, 7, s & 7));
}

static void encode_inc_reg(EncoderCtx *e, IrInst *inst) {
	int d = ir_reg_id(inst->dst);
	enc_byte(e, rex(true, false, false, d >= 8));
	enc_byte(e, 0xFF);
	enc_byte(e, modrm(3, 0, d & 7));
}

static void encode_inc_mem(EncoderCtx *e, IrInst *inst) {
	/* inc qword [base + disp] */
	int b = ir_reg_id(inst->dst);
	enc_byte(e, rex(true, false, false, b >= 8));
	enc_byte(e, 0xFF);
	int64_t disp = inst->imm;
	bool need_sib = (b & 7) == 4;
	bool force_disp = (b & 7) == 5 && disp == 0;
	if (disp == 0 && !force_disp) {
		enc_byte(e, modrm(0, 0, b & 7));
		if (need_sib) enc_byte(e, 0x24);
	} else if (disp >= -128 && disp <= 127) {
		enc_byte(e, modrm(1, 0, b & 7));
		if (need_sib) enc_byte(e, 0x24);
		enc_byte(e, (uint8_t)(int8_t)disp);
	} else {
		enc_byte(e, modrm(2, 0, b & 7));
		if (need_sib) enc_byte(e, 0x24);
		enc_i32(e, (int32_t)disp);
	}
}

static void encode_neg_reg(EncoderCtx *e, IrInst *inst) {
	int d = ir_reg_id(inst->dst);
	enc_byte(e, rex(true, false, false, d >= 8));
	enc_byte(e, 0xF7);
	enc_byte(e, modrm(3, 3, d & 7));
}

static void encode_not_reg(EncoderCtx *e, IrInst *inst) {
	int d = ir_reg_id(inst->dst);
	enc_byte(e, rex(true, false, false, d >= 8));
	enc_byte(e, 0xF7);
	enc_byte(e, modrm(3, 2, d & 7));
}

static void encode_cqo(EncoderCtx *e, IrInst *inst) {
	(void)inst;
	enc_byte(e, rex(true, false, false, false));
	enc_byte(e, 0x99);
}

static void encode_test_reg_reg(EncoderCtx *e, IrInst *inst) {
	enc_rr(e, 0x85, inst->dst, inst->src, true);
}

static void encode_shl_reg_imm(EncoderCtx *e, IrInst *inst) {
	int d = ir_reg_id(inst->dst);
	enc_byte(e, rex(true, false, false, d >= 8));
	enc_byte(e, 0xC1);
	enc_byte(e, modrm(3, 4, d & 7));
	enc_byte(e, (uint8_t)inst->imm);
}

static void encode_shr_reg_imm(EncoderCtx *e, IrInst *inst) {
	int d = ir_reg_id(inst->dst);
	enc_byte(e, rex(true, false, false, d >= 8));
	enc_byte(e, 0xC1);
	enc_byte(e, modrm(3, 5, d & 7));
	enc_byte(e, (uint8_t)inst->imm);
}

static void encode_sar_reg_imm(EncoderCtx *e, IrInst *inst) {
	int d = ir_reg_id(inst->dst);
	enc_byte(e, rex(true, false, false, d >= 8));
	enc_byte(e, 0xC1);
	enc_byte(e, modrm(3, 7, d & 7));
	enc_byte(e, (uint8_t)inst->imm);
}

static void encode_shift_reg_cl(EncoderCtx *e, IrInst *inst, int extension) {
	int d = ir_reg_id(inst->dst);
	enc_byte(e, rex(true, false, false, d >= 8));
	enc_byte(e, 0xD3);
	enc_byte(e, modrm(3, extension, d & 7));
}

static void encode_shl_reg_cl(EncoderCtx *e, IrInst *inst) { encode_shift_reg_cl(e, inst, 4); }
static void encode_shr_reg_cl(EncoderCtx *e, IrInst *inst) { encode_shift_reg_cl(e, inst, 5); }
static void encode_sar_reg_cl(EncoderCtx *e, IrInst *inst) { encode_shift_reg_cl(e, inst, 7); }

/* --- 閺夆€叉鐠佸墽鐤?SETcc --- */
static void encode_setcc(EncoderCtx *e, IrInst *inst, uint8_t cc) {
	int d = ir_reg_id(inst->dst);
	/* SETcc needs REX for accessing sil/dil/bpl/spl */
	if (d >= 4 || d >= 8) enc_byte(e, rex(false, false, false, d >= 8));
	enc_byte(e, 0x0F);
	enc_byte(e, 0x90 + cc);
	enc_byte(e, modrm(3, 0, d & 7));
}

static void encode_sete(EncoderCtx *e, IrInst *inst)  { encode_setcc(e, inst, 0x04); }
static void encode_setne(EncoderCtx *e, IrInst *inst) { encode_setcc(e, inst, 0x05); }
static void encode_setl(EncoderCtx *e, IrInst *inst)  { encode_setcc(e, inst, 0x0C); }
static void encode_setle(EncoderCtx *e, IrInst *inst) { encode_setcc(e, inst, 0x0E); }
static void encode_setg(EncoderCtx *e, IrInst *inst)  { encode_setcc(e, inst, 0x0F); }
static void encode_setge(EncoderCtx *e, IrInst *inst) { encode_setcc(e, inst, 0x0D); }
static void encode_seta(EncoderCtx *e, IrInst *inst)  { encode_setcc(e, inst, 0x07); }
static void encode_setae(EncoderCtx *e, IrInst *inst) { encode_setcc(e, inst, 0x03); }
static void encode_setb(EncoderCtx *e, IrInst *inst)  { encode_setcc(e, inst, 0x02); }
static void encode_setbe(EncoderCtx *e, IrInst *inst) { encode_setcc(e, inst, 0x06); }
static void encode_setz(EncoderCtx *e, IrInst *inst)  { encode_setcc(e, inst, 0x04); } /* same as sete */
static void encode_setnp(EncoderCtx *e, IrInst *inst) { encode_setcc(e, inst, 0x0B); }

static void encode_cmovcc(EncoderCtx *e, IrInst *inst, uint8_t cc) {
	int d = ir_reg_id(inst->dst), s = ir_reg_id(inst->src);
	enc_byte(e, rex(true, d >= 8, false, s >= 8));
	enc_byte(e, 0x0F); enc_byte(e, 0x40 + cc);
	enc_byte(e, modrm(3, d & 7, s & 7));
}
static void encode_cmove(EncoderCtx *e, IrInst *i)  { encode_cmovcc(e, i, 0x04); }
static void encode_cmovne(EncoderCtx *e, IrInst *i) { encode_cmovcc(e, i, 0x05); }
static void encode_cmovl(EncoderCtx *e, IrInst *i)  { encode_cmovcc(e, i, 0x0C); }
static void encode_cmovle(EncoderCtx *e, IrInst *i) { encode_cmovcc(e, i, 0x0E); }
static void encode_cmovg(EncoderCtx *e, IrInst *i)  { encode_cmovcc(e, i, 0x0F); }
static void encode_cmovge(EncoderCtx *e, IrInst *i) { encode_cmovcc(e, i, 0x0D); }
static void encode_cmova(EncoderCtx *e, IrInst *i)  { encode_cmovcc(e, i, 0x07); }
static void encode_cmovae(EncoderCtx *e, IrInst *i) { encode_cmovcc(e, i, 0x03); }
static void encode_cmovb(EncoderCtx *e, IrInst *i)  { encode_cmovcc(e, i, 0x02); }
static void encode_cmovbe(EncoderCtx *e, IrInst *i) { encode_cmovcc(e, i, 0x06); }

static void encode_movzx_reg8(EncoderCtx *e, IrInst *inst) {
	/* movzx eax, al => 0F B6 C0 */
	int d = ir_reg_id(inst->dst), s = ir_reg_id(inst->src);
	bool need_rex = d >= 8 || s >= 8 || s >= 4;
	if (need_rex) enc_byte(e, rex(false, d >= 8, false, s >= 8));
	enc_byte(e, 0x0F); enc_byte(e, 0xB6);
	enc_byte(e, modrm(3, d & 7, s & 7));
}

/* --- 鐠哄疇娴?--- */
static void encode_jmp(EncoderCtx *e, IrInst *inst) {
	enc_label_ensure(e, inst->label_id);
	if (e->short_branch && e->short_branch[e->current_inst]) {
		enc_byte(e, 0xEB);
		int patch_off = e->len; enc_byte(e, 0);
		enc_add_patch_size(e, patch_off, inst->label_id, e->len, 1);
		return;
	}
	if (e->label_offsets[inst->label_id] >= 0) {
		int rel8 = e->label_offsets[inst->label_id] - (e->len + 2);
		if (rel8 >= -128 && rel8 <= 127) {
			enc_byte(e, 0xEB);
			enc_byte(e, (uint8_t)(int8_t)rel8);
			return;
		}
	}
	enc_byte(e, 0xE9);
	int patch_off = e->len;
	enc_i32(e, 0);
	enc_add_patch(e, patch_off, inst->label_id, e->len);
}

static void encode_jmp_extern(EncoderCtx *e, IrInst *inst) {
	enc_byte(e, 0xE9);
	int patch_off = e->len;
	enc_i32(e, 0);
	enc_add_reloc(e, patch_off, inst->sym_name, 4, false, 0);
}

static void encode_jcc(EncoderCtx *e, IrInst *inst, uint8_t cc) {
	enc_label_ensure(e, inst->label_id);
	if (e->short_branch && e->short_branch[e->current_inst]) {
		enc_byte(e, 0x70 + cc);
		int patch_off = e->len; enc_byte(e, 0);
		enc_add_patch_size(e, patch_off, inst->label_id, e->len, 1);
		return;
	}
	if (e->label_offsets[inst->label_id] >= 0) {
		int rel8 = e->label_offsets[inst->label_id] - (e->len + 2);
		if (rel8 >= -128 && rel8 <= 127) {
			enc_byte(e, 0x70 + cc);
			enc_byte(e, (uint8_t)(int8_t)rel8);
			return;
		}
	}
	enc_byte(e, 0x0F);
	enc_byte(e, 0x80 + cc);
	int patch_off = e->len;
	enc_i32(e, 0);
	enc_add_patch(e, patch_off, inst->label_id, e->len);
}

static void encode_je(EncoderCtx *e, IrInst *inst)  { encode_jcc(e, inst, 0x04); }
static void encode_jne(EncoderCtx *e, IrInst *inst) { encode_jcc(e, inst, 0x05); }
static void encode_jz(EncoderCtx *e, IrInst *inst)  { encode_jcc(e, inst, 0x04); }
static void encode_jnz(EncoderCtx *e, IrInst *inst) { encode_jcc(e, inst, 0x05); }
static void encode_jg(EncoderCtx *e, IrInst *inst)  { encode_jcc(e, inst, 0x0F); }
static void encode_jge(EncoderCtx *e, IrInst *inst) { encode_jcc(e, inst, 0x0D); }
static void encode_jl(EncoderCtx *e, IrInst *inst)  { encode_jcc(e, inst, 0x0C); }
static void encode_jle(EncoderCtx *e, IrInst *inst) { encode_jcc(e, inst, 0x0E); }
static void encode_ja(EncoderCtx *e, IrInst *inst)  { encode_jcc(e, inst, 0x07); }
static void encode_jae(EncoderCtx *e, IrInst *inst) { encode_jcc(e, inst, 0x03); }
static void encode_jb(EncoderCtx *e, IrInst *inst)  { encode_jcc(e, inst, 0x02); }
static void encode_jbe(EncoderCtx *e, IrInst *inst) { encode_jcc(e, inst, 0x06); }
static void encode_js(EncoderCtx *e, IrInst *inst)  { encode_jcc(e, inst, 0x08); }
static void encode_jns(EncoderCtx *e, IrInst *inst) { encode_jcc(e, inst, 0x09); }

/* --- 鐠嬪啰鏁?鏉╂柨娲?--- */
static void encode_call_label(EncoderCtx *e, IrInst *inst) {
	enc_byte(e, 0xE8);
	int patch_off = e->len;
	enc_i32(e, 0);
	enc_add_patch(e, patch_off, inst->label_id, e->len);
}

static void encode_call_extern(EncoderCtx *e, IrInst *inst) {
	enc_byte(e, 0xE8);
	int patch_off = e->len;
	enc_i32(e, 0);
	/* IMAGE_REL_AMD64_REL32 = 4 */
	enc_add_reloc(e, patch_off, inst->sym_name, 4, false, 0);
}

static void encode_call_reg(EncoderCtx *e, IrInst *inst) {
	/* call reg: FF /2 */
	int r = ir_reg_id(inst->dst);
	if (r >= 8) enc_byte(e, rex(false, false, false, true));
	enc_byte(e, 0xFF);
	enc_byte(e, modrm(3, 2, r & 7));
}

static void encode_ret(EncoderCtx *e, IrInst *inst) {
	(void)inst;
	enc_byte(e, 0xC3);
}

/* --- 閺嶅洨顒?--- */
static void encode_label(EncoderCtx *e, IrInst *inst) {
	enc_label_ensure(e, inst->label_id);
	e->label_offsets[inst->label_id] = e->len;
}

static void encode_label_named(EncoderCtx *e, IrInst *inst) {
	enc_add_sym(e, inst->sym_name, (uint32_t)e->len);
}

static void encode_nop(EncoderCtx *e, IrInst *inst) {
	(void)e; (void)inst; /* extern/global are metadata, no code */
}

/* --- SSE 濞搭喚鍋?--- */
static void encode_movq_xmm_reg(EncoderCtx *e, IrInst *inst) {
	/* 66 REX.W 0F 6E /r */
	int x = ir_reg_id(inst->dst), r = ir_reg_id(inst->src);
	enc_byte(e, 0x66);
	enc_byte(e, rex(true, x >= 8, false, r >= 8));
	enc_byte(e, 0x0F); enc_byte(e, 0x6E);
	enc_byte(e, modrm(3, x & 7, r & 7));
}

static void encode_movq_reg_xmm(EncoderCtx *e, IrInst *inst) {
	/* 66 REX.W 0F 7E /r */
	int r = ir_reg_id(inst->dst), x = ir_reg_id(inst->src);
	enc_byte(e, 0x66);
	enc_byte(e, rex(true, x >= 8, false, r >= 8));
	enc_byte(e, 0x0F); enc_byte(e, 0x7E);
	enc_byte(e, modrm(3, x & 7, r & 7));
}

static void encode_movsd_xmm_mem(EncoderCtx *e, IrInst *inst) {
	/* F2 0F 10 /r 閳?movsd xmm, [rip + disp32] */
	int x = ir_reg_id(inst->dst);
	enc_byte(e, 0xF2);
	if (x >= 8) enc_byte(e, rex(false, x >= 8, false, false));
	enc_byte(e, 0x0F); enc_byte(e, 0x10);
	enc_byte(e, modrm(0, x & 7, 5)); /* RIP-relative */
	int patch_off = e->len;
	enc_i32(e, 0);
	if (inst->sym_name)
		enc_add_reloc(e, patch_off, inst->sym_name, 4, true, 0);
}

static void encode_cvtsi2sd(EncoderCtx *e, IrInst *inst) {
	/* F2 REX.W 0F 2A /r */
	int x = ir_reg_id(inst->dst), r = ir_reg_id(inst->src);
	enc_byte(e, 0xF2);
	enc_byte(e, rex(true, x >= 8, false, r >= 8));
	enc_byte(e, 0x0F); enc_byte(e, 0x2A);
	enc_byte(e, modrm(3, x & 7, r & 7));
}

static void encode_cvttsd2si(EncoderCtx *e, IrInst *inst) {
	/* F2 REX.W 0F 2C /r */
	int r = ir_reg_id(inst->dst), x = ir_reg_id(inst->src);
	enc_byte(e, 0xF2);
	enc_byte(e, rex(true, r >= 8, false, x >= 8));
	enc_byte(e, 0x0F); enc_byte(e, 0x2C);
	enc_byte(e, modrm(3, r & 7, x & 7));
}

static void encode_sse_arith(EncoderCtx *e, IrInst *inst, uint8_t op2) {
	/* F2 0F xx /r */
	int d = ir_reg_id(inst->dst), s = ir_reg_id(inst->src);
	enc_byte(e, 0xF2);
	if (d >= 8 || s >= 8) enc_byte(e, rex(false, d >= 8, false, s >= 8));
	enc_byte(e, 0x0F); enc_byte(e, op2);
	enc_byte(e, modrm(3, d & 7, s & 7));
}

static void encode_addsd(EncoderCtx *e, IrInst *inst) { encode_sse_arith(e, inst, 0x58); }
static void encode_subsd(EncoderCtx *e, IrInst *inst) { encode_sse_arith(e, inst, 0x5C); }
static void encode_mulsd(EncoderCtx *e, IrInst *inst) { encode_sse_arith(e, inst, 0x59); }
static void encode_divsd(EncoderCtx *e, IrInst *inst) { encode_sse_arith(e, inst, 0x5E); }

static void encode_ucomisd(EncoderCtx *e, IrInst *inst) {
	/* 66 0F 2E /r */
	int d = ir_reg_id(inst->dst), s = ir_reg_id(inst->src);
	enc_byte(e, 0x66);
	if (d >= 8 || s >= 8) enc_byte(e, rex(false, d >= 8, false, s >= 8));
	enc_byte(e, 0x0F); enc_byte(e, 0x2E);
	enc_byte(e, modrm(3, d & 7, s & 7));
}

static void encode_vfmadd132sd(EncoderCtx *e, IrInst *inst) {
	int d = ir_reg_id(inst->dst), add = ir_reg_id(inst->src), mul = ir_reg_id(inst->src2);
	/* VEX.NDS.LIG.66.0F38.W1 99 /r: dst = dst * r/m + vvvv. */
	enc_byte(e, 0xC4);
	enc_byte(e, (uint8_t)(((d < 8) ? 0x80 : 0) | 0x40 | ((mul < 8) ? 0x20 : 0) | 2));
	enc_byte(e, (uint8_t)(0x80 | (((~add) & 15) << 3) | 1));
	enc_byte(e, 0x99);
	enc_byte(e, modrm(3, d, mul));
}

static void encode_imul_reg_imm(EncoderCtx *e, IrInst *inst) {
	int d = ir_reg_id(inst->dst), s = ir_reg_id(inst->src);
	enc_byte(e, rex(true, d >= 8, false, s >= 8));
	if (inst->imm >= -128 && inst->imm <= 127) {
		enc_byte(e, 0x6B);
		enc_byte(e, modrm(3, d & 7, s & 7));
		enc_byte(e, (uint8_t)(int8_t)inst->imm);
	} else {
		enc_byte(e, 0x69);
		enc_byte(e, modrm(3, d & 7, s & 7));
		enc_i32(e, (int32_t)inst->imm);
	}
}

static void encode_vpaddq(EncoderCtx *e, IrInst *inst) {
	enc_vex3_rrr(e, 1, 1, false, 0xD4, inst->dst, inst->src, inst->src2);
}

static void encode_vpsubq(EncoderCtx *e, IrInst *inst) {
	enc_vex3_rrr(e, 1, 1, false, 0xFB, inst->dst, inst->src, inst->src2);
}
static void encode_vpsadbw(EncoderCtx *e, IrInst *inst) {
	enc_vex3_rrr(e, 1, 1, false, 0xF6, inst->dst, inst->src, inst->src2);
}
static void encode_vpxor(EncoderCtx *e, IrInst *inst) {
	enc_vex3_rrr(e, 1, 1, false, 0xEF, inst->dst, inst->src, inst->src2);
}
static void encode_vpand(EncoderCtx *e, IrInst *inst) {
	enc_vex3_rrr(e, 1, 1, false, 0xDB, inst->dst, inst->src, inst->src2);
}
static void encode_vpor(EncoderCtx *e, IrInst *inst) {
	enc_vex3_rrr(e, 1, 1, false, 0xEB, inst->dst, inst->src, inst->src2);
}

static void encode_vpmulld(EncoderCtx *e, IrInst *inst) {
	enc_vex3_rrr(e, 2, 1, false, 0x40, inst->dst, inst->src, inst->src2);
}

static void encode_vpcmpeqb(EncoderCtx *e, IrInst *inst) {
	enc_vex3_rrr(e, 1, 1, false, 0x74, inst->dst, inst->src, inst->src2);
}
static void encode_vpcmpeqq(EncoderCtx *e, IrInst *inst) {
	enc_vex3_rrr(e, 2, 1, false, 0x29, inst->dst, inst->src, inst->src2);
}
static void encode_vpbroadcastb(EncoderCtx *e, IrInst *inst) {
	enc_vex3_rrr(e, 2, 1, false, 0x78, inst->dst, REG_XMM0, inst->src);
}
static void encode_vpbroadcastq(EncoderCtx *e, IrInst *inst) {
	/* VEX.256.66.0F38.W0 59 /r，vvvv 必须是编码后的 1111。 */
	enc_vex3_rrr(e, 2, 1, false, 0x59, inst->dst, REG_XMM0, inst->src);
}
static void encode_vmovdqu_load(EncoderCtx *e, IrInst *inst) {
	enc_vex3_mem(e, 1, 2, 0x6F, inst->dst, inst->src, inst->imm);
}
static void encode_vmovdqu_store(EncoderCtx *e, IrInst *inst) {
	enc_vex3_mem(e, 1, 2, 0x7F, inst->src, inst->dst, inst->imm);
}

static void encode_vzeroupper(EncoderCtx *e, IrInst *inst) {
	(void)inst;
	enc_byte(e, 0xC5); enc_byte(e, 0xF8); enc_byte(e, 0x77);
}

/* ========== 缂傛牜鐖滈崳銊ュ瀻閸欐垼銆?(閸濆牆绗囩悰? ========== */

typedef void (*encode_fn_t)(EncoderCtx *e, IrInst *inst);

static encode_fn_t dispatch_table[IR_OPCODE_COUNT];
static bool dispatch_init_done = false;

static void dispatch_init(void) {
	if (dispatch_init_done) return;
	memset(dispatch_table, 0, sizeof(dispatch_table));

	dispatch_table[IR_MOV_REG_REG]    = encode_mov_reg_reg;
	dispatch_table[IR_MOV_REG_IMM]    = encode_mov_reg_imm;
	dispatch_table[IR_MOV_REG_MEM]    = encode_mov_reg_mem;
	dispatch_table[IR_MOV_MEM_REG]    = encode_mov_mem_reg;
	dispatch_table[IR_MOV_MEM_IMM]    = encode_mov_mem_imm;
	dispatch_table[IR_MOV_MEM8_REG]   = encode_mov_mem8_reg;
	dispatch_table[IR_MOVZX_REG_MEM8] = encode_movzx_reg_mem8;
	dispatch_table[IR_LEA]            = encode_lea;
	dispatch_table[IR_LEA_RIP]        = encode_lea_rip;
	dispatch_table[IR_LEA_IDX]        = encode_lea_idx;
	dispatch_table[IR_PUSH_REG]       = encode_push_reg;
	dispatch_table[IR_POP_REG]        = encode_pop_reg;
	dispatch_table[IR_ADD_REG_REG]    = encode_add_reg_reg;
	dispatch_table[IR_ADD_REG_IMM]    = encode_add_reg_imm;
	dispatch_table[IR_ADD_MEM_IMM]    = encode_add_mem_imm;
	dispatch_table[IR_SUB_REG_REG]    = encode_sub_reg_reg;
	dispatch_table[IR_SUB_REG_IMM]    = encode_sub_reg_imm;
	dispatch_table[IR_IMUL_REG_REG]   = encode_imul_reg_reg;
	dispatch_table[IR_IMUL_REG_IMM]   = encode_imul_reg_imm;
	dispatch_table[IR_IMUL_WIDE_REG]  = encode_imul_wide_reg;
	dispatch_table[IR_MUL_WIDE_REG]   = encode_mul_wide_reg;
	dispatch_table[IR_ALIGN32]        = encode_align32;
	dispatch_table[IR_IDIV_REG]       = encode_idiv_reg;
	dispatch_table[IR_INC_REG]        = encode_inc_reg;
	dispatch_table[IR_INC_MEM]        = encode_inc_mem;
	dispatch_table[IR_NEG_REG]        = encode_neg_reg;
	dispatch_table[IR_CQO]            = encode_cqo;
	dispatch_table[IR_XOR_REG_REG]    = encode_xor_reg_reg;
	dispatch_table[IR_XOR_REG_IMM]    = encode_xor_reg_imm;
	dispatch_table[IR_AND_REG_REG]    = encode_and_reg_reg;
	dispatch_table[IR_AND_REG_IMM]    = encode_and_reg_imm;
	dispatch_table[IR_OR_REG_REG]     = encode_or_reg_reg;
	dispatch_table[IR_SHL_REG_IMM]    = encode_shl_reg_imm;
	dispatch_table[IR_SHR_REG_IMM]    = encode_shr_reg_imm;
	dispatch_table[IR_SAR_REG_IMM]    = encode_sar_reg_imm;
	dispatch_table[IR_SHL_REG_CL]     = encode_shl_reg_cl;
	dispatch_table[IR_SHR_REG_CL]     = encode_shr_reg_cl;
	dispatch_table[IR_SAR_REG_CL]     = encode_sar_reg_cl;
	dispatch_table[IR_NOT_REG]        = encode_not_reg;
	dispatch_table[IR_CMP_REG_REG]    = encode_cmp_reg_reg;
	dispatch_table[IR_CMP_REG_IMM]    = encode_cmp_reg_imm;
	dispatch_table[IR_TEST_REG_REG]   = encode_test_reg_reg;
	dispatch_table[IR_SETE]           = encode_sete;
	dispatch_table[IR_SETNE]          = encode_setne;
	dispatch_table[IR_SETL]           = encode_setl;
	dispatch_table[IR_SETLE]          = encode_setle;
	dispatch_table[IR_SETG]           = encode_setg;
	dispatch_table[IR_SETGE]          = encode_setge;
	dispatch_table[IR_SETA]           = encode_seta;
	dispatch_table[IR_SETAE]          = encode_setae;
	dispatch_table[IR_SETB]           = encode_setb;
	dispatch_table[IR_SETBE]          = encode_setbe;
	dispatch_table[IR_SETZ]           = encode_setz;
	dispatch_table[IR_SETNP]          = encode_setnp;
	dispatch_table[IR_MOVZX_REG8]     = encode_movzx_reg8;
	dispatch_table[IR_CMOVE]          = encode_cmove;
	dispatch_table[IR_CMOVNE]         = encode_cmovne;
	dispatch_table[IR_CMOVL]          = encode_cmovl;
	dispatch_table[IR_CMOVLE]         = encode_cmovle;
	dispatch_table[IR_CMOVG]          = encode_cmovg;
	dispatch_table[IR_CMOVGE]         = encode_cmovge;
	dispatch_table[IR_CMOVA]          = encode_cmova;
	dispatch_table[IR_CMOVAE]         = encode_cmovae;
	dispatch_table[IR_CMOVB]          = encode_cmovb;
	dispatch_table[IR_CMOVBE]         = encode_cmovbe;
	dispatch_table[IR_JMP]            = encode_jmp;
	dispatch_table[IR_JMP_EXTERN]     = encode_jmp_extern;
	dispatch_table[IR_JE]             = encode_je;
	dispatch_table[IR_JNE]            = encode_jne;
	dispatch_table[IR_JZ]             = encode_jz;
	dispatch_table[IR_JNZ]            = encode_jnz;
	dispatch_table[IR_JG]             = encode_jg;
	dispatch_table[IR_JGE]            = encode_jge;
	dispatch_table[IR_JL]             = encode_jl;
	dispatch_table[IR_JLE]            = encode_jle;
	dispatch_table[IR_JA]             = encode_ja;
	dispatch_table[IR_JAE]            = encode_jae;
	dispatch_table[IR_JB]             = encode_jb;
	dispatch_table[IR_JBE]            = encode_jbe;
	dispatch_table[IR_JS]             = encode_js;
	dispatch_table[IR_JNS]            = encode_jns;
	dispatch_table[IR_CALL_LABEL]     = encode_call_label;
	dispatch_table[IR_CALL_EXTERN]    = encode_call_extern;
	dispatch_table[IR_CALL_REG]       = encode_call_reg;
	dispatch_table[IR_RET]            = encode_ret;
	dispatch_table[IR_LABEL]          = encode_label;
	dispatch_table[IR_LABEL_NAMED]    = encode_label_named;
	dispatch_table[IR_EXTERN]         = encode_nop;
	dispatch_table[IR_GLOBAL]         = encode_nop;
	dispatch_table[IR_MOVQ_XMM_REG]   = encode_movq_xmm_reg;
	dispatch_table[IR_MOVQ_REG_XMM]   = encode_movq_reg_xmm;
	dispatch_table[IR_MOVSD_XMM_MEM]  = encode_movsd_xmm_mem;
	dispatch_table[IR_CVTSI2SD]       = encode_cvtsi2sd;
	dispatch_table[IR_CVTTSD2SI]      = encode_cvttsd2si;
	dispatch_table[IR_ADDSD]          = encode_addsd;
	dispatch_table[IR_SUBSD]          = encode_subsd;
	dispatch_table[IR_MULSD]          = encode_mulsd;
	dispatch_table[IR_DIVSD]          = encode_divsd;
	dispatch_table[IR_UCOMISD]        = encode_ucomisd;
	dispatch_table[IR_VFMADD132SD]    = encode_vfmadd132sd;
	dispatch_table[IR_VPADDQ]         = encode_vpaddq;
	dispatch_table[IR_VPSUBQ]         = encode_vpsubq;
	dispatch_table[IR_VPSADBW]        = encode_vpsadbw;
	dispatch_table[IR_VPXOR]          = encode_vpxor;
	dispatch_table[IR_VPAND]          = encode_vpand;
	dispatch_table[IR_VPOR]           = encode_vpor;
	dispatch_table[IR_VPMULLD]        = encode_vpmulld;
	dispatch_table[IR_VPCMPEQB]       = encode_vpcmpeqb;
	dispatch_table[IR_VPCMPEQQ]       = encode_vpcmpeqq;
	dispatch_table[IR_VPBROADCASTB]   = encode_vpbroadcastb;
	dispatch_table[IR_VPBROADCASTQ]   = encode_vpbroadcastq;
	dispatch_table[IR_VMOVDQU_LOAD]   = encode_vmovdqu_load;
	dispatch_table[IR_VMOVDQU_STORE]  = encode_vmovdqu_store;
	dispatch_table[IR_VZEROUPPER]     = encode_vzeroupper;

	dispatch_init_done = true;
}

/* ========== 閺佺増宓佸▓鐢电椽閻?========== */

static void encode_data_section(IrBuffer *ir, EncodeResult *out) {
	/* 妫板嫪鍙婃径褍鐨?*/
	int cap = 4096;
	out->data_buf = (uint8_t *)malloc(cap);
	out->data_len = 0;

	/* 閺佺増宓佸▓鐢殿儊閸欏嘲浜哥粔鏄忣唶瑜?*/
	for (int i = 0; i < ir->data_count; i++) {
		IrInst *inst = &ir->data[i];
		switch (inst->IrNode) {
		case IR_DATA_LABEL:
			/* 鐠佹澘缍嶇粭锕€褰块崑蹇曅?*/
			if (out->sym_count >= out->sym_cap) {
				out->sym_cap = out->sym_cap ? out->sym_cap * 2 : 64;
				out->symbols = realloc(out->symbols, out->sym_cap * sizeof(out->symbols[0]));
			}
			out->symbols[out->sym_count].name = strdup(inst->sym_name);
			out->symbols[out->sym_count].offset = out->data_len;
			out->symbols[out->sym_count].section = 2; /* .data = section 2 */
			out->sym_count++;
			break;

		case IR_DATA_BYTES:
			while (out->data_len + inst->data_len > cap) {
				cap *= 2;
				out->data_buf = realloc(out->data_buf, cap);
			}
			memcpy(out->data_buf + out->data_len, inst->data, inst->data_len);
			out->data_len += inst->data_len;
			break;

		case IR_DATA_QWORD: {
			while (out->data_len + 8 > cap) { cap *= 2; out->data_buf = realloc(out->data_buf, cap); }
			memcpy(out->data_buf + out->data_len, &inst->imm, 8);
			out->data_len += 8;
			break;
		}

		case IR_DATA_QWORD_SYM: {
			while (out->data_len + 8 > cap) { cap *= 2; out->data_buf = realloc(out->data_buf, cap); }
			/* 闂団偓鐟曚線鍣哥€规矮缍?閳?閸忓牆鍟?0閿涘本鍧婇崝?reloc */
			uint64_t zero = 0;
			memcpy(out->data_buf + out->data_len, &zero, 8);
			/* 閺佺増宓佸▓闈涘敶閻ㄥ嫰鍣哥€规矮缍呴崥搴ｇ敾閻?coff_writer 婢跺嫮鎮?*/
			if (out->reloc_count >= out->reloc_cap) {
				out->reloc_cap = out->reloc_cap ? out->reloc_cap * 2 : 32;
				out->relocs = realloc(out->relocs, out->reloc_cap * sizeof(IrReloc));
			}
			out->relocs[out->reloc_count].offset = out->data_len;
			out->relocs[out->reloc_count].sym_name = strdup(inst->sym_name);
			out->relocs[out->reloc_count].type = 1; /* IMAGE_REL_AMD64_ADDR64 */
			out->relocs[out->reloc_count].is_rip_data = true;
			out->relocs[out->reloc_count].label_id = 0;
			out->reloc_count++;
			out->data_len += 8;
			break;
		}

		default:
			break;
		}
	}
}

/* BSS 濞堥潧銇囩亸蹇氼吀缁?*/
static void encode_bss_section(IrBuffer *ir, EncodeResult *out) {
	out->bss_size = 0;
	for (int i = 0; i < ir->bss_count; i++) {
		IrInst *inst = &ir->bss[i];
		if (inst->IrNode == IR_BSS_LABEL) {
			if (out->sym_count >= out->sym_cap) {
				out->sym_cap = out->sym_cap ? out->sym_cap * 2 : 64;
				out->symbols = realloc(out->symbols, out->sym_cap * sizeof(out->symbols[0]));
			}
			out->symbols[out->sym_count].name = strdup(inst->sym_name);
			out->symbols[out->sym_count].offset = out->bss_size;
			out->symbols[out->sym_count].section = 3; /* .bss = section 3 */
			out->sym_count++;
		} else if (inst->IrNode == IR_BSS_RESQ) {
			out->bss_size += (int)(inst->imm * 8);
		}
	}
}

/* ========== 娑撹崵绱惍浣稿毐閺?========== */

static void encode_text_insts(IrBuffer *ir, EncoderCtx *e) {
	for (int i = 0; i < ir->text_count; i++) {
		IrInst *inst = &ir->text[i];
		e->current_inst = i;
		if (e->inst_offsets) e->inst_offsets[i] = e->len;
		if (inst->IrNode >= IR_OPCODE_COUNT) {
			fprintf(stderr, "encoder: unknown opcode %d\n", inst->IrNode);
			continue;
		}
		encode_fn_t fn = dispatch_table[inst->IrNode];
		if (fn) fn(e, inst);
		else fprintf(stderr, "encoder: unhandled opcode %d\n", inst->IrNode);
	}
}

static int is_local_branch_opcode(IrOpcode op) {
	switch (op) {
	case IR_JMP: case IR_JE: case IR_JNE: case IR_JZ: case IR_JNZ:
	case IR_JG: case IR_JGE: case IR_JL: case IR_JLE:
	case IR_JA: case IR_JAE: case IR_JB: case IR_JBE:
	case IR_JS: case IR_JNS: return 1;
	default: return 0;
	}
}

static void enc_discard(EncoderCtx *e) {
	free(e->buf); free(e->label_offsets); free(e->patches);
	for (int i = 0; i < e->reloc_count; ++i) free(e->relocs[i].sym_name);
	for (int i = 0; i < e->sym_count; ++i) free(e->syms[i].name);
	free(e->relocs); free(e->syms); free(e->inst_offsets);
}

int ir_encode(IrBuffer *ir, EncodeResult *out) {
	dispatch_init();
	memset(out, 0, sizeof(*out));

	EncoderCtx e;
	enc_init(&e);
	e.inst_offsets = (int *)calloc((size_t)ir->text_count, sizeof(int));
	encode_text_insts(ir, &e);

	uint8_t *short_branch = (uint8_t *)calloc((size_t)ir->text_count, 1);
	int relax = 0;
	for (int i = 0; i < ir->text_count; ++i) {
		IrInst *inst = &ir->text[i];
		if (!is_local_branch_opcode(inst->IrNode) || inst->label_id < 0 ||
			inst->label_id >= e.label_cap || e.label_offsets[inst->label_id] < 0) continue;
		int start = e.inst_offsets[i], target = e.label_offsets[inst->label_id];
		if (target > start && target - (start + 2) <= 127) {
			short_branch[i] = 1;
			relax = 1;
		}
	}
	if (relax) {
		enc_discard(&e);
		enc_init(&e);
		e.short_branch = short_branch;
		encode_text_insts(ir, &e);
	} else {
		free(e.inst_offsets);
		e.inst_offsets = NULL;
	}

	/* 缂傛牜鐖?.text 濞?*/
	/* 閸ョ偛锝為弽鍥╊劮鐠哄疇娴嗛崑蹇曅?*/
	for (int i = 0; i < e.patch_count; i++) {
		int off = e.patches[i].buf_off;
		int target_label = e.patches[i].label_id;
		int inst_end = e.patches[i].inst_end;
		if (target_label < e.label_cap && e.label_offsets[target_label] >= 0) {
			int32_t rel = e.label_offsets[target_label] - inst_end;
			if (e.patches[i].size == 1) {
				if (rel < -128 || rel > 127) fprintf(stderr, "encoder: short branch overflow\n");
				e.buf[off] = (uint8_t)(int8_t)rel;
			} else memcpy(e.buf + off, &rel, 4);
		} else {
			fprintf(stderr, "encoder: unresolved label %d\n", target_label);
		}
	}

	/* 鏉堟挸鍤?.text */
	out->text_code = e.buf;
	out->text_len = e.len;

	/* 缂傛牜鐖?.data */
	encode_data_section(ir, out);

	/* 缂傛牜鐖?.bss */
	encode_bss_section(ir, out);

	/* 婢跺秴鍩楅弬鍥ㄦ拱濞堢數娈戦柌宥呯暰娴ｅ秴鎷扮粭锕€褰?*/
	for (int i = 0; i < e.reloc_count; i++) {
		if (out->reloc_count >= out->reloc_cap) {
			out->reloc_cap = out->reloc_cap ? out->reloc_cap * 2 : 64;
			out->relocs = realloc(out->relocs, out->reloc_cap * sizeof(IrReloc));
		}
		out->relocs[out->reloc_count++] = e.relocs[i];
	}
	for (int i = 0; i < e.sym_count; i++) {
		if (out->sym_count >= out->sym_cap) {
			out->sym_cap = out->sym_cap ? out->sym_cap * 2 : 64;
			out->symbols = realloc(out->symbols, out->sym_cap * sizeof(out->symbols[0]));
		}
		out->symbols[out->sym_count].name = e.syms[i].name;
		out->symbols[out->sym_count].offset = e.syms[i].offset;
		out->symbols[out->sym_count].section = 1; /* .text = section 1 */
		out->sym_count++;
	}

	/* 濞撳懐鎮?encoder ctx 閸愬懘鍎撮敍鍫滅瑝闁插﹥鏂?buf 閸?syms閿涘苯鍑℃潪顒傂╃紒?out閿?*/
	free(e.label_offsets);
	free(e.patches);
	free(e.relocs);
	free(e.syms);
	free(short_branch);

	return 0;
}

void encode_result_free(EncodeResult *r) {
	free(r->text_code);
	free(r->data_buf);
	for (int i = 0; i < r->reloc_count; i++)
		free(r->relocs[i].sym_name);
	free(r->relocs);
	for (int i = 0; i < r->sym_count; i++)
		free(r->symbols[i].name);
	free(r->symbols);
	memset(r, 0, sizeof(*r));
}

