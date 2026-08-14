/* words_debug.c â€?è°ƒè¯•è¯?(dup, drop, swap, over) (IR) */
#include "codegen.h"
#include <string.h>

bool gen_word_debug(IrNode *o, const char *name, size_t len) {
	(void)o;
	/* dup â€?å¤åˆ¶æ ˆé¡¶ */
	if (len == 3 && memcmp(name, "dup", 3) == 0) {
		int t = cg->type_depth > 0 ? cg->type_stack[cg->type_depth - 1] : TOS_INT;
		ir_mov_reg_mem(&cg->ir, REG_RAX, REG_R12, -8);  /* peek top */
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = t;
		return true;
	}
	/* drop â€?ä¸¢å¼ƒæ ˆé¡¶ */
	if (len == 4 && memcmp(name, "drop", 4) == 0) {
		if (cg->type_depth > 0) cg->type_depth--;
		ir_sub_reg_imm(&cg->ir, REG_R12, 8);
		cg->stack_depth--;
		return true;
	}
	/* swap â€?äº¤æ¢å‰ä¸¤ä¸?*/
	if (len == 4 && memcmp(name, "swap", 4) == 0) {
		ir_mov_reg_mem(&cg->ir, REG_RAX, REG_R12, -8);
		ir_mov_reg_mem(&cg->ir, REG_RCX, REG_R12, -16);
		ir_mov_mem_reg(&cg->ir, REG_R12, -8, REG_RCX);
		ir_mov_mem_reg(&cg->ir, REG_R12, -16, REG_RAX);
		if (cg->type_depth >= 2) {
			int tmp = cg->type_stack[cg->type_depth - 1];
			cg->type_stack[cg->type_depth - 1] = cg->type_stack[cg->type_depth - 2];
			cg->type_stack[cg->type_depth - 2] = tmp;
		}
		return true;
	}
	/* over â€?å¤åˆ¶æ¬¡é¡¶ */
	if (len == 4 && memcmp(name, "over", 4) == 0) {
		int t = cg->type_depth >= 2 ? cg->type_stack[cg->type_depth - 2] : TOS_INT;
		ir_mov_reg_mem(&cg->ir, REG_RAX, REG_R12, -16);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = t;
		return true;
	}
	/* nip â€?remove second item */
	if (len == 3 && memcmp(name, "nip", 3) == 0) {
		ir_mov_reg_mem(&cg->ir, REG_RAX, REG_R12, -8);
		ir_mov_mem_reg(&cg->ir, REG_R12, -16, REG_RAX);
		ir_sub_reg_imm(&cg->ir, REG_R12, 8);
		cg->stack_depth--;
		if (cg->type_depth >= 2) {
			cg->type_stack[cg->type_depth - 2] = cg->type_stack[cg->type_depth - 1];
			cg->type_depth--;
		}
		return true;
	}
	/* rot â€?rotate top 3 */
	if (len == 3 && memcmp(name, "rot", 3) == 0) {
		ir_mov_reg_mem(&cg->ir, REG_RAX, REG_R12, -8);
		ir_mov_reg_mem(&cg->ir, REG_RCX, REG_R12, -16);
		ir_mov_reg_mem(&cg->ir, REG_RDX, REG_R12, -24);
		ir_mov_mem_reg(&cg->ir, REG_R12, -8, REG_RDX);
		ir_mov_mem_reg(&cg->ir, REG_R12, -16, REG_RAX);
		ir_mov_mem_reg(&cg->ir, REG_R12, -24, REG_RCX);
		if (cg->type_depth >= 3) {
			int t = cg->type_stack[cg->type_depth - 3];
			cg->type_stack[cg->type_depth - 3] = cg->type_stack[cg->type_depth - 2];
			cg->type_stack[cg->type_depth - 2] = cg->type_stack[cg->type_depth - 1];
			cg->type_stack[cg->type_depth - 1] = t;
		}
		return true;
	}
	/* depth â€?push stack depth */
	if (len == 5 && memcmp(name, "depth", 5) == 0) {
		emit_push_imm(cg->stack_depth);
		cg->type_stack[cg->type_depth++] = TOS_INT;
		return true;
	}
	return false;
}
