/* words_string.c ¡ª ×Ö·û´®´Ê (IR) */
#include "codegen.h"
#include <string.h>

bool gen_word_string(IrNode *o, const char *name, size_t len) {
	(void)o;
	/* str-len */
	if (len == 7 && memcmp(name, "str-len", 7) == 0) {
		if (cg->type_depth > 0) cg->type_depth--;
		emit_pop_rax();
		ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_str_len");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_INT;
		return true;
	}
	/* str-cat */
	if (len == 7 && memcmp(name, "str-cat", 7) == 0) {
		cg->type_depth -= 2;
		emit_pop_rax(); ir_mov_reg_reg(&cg->ir, REG_RDX, REG_RAX);
		emit_pop_rax(); ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_str_concat");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_STR;
		return true;
	}
	/* str-eq */
	if (len == 6 && memcmp(name, "str-eq", 6) == 0) {
		cg->type_depth -= 2;
		emit_pop_rax(); ir_mov_reg_reg(&cg->ir, REG_RDX, REG_RAX);
		emit_pop_rax(); ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_str_eq");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_BOOL;
		return true;
	}
	/* str-sub */
	if (len == 7 && memcmp(name, "str-sub", 7) == 0) {
		cg->type_depth -= 3;
		emit_pop_rax(); ir_mov_reg_reg(&cg->ir, REG_R8, REG_RAX);
		emit_pop_rax(); ir_mov_reg_reg(&cg->ir, REG_RDX, REG_RAX);
		emit_pop_rax(); ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_str_substr");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_STR;
		return true;
	}
	/* str-find */
	if (len == 8 && memcmp(name, "str-find", 8) == 0) {
		cg->type_depth -= 2;
		emit_pop_rax(); ir_mov_reg_reg(&cg->ir, REG_RDX, REG_RAX);
		emit_pop_rax(); ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_str_contains");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_INT;
		return true;
	}
	/* str-at */
	if (len == 6 && memcmp(name, "str-at", 6) == 0) {
		cg->type_depth -= 2;
		emit_pop_rax(); ir_mov_reg_reg(&cg->ir, REG_RDX, REG_RAX);
		emit_pop_rax(); ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_str_at");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_INT;
		return true;
	}
	/* to-int */
	if (len == 6 && memcmp(name, "to-int", 6) == 0) {
		if (cg->type_depth > 0) cg->type_depth--;
		emit_pop_rax();
		ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_str_to_int");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_INT;
		return true;
	}
	/* to-str */
	if (len == 6 && memcmp(name, "to-str", 6) == 0) {
		if (cg->type_depth > 0) cg->type_depth--;
		emit_pop_rax();
		ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_to_str");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_STR;
		return true;
	}
	/* to-float */
	if (len == 8 && memcmp(name, "to-float", 8) == 0) {
		if (cg->type_depth > 0) cg->type_depth--;
		emit_pop_rax();
		ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_int_to_float");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_FLOAT;
		return true;
	}
	/* cr (print newline) */
	if (len == 2 && name[0] == 'c' && name[1] == 'r') {
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_cr");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		return true;
	}
	/* nl (same) */
	if (len == 2 && name[0] == 'n' && name[1] == 'l') {
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_cr");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		return true;
	}
	return false;
}
