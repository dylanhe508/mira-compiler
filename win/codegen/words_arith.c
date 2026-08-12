/* words_arith.c �?算术 (+, -, *, /, %, f+, f-, f*, f/) (IR) */
#include "codegen.h"
#include <string.h>

bool gen_word_arith(IrNode *o, const char *name, size_t len) {
	(void)o;
	int t1 = cg->type_depth >= 2 ? cg->type_stack[cg->type_depth - 2] : TOS_INT;
	int t2 = cg->type_depth >= 1 ? cg->type_stack[cg->type_depth - 1] : TOS_INT;
	int both_int = (t1 == TOS_INT && t2 == TOS_INT);

	/* + */
	if (len == 1 && *name == '+') {
		cg->type_depth -= 2;
		emit_pop_rax();
		ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		emit_pop_rax();
		if (both_int) {
			ir_add_reg_reg(&cg->ir, REG_RAX, REG_RCX);
			emit_push_rax();
			cg->type_stack[cg->type_depth++] = TOS_INT;
		} else {
			if (t2 == TOS_INT) ir_cvtsi2sd(&cg->ir, REG_XMM1, REG_RCX);
			else ir_movq_xmm_reg(&cg->ir, REG_XMM1, REG_RCX);
			if (t1 == TOS_INT) ir_cvtsi2sd(&cg->ir, REG_XMM0, REG_RAX);
			else ir_movq_xmm_reg(&cg->ir, REG_XMM0, REG_RAX);
			ir_addsd(&cg->ir, REG_XMM0, REG_XMM1);
			ir_movq_reg_xmm(&cg->ir, REG_RAX, REG_XMM0);
			emit_push_rax();
			cg->type_stack[cg->type_depth++] = TOS_FLOAT;
		}
		return true;
	}
	/* - */
	if (len == 1 && *name == '-') {
		cg->type_depth -= 2;
		emit_pop_rax();
		ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		emit_pop_rax();
		if (both_int) {
			ir_sub_reg_reg(&cg->ir, REG_RAX, REG_RCX);
			emit_push_rax();
			cg->type_stack[cg->type_depth++] = TOS_INT;
		} else {
			if (t2 == TOS_INT) ir_cvtsi2sd(&cg->ir, REG_XMM1, REG_RCX);
			else ir_movq_xmm_reg(&cg->ir, REG_XMM1, REG_RCX);
			if (t1 == TOS_INT) ir_cvtsi2sd(&cg->ir, REG_XMM0, REG_RAX);
			else ir_movq_xmm_reg(&cg->ir, REG_XMM0, REG_RAX);
			ir_subsd(&cg->ir, REG_XMM0, REG_XMM1);
			ir_movq_reg_xmm(&cg->ir, REG_RAX, REG_XMM0);
			emit_push_rax();
			cg->type_stack[cg->type_depth++] = TOS_FLOAT;
		}
		return true;
	}
	/* * */
	if (len == 1 && *name == '*') {
		cg->type_depth -= 2;
		emit_pop_rax();
		ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		emit_pop_rax();
		if (both_int) {
			ir_imul_reg_reg(&cg->ir, REG_RAX, REG_RCX);
			emit_push_rax();
			cg->type_stack[cg->type_depth++] = TOS_INT;
		} else {
			if (t2 == TOS_INT) ir_cvtsi2sd(&cg->ir, REG_XMM1, REG_RCX);
			else ir_movq_xmm_reg(&cg->ir, REG_XMM1, REG_RCX);
			if (t1 == TOS_INT) ir_cvtsi2sd(&cg->ir, REG_XMM0, REG_RAX);
			else ir_movq_xmm_reg(&cg->ir, REG_XMM0, REG_RAX);
			ir_mulsd(&cg->ir, REG_XMM0, REG_XMM1);
			ir_movq_reg_xmm(&cg->ir, REG_RAX, REG_XMM0);
			emit_push_rax();
			cg->type_stack[cg->type_depth++] = TOS_FLOAT;
		}
		return true;
	}
	/* / */
	if (len == 1 && *name == '/') {
		cg->type_depth -= 2;
		emit_pop_rax();
		ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		emit_pop_rax();
		if (both_int) {
			ir_cqo(&cg->ir);
			ir_idiv(&cg->ir, REG_RCX);
			emit_push_rax();
			cg->type_stack[cg->type_depth++] = TOS_INT;
		} else {
			if (t2 == TOS_INT) ir_cvtsi2sd(&cg->ir, REG_XMM1, REG_RCX);
			else ir_movq_xmm_reg(&cg->ir, REG_XMM1, REG_RCX);
			if (t1 == TOS_INT) ir_cvtsi2sd(&cg->ir, REG_XMM0, REG_RAX);
			else ir_movq_xmm_reg(&cg->ir, REG_XMM0, REG_RAX);
			ir_divsd(&cg->ir, REG_XMM0, REG_XMM1);
			ir_movq_reg_xmm(&cg->ir, REG_RAX, REG_XMM0);
			emit_push_rax();
			cg->type_stack[cg->type_depth++] = TOS_FLOAT;
		}
		return true;
	}
	/* % */
	if (len == 1 && *name == '%') {
		cg->type_depth -= 2;
		emit_pop_rax();
		ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		emit_pop_rax();
		ir_cqo(&cg->ir);
		ir_idiv(&cg->ir, REG_RCX);
		ir_mov_reg_reg(&cg->ir, REG_RAX, REG_RDX);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_INT;
		return true;
	}
	/* f+ f- f* f/ �?强制浮点 */
	if (len == 2 && name[0] == 'f' && (name[1]=='+' || name[1]=='-' || name[1]=='*' || name[1]=='/')) {
		cg->type_depth -= 2;
		emit_pop_rax();
		ir_movq_xmm_reg(&cg->ir, REG_XMM1, REG_RAX);
		emit_pop_rax();
		ir_movq_xmm_reg(&cg->ir, REG_XMM0, REG_RAX);
		if (name[1]=='+') ir_addsd(&cg->ir, REG_XMM0, REG_XMM1);
		else if (name[1]=='-') ir_subsd(&cg->ir, REG_XMM0, REG_XMM1);
		else if (name[1]=='*') ir_mulsd(&cg->ir, REG_XMM0, REG_XMM1);
		else ir_divsd(&cg->ir, REG_XMM0, REG_XMM1);
		ir_movq_reg_xmm(&cg->ir, REG_RAX, REG_XMM0);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_FLOAT;
		return true;
	}
	/* neg */
	if (len == 3 && memcmp(name, "neg", 3) == 0) {
		if (cg->type_depth > 0) cg->type_depth--;
		emit_pop_rax();
		ir_neg(&cg->ir, REG_RAX);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_INT;
		return true;
	}
	return false;
}
