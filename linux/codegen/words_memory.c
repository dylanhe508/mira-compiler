/* words_memory.c â€?å†…å­˜å­˜å– (@, !, c@, c!, allocate, free, move, erase, dump, md) (IR) */
#include "codegen.h"
#include <string.h>

bool gen_word_memory(IrNode *o, const char *name, size_t len) {
	(void)o;
	/* @ â€?fetch */
	if (len == 1 && *name == '@') {
		int t = cg->type_depth > 0 ? cg->type_stack[cg->type_depth - 1] : TOS_INT;
		cg->type_depth--;
		emit_pop_rax();
		ir_mov_reg_mem(&cg->ir, REG_RAX, REG_RAX, 0);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = t;
		return true;
	}
	/* ! â€?store */
	if (len == 1 && *name == '!') {
		if (cg->type_depth >= 2) cg->type_depth -= 2;
		else cg->type_depth = 0;
		emit_pop_rax();
		ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		if (cg->lIR_var_slot >= 0 && cg->lIR_var_slot < cg->var_types_cap)
			cg->var_types[cg->lIR_var_slot] = TOS_INT;
		emit_pop_rax();
		ir_mov_mem_reg(&cg->ir, REG_RCX, 0, REG_RAX);
		return true;
	}
	/* c! */
	if (len == 2 && name[0] == 'c' && name[1] == '!') {
		(void)cg->type_stack[--cg->type_depth];
		int value_type = cg->type_stack[--cg->type_depth];
		emit_pop_rax();
		ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		emit_pop_rax();
		ir_mov_mem8_reg(&cg->ir, REG_RCX, 0, REG_AL);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = value_type;
		return true;
	}
	/* c@ */
	if (len == 2 && name[0] == 'c' && name[1] == '@') {
		if (cg->type_depth > 0) cg->type_depth--;
		emit_pop_rax();
		ir_movzx_reg_mem8(&cg->ir, REG_EAX, REG_RAX, 0);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_INT;
		return true;
	}
	/* allocate */
	if (len == 8 && memcmp(name, "allocate", 8) == 0) {
		if (cg->type_depth > 0) cg->type_depth--;
		emit_pop_rax();
		ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mem_alloc");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_INT;
		return true;
	}
	/* free */
	if (len == 4 && memcmp(name, "free", 4) == 0) {
		if (cg->type_depth > 0) cg->type_depth--;
		emit_pop_rax();
		ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mem_free");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		return true;
	}
	/* move */
	if (len == 4 && memcmp(name, "move", 4) == 0) {
		cg->type_depth -= 3;
		emit_pop_rax(); ir_mov_reg_reg(&cg->ir, REG_R8, REG_RAX);
		emit_pop_rax(); ir_mov_reg_reg(&cg->ir, REG_RDX, REG_RAX);
		emit_pop_rax(); ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mem_move");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		return true;
	}
	/* erase */
	if (len == 5 && memcmp(name, "erase", 5) == 0) {
		cg->type_depth -= 2;
		emit_pop_rax(); ir_mov_reg_reg(&cg->ir, REG_RDX, REG_RAX);
		emit_pop_rax(); ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mem_erase");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		return true;
	}
	/* dump, md */
	if ((len == 4 && memcmp(name, "dump", 4) == 0) || (len == 2 && name[0]=='m' && name[1]=='d')) {
		if (cg->type_depth > 0) cg->type_depth--;
		emit_pop_rax(); ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_mem_dump");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		return true;
	}
	return false;
}
