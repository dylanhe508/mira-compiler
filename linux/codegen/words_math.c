/* words_math.c ¡ª ÊýÑ§´Ê (abs, min, max, sqrt, pow, rand, srand) (IR) */
#include "codegen.h"
#include <string.h>

bool gen_word_math(IrNode *o, const char *name, size_t len) {
	(void)o;
	if (len == 3 && memcmp(name, "abs", 3) == 0) {
		if (cg->type_depth > 0) cg->type_depth--;
		emit_pop_rax();
		ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_abs");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_INT;
		return true;
	}
	if (len == 3 && memcmp(name, "min", 3) == 0) {
		cg->type_depth -= 2;
		emit_pop_rax(); ir_mov_reg_reg(&cg->ir, REG_RDX, REG_RAX);
		emit_pop_rax(); ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_min");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_INT;
		return true;
	}
	if (len == 3 && memcmp(name, "max", 3) == 0) {
		cg->type_depth -= 2;
		emit_pop_rax(); ir_mov_reg_reg(&cg->ir, REG_RDX, REG_RAX);
		emit_pop_rax(); ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_max");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_INT;
		return true;
	}
	if (len == 4 && memcmp(name, "sqrt", 4) == 0) {
		if (cg->type_depth > 0) cg->type_depth--;
		emit_pop_rax();
		ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_f_sqrt");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_FLOAT;
		return true;
	}
	if (len == 3 && memcmp(name, "pow", 3) == 0) {
		cg->type_depth -= 2;
		emit_pop_rax(); ir_mov_reg_reg(&cg->ir, REG_RDX, REG_RAX);
		emit_pop_rax(); ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_f_pow");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_FLOAT;
		return true;
	}
	if (len == 4 && memcmp(name, "rand", 4) == 0) {
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_random");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_INT;
		return true;
	}
	if (len == 5 && memcmp(name, "srand", 5) == 0) {
		if (cg->type_depth > 0) cg->type_depth--;
		emit_pop_rax();
		ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_random_seed");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		return true;
	}
	return false;
}
