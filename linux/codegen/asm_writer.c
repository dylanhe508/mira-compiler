#include "asm_writer.h"

#include <stdint.h>
#include <stdio.h>

static const char *const reg_names[] = {
	"rax","rcx","rdx","rbx","rsp","rbp","rsi","rdi",
	"r8","r9","r10","r11","r12","r13","r14","r15",
	"eax","ecx","edx","ebx","esp","ebp","esi","edi",
	"r8d","r9d","r10d","r11d","r12d","r13d","r14d","r15d",
	"al","cl","dl","bl",
	"xmm0","xmm1","xmm2","xmm3","xmm4","xmm5",
	"ymm0","ymm1","ymm2","ymm3","ymm4","ymm5","ymm6","ymm7",
	"ymm8","ymm9","ymm10","ymm11","ymm12","ymm13","ymm14","ymm15",
	"none"
};

static const char *reg_name(IrReg reg) {
	if (reg >= REG_RAX && reg <= REG_NONE) return reg_names[reg];
	return "invalid_register";
}

static const char *scalar_reg_name(IrReg reg) {
	static const char *const xmm_names[] = {
		"xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7",
		"xmm8","xmm9","xmm10","xmm11","xmm12","xmm13","xmm14","xmm15"
	};
	if (reg >= REG_YMM0 && reg <= REG_YMM15)
		return xmm_names[reg - REG_YMM0];
	return reg_name(reg);
}

static void write_memory(FILE *out, const char *size, IrReg base, int64_t displacement) {
	if (size && *size) fprintf(out, "%s PTR ", size);
	if (displacement > 0)
		fprintf(out, "[%s + %lld]", reg_name(base), (long long)displacement);
	else if (displacement < 0)
		fprintf(out, "[%s - %lld]", reg_name(base), (long long)-displacement);
	else
		fprintf(out, "[%s]", reg_name(base));
}

bool ir_asm_supports_opcode(IrOpcode opcode) {
	return opcode >= IR_MOV_REG_REG && opcode < IR_OPCODE_COUNT;
}

static bool write_inst(const IrInst *inst, FILE *out, IrOpcode *unsupported,
		int label_scope) {
	switch (inst->IrNode) {
	case IR_MOV_REG_REG:
		fprintf(out, "  mov %s, %s\n", reg_name(inst->dst), reg_name(inst->src)); return true;
	case IR_MOV_REG_IMM:
		fprintf(out, "  mov %s, %lld\n", reg_name(inst->dst), (long long)inst->imm); return true;
	case IR_MOV_REG_MEM:
		fprintf(out, "  mov %s, ", reg_name(inst->dst));
		write_memory(out, "", inst->src, inst->imm); fputc('\n', out); return true;
	case IR_MOV_MEM_REG:
		fputs("  mov ", out); write_memory(out, "", inst->dst, inst->imm);
		fprintf(out, ", %s\n", reg_name(inst->src)); return true;
	case IR_MOV_MEM_IMM:
		fputs("  mov ", out); write_memory(out, "QWORD", inst->dst, inst->imm);
		fprintf(out, ", %lld\n", (long long)inst->extra_imm); return true;
	case IR_MOV_MEM8_REG:
		fputs("  mov ", out); write_memory(out, "BYTE", inst->dst, inst->imm);
		fprintf(out, ", %s\n", reg_name(inst->src)); return true;
	case IR_MOVZX_REG_MEM8:
		fprintf(out, "  movzx %s, ", reg_name(inst->dst));
		write_memory(out, "BYTE", inst->src, inst->imm); fputc('\n', out); return true;
	case IR_LEA:
		fprintf(out, "  lea %s, ", reg_name(inst->dst));
		write_memory(out, "", inst->src, inst->imm); fputc('\n', out); return true;
	case IR_LEA_RIP:
		if (inst->sym_name)
			fprintf(out, "  lea %s, [rip + %s]\n", reg_name(inst->dst), inst->sym_name);
		else
			fprintf(out, "  lea %s, [rip + .L%d_%d]\n", reg_name(inst->dst), label_scope, inst->label_id);
		return true;
	case IR_PUSH_REG: fprintf(out, "  push %s\n", reg_name(inst->dst)); return true;
	case IR_POP_REG: fprintf(out, "  pop %s\n", reg_name(inst->dst)); return true;
	case IR_ADD_REG_REG: fprintf(out, "  add %s, %s\n", reg_name(inst->dst), reg_name(inst->src)); return true;
	case IR_ADD_REG_IMM: fprintf(out, "  add %s, %lld\n", reg_name(inst->dst), (long long)inst->imm); return true;
	case IR_ADD_MEM_IMM:
		fputs("  add ", out); write_memory(out, "QWORD", inst->dst, inst->imm);
		fprintf(out, ", %lld\n", (long long)inst->extra_imm); return true;
	case IR_SUB_REG_REG: fprintf(out, "  sub %s, %s\n", reg_name(inst->dst), reg_name(inst->src)); return true;
	case IR_SUB_REG_IMM: fprintf(out, "  sub %s, %lld\n", reg_name(inst->dst), (long long)inst->imm); return true;
	case IR_IMUL_REG_REG: fprintf(out, "  imul %s, %s\n", reg_name(inst->dst), reg_name(inst->src)); return true;
	case IR_IMUL_REG_IMM: fprintf(out, "  imul %s, %s, %lld\n", reg_name(inst->dst), reg_name(inst->src), (long long)inst->imm); return true;
	case IR_IMUL_WIDE_REG: fprintf(out, "  imul %s\n", reg_name(inst->src)); return true;
	case IR_MUL_WIDE_REG: fprintf(out, "  mul %s\n", reg_name(inst->src)); return true;
	case IR_ALIGN32: fputs("  .p2align 5\n", out); return true;
	case IR_IDIV_REG: fprintf(out, "  idiv %s\n", reg_name(inst->src)); return true;
	case IR_INC_REG: fprintf(out, "  inc %s\n", reg_name(inst->dst)); return true;
	case IR_INC_MEM:
		fputs("  inc ", out); write_memory(out, "QWORD", inst->dst, inst->imm);
		fputc('\n', out); return true;
	case IR_NEG_REG: fprintf(out, "  neg %s\n", reg_name(inst->dst)); return true;
	case IR_CQO: fputs("  cqo\n", out); return true;
	case IR_XOR_REG_REG: fprintf(out, "  xor %s, %s\n", reg_name(inst->dst), reg_name(inst->src)); return true;
	case IR_XOR_REG_IMM: fprintf(out, "  xor %s, %lld\n", reg_name(inst->dst), (long long)inst->imm); return true;
	case IR_AND_REG_REG: fprintf(out, "  and %s, %s\n", reg_name(inst->dst), reg_name(inst->src)); return true;
	case IR_AND_REG_IMM: fprintf(out, "  and %s, %lld\n", reg_name(inst->dst), (long long)inst->imm); return true;
	case IR_OR_REG_REG: fprintf(out, "  or %s, %s\n", reg_name(inst->dst), reg_name(inst->src)); return true;
	case IR_SHL_REG_IMM: fprintf(out, "  shl %s, %lld\n", reg_name(inst->dst), (long long)inst->imm); return true;
	case IR_SHR_REG_IMM: fprintf(out, "  shr %s, %lld\n", reg_name(inst->dst), (long long)inst->imm); return true;
	case IR_SAR_REG_IMM: fprintf(out, "  sar %s, %lld\n", reg_name(inst->dst), (long long)inst->imm); return true;
	case IR_SHL_REG_CL: fprintf(out, "  shl %s, cl\n", reg_name(inst->dst)); return true;
	case IR_SHR_REG_CL: fprintf(out, "  shr %s, cl\n", reg_name(inst->dst)); return true;
	case IR_SAR_REG_CL: fprintf(out, "  sar %s, cl\n", reg_name(inst->dst)); return true;
	case IR_NOT_REG: fprintf(out, "  not %s\n", reg_name(inst->dst)); return true;
	case IR_CMP_REG_REG: fprintf(out, "  cmp %s, %s\n", reg_name(inst->dst), reg_name(inst->src)); return true;
	case IR_CMP_REG_IMM: fprintf(out, "  cmp %s, %lld\n", reg_name(inst->dst), (long long)inst->imm); return true;
	case IR_TEST_REG_REG: fprintf(out, "  test %s, %s\n", reg_name(inst->dst), reg_name(inst->src)); return true;
	case IR_SETE: fprintf(out, "  sete %s\n", reg_name(inst->dst)); return true;
	case IR_SETNE: fprintf(out, "  setne %s\n", reg_name(inst->dst)); return true;
	case IR_SETL: fprintf(out, "  setl %s\n", reg_name(inst->dst)); return true;
	case IR_SETLE: fprintf(out, "  setle %s\n", reg_name(inst->dst)); return true;
	case IR_SETG: fprintf(out, "  setg %s\n", reg_name(inst->dst)); return true;
	case IR_SETGE: fprintf(out, "  setge %s\n", reg_name(inst->dst)); return true;
	case IR_SETA: fprintf(out, "  seta %s\n", reg_name(inst->dst)); return true;
	case IR_SETAE: fprintf(out, "  setae %s\n", reg_name(inst->dst)); return true;
	case IR_SETB: fprintf(out, "  setb %s\n", reg_name(inst->dst)); return true;
	case IR_SETBE: fprintf(out, "  setbe %s\n", reg_name(inst->dst)); return true;
	case IR_SETZ: fprintf(out, "  setz %s\n", reg_name(inst->dst)); return true;
	case IR_SETNP: fprintf(out, "  setnp %s\n", reg_name(inst->dst)); return true;
	case IR_MOVZX_REG8: fprintf(out, "  movzx %s, %s\n", reg_name(inst->dst), reg_name(inst->src)); return true;
	case IR_CMOVE: fprintf(out, "  cmove %s, %s\n", reg_name(inst->dst), reg_name(inst->src)); return true;
	case IR_CMOVNE: fprintf(out, "  cmovne %s, %s\n", reg_name(inst->dst), reg_name(inst->src)); return true;
	case IR_CMOVL: fprintf(out, "  cmovl %s, %s\n", reg_name(inst->dst), reg_name(inst->src)); return true;
	case IR_CMOVLE: fprintf(out, "  cmovle %s, %s\n", reg_name(inst->dst), reg_name(inst->src)); return true;
	case IR_CMOVG: fprintf(out, "  cmovg %s, %s\n", reg_name(inst->dst), reg_name(inst->src)); return true;
	case IR_CMOVGE: fprintf(out, "  cmovge %s, %s\n", reg_name(inst->dst), reg_name(inst->src)); return true;
	case IR_CMOVA: fprintf(out, "  cmova %s, %s\n", reg_name(inst->dst), reg_name(inst->src)); return true;
	case IR_CMOVAE: fprintf(out, "  cmovae %s, %s\n", reg_name(inst->dst), reg_name(inst->src)); return true;
	case IR_CMOVB: fprintf(out, "  cmovb %s, %s\n", reg_name(inst->dst), reg_name(inst->src)); return true;
	case IR_CMOVBE: fprintf(out, "  cmovbe %s, %s\n", reg_name(inst->dst), reg_name(inst->src)); return true;
	case IR_JMP: fprintf(out, "  jmp .L%d_%d\n", label_scope, inst->label_id); return true;
	case IR_JMP_EXTERN: fprintf(out, "  jmp %s\n", inst->sym_name); return true;
	case IR_JE: fprintf(out, "  je .L%d_%d\n", label_scope, inst->label_id); return true;
	case IR_JNE: fprintf(out, "  jne .L%d_%d\n", label_scope, inst->label_id); return true;
	case IR_JZ: fprintf(out, "  jz .L%d_%d\n", label_scope, inst->label_id); return true;
	case IR_JNZ: fprintf(out, "  jnz .L%d_%d\n", label_scope, inst->label_id); return true;
	case IR_JG: fprintf(out, "  jg .L%d_%d\n", label_scope, inst->label_id); return true;
	case IR_JGE: fprintf(out, "  jge .L%d_%d\n", label_scope, inst->label_id); return true;
	case IR_JL: fprintf(out, "  jl .L%d_%d\n", label_scope, inst->label_id); return true;
	case IR_JLE: fprintf(out, "  jle .L%d_%d\n", label_scope, inst->label_id); return true;
	case IR_JA: fprintf(out, "  ja .L%d_%d\n", label_scope, inst->label_id); return true;
	case IR_JAE: fprintf(out, "  jae .L%d_%d\n", label_scope, inst->label_id); return true;
	case IR_JB: fprintf(out, "  jb .L%d_%d\n", label_scope, inst->label_id); return true;
	case IR_JBE: fprintf(out, "  jbe .L%d_%d\n", label_scope, inst->label_id); return true;
	case IR_JS: fprintf(out, "  js .L%d_%d\n", label_scope, inst->label_id); return true;
	case IR_JNS: fprintf(out, "  jns .L%d_%d\n", label_scope, inst->label_id); return true;
	case IR_CALL_LABEL: fprintf(out, "  call .L%d_%d\n", label_scope, inst->label_id); return true;
	case IR_CALL_EXTERN: fprintf(out, "  call %s\n", inst->sym_name); return true;
	case IR_CALL_REG: fprintf(out, "  call %s\n", reg_name(inst->dst)); return true;
	case IR_RET: fputs("  ret\n", out); return true;
	case IR_LABEL: fprintf(out, ".L%d_%d:\n", label_scope, inst->label_id); return true;
	case IR_LABEL_NAMED: fprintf(out, "%s:\n", inst->sym_name); return true;
	case IR_EXTERN: fprintf(out, ".extern %s\n", inst->sym_name); return true;
	case IR_GLOBAL: fprintf(out, ".globl %s\n", inst->sym_name); return true;
	case IR_MOVQ_XMM_REG: fprintf(out, "  movq %s, %s\n", scalar_reg_name(inst->dst), reg_name(inst->src)); return true;
	case IR_MOVQ_REG_XMM: fprintf(out, "  movq %s, %s\n", reg_name(inst->dst), scalar_reg_name(inst->src)); return true;
	case IR_MOVSD_XMM_MEM: fprintf(out, "  movsd %s, QWORD PTR [rip + %s]\n", scalar_reg_name(inst->dst), inst->sym_name); return true;
	case IR_CVTSI2SD: fprintf(out, "  cvtsi2sd %s, %s\n", scalar_reg_name(inst->dst), reg_name(inst->src)); return true;
	case IR_CVTTSD2SI: fprintf(out, "  cvttsd2si %s, %s\n", reg_name(inst->dst), scalar_reg_name(inst->src)); return true;
	case IR_ADDSD: fprintf(out, "  addsd %s, %s\n", scalar_reg_name(inst->dst), scalar_reg_name(inst->src)); return true;
	case IR_SUBSD: fprintf(out, "  subsd %s, %s\n", scalar_reg_name(inst->dst), scalar_reg_name(inst->src)); return true;
	case IR_MULSD: fprintf(out, "  mulsd %s, %s\n", scalar_reg_name(inst->dst), scalar_reg_name(inst->src)); return true;
	case IR_DIVSD: fprintf(out, "  divsd %s, %s\n", scalar_reg_name(inst->dst), scalar_reg_name(inst->src)); return true;
	case IR_UCOMISD: fprintf(out, "  ucomisd %s, %s\n", scalar_reg_name(inst->dst), scalar_reg_name(inst->src)); return true;
	case IR_VFMADD132SD: fprintf(out, "  vfmadd132sd %s, %s, %s\n", scalar_reg_name(inst->dst), scalar_reg_name(inst->src), scalar_reg_name(inst->src2)); return true;
	case IR_VPADDQ: fprintf(out, "  vpaddq %s, %s, %s\n", reg_name(inst->dst), reg_name(inst->src), reg_name(inst->src2)); return true;
	case IR_VPSUBQ: fprintf(out, "  vpsubq %s, %s, %s\n", reg_name(inst->dst), reg_name(inst->src), reg_name(inst->src2)); return true;
	case IR_VPSADBW: fprintf(out, "  vpsadbw %s, %s, %s\n", reg_name(inst->dst), reg_name(inst->src), reg_name(inst->src2)); return true;
	case IR_VPXOR: fprintf(out, "  vpxor %s, %s, %s\n", reg_name(inst->dst), reg_name(inst->src), reg_name(inst->src2)); return true;
	case IR_VPAND: fprintf(out, "  vpand %s, %s, %s\n", reg_name(inst->dst), reg_name(inst->src), reg_name(inst->src2)); return true;
	case IR_VPOR: fprintf(out, "  vpor %s, %s, %s\n", reg_name(inst->dst), reg_name(inst->src), reg_name(inst->src2)); return true;
	case IR_VPMULLD: fprintf(out, "  vpmulld %s, %s, %s\n", reg_name(inst->dst), reg_name(inst->src), reg_name(inst->src2)); return true;
	case IR_VPCMPEQB: fprintf(out, "  vpcmpeqb %s, %s, %s\n", reg_name(inst->dst), reg_name(inst->src), reg_name(inst->src2)); return true;
	case IR_VPCMPEQQ: fprintf(out, "  vpcmpeqq %s, %s, %s\n", reg_name(inst->dst), reg_name(inst->src), reg_name(inst->src2)); return true;
	case IR_VPBROADCASTB: fprintf(out, "  vpbroadcastb %s, %s\n", reg_name(inst->dst), reg_name(inst->src)); return true;
	case IR_VPBROADCASTQ: fprintf(out, "  vpbroadcastq %s, %s\n", reg_name(inst->dst), reg_name(inst->src)); return true;
	case IR_VMOVDQU_LOAD:
		fprintf(out, "  vmovdqu %s, ", reg_name(inst->dst));
		write_memory(out, "YMMWORD", inst->src, inst->imm); fputc('\n', out); return true;
	case IR_VMOVDQU_STORE:
		fputs("  vmovdqu ", out); write_memory(out, "YMMWORD", inst->dst, inst->imm);
		fprintf(out, ", %s\n", reg_name(inst->src)); return true;
	case IR_VZEROUPPER: fputs("  vzeroupper\n", out); return true;
	case IR_DATA_LABEL: fprintf(out, "%s:\n", inst->sym_name); return true;
	case IR_DATA_BYTES:
		fputs("  .byte ", out);
		for (int i = 0; i < inst->data_len; ++i)
			fprintf(out, "%u%s", inst->data[i], i + 1 == inst->data_len ? "" : ", ");
		fputc('\n', out); return true;
	case IR_DATA_QWORD: fprintf(out, "  .quad 0x%016llx\n", (unsigned long long)inst->imm); return true;
	case IR_DATA_QWORD_SYM: fprintf(out, "  .quad %s\n", inst->sym_name); return true;
	case IR_BSS_LABEL: fprintf(out, "%s:\n", inst->sym_name); return true;
	case IR_BSS_RESQ: fprintf(out, "  .zero %lld\n", (long long)inst->imm * 8LL); return true;
	case IR_LEA_IDX:
		fprintf(out, "  lea %s, [%s + %s]\n", reg_name(inst->dst), reg_name(inst->src), reg_name((IrReg)inst->imm)); return true;
	default:
		if (unsupported) *unsupported = inst->IrNode;
		return false;
	}
}

bool ir_write_gas_intel(const IrBuffer *ir, FILE *out, IrOpcode *unsupported) {
	fputs(".intel_syntax noprefix\n", out);
	for (int i = 0; i < ir->extern_count; ++i)
		fprintf(out, ".extern %s\n", ir->externs[i]);
	for (int i = 0; i < ir->global_count; ++i)
		fprintf(out, ".globl %s\n", ir->globals[i]);

	fputs("\n.section .data\n", out);
	for (int i = 0; i < ir->data_count; ++i)
		if (!write_inst(&ir->data[i], out, unsupported, 0)) return false;

	fputs("\n.section .bss\n", out);
	for (int i = 0; i < ir->bss_count; ++i)
		if (!write_inst(&ir->bss[i], out, unsupported, 0)) return false;

	fputs("\n.section .text\n", out);
	int label_scope = -1;
	for (int i = 0; i < ir->text_count; ++i) {
		if (ir->text[i].IrNode == IR_LABEL_NAMED) ++label_scope;
		if (!write_inst(&ir->text[i], out, unsupported, label_scope)) return false;
	}
	return true;
}
