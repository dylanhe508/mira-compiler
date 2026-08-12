/* literal.c �?IR_INT, IR_FLOAT, IR_CONST, IR_VAR, IR_STR (IR) */
#include "codegen.h"
#include <stdio.h>

void gen_op_int(IrNode *o) {
	emit_push_imm(o->u.i);
	cg->type_stack[cg->type_depth++] = TOS_INT;
}

void gen_op_float(IrNode *o) {
	int id = new_label();
	char lbl[64];
	snprintf(lbl, sizeof(lbl), "dbl.%d", id);
	/* �?.data 段发射浮点常�?*/
	ir_data_label(&cg->ir, lbl);
	ir_data_qword_dbl(&cg->ir, o->u.d);
	/* 加载�?rax �?push */
	ir_mov_reg_mem(&cg->ir, REG_RAX, REG_NONE, 0); /* placeholder �?�?lea_rip */
	/* 实际上需要用 RIP-relative 加载 */
	/* 回退刚才�?mov，改用正确方�?*/
	cg->ir.text_count--; /* undo */
	ir_lea_rip(&cg->ir, REG_RAX, lbl);
	ir_mov_reg_mem(&cg->ir, REG_RAX, REG_RAX, 0); /* mov rax, [rax] */
	emit_push_rax();
	cg->type_stack[cg->type_depth++] = TOS_FLOAT;
}

void gen_op_const(IrNode *o) {
	int slot = o->u.const_slot;
	ConstKind k = cg->prog->const_kinds[slot];
	if (k == CONST_INT) {
		emit_push_imm(cg->prog->const_ints[slot]);
		cg->type_stack[cg->type_depth++] = TOS_INT;
		return;
	}
	if (k == CONST_DOUBLE) {
		char lbl[64];
		snprintf(lbl, sizeof(lbl), "const_dbl.%d", slot);
		ir_lea_rip(&cg->ir, REG_RAX, lbl);
		ir_mov_reg_mem(&cg->ir, REG_RAX, REG_RAX, 0);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_FLOAT;
		return;
	}
	/* CONST_STR */
	char lbl[64];
	snprintf(lbl, sizeof(lbl), "const_str.%d", slot);
	ir_lea_rip(&cg->ir, REG_RAX, lbl);
	emit_push_rax();
	cg->type_stack[cg->type_depth++] = TOS_STR;
}

void gen_op_var(IrNode *o) {
	cg->lIR_var_slot = o->u.var_slot;
	char lbl[64];
	snprintf(lbl, sizeof(lbl), "mira_vars");
	ir_lea_rip(&cg->ir, REG_RAX, lbl);
	if (o->u.var_slot != 0)
		ir_add_reg_imm(&cg->ir, REG_RAX, o->u.var_slot * 8);
	emit_push_rax();
	cg->type_stack[cg->type_depth++] = cg->var_types[o->u.var_slot];
}

void gen_op_str(IrNode *o) {
	int id = new_label();
	const char *s = o->u.str.s;
	size_t len = o->u.str.len;
	char lbl[64];
	snprintf(lbl, sizeof(lbl), "str.%d", id);

	/* �?.data 段发射字符串 */
	ir_data_label(&cg->ir, lbl);
	/* 构建字节数组（含 null 终止符） */
	uint8_t *bytes = (uint8_t *)malloc(len + 1);
	memcpy(bytes, s, len);
	bytes[len] = 0;
	ir_data_bytes(&cg->ir, bytes, (int)(len + 1));
	free(bytes);

	/* lea rax, [rip + str.N] */
	ir_lea_rip(&cg->ir, REG_RAX, lbl);
	emit_push_rax();
	cg->type_stack[cg->type_depth++] = TOS_STR;
}
