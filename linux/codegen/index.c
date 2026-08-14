/* codegen dispatch: gen_ops + gen_op main loop + all sub-modules via #include */
#include "codegen.h"
#include <stdlib.h>
#include <string.h>

/* Unified compilation of all codegen/ sub-files */
#include "state.c"
#include "emit.c"
#include "literal.c"
#include "block.c"
#include "list_literal.c"
#include "loops.c"
#include "words.c"

static bool is_block_consumer(IrNode *o) {
	if (!o) return false;
	if (o->kind == IR_IF) return true;
	if (o->kind != IR_WORD) return false;
	const char *n = o->u.word.name;
	size_t len = o->u.word.len;
	return (len == 2 && memcmp(n, "if", 2) == 0) ||
	       (len == 4 && memcmp(n, "when", 4) == 0) ||
	       (len == 5 && memcmp(n, "async", 5) == 0);
}

#define IS_FIR_VAR(slot) ((slot) >= 0 && (slot) < 6)
static int fIR_var_reg(int slot) {
	switch (slot) {
		case 0: return REG_R13;
		case 1: return REG_R14;
		case 2: return REG_R15;
		case 3: return REG_RBX;
		case 4: return REG_RDI;
		case 5: return REG_RSI;
		default: return -1;
	}
}

void gen_ops(IrNode *list) {
	for (IrNode *curr = list; curr; curr = curr->next) {
		printf("OLD CODEGEN INVOKED: kind=%d\n", curr->kind); fflush(stdout);
		if (curr->kind == IR_VAR && IS_FIR_VAR(curr->u.var_slot) && curr->next && curr->next->kind == IR_WORD) {
			const char *wn = curr->next->u.word.name;
			if (strcmp(wn, "@") == 0) {
				int reg = fIR_var_reg(curr->u.var_slot);
				ir_mov_reg_reg(&cg->ir, REG_RAX, reg);
				emit_push_rax();
				cg->type_stack[cg->type_depth++] = cg->var_types[curr->u.var_slot];
				curr = curr->next;
				continue;
			}
			if (strcmp(wn, "!") == 0) {
				int reg = fIR_var_reg(curr->u.var_slot);
				if (cg->type_depth > 0) cg->type_depth--;
				emit_pop_rax();
				ir_mov_reg_reg(&cg->ir, reg, REG_RAX);
				/* Mirror memory */
				char lbl[64]; snprintf(lbl, sizeof(lbl), "mira_vars");
				ir_lea_rip(&cg->ir, REG_RCX, lbl);
				ir_mov_mem_reg(&cg->ir, REG_RCX, curr->u.var_slot * 8, REG_RAX);
				cg->var_types[curr->u.var_slot] = TOS_INT;
				curr = curr->next;
				continue;
			}
		}
		gen_op(curr, curr->next);
	}
}

void gen_op(IrNode *o, IrNode *next) {
	if (!o) return;
	switch (o->kind) {
	case IR_INT: gen_op_int(o); return;
	case IR_FLOAT: gen_op_float(o); return;
	case IR_CONST: gen_op_const(o); return;
	case IR_VAR: gen_op_var(o); return;
	case IR_STR: gen_op_str(o); return;
	case IR_WORD: gen_op_word(o); return;
	case IR_BLOCK:
		if (is_block_consumer(next)) gen_op_block(o);
		else gen_op_block_execute(o);
		return;
	case IR_IF: gen_op_if(o); return;
	case IR_SWITCH: gen_op_switch(o); return;
	case IR_LIST_LITERAL: gen_op_list_literal(o); return;
	case IR_FOR_EXT: gen_op_for_ext(o); return;
	case IR_WHILE_INF: gen_op_while_inf(o); return;
	case IR_FOR_CSTYLE: gen_op_for_cstyle(o); return;
	case IR_FOR_RANGE: gen_op_for_range(o); return;
	case IR_EACH: gen_op_each(o); return;
	case IR_WHILE_COND: gen_op_while_cond(o); return;
	case IR_TRY: {
		IrNode *body_block = o->u.try_block.body;
		int Ltry_err = new_label(), Ltry_end = new_label();

		/* 1. mira_try_begin() → RAX = jmp_buf 指针 */
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_try_begin");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);

		/* 2. _setjmpex(jmp_buf) → 0=正常, 非0=异常 */
		ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "_setjmpex");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);

		/* 3. if (setjmp != 0) goto error */
		ir_test_reg_reg(&cg->ir, REG_RAX, REG_RAX);
		ir_jcc(&cg->ir, IR_JNZ, Ltry_err);

		/* 4. 正常路径 — 直接内联执行 block body */
		gen_ops(body_block && body_block->kind == IR_BLOCK ?
				body_block->u.block : body_block);

		/* 5. mira_try_end() */
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_try_end");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);

		/* 成功: push 1 */
		emit_push_imm(1);
		if (cg->type_depth < 64) cg->type_stack[cg->type_depth++] = TOS_INT;
		ir_jmp(&cg->ir, Ltry_end);

		/* 异常路径 */
		ir_label(&cg->ir, Ltry_err);
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_get_error");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		emit_push_rax();
		emit_push_imm(0);

		ir_label(&cg->ir, Ltry_end);
		return;
	}
	case IR_LAMBDA: /* handled elsewhere */ return;
	}
}

#include "program.c"
