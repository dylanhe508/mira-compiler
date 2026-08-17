/* ir_dump.c 鈥?IR 鍙鏂囨湰杈撳嚭
 *
 * 灏?IrBuffer 鎵撳嵃涓虹被浼?NASM 鐨勪汉绫诲彲璇绘牸寮忥紝鐢ㄤ簬 -S 鍜岃皟璇曘€? */
#include "ir.h"
#include <stdio.h>
#include <string.h>

static const char *reg_names[] = {
	"rax","rcx","rdx","rbx","rsp","rbp","rsi","rdi",
	"r8","r9","r10","r11","r12","r13","r14","r15",
	"eax","ecx","edx","ebx","esp","ebp","esi","edi",
	"r8d","r9d","r10d","r11d","r12d","r13d","r14d","r15d",
	"al","cl","dl","bl",
	"xmm0","xmm1",
	"ymm0","ymm1","ymm2","ymm3","ymm4","ymm5","ymm6","ymm7",
	"ymm8","ymm9","ymm10","ymm11","ymm12","ymm13","ymm14","ymm15",
	"none"
};

static const char *rn(IrReg r) {
	if (r >= 0 && r <= REG_NONE) return reg_names[r];
	return "???";
}

static bool dump_text(const IrInst *insts, int count, FILE *out,
		IrOpcode *unsupported) {
	fprintf(out, "\n; === .text ===\n");
	for (int i = 0; i < count; i++) {
		const IrInst *inst = &insts[i];
		switch (inst->IrNode) {
		case IR_MOV_REG_REG:    fprintf(out, "  mov %s, %s\n", rn(inst->dst), rn(inst->src)); break;
		case IR_MOV_REG_IMM:    fprintf(out, "  mov %s, %lld\n", rn(inst->dst), (long long)inst->imm); break;
		case IR_MOV_REG_MEM:
			if (inst->imm) fprintf(out, "  mov %s, [%s + %lld]\n", rn(inst->dst), rn(inst->src), (long long)inst->imm);
			else fprintf(out, "  mov %s, [%s]\n", rn(inst->dst), rn(inst->src));
			break;
		case IR_MOV_MEM_REG:
			if (inst->imm) fprintf(out, "  mov [%s + %lld], %s\n", rn(inst->dst), (long long)inst->imm, rn(inst->src));
			else fprintf(out, "  mov [%s], %s\n", rn(inst->dst), rn(inst->src));
			break;
		case IR_MOV_MEM_IMM:
			if (inst->imm) fprintf(out, "  mov qword [%s + %lld], %lld\n", rn(inst->dst), (long long)inst->imm, (long long)inst->extra_imm);
			else fprintf(out, "  mov qword [%s], %lld\n", rn(inst->dst), (long long)inst->extra_imm);
			break;
		case IR_MOV_MEM8_REG:
			if (inst->imm) fprintf(out, "  mov byte [%s + %lld], %s\n", rn(inst->dst), (long long)inst->imm, rn(inst->src));
			else fprintf(out, "  mov byte [%s], %s\n", rn(inst->dst), rn(inst->src));
			break;
		case IR_MOVZX_REG_MEM8:
			if (inst->imm) fprintf(out, "  movzx %s, byte [%s + %lld]\n", rn(inst->dst), rn(inst->src), (long long)inst->imm);
			else fprintf(out, "  movzx %s, byte [%s]\n", rn(inst->dst), rn(inst->src));
			break;
		case IR_LEA:
			if (inst->imm) fprintf(out, "  lea %s, [%s + %lld]\n", rn(inst->dst), rn(inst->src), (long long)inst->imm);
			else fprintf(out, "  lea %s, [%s]\n", rn(inst->dst), rn(inst->src));
			break;
		case IR_LEA_RIP:
			if (inst->sym_name) fprintf(out, "  lea %s, [rel %s]\n", rn(inst->dst), inst->sym_name);
			else fprintf(out, "  lea %s, [rel .L%d]\n", rn(inst->dst), inst->label_id);
			break;
			case IR_LEA_IDX:
			fprintf(out, "  lea %s, [%s + %s]\n", rn(inst->dst), rn(inst->src), rn((IrReg)inst->imm));
			break;
		case IR_PUSH_REG:   fprintf(out, "  push %s\n", rn(inst->dst)); break;
		case IR_POP_REG:    fprintf(out, "  pop %s\n", rn(inst->dst)); break;
		case IR_ADD_REG_REG: fprintf(out, "  add %s, %s\n", rn(inst->dst), rn(inst->src)); break;
		case IR_ADD_REG_IMM: fprintf(out, "  add %s, %lld\n", rn(inst->dst), (long long)inst->imm); break;
		case IR_ADD_MEM_IMM:
			if (inst->imm) fprintf(out, "  add qword [%s + %lld], %lld\n", rn(inst->dst), (long long)inst->imm, (long long)inst->extra_imm);
			else fprintf(out, "  add qword [%s], %lld\n", rn(inst->dst), (long long)inst->extra_imm);
			break;
		case IR_SUB_REG_REG: fprintf(out, "  sub %s, %s\n", rn(inst->dst), rn(inst->src)); break;
		case IR_SUB_REG_IMM: fprintf(out, "  sub %s, %lld\n", rn(inst->dst), (long long)inst->imm); break;
		case IR_IMUL_REG_REG: fprintf(out, "  imul %s, %s\n", rn(inst->dst), rn(inst->src)); break;
		case IR_IMUL_REG_IMM: fprintf(out, "  imul %s, %s, %lld\n", rn(inst->dst), rn(inst->src), (long long)inst->imm); break;
		case IR_IMUL_WIDE_REG: fprintf(out, "  imul %s\n", rn(inst->src)); break;
		case IR_MUL_WIDE_REG: fprintf(out, "  mul %s\n", rn(inst->src)); break;
		case IR_ALIGN32: fprintf(out, "  align 32\n"); break;
		case IR_IDIV_REG:    fprintf(out, "  idiv %s\n", rn(inst->src)); break;
		case IR_INC_REG:     fprintf(out, "  inc %s\n", rn(inst->dst)); break;
		case IR_INC_MEM:
			if (inst->imm) fprintf(out, "  inc qword [%s + %lld]\n", rn(inst->dst), (long long)inst->imm);
			else fprintf(out, "  inc qword [%s]\n", rn(inst->dst));
			break;
		case IR_NEG_REG:     fprintf(out, "  neg %s\n", rn(inst->dst)); break;
		case IR_CQO:         fprintf(out, "  cqo\n"); break;
		case IR_XOR_REG_REG: fprintf(out, "  xor %s, %s\n", rn(inst->dst), rn(inst->src)); break;
		case IR_XOR_REG_IMM: fprintf(out, "  xor %s, %lld\n", rn(inst->dst), (long long)inst->imm); break;
		case IR_AND_REG_REG: fprintf(out, "  and %s, %s\n", rn(inst->dst), rn(inst->src)); break;
		case IR_AND_REG_IMM: fprintf(out, "  and %s, %lld\n", rn(inst->dst), (long long)inst->imm); break;
		case IR_OR_REG_REG:  fprintf(out, "  or %s, %s\n", rn(inst->dst), rn(inst->src)); break;
		case IR_SHL_REG_IMM: fprintf(out, "  shl %s, %lld\n", rn(inst->dst), (long long)inst->imm); break;
		case IR_SHR_REG_IMM: fprintf(out, "  shr %s, %lld\n", rn(inst->dst), (long long)inst->imm); break;
		case IR_SAR_REG_IMM: fprintf(out, "  sar %s, %lld\n", rn(inst->dst), (long long)inst->imm); break;
		case IR_SHL_REG_CL: fprintf(out, "  shl %s, cl\n", rn(inst->dst)); break;
		case IR_SHR_REG_CL: fprintf(out, "  shr %s, cl\n", rn(inst->dst)); break;
		case IR_SAR_REG_CL: fprintf(out, "  sar %s, cl\n", rn(inst->dst)); break;
		case IR_NOT_REG:     fprintf(out, "  not %s\n", rn(inst->dst)); break;
		case IR_CMP_REG_REG: fprintf(out, "  cmp %s, %s\n", rn(inst->dst), rn(inst->src)); break;
		case IR_CMP_REG_IMM: fprintf(out, "  cmp %s, %lld\n", rn(inst->dst), (long long)inst->imm); break;
		case IR_TEST_REG_REG: fprintf(out, "  test %s, %s\n", rn(inst->dst), rn(inst->src)); break;
		case IR_SETE:    fprintf(out, "  sete %s\n", rn(inst->dst)); break;
		case IR_SETNE:   fprintf(out, "  setne %s\n", rn(inst->dst)); break;
		case IR_SETL:    fprintf(out, "  setl %s\n", rn(inst->dst)); break;
		case IR_SETLE:   fprintf(out, "  setle %s\n", rn(inst->dst)); break;
		case IR_SETG:    fprintf(out, "  setg %s\n", rn(inst->dst)); break;
		case IR_SETGE:   fprintf(out, "  setge %s\n", rn(inst->dst)); break;
		case IR_SETA:    fprintf(out, "  seta %s\n", rn(inst->dst)); break;
		case IR_SETAE:   fprintf(out, "  setae %s\n", rn(inst->dst)); break;
		case IR_SETB:    fprintf(out, "  setb %s\n", rn(inst->dst)); break;
		case IR_SETBE:   fprintf(out, "  setbe %s\n", rn(inst->dst)); break;
		case IR_SETZ:    fprintf(out, "  setz %s\n", rn(inst->dst)); break;
		case IR_SETNP:   fprintf(out, "  setnp %s\n", rn(inst->dst)); break;
		case IR_MOVZX_REG8: fprintf(out, "  movzx %s, %s\n", rn(inst->dst), rn(inst->src)); break;
		case IR_CMOVE:  fprintf(out, "  cmove %s, %s\n", rn(inst->dst), rn(inst->src)); break;
		case IR_CMOVNE: fprintf(out, "  cmovne %s, %s\n", rn(inst->dst), rn(inst->src)); break;
		case IR_CMOVL:  fprintf(out, "  cmovl %s, %s\n", rn(inst->dst), rn(inst->src)); break;
		case IR_CMOVLE: fprintf(out, "  cmovle %s, %s\n", rn(inst->dst), rn(inst->src)); break;
		case IR_CMOVG:  fprintf(out, "  cmovg %s, %s\n", rn(inst->dst), rn(inst->src)); break;
		case IR_CMOVGE: fprintf(out, "  cmovge %s, %s\n", rn(inst->dst), rn(inst->src)); break;
		case IR_CMOVA:  fprintf(out, "  cmova %s, %s\n", rn(inst->dst), rn(inst->src)); break;
		case IR_CMOVAE: fprintf(out, "  cmovae %s, %s\n", rn(inst->dst), rn(inst->src)); break;
		case IR_CMOVB:  fprintf(out, "  cmovb %s, %s\n", rn(inst->dst), rn(inst->src)); break;
		case IR_CMOVBE: fprintf(out, "  cmovbe %s, %s\n", rn(inst->dst), rn(inst->src)); break;
		case IR_JMP:     fprintf(out, "  jmp .L%d\n", inst->label_id); break;
		case IR_JMP_EXTERN: fprintf(out, "  jmp %s\n", inst->sym_name); break;
		case IR_JE:      fprintf(out, "  je .L%d\n", inst->label_id); break;
		case IR_JNE:     fprintf(out, "  jne .L%d\n", inst->label_id); break;
		case IR_JZ:      fprintf(out, "  jz .L%d\n", inst->label_id); break;
		case IR_JNZ:     fprintf(out, "  jnz .L%d\n", inst->label_id); break;
		case IR_JG:      fprintf(out, "  jg .L%d\n", inst->label_id); break;
		case IR_JGE:     fprintf(out, "  jge .L%d\n", inst->label_id); break;
		case IR_JL:      fprintf(out, "  jl .L%d\n", inst->label_id); break;
		case IR_JLE:     fprintf(out, "  jle .L%d\n", inst->label_id); break;
		case IR_JA:      fprintf(out, "  ja .L%d\n", inst->label_id); break;
		case IR_JAE:     fprintf(out, "  jae .L%d\n", inst->label_id); break;
		case IR_JB:      fprintf(out, "  jb .L%d\n", inst->label_id); break;
		case IR_JBE:     fprintf(out, "  jbe .L%d\n", inst->label_id); break;
		case IR_JS:      fprintf(out, "  js .L%d\n", inst->label_id); break;
		case IR_JNS:     fprintf(out, "  jns .L%d\n", inst->label_id); break;
		case IR_CALL_LABEL:  fprintf(out, "  call .L%d\n", inst->label_id); break;
		case IR_CALL_EXTERN: fprintf(out, "  call %s\n", inst->sym_name); break;
		case IR_CALL_REG:    fprintf(out, "  call %s\n", rn(inst->dst)); break;
		case IR_RET:     fprintf(out, "  ret\n"); break;
		case IR_LABEL:   fprintf(out, ".L%d:\n", inst->label_id); break;
		case IR_LABEL_NAMED: fprintf(out, "%s:\n", inst->sym_name); break;
		case IR_EXTERN:  fprintf(out, "extern %s\n", inst->sym_name ? inst->sym_name : "?"); break;
		case IR_GLOBAL:  fprintf(out, "global %s\n", inst->sym_name ? inst->sym_name : "?"); break;
		/* SSE */
		case IR_MOVQ_XMM_REG: fprintf(out, "  movq %s, %s\n", rn(inst->dst), rn(inst->src)); break;
		case IR_MOVQ_REG_XMM: fprintf(out, "  movq %s, %s\n", rn(inst->dst), rn(inst->src)); break;
		case IR_MOVSD_XMM_MEM: fprintf(out, "  movsd %s, [rel %s]\n", rn(inst->dst), inst->sym_name ? inst->sym_name : "?"); break;
		case IR_CVTSI2SD:  fprintf(out, "  cvtsi2sd %s, %s\n", rn(inst->dst), rn(inst->src)); break;
		case IR_CVTTSD2SI: fprintf(out, "  cvttsd2si %s, %s\n", rn(inst->dst), rn(inst->src)); break;
		case IR_ADDSD:  fprintf(out, "  addsd %s, %s\n", rn(inst->dst), rn(inst->src)); break;
		case IR_SUBSD:  fprintf(out, "  subsd %s, %s\n", rn(inst->dst), rn(inst->src)); break;
		case IR_MULSD:  fprintf(out, "  mulsd %s, %s\n", rn(inst->dst), rn(inst->src)); break;
		case IR_DIVSD:  fprintf(out, "  divsd %s, %s\n", rn(inst->dst), rn(inst->src)); break;
		case IR_UCOMISD: fprintf(out, "  ucomisd %s, %s\n", rn(inst->dst), rn(inst->src)); break;
		case IR_VFMADD132SD: fprintf(out, "  vfmadd132sd %s, %s, %s\n", rn(inst->dst), rn(inst->src), rn(inst->src2)); break;
		case IR_VPADDQ: fprintf(out, "  vpaddq %s, %s, %s\n", rn(inst->dst), rn(inst->src), rn(inst->src2)); break;
		case IR_VPSADBW: fprintf(out, "  vpsadbw %s, %s, %s\n", rn(inst->dst), rn(inst->src), rn(inst->src2)); break;
		case IR_VPSUBQ: fprintf(out, "  vpsubq %s, %s, %s\n", rn(inst->dst), rn(inst->src), rn(inst->src2)); break;
		case IR_VPXOR: fprintf(out, "  vpxor %s, %s, %s\n", rn(inst->dst), rn(inst->src), rn(inst->src2)); break;
		case IR_VPAND: fprintf(out, "  vpand %s, %s, %s\n", rn(inst->dst), rn(inst->src), rn(inst->src2)); break;
		case IR_VPOR: fprintf(out, "  vpor %s, %s, %s\n", rn(inst->dst), rn(inst->src), rn(inst->src2)); break;
		case IR_VPMULLD: fprintf(out, "  vpmulld %s, %s, %s\n", rn(inst->dst), rn(inst->src), rn(inst->src2)); break;
		case IR_VPCMPEQB: fprintf(out, "  vpcmpeqb %s, %s, %s\n", rn(inst->dst), rn(inst->src), rn(inst->src2)); break;
		case IR_VPCMPEQQ: fprintf(out, "  vpcmpeqq %s, %s, %s\n", rn(inst->dst), rn(inst->src), rn(inst->src2)); break;
		case IR_VPBROADCASTB: fprintf(out, "  vpbroadcastb %s, %s\n", rn(inst->dst), rn(inst->src)); break;
		case IR_VPBROADCASTQ: fprintf(out, "  vpbroadcastq %s, %s\n", rn(inst->dst), rn(inst->src)); break;
		case IR_VMOVDQU_LOAD: fprintf(out, "  vmovdqu %s, [%s + %lld]\n", rn(inst->dst), rn(inst->src), (long long)inst->imm); break;
		case IR_VMOVDQU_STORE: fprintf(out, "  vmovdqu [%s + %lld], %s\n", rn(inst->dst), (long long)inst->imm, rn(inst->src)); break;
		case IR_VZEROUPPER: fprintf(out, "  vzeroupper\n"); break;
		default:
			if (unsupported) *unsupported = inst->IrNode;
			return false;
		}
	}
	return true;
}

bool ir_dump(const IrBuffer *ir, FILE *out, IrOpcode *unsupported) {
	fprintf(out, ";; Mira IR dump\n");

	/* externs */
	for (int i = 0; i < ir->extern_count; i++)
		fprintf(out, "extern %s\n", ir->externs[i]);

	/* globals */
	for (int i = 0; i < ir->global_count; i++)
		fprintf(out, "global %s\n", ir->globals[i]);

	/* .data */
	if (ir->data_count > 0) {
		fprintf(out, "\n; === .data ===\n");
		fprintf(out, "section .data\n");
		for (int i = 0; i < ir->data_count; i++) {
			const IrInst *inst = &ir->data[i];
			switch (inst->IrNode) {
			case IR_DATA_LABEL:
				fprintf(out, "%s:\n", inst->sym_name);
				break;
			case IR_DATA_BYTES:
				fprintf(out, "  db ");
				for (int j = 0; j < inst->data_len; j++)
					fprintf(out, "%u%s", inst->data[j], j + 1 < inst->data_len ? "," : "");
				fprintf(out, "\n");
				break;
			case IR_DATA_QWORD: {
				double d;
				memcpy(&d, &inst->imm, sizeof(double));
				if (inst->imm > 0x7FF0000000000000LL || (inst->imm != 0 && inst->imm < 1000000 && inst->imm > -1000000))
					fprintf(out, "  dq %lld\n", (long long)inst->imm);
				else
					fprintf(out, "  dq 0x%016llx  ; %.17e\n", (unsigned long long)inst->imm, d);
				break;
			}
			case IR_DATA_QWORD_SYM:
				fprintf(out, "  dq %s\n", inst->sym_name);
				break;
			default:
				if (unsupported) *unsupported = inst->IrNode;
				return false;
			}
		}
	}

	/* .bss */
	if (ir->bss_count > 0) {
		fprintf(out, "\n; === .bss ===\n");
		fprintf(out, "section .bss\n");
		for (int i = 0; i < ir->bss_count; i++) {
			const IrInst *inst = &ir->bss[i];
			if (inst->IrNode == IR_BSS_LABEL)
				fprintf(out, "%s:\n", inst->sym_name);
			else if (inst->IrNode == IR_BSS_RESQ)
				fprintf(out, "  resq %lld\n", (long long)inst->imm);
			else {
				if (unsupported) *unsupported = inst->IrNode;
				return false;
			}
		}
	}

	/* .text */
	fprintf(out, "\nsection .text\n");
	return dump_text(ir->text, ir->text_count, out, unsupported);
}

