/* words_compare.c �?比较/逻辑 (IR) */
#include "codegen.h"
#include <string.h>

bool gen_word_compare(IrNode *o, const char *name, size_t len) {
	(void)o;
	int t1 = cg->type_depth >= 2 ? cg->type_stack[cg->type_depth - 2] : TOS_INT;
	int t2 = cg->type_depth >= 1 ? cg->type_stack[cg->type_depth - 1] : TOS_INT;
	int both_int = (t1 == TOS_INT && t2 == TOS_INT);

	/* = */
	if (len == 1 && *name == '=') {
		cg->type_depth -= 2;
		emit_pop_rax();
		ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		emit_pop_rax();
		if (both_int) {
			ir_cmp_reg_reg(&cg->ir, REG_RAX, REG_RCX);
			ir_setcc(&cg->ir, IR_SETE, REG_AL);
		} else {
			if (t2 == TOS_INT) ir_cvtsi2sd(&cg->ir, REG_XMM1, REG_RCX);
			else ir_movq_xmm_reg(&cg->ir, REG_XMM1, REG_RCX);
			if (t1 == TOS_INT) ir_cvtsi2sd(&cg->ir, REG_XMM0, REG_RAX);
			else ir_movq_xmm_reg(&cg->ir, REG_XMM0, REG_RAX);
			ir_ucomisd(&cg->ir, REG_XMM0, REG_XMM1);
			ir_setcc(&cg->ir, IR_SETZ, REG_AL);
			ir_setcc(&cg->ir, IR_SETNP, REG_CL);
			ir_and_reg_reg(&cg->ir, REG_AL, REG_CL);
		}
		ir_movzx_reg8(&cg->ir, REG_EAX, REG_AL);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_BOOL;
		return true;
	}
	/* != */
	if (len == 2 && name[0] == '!' && name[1] == '=') {
		cg->type_depth -= 2;
		emit_pop_rax();
		ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		emit_pop_rax();
		if (both_int) {
			ir_cmp_reg_reg(&cg->ir, REG_RAX, REG_RCX);
			ir_setcc(&cg->ir, IR_SETNE, REG_AL);
		} else {
			if (t2 == TOS_INT) ir_cvtsi2sd(&cg->ir, REG_XMM1, REG_RCX);
			else ir_movq_xmm_reg(&cg->ir, REG_XMM1, REG_RCX);
			if (t1 == TOS_INT) ir_cvtsi2sd(&cg->ir, REG_XMM0, REG_RAX);
			else ir_movq_xmm_reg(&cg->ir, REG_XMM0, REG_RAX);
			ir_ucomisd(&cg->ir, REG_XMM0, REG_XMM1);
			ir_setcc(&cg->ir, IR_SETZ, REG_AL);
			ir_setcc(&cg->ir, IR_SETNP, REG_CL);
			ir_and_reg_reg(&cg->ir, REG_AL, REG_CL);
			ir_xor_reg_imm(&cg->ir, REG_AL, 1);
		}
		ir_movzx_reg8(&cg->ir, REG_EAX, REG_AL);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_BOOL;
		return true;
	}
	/* < > <= >= */
	{
		IrOpcode setop = 0;
		bool is_cmp = false;
		if (len == 1 && *name == '<')  { setop = IR_SETL;  is_cmp = true; }
		if (len == 1 && *name == '>')  { setop = IR_SETG;  is_cmp = true; }
		if (len == 2 && name[0] == '<' && name[1] == '=') { setop = IR_SETLE; is_cmp = true; }
		if (len == 2 && name[0] == '>' && name[1] == '=') { setop = IR_SETGE; is_cmp = true; }
		if (is_cmp) {
			cg->type_depth -= 2;
			emit_pop_rax();
			ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
			emit_pop_rax();
			if (both_int) {
				ir_cmp_reg_reg(&cg->ir, REG_RAX, REG_RCX);
				ir_setcc(&cg->ir, setop, REG_AL);
			} else {
				/* float compare via ucomisd */
				if (t2 == TOS_INT) ir_cvtsi2sd(&cg->ir, REG_XMM1, REG_RCX);
				else ir_movq_xmm_reg(&cg->ir, REG_XMM1, REG_RCX);
				if (t1 == TOS_INT) ir_cvtsi2sd(&cg->ir, REG_XMM0, REG_RAX);
				else ir_movq_xmm_reg(&cg->ir, REG_XMM0, REG_RAX);
				ir_ucomisd(&cg->ir, REG_XMM0, REG_XMM1);
				/* ucomisd sets CF/ZF, use unsigned conditions */
				IrOpcode fsetop = IR_SETA;
				if (setop == IR_SETL) fsetop = IR_SETB;
				if (setop == IR_SETLE) fsetop = IR_SETBE;
				if (setop == IR_SETG) fsetop = IR_SETA;
				if (setop == IR_SETGE) fsetop = IR_SETAE;
				ir_setcc(&cg->ir, fsetop, REG_AL);
			}
			ir_movzx_reg8(&cg->ir, REG_EAX, REG_AL);
			emit_push_rax();
			cg->type_stack[cg->type_depth++] = TOS_BOOL;
			return true;
		}
	}
	/* and */
	if (len == 3 && memcmp(name, "and", 3) == 0) {
		cg->type_depth -= 2;
		emit_pop_rax();
		ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		emit_pop_rax();
		ir_and_reg_reg(&cg->ir, REG_RAX, REG_RCX);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_BOOL;
		return true;
	}
	/* or */
	if (len == 2 && memcmp(name, "or", 2) == 0) {
		cg->type_depth -= 2;
		emit_pop_rax();
		ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		emit_pop_rax();
		ir_or_reg_reg(&cg->ir, REG_RAX, REG_RCX);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_BOOL;
		return true;
	}
	/* xor */
	if (len == 3 && memcmp(name, "xor", 3) == 0) {
		cg->type_depth -= 2;
		emit_pop_rax();
		ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		emit_pop_rax();
		ir_xor_reg_reg(&cg->ir, REG_RAX, REG_RCX);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_BOOL;
		return true;
	}
	/* not */
	if (len == 3 && memcmp(name, "not", 3) == 0) {
		if (cg->type_depth > 0) cg->type_depth--;
		emit_pop_rax();
		ir_test_reg_reg(&cg->ir, REG_RAX, REG_RAX);
		ir_setcc(&cg->ir, IR_SETE, REG_AL);
		ir_movzx_reg8(&cg->ir, REG_EAX, REG_AL);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_BOOL;
		return true;
	}
	return false;
}
