/* loops.c �?循环代码生成 (IR) */
#include "codegen.h"
#include <string.h>

/* start..end for { body } �?range for */
void gen_op_for_range(IrNode *o) {
	int64_t start = o->u.for_range.start;
	int64_t end = o->u.for_range.end;
	IrNode *body = o->u.for_range.body;
	char *var = o->u.for_range.var;
	size_t var_len = o->u.for_range.var_len;
	int Lloop = new_label(), Lend = new_label();
	push_loop_context(Lend, Lloop);

	if (var && var_len > 0) {
		int slot = prog_var_slot(cg->prog, var, var_len);
		if (slot < 0) slot = 0;
		char vlbl[64]; snprintf(vlbl, sizeof(vlbl), "mira_vars");
		ir_lea_rip(&cg->ir, REG_RBX, vlbl);
		ir_mov_mem_imm(&cg->ir, REG_RBX, slot * 8, start);
		ir_mov_reg_imm(&cg->ir, REG_R14, end);

		ir_label(&cg->ir, Lloop);
		ir_lea_rip(&cg->ir, REG_RBX, vlbl);
		ir_mov_reg_mem(&cg->ir, REG_RBX, REG_RBX, slot * 8);
		ir_cmp_reg_reg(&cg->ir, REG_RBX, REG_R14);
		ir_jcc(&cg->ir, IR_JGE, Lend);

		gen_ops(body && body->kind == IR_BLOCK ? body->u.block : body);

		ir_lea_rip(&cg->ir, REG_RBX, vlbl);
		ir_inc_mem(&cg->ir, REG_RBX, slot * 8);
		ir_jmp(&cg->ir, Lloop);
		ir_label(&cg->ir, Lend);
	} else {
		/* 无变�?for：用 callee-saved R13/R14, push/pop 保护 */
		ir_push(&cg->ir, REG_R13);
		ir_push(&cg->ir, REG_R14);
		ir_mov_reg_imm(&cg->ir, REG_R13, start);
		ir_mov_reg_imm(&cg->ir, REG_R14, end);

		ir_label(&cg->ir, Lloop);
		ir_cmp_reg_reg(&cg->ir, REG_R13, REG_R14);
		ir_jcc(&cg->ir, IR_JGE, Lend);

		gen_ops(body && body->kind == IR_BLOCK ? body->u.block : body);

		ir_add_reg_imm(&cg->ir, REG_R13, 1);
		ir_jmp(&cg->ir, Lloop);
		ir_label(&cg->ir, Lend);
		ir_pop(&cg->ir, REG_R14);
		ir_pop(&cg->ir, REG_R13);
	}
	pop_loop_context();
}

/* { init } { cond } { step } { body } for �?C-style */
void gen_op_for_cstyle(IrNode *o) {
	IrNode *init = o->u.for_cstyle.init;
	IrNode *cond = o->u.for_cstyle.cond;
	IrNode *step = o->u.for_cstyle.step;
	IrNode *body = o->u.for_cstyle.body;
	int Lloop = new_label(), Lend = new_label();

	gen_ops(init && init->kind == IR_BLOCK ? init->u.block : init);

	push_loop_context(Lend, Lloop);

	ir_label(&cg->ir, Lloop);

	gen_ops(cond && cond->kind == IR_BLOCK ? cond->u.block : cond);

	if (cg->type_depth > 0) cg->type_depth--;
	emit_pop_rax();
	ir_test_reg_reg(&cg->ir, REG_RAX, REG_RAX);
	ir_jcc(&cg->ir, IR_JZ, Lend);

	gen_ops(body && body->kind == IR_BLOCK ? body->u.block : body);
	gen_ops(step && step->kind == IR_BLOCK ? step->u.block : step);

	ir_jmp(&cg->ir, Lloop);
	ir_label(&cg->ir, Lend);

	pop_loop_context();
}

/* { cond } { body } while */
void gen_op_while_cond(IrNode *o) {
	IrNode *cond = o->u.while_cond.cond;
	IrNode *body = o->u.while_cond.body;
	int Lloop = new_label(), Lend = new_label();

	push_loop_context(Lend, Lloop);

	ir_label(&cg->ir, Lloop);
	gen_ops(cond && cond->kind == IR_BLOCK ? cond->u.block : cond);

	if (cg->type_depth > 0) cg->type_depth--;
	emit_pop_rax();
	ir_test_reg_reg(&cg->ir, REG_RAX, REG_RAX);
	ir_jcc(&cg->ir, IR_JZ, Lend);

	gen_ops(body && body->kind == IR_BLOCK ? body->u.block : body);

	ir_jmp(&cg->ir, Lloop);
	ir_label(&cg->ir, Lend);

	pop_loop_context();
}

/* start end step for var ... loop */
void gen_op_for_ext(IrNode *o) {
	int64_t start = o->u.for_ext.start;
	int64_t end = o->u.for_ext.end;
	int64_t step = o->u.for_ext.step;
	char *var = o->u.for_ext.var;
	size_t var_len = o->u.for_ext.var_len;
	IrNode *body = o->u.for_ext.body;

	int slot = prog_var_slot(cg->prog, var, var_len);
	if (slot < 0) slot = 0;

	char vlbl[64]; snprintf(vlbl, sizeof(vlbl), "mira_vars");

	ir_lea_rip(&cg->ir, REG_RBX, vlbl);
	ir_mov_mem_imm(&cg->ir, REG_RBX, slot * 8, start);
	ir_mov_reg_imm(&cg->ir, REG_R14, end);

	int Lloop = new_label(), Lend = new_label();
	push_loop_context(Lend, Lloop);

	ir_label(&cg->ir, Lloop);
	ir_lea_rip(&cg->ir, REG_RBX, vlbl);
	ir_mov_reg_mem(&cg->ir, REG_RBX, REG_RBX, slot * 8);
	ir_cmp_reg_reg(&cg->ir, REG_RBX, REG_R14);
	if (step > 0)
		ir_jcc(&cg->ir, IR_JGE, Lend);
	else
		ir_jcc(&cg->ir, IR_JLE, Lend);

	gen_ops(body);

	ir_lea_rip(&cg->ir, REG_RBX, vlbl);
	ir_add_mem_imm(&cg->ir, REG_RBX, slot * 8, step);
	ir_jmp(&cg->ir, Lloop);
	ir_label(&cg->ir, Lend);

	pop_loop_context();
}

/* 列表 each { body } */
void gen_op_each(IrNode *o) {
	IrNode *list_op = o->u.each.list;
	IrNode *body = o->u.each.body;
	gen_op(list_op, NULL);

	int Lloop = new_label(), Lend = new_label();
	push_loop_context(Lend, Lloop);

	ir_sub_reg_imm(&cg->ir, REG_R12, 8);
	ir_mov_reg_mem(&cg->ir, REG_R15, REG_R12, 0);
	cg->stack_depth--;
	if (cg->type_depth > 0) cg->type_depth--;

	ir_mov_reg_reg(&cg->ir, REG_RCX, REG_R15);
	ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
	ir_call_extern(&cg->ir, "mira_list_len");
	ir_add_reg_imm(&cg->ir, REG_RSP, 32);
	ir_mov_reg_reg(&cg->ir, REG_R14, REG_RAX);
	ir_xor_reg_reg(&cg->ir, REG_EBX, REG_EBX);

	ir_label(&cg->ir, Lloop);
	ir_cmp_reg_reg(&cg->ir, REG_RBX, REG_R14);
	ir_jcc(&cg->ir, IR_JGE, Lend);

	ir_mov_reg_reg(&cg->ir, REG_RCX, REG_R15);
	ir_mov_reg_reg(&cg->ir, REG_RDX, REG_RBX);
	ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
	ir_call_extern(&cg->ir, "mira_list_get");
	ir_add_reg_imm(&cg->ir, REG_RSP, 32);

	ir_mov_mem_reg(&cg->ir, REG_R12, 0, REG_RAX);
	ir_add_reg_imm(&cg->ir, REG_R12, 8);
	cg->stack_depth++;
	cg->type_stack[cg->type_depth++] = TOS_INT;

	gen_ops(body && body->kind == IR_BLOCK ? body->u.block : body);

	ir_inc_reg(&cg->ir, REG_RBX);
	ir_jmp(&cg->ir, Lloop);
	ir_label(&cg->ir, Lend);

	pop_loop_context();
}

/* while ... loop 无限循环 */
void gen_op_while_inf(IrNode *o) {
	IrNode *body = o->u.while_inf.body;
	int Lloop = new_label(), Lend = new_label();
	push_loop_context(Lend, Lloop);

	ir_label(&cg->ir, Lloop);
	gen_ops(body);
	ir_jmp(&cg->ir, Lloop);
	ir_label(&cg->ir, Lend);

	pop_loop_context();
}
