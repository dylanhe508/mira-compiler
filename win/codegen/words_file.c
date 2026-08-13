/* words_file.c �?文件操作�?(IR) */
#include "codegen.h"
#include <string.h>

bool gen_word_file(IrNode *o, const char *name, size_t len) {
	(void)o;
	/* file-read */
	if (len == 9 && memcmp(name, "file-read", 9) == 0) {
		if (cg->type_depth > 0) cg->type_depth--;
		emit_pop_rax();
		ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_file_read");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_STR;
		return true;
	}
	/* file-write */
	if (len == 10 && memcmp(name, "file-write", 10) == 0) {
		cg->type_depth -= 2;
		emit_pop_rax(); ir_mov_reg_reg(&cg->ir, REG_RDX, REG_RAX);
		emit_pop_rax(); ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_file_write");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		return true;
	}
	/* file-append */
	if (len == 11 && memcmp(name, "file-append", 11) == 0) {
		cg->type_depth -= 2;
		emit_pop_rax(); ir_mov_reg_reg(&cg->ir, REG_RDX, REG_RAX);
		emit_pop_rax(); ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_file_append");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		return true;
	}
	/* file-exists */
	if (len == 11 && memcmp(name, "file-exists", 11) == 0) {
		if (cg->type_depth > 0) cg->type_depth--;
		emit_pop_rax();
		ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_file_exists");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_BOOL;
		return true;
	}
	/* file-delete */
	if (len == 11 && memcmp(name, "file-delete", 11) == 0) {
		if (cg->type_depth > 0) cg->type_depth--;
		emit_pop_rax();
		ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_file_delete");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		return true;
	}
	return false;
}
