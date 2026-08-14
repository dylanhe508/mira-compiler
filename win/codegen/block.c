/* block.c �?IR_BLOCK (IR) */
#include "codegen.h"

/* 块作为值：push 块地址 (for/while/if/when 消费) */
void gen_op_block(IrNode *o) {
	int L = new_label();
	int Lskip = new_label();
	ir_jmp(&cg->ir, Lskip);
	ir_label(&cg->ir, L);
	ir_push(&cg->ir, REG_RBP);
	ir_mov_reg_reg(&cg->ir, REG_RBP, REG_RSP);
	gen_ops(o->u.block);
	ir_pop(&cg->ir, REG_RBP);
	ir_ret(&cg->ir);
	ir_label(&cg->ir, Lskip);
	ir_lea_rip_label(&cg->ir, REG_RAX, L);
	emit_push_rax();
	cg->type_stack[cg->type_depth++] = TOS_INT;
}

/* 块作为语句：立即执行 */
void gen_op_block_execute(IrNode *o) {
	int L = new_label();
	int Lskip = new_label();
	ir_jmp(&cg->ir, Lskip);
	ir_label(&cg->ir, L);
	ir_push(&cg->ir, REG_RBP);
	ir_mov_reg_reg(&cg->ir, REG_RBP, REG_RSP);
	gen_ops(o->u.block);
	ir_pop(&cg->ir, REG_RBP);
	ir_ret(&cg->ir);
	ir_label(&cg->ir, Lskip);
	ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
	ir_call_label(&cg->ir, L);
	ir_add_reg_imm(&cg->ir, REG_RSP, 32);
}
