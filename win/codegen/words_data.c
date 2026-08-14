/* words_data.c �?数据结构�?(list-new, list-len, list-get, list-set, list-free, list-push) (IR) */
#include "codegen.h"
#include <string.h>

bool gen_word_data(IrNode *o, const char *name, size_t len) {
	(void)o;
	/* list-new */
	if (len == 8 && memcmp(name, "list-new", 8) == 0) {
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_list_new");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_INT;
		return true;
	}
	/* list-len */
	if (len == 8 && memcmp(name, "list-len", 8) == 0) {
		if (cg->type_depth > 0) cg->type_depth--;
		emit_pop_rax();
		ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_list_len");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_INT;
		return true;
	}
	/* list-get */
	if (len == 8 && memcmp(name, "list-get", 8) == 0) {
		cg->type_depth -= 2;
		emit_pop_rax(); ir_mov_reg_reg(&cg->ir, REG_RDX, REG_RAX);
		emit_pop_rax(); ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_list_get");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_INT;
		return true;
	}
	/* list-set */
	if (len == 8 && memcmp(name, "list-set", 8) == 0) {
		cg->type_depth -= 3;
		emit_pop_rax(); ir_mov_reg_reg(&cg->ir, REG_R8, REG_RAX);
		emit_pop_rax(); ir_mov_reg_reg(&cg->ir, REG_RDX, REG_RAX);
		emit_pop_rax(); ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_list_set");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		return true;
	}
	/* list-free */
	if (len == 9 && memcmp(name, "list-free", 9) == 0) {
		if (cg->type_depth > 0) cg->type_depth--;
		emit_pop_rax();
		ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_list_free");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		return true;
	}
	/* list-push */
	if (len == 9 && memcmp(name, "list-push", 9) == 0) {
		cg->type_depth -= 2;
		emit_pop_rax(); ir_mov_reg_reg(&cg->ir, REG_RDX, REG_RAX);
		emit_pop_rax(); ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_list_push");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		return true;
	}
	return false;
}
