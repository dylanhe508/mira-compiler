/* words_winapi.c â€?Windows API è¯?(IR) */
#include "codegen.h"
#include <string.h>

bool gen_word_winapi(IrNode *o, const char *name, size_t len) {
	(void)o;
	/* msgbox */
	if (len == 6 && memcmp(name, "msgbox", 6) == 0) {
		cg->type_depth -= 2;
		emit_pop_rax(); ir_mov_reg_reg(&cg->ir, REG_R8, REG_RAX);   /* title â†?R8 */
		emit_pop_rax(); ir_mov_reg_reg(&cg->ir, REG_RDX, REG_RAX);  /* text  â†?RDX */
		ir_xor_reg_reg(&cg->ir, REG_RCX, REG_RCX);                  /* hwnd  â†?RCX = 0 */
		ir_xor_reg_reg(&cg->ir, REG_R9, REG_R9);                    /* flags â†?R9  = 0 */
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_win_msgbox");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		return true;
	}
	/* sleep */
	if (len == 5 && memcmp(name, "sleep", 5) == 0) {
		if (cg->type_depth > 0) cg->type_depth--;
		emit_pop_rax();
		ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_win_sleep");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		return true;
	}
	/* shell */
	if (len == 5 && memcmp(name, "shell", 5) == 0) {
		if (cg->type_depth > 0) cg->type_depth--;
		emit_pop_rax();
		ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_win_shell");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_INT;
		return true;
	}
	/* env */
	if (len == 3 && memcmp(name, "env", 3) == 0) {
		if (cg->type_depth > 0) cg->type_depth--;
		emit_pop_rax();
		ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_win_env");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_STR;
		return true;
	}
	/* clipboard */
	if (len == 9 && memcmp(name, "clipboard", 9) == 0) {
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_win_clip_get");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_STR;
		return true;
	}
	/* pid */
	if (len == 3 && memcmp(name, "pid", 3) == 0) {
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_win_pid");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_INT;
		return true;
	}
	/* clock */
	if (len == 5 && memcmp(name, "clock", 5) == 0) {
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_win_tick");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_INT;
		return true;
	}
	/* clock-ns: high-resolution monotonic timestamp in nanoseconds */
	if (len == 8 && memcmp(name, "clock-ns", 8) == 0) {
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_win_tick_ns");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_INT;
		return true;
	}
	return false;
}
