/* words_io.c â€?IO è¯?(print, ., read, input, exit, return) (IR) */
#include "codegen.h"
#include <string.h>

bool gen_word_io(IrNode *o, const char *name, size_t len) {
	(void)o;
	/* . (ä¸?print ç­‰ä»·) */
	if ((len == 1 && *name == '.') || (len == 5 && memcmp(name, "print", 5) == 0)) {
		int t = cg->type_depth > 0 ? cg->type_stack[--cg->type_depth] : TOS_INT;
		emit_pop_rax();
		if (t == TOS_FLOAT) {
			ir_lea_rip(&cg->ir, REG_RDX, "mira_float_tmp");
			ir_mov_mem_reg(&cg->ir, REG_RDX, 0, REG_RAX);
			ir_mov_reg_imm(&cg->ir, REG_ECX, 2);
		} else {
			ir_mov_reg_imm(&cg->ir, REG_ECX, t);
			ir_mov_reg_reg(&cg->ir, REG_RDX, REG_RAX);
		}
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_print");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		return true;
	}
	/* read */
	if (len == 4 && memcmp(name, "read", 4) == 0) {
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_read_int");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_INT;
		return true;
	}
	/* input */
	if (len == 5 && memcmp(name, "input", 5) == 0) {
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_input");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_STR;
		return true;
	}
	/* exit */
	if (len == 4 && memcmp(name, "exit", 4) == 0) {
		if (cg->type_depth > 0) cg->type_depth--;
		emit_pop_rax();
		ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "ExitProcess");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		return true;
	}
	/* return */
	if (len == 6 && memcmp(name, "return", 6) == 0) {
		ir_pop(&cg->ir, REG_RBP);
		ir_ret(&cg->ir);
		return true;
	}
	return false;
}
