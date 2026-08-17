/* ir.h ? Mira IR (Intermediate Representation)
*
* NASM ?????codegen ?? IrInst ? IrBuffer??
* encoder ?? IrInst ??? x86-64 ?????coff_writer ?? .obj??
*/
#ifndef IR_H
#define IR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

/* ========== ??? ========== */
typedef enum {
REG_RAX = 0, REG_RCX, REG_RDX, REG_RBX,
REG_RSP, REG_RBP, REG_RSI, REG_RDI,
REG_R8, REG_R9, REG_R10, REG_R11,
REG_R12, REG_R13, REG_R14, REG_R15,
/* 32-bit aliases */
REG_EAX, REG_ECX, REG_EDX, REG_EBX,
REG_ESP, REG_EBP, REG_ESI, REG_EDI,
REG_R8D, REG_R9D, REG_R10D, REG_R11D,
REG_R12D, REG_R13D, REG_R14D, REG_R15D,
/* 8-bit aliases */
REG_AL, REG_CL, REG_DL, REG_BL,
/* SSE */
REG_XMM0, REG_XMM1, REG_XMM2, REG_XMM3, REG_XMM4, REG_XMM5,
REG_YMM0, REG_YMM1, REG_YMM2, REG_YMM3,
REG_YMM4, REG_YMM5, REG_YMM6, REG_YMM7,
REG_YMM8, REG_YMM9, REG_YMM10, REG_YMM11,
REG_YMM12, REG_YMM13, REG_YMM14, REG_YMM15,
REG_NONE
} IrReg;

/* ????? */
static inline int ir_reg_bits(IrReg r) {
if (r <= REG_R15) return 64;
if (r <= REG_R15D) return 32;
if (r <= REG_BL) return 8;
if (r <= REG_XMM5) return 128;
if (r <= REG_YMM15) return 256;
return 0;
}

/* 64-bit ???????(0-15) */
static inline int ir_reg_id(IrReg r) {
if (r <= REG_R15) return (int)r;
if (r <= REG_R15D) return (int)(r - REG_EAX);
if (r <= REG_BL) return (int)(r - REG_AL);
if (r >= REG_XMM0 && r <= REG_XMM5) return (int)(r - REG_XMM0);
if (r >= REG_YMM0 && r <= REG_YMM15) return (int)(r - REG_YMM0);
return -1;
}

/* ??? REX.R/B ?? */
static inline bool ir_reg_ext(IrReg r) {
int id = ir_reg_id(r);
return id >= 8;
}

/* ========== IR ??? ========== */
typedef enum {
/* ???? */
IR_MOV_REG_REG,       /* mov dst, src (64-bit) */
IR_MOV_REG_IMM,       /* mov dst, imm64 */
IR_MOV_REG_MEM,       /* mov dst, [src + imm] (64-bit) */
IR_MOV_MEM_REG,       /* mov [dst + imm], src (64-bit) */
IR_MOV_MEM_IMM,       /* mov qword [dst + imm], imm64 */
IR_MOV_MEM8_REG,      /* mov byte [dst + imm], src8 (al/cl/dl/bl) */
IR_MOVZX_REG_MEM8,    /* movzx dst32, byte [src + imm] */
IR_LEA,               /* lea dst, [src + imm] */
IR_LEA_RIP,           /* lea dst, [rip + sym/label] */

/* ??? */
IR_PUSH_REG,          /* push reg */
IR_POP_REG,           /* pop reg */

/* ?? */
IR_ADD_REG_REG,       /* add dst, src */
IR_ADD_REG_IMM,       /* add dst, imm32 */
IR_ADD_MEM_IMM,       /* add qword [dst + imm_disp], imm_value */
IR_SUB_REG_REG,       /* sub dst, src */
IR_SUB_REG_IMM,       /* sub dst, imm32 */
IR_IMUL_REG_REG,      /* imul dst, src */
IR_IMUL_REG_IMM,      /* imul dst, src, imm32 */
IR_IMUL_WIDE_REG,     /* imul src: RDX:RAX = RAX * src */
IR_MUL_WIDE_REG,      /* mul src:  RDX:RAX = RAX * src (unsigned) */
IR_ALIGN32,           /* pad next instruction/label to a 32-byte boundary */
IR_IDIV_REG,          /* idiv src  rax=quot, rdx=rem */
IR_INC_REG,           /* inc reg */
IR_INC_MEM,           /* inc qword [dst + imm] */
IR_NEG_REG,           /* neg reg */
IR_CQO,               /* cqo (sign-extend rax rdx:rax) */

/* ??? */
IR_XOR_REG_REG,       /* xor dst, src */
IR_XOR_REG_IMM,       /* xor dst, imm (8-bit) */
IR_AND_REG_REG,       /* and dst, src */
IR_AND_REG_IMM,       /* and dst, imm32 */
IR_OR_REG_REG,        /* or dst, src */
IR_SHL_REG_IMM,       /* shl dst, imm8 */
IR_SHR_REG_IMM,       /* shr dst, imm8 */
IR_SAR_REG_IMM,       /* sar dst, imm8 */
IR_SHL_REG_CL,        /* shl dst, cl */
IR_SHR_REG_CL,        /* shr dst, cl */
IR_SAR_REG_CL,        /* sar dst, cl */
IR_NOT_REG,           /* not reg */

/* ?? */
IR_CMP_REG_REG,       /* cmp dst, src */
IR_CMP_REG_IMM,       /* cmp dst, imm32 */
IR_TEST_REG_REG,      /* test dst, src */

/* ?????? */
IR_SETE,   IR_SETNE,
IR_SETL,   IR_SETLE,
IR_SETG,   IR_SETGE,
IR_SETA,   IR_SETAE,
IR_SETB,   IR_SETBE,
IR_SETZ,   IR_SETNP,
IR_MOVZX_REG8,        /* movzx eax, al (dst=32-bit, src=8-bit) */
IR_CMOVE, IR_CMOVNE,
IR_CMOVL, IR_CMOVLE,
IR_CMOVG, IR_CMOVGE,
IR_CMOVA, IR_CMOVAE,
IR_CMOVB, IR_CMOVBE,

/* ?? */
IR_JMP,
IR_JMP_EXTERN,        /* jmp symbol (REL32) */
IR_JE,  IR_JNE,
IR_JZ,  IR_JNZ,
IR_JG,  IR_JGE,
IR_JL,  IR_JLE,
IR_JA,  IR_JAE,
IR_JB,  IR_JBE,
IR_JS,  IR_JNS,

/* ????? */
IR_CALL_LABEL,        /* call ??? (label_id) */
IR_CALL_EXTERN,       /* call ???? (sym_name) */
IR_CALL_REG,          /* call reg */
IR_RET,

/* ?? / ???? */
IR_LABEL,             /* ???? */
IR_LABEL_NAMED,       /* ????  */
IR_EXTERN,            /* ?????? */
IR_GLOBAL,            /* ?????? */

/* SSE ?? */
IR_MOVQ_XMM_REG,     /* movq xmm, reg */
IR_MOVQ_REG_XMM,     /* movq reg, xmm */
IR_MOVSD_XMM_MEM,    /* movsd xmm, [rip + sym] */
IR_CVTSI2SD,          /* cvtsi2sd xmm, reg */
IR_CVTTSD2SI,         /* cvttsd2si reg, xmm */
IR_ADDSD,             /* addsd xmm0, xmm1 */
IR_SUBSD,
IR_MULSD,
IR_DIVSD,
IR_UCOMISD,           /* ucomisd xmm0, xmm1 */
IR_VFMADD132SD,       /* dst = dst * src2 + src (scalar double) */
IR_VPADDQ,
IR_VPSUBQ,
IR_VPSADBW,
IR_VPXOR,
IR_VPAND,
IR_VPOR,
IR_VPMULLD,
IR_VPCMPEQB,
IR_VPCMPEQQ,
IR_VPBROADCASTB,
IR_VPBROADCASTQ,
IR_VMOVDQU_LOAD,
IR_VMOVDQU_STORE,
IR_VZEROUPPER,

/* ?????? */
IR_DATA_LABEL,        /* ????? */
IR_DATA_BYTES,        /* db byte, byte, ... */
IR_DATA_QWORD,        /* dq value */
IR_DATA_QWORD_SYM,   /* dq symbol_name */
IR_BSS_LABEL,         /* BSS ??? */
IR_BSS_RESQ,          /* resq N */

IR_LEA_IDX,           /* lea dst, [src + index*1] (index stored in imm) */

IR_OPCODE_COUNT       /* 鏋氫妇鎬绘暟 */
} IrOpcode;

/* ========== IR ?? ========== */
typedef struct {
IrOpcode IrNode;
IrReg    dst;          /* ????? */
IrReg    src;          /* ????? */
IrReg    src2;         /* third vector source */
int64_t  imm;          /* imm / displacement */
int64_t  extra_imm;    /* ????? ( add [addr+disp], extra_imm) */
uint8_t  branch_policy; /* SsaBranchPolicy-compatible lowering hint */
int      label_id;     /* ?? ID */
char    *sym_name;     /* ??/?? ??? */
uint8_t *data;         /* IR_DATA_BYTES ?????? */
int      data_len;     /* ?????? */
} IrInst;

enum { IR_BRANCH_UNKNOWN = 0, IR_BRANCH_PREFER_JUMP = 1,
       IR_BRANCH_PREFER_BRANCHLESS = 2 };

/* ========== IR ??? ========== */
typedef struct {
/* .text ??? */
IrInst *text;
int     text_count;
int     text_cap;

/* .data ??? */
IrInst *data;
int     data_count;
int     data_cap;

/* .bss ? */
IrInst *bss;
int     bss_count;
int     bss_cap;

/* ?????? */
char  **externs;
int     extern_count;
int     extern_cap;

/* ?????? */
char  **globals;
int     global_count;
int     global_cap;
} IrBuffer;

/* ========== IR API ========== */

/* Init / Free */
void ir_init(IrBuffer *ir);
void ir_free(IrBuffer *ir);

/* === .text ??? === */

/* ???? */
void ir_emit_raw(IrBuffer *ir, IrInst inst);

/* ???? */
void ir_mov_reg_reg(IrBuffer *ir, IrReg dst, IrReg src);
void ir_mov_reg_imm(IrBuffer *ir, IrReg dst, int64_t imm);
void ir_mov_reg_mem(IrBuffer *ir, IrReg dst, IrReg base, int64_t disp);
void ir_mov_mem_reg(IrBuffer *ir, IrReg base, int64_t disp, IrReg src);
void ir_mov_mem_imm(IrBuffer *ir, IrReg base, int64_t disp, int64_t imm);
void ir_mov_mem8_reg(IrBuffer *ir, IrReg base, int64_t disp, IrReg src8);
void ir_movzx_reg_mem8(IrBuffer *ir, IrReg dst, IrReg base, int64_t disp);
void ir_lea(IrBuffer *ir, IrReg dst, IrReg base, int64_t disp);
void ir_lea_rip(IrBuffer *ir, IrReg dst, const char *sym);
void ir_lea_rip_label(IrBuffer *ir, IrReg dst, int label_id);
void ir_lea_idx(IrBuffer *ir, IrReg dst, IrReg base, IrReg index);

/* ??? */
void ir_push(IrBuffer *ir, IrReg reg);
void ir_pop(IrBuffer *ir, IrReg reg);

/* ?? */
void ir_add_reg_reg(IrBuffer *ir, IrReg dst, IrReg src);
void ir_add_reg_imm(IrBuffer *ir, IrReg dst, int64_t imm);
void ir_add_mem_imm(IrBuffer *ir, IrReg base, int64_t disp, int64_t val);
void ir_sub_reg_reg(IrBuffer *ir, IrReg dst, IrReg src);
void ir_sub_reg_imm(IrBuffer *ir, IrReg dst, int64_t imm);
void ir_imul_reg_reg(IrBuffer *ir, IrReg dst, IrReg src);
void ir_imul_reg_imm(IrBuffer *ir, IrReg dst, IrReg src, int64_t imm);
void ir_imul_wide(IrBuffer *ir, IrReg src);
void ir_mul_wide(IrBuffer *ir, IrReg src);
void ir_idiv(IrBuffer *ir, IrReg divisor);
void ir_inc_reg(IrBuffer *ir, IrReg reg);
void ir_inc_mem(IrBuffer *ir, IrReg base, int64_t disp);
void ir_neg(IrBuffer *ir, IrReg reg);
void ir_cqo(IrBuffer *ir);

/* ??? */
void ir_xor_reg_reg(IrBuffer *ir, IrReg dst, IrReg src);
void ir_xor_reg_imm(IrBuffer *ir, IrReg dst, int64_t imm);
void ir_and_reg_reg(IrBuffer *ir, IrReg dst, IrReg src);
void ir_and_reg_imm(IrBuffer *ir, IrReg dst, int64_t imm);
void ir_or_reg_reg(IrBuffer *ir, IrReg dst, IrReg src);
void ir_shl_reg_imm(IrBuffer *ir, IrReg dst, int imm);
void ir_shr_reg_imm(IrBuffer *ir, IrReg dst, int imm);
void ir_sar_reg_imm(IrBuffer *ir, IrReg dst, int imm);
void ir_shl_reg_cl(IrBuffer *ir, IrReg dst);
void ir_shr_reg_cl(IrBuffer *ir, IrReg dst);
void ir_sar_reg_cl(IrBuffer *ir, IrReg dst);
void ir_not_reg(IrBuffer *ir, IrReg reg);

/* ?? */
void ir_cmp_reg_reg(IrBuffer *ir, IrReg dst, IrReg src);
void ir_cmp_reg_imm(IrBuffer *ir, IrReg dst, int64_t imm);
void ir_test_reg_reg(IrBuffer *ir, IrReg dst, IrReg src);

/* ?????? */
void ir_setcc(IrBuffer *ir, IrOpcode setop, IrReg dst8);
void ir_movzx_reg8(IrBuffer *ir, IrReg dst32, IrReg src8);
void ir_cmovcc(IrBuffer *ir, IrOpcode cmovop, IrReg dst, IrReg src);

/* ?? */
void ir_jmp(IrBuffer *ir, int label_id);
void ir_jmp_extern(IrBuffer *ir, const char *sym);
void ir_jcc(IrBuffer *ir, IrOpcode jop, int label_id);

/* ?? */
void ir_call_label(IrBuffer *ir, int label_id);
void ir_call_extern(IrBuffer *ir, const char *sym);
void ir_call_reg(IrBuffer *ir, IrReg reg);
void ir_ret(IrBuffer *ir);

/* ?? */
void ir_label(IrBuffer *ir, int label_id);
void ir_label_named(IrBuffer *ir, const char *name);

/* ???? */
void ir_extern(IrBuffer *ir, const char *name);
void ir_global(IrBuffer *ir, const char *name);

/* SSE ?? */
void ir_movq_xmm_reg(IrBuffer *ir, IrReg xmm, IrReg reg);
void ir_movq_reg_xmm(IrBuffer *ir, IrReg reg, IrReg xmm);
void ir_movsd_xmm_rip(IrBuffer *ir, IrReg xmm, const char *sym);
void ir_cvtsi2sd(IrBuffer *ir, IrReg xmm, IrReg reg);
void ir_cvttsd2si(IrBuffer *ir, IrReg reg, IrReg xmm);
void ir_addsd(IrBuffer *ir, IrReg dst, IrReg src);
void ir_subsd(IrBuffer *ir, IrReg dst, IrReg src);
void ir_mulsd(IrBuffer *ir, IrReg dst, IrReg src);
void ir_divsd(IrBuffer *ir, IrReg dst, IrReg src);
void ir_ucomisd(IrBuffer *ir, IrReg dst, IrReg src);
void ir_vfmadd132sd(IrBuffer *ir, IrReg dst, IrReg add, IrReg mul);
void ir_vpaddq(IrBuffer *ir, IrReg dst, IrReg src1, IrReg src2);
void ir_vpsubq(IrBuffer *ir, IrReg dst, IrReg src1, IrReg src2);
void ir_vpxor(IrBuffer *ir, IrReg dst, IrReg src1, IrReg src2);
void ir_vpand(IrBuffer *ir, IrReg dst, IrReg src1, IrReg src2);
void ir_vpor(IrBuffer *ir, IrReg dst, IrReg src1, IrReg src2);
void ir_vpmulld(IrBuffer *ir, IrReg dst, IrReg src1, IrReg src2);
void ir_vpcmpeqq(IrBuffer *ir, IrReg dst, IrReg src1, IrReg src2);
void ir_vpbroadcastq(IrBuffer *ir, IrReg dst, IrReg src);
void ir_vmovdqu_load(IrBuffer *ir, IrReg dst, IrReg base, int64_t disp);
void ir_vmovdqu_store(IrBuffer *ir, IrReg base, int64_t disp, IrReg src);
void ir_vzeroupper(IrBuffer *ir);

/* === .data ??? === */
void ir_data_label(IrBuffer *ir, const char *name);
void ir_data_label_id(IrBuffer *ir, const char *prefix, int id);
void ir_data_bytes(IrBuffer *ir, const uint8_t *bytes, int len);
void ir_data_qword(IrBuffer *ir, int64_t val);
void ir_data_qword_dbl(IrBuffer *ir, double val);
void ir_data_qword_sym(IrBuffer *ir, const char *sym);

/* === .bss ??? === */
void ir_bss_label(IrBuffer *ir, const char *name);
void ir_bss_resq(IrBuffer *ir, int count);

/* === ?? & ?? === */

/* ????? */
typedef struct {
uint32_t offset;          /* .text ????? */
char    *sym_name;        /* ??? */
uint16_t type;            /* COFF ?????? */
bool     is_rip_data;     /* ??? RIP-relative ???? */
int      label_id;        /* ?? RIP-relative label ?? */
} IrReloc;

/* ???? */
typedef struct {
uint8_t *text_code;       /* .text ??? */
int      text_len;

uint8_t *data_buf;        /* .data ????? */
int      data_len;

int      bss_size;        /* .bss ????? */

IrReloc *relocs;          /* ???? */
int      reloc_count;
int      reloc_cap;

/* ??? */
struct { char *name; uint32_t offset; int section; } *symbols;
int      sym_count;
int      sym_cap;
} EncodeResult;

/* encoder */
int ir_encode(IrBuffer *ir, EncodeResult *out);
void encode_result_free(EncodeResult *r);

/* COFF writer */
int coff_write_obj(EncodeResult *enc, IrBuffer *ir, const char *path);
int coff_write_mem(EncodeResult *enc, IrBuffer *ir, uint8_t **out_buf, int *out_len);

/* ELF writer (Linux/SysV 目标) */
int elf_write_obj(EncodeResult *enc, IrBuffer *ir, const char *path);
int elf_write_mem(EncodeResult *enc, IrBuffer *ir, uint8_t **out_buf, int *out_len);

/* IR dump */
bool ir_dump(const IrBuffer *ir, FILE *out, IrOpcode *unsupported);

/* ?? passes */
void ir_opt_constant_fold(IrBuffer *ir);
void ir_opt_peephole(IrBuffer *ir);
void ir_opt_strength_reduce(IrBuffer *ir);
void ir_opt_redundant_load(IrBuffer *ir);
void ir_opt_const_fold_div(IrBuffer *ir);
void ir_opt_auto_vectorize(IrBuffer *ir);
void ir_opt_countdown_loops(IrBuffer *ir);
void ir_opt_uniquify_labels(IrBuffer *ir);
void ir_opt_repair_loop_bounds(IrBuffer *ir);
void ir_opt_unroll4_remainder(IrBuffer *ir);
void ir_opt_rotate_canonical_loop_test(IrBuffer *ir);
void ir_opt_hoist_loop_scratch_constants(IrBuffer *ir);
uint64_t ir_opt_hoist_loop_scratch_constants_counted(IrBuffer *ir);
void ir_opt_register_rotation(IrBuffer *ir);
void ir_opt_ilp_schedule(IrBuffer *ir);
void ir_opt_align_loop_headers(IrBuffer *ir);

#endif /* IR_H */

