/* list_literal.c �?IR_LIST_LITERAL (IR) */
#include "codegen.h"

void gen_op_list_literal(IrNode *o) {
	int size = o->u.list_literal.size;
	int temp_slot = o->u.list_literal.temp_slot;
	IrNode *elements = o->u.list_literal.elements;

	/* mira_list_new(size) */
	ir_mov_reg_imm(&cg->ir, REG_ECX, (int64_t)size);
	ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
	ir_call_extern(&cg->ir, "mira_list_new");
	ir_add_reg_imm(&cg->ir, REG_RSP, 32);

	/* 存到 mira_vars[temp_slot] */
	ir_lea_rip(&cg->ir, REG_RCX, "mira_vars");
	ir_mov_mem_reg(&cg->ir, REG_RCX, temp_slot * 8, REG_RAX);

	int idx = 0;
	for (IrNode *e = elements; e; e = e->next, idx++) {
		gen_op(e, e->next);

		/* 值在栈上, pop �?r8 */
		if (cg->type_depth > 0) cg->type_depth--;
		emit_pop_rax();
		ir_mov_reg_reg(&cg->ir, REG_R8, REG_RAX);

		/* mira_list_set(list, idx, value) */
		ir_lea_rip(&cg->ir, REG_RCX, "mira_vars");
		ir_mov_reg_mem(&cg->ir, REG_RCX, REG_RCX, temp_slot * 8);
		ir_mov_reg_imm(&cg->ir, REG_EDX, (int64_t)idx);
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_list_set");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
	}

	/* push 列表地址回栈 */
	ir_lea_rip(&cg->ir, REG_RAX, "mira_vars");
	ir_mov_reg_mem(&cg->ir, REG_RAX, REG_RAX, temp_slot * 8);
	emit_push_rax();
	cg->type_stack[cg->type_depth++] = TOS_INT;
}
