/* words.c ?IR_WORD 分发、IR_IF / IR_SWITCH ?(IR) */
#include "codegen.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ?words 子模块（通过 #include 统一编译?*/
#include "words_debug.c"
#include "words_arith.c"
#include "words_compare.c"
#include "words_memory.c"
#include "words_io.c"
#include "words_data.c"
#include "words_string.c"
#include "words_file.c"
#include "words_math.c"
#include "words_async.c"
#include "words_winapi.c"

/* words_init：注册所有内?word 的哈希表 */
void words_init(void) {
	/* ?gen_word_* 函数处理，无需逐个注册 */
}

/* IR_SWITCH 处理 */
void gen_op_switch(IrNode *o) {
	if (cg->type_depth > 0) cg->type_depth--;
	emit_pop_rax();
	cg->stack_depth--;
	ir_mov_reg_reg(&cg->ir, REG_RDI, REG_RAX);

	int Lsw_end = new_label();
	IrNode *c = o->u.switch_.cases;
	while (c) {
		IrNode *pattern = c;
		IrNode *block = c->next;
		if (!block) break;

		int Lnext = new_label();
		gen_op(pattern, NULL);
		if (cg->type_depth > 0) cg->type_depth--;
		emit_pop_rax();
		cg->stack_depth--;
		ir_cmp_reg_reg(&cg->ir, REG_RDI, REG_RAX);
		ir_jcc(&cg->ir, IR_JNE, Lnext);

		push_switch_context(Lsw_end, Lnext, 0);
		if (block->kind == IR_BLOCK)
			gen_ops(block->u.block);
		else
			gen_op(block, NULL);
		pop_switch_context();
		ir_jmp(&cg->ir, Lsw_end);
		ir_label(&cg->ir, Lnext);
		c = block->next;
	}

	if (o->u.switch_.default_block) {
		IrNode *def = o->u.switch_.default_block;
		push_switch_context(Lsw_end, Lsw_end, 1);
		if (def->kind == IR_BLOCK)
			gen_ops(def->u.block);
		else
			gen_op(def, NULL);
		pop_switch_context();
	}
	ir_label(&cg->ir, Lsw_end);
}

/* IR_IF 处理 */
void gen_op_if(IrNode *o) {
	int Lelse = new_label(), Lend = new_label();
	if (o->u.iff.cond)
		gen_ops(o->u.iff.cond);
	if (cg->type_depth > 0) cg->type_depth--;
	emit_pop_rax();
	cg->stack_depth--;
	ir_test_reg_reg(&cg->ir, REG_RAX, REG_RAX);
	ir_jcc(&cg->ir, IR_JZ, Lelse);
	if (o->u.iff.then_b)
		gen_ops(o->u.iff.then_b->u.block);
	ir_jmp(&cg->ir, Lend);
	ir_label(&cg->ir, Lelse);
	if (o->u.iff.else_b)
		gen_ops(o->u.iff.else_b->u.block);
	ir_label(&cg->ir, Lend);
}

/* gen_op_word：word 分发函数 */
void gen_op_word(IrNode *o) {
	const char *name = o->u.word.name;
	size_t len = o->u.word.len;

	/* 尝试各子模块 */
	if (gen_word_debug(o, name, len)) return;
	if (gen_word_arith(o, name, len)) return;
	if (gen_word_compare(o, name, len)) return;
	if (gen_word_memory(o, name, len)) return;
	if (gen_word_io(o, name, len)) return;
	if (gen_word_data(o, name, len)) return;
	if (gen_word_string(o, name, len)) return;
	if (gen_word_file(o, name, len)) return;
	if (gen_word_math(o, name, len)) return;
	if (gen_word_async(o, name, len)) return;
	if (gen_word_winapi(o, name, len)) return;

	/* break */
	if (len == 5 && memcmp(name, "break", 5) == 0) {
		int Lend = get_loop_end();
		if (Lend < 0) Lend = get_switch_end();
		if (Lend >= 0) ir_jmp(&cg->ir, Lend);
		return;
	}
	/* continue */
	if (len == 8 && memcmp(name, "continue", 8) == 0) {
		int Lcont = get_loop_continue();
		if (Lcont >= 0) ir_jmp(&cg->ir, Lcont);
		return;
	}
	/* when（条件执行块*/
	if (len == 4 && memcmp(name, "when", 4) == 0) {
		if (cg->type_depth > 0) cg->type_depth--;
		emit_pop_rax();
		int Lskip = new_label();
		ir_test_reg_reg(&cg->ir, REG_RAX, REG_RAX);
		ir_jcc(&cg->ir, IR_JZ, Lskip);
		/* 执行?*/
		if (cg->type_depth > 0) cg->type_depth--;
		emit_pop_rax();
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_reg(&cg->ir, REG_RAX);
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		int Lend = new_label();
		ir_jmp(&cg->ir, Lend);
		ir_label(&cg->ir, Lskip);
		/* drop block ptr */
		ir_sub_reg_imm(&cg->ir, REG_R12, 8);
		cg->stack_depth--;
		if (cg->type_depth > 0) cg->type_depth--;
		ir_label(&cg->ir, Lend);
		return;
	}
	/* throw */
	if (len == 5 && memcmp(name, "throw", 5) == 0) {
		if (cg->type_depth > 0) cg->type_depth--;
		emit_pop_rax();
		ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_throw");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		return;
	}
	/* get-error */
	if (len == 9 && memcmp(name, "get-error", 9) == 0) {
		ir_sub_reg_imm(&cg->ir, REG_RSP, 32);
		ir_call_extern(&cg->ir, "mira_get_error");
		ir_add_reg_imm(&cg->ir, REG_RSP, 32);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_INT;
		return;
	}
	/* 检查是否是函数参数 */
	int ps = param_slot(name, len);
	if (ps >= 0) {
		/* ?[rbp + 16 + slot*8] 加载参数 */
		ir_mov_reg_mem(&cg->ir, REG_RAX, REG_RBP, 16 + ps * 8);
		emit_push_rax();
		cg->type_stack[cg->type_depth++] = TOS_INT;
		return;
	}
	/* 用户定义函数调用 */
	char sym[256];
	if (len >= sizeof(sym)) len = sizeof(sym) - 1;
	for (size_t i = 0; i < len; i++) sym[i] = (name[i] == '-') ? '_' : name[i];
	sym[len] = '\0';

	/* 查找函数定义，确定参数数量及是否是外部函?*/
	int nparams = 0;
	bool is_ext = false;
	for (Def *dd = cg->prog->defs; dd; dd = dd->next) {
		if (dd->name_len == len && memcmp(dd->name, name, len) == 0) {
			nparams = dd->param_count;
			is_ext = dd->is_extern;
			break;
		}
	}
	if (nparams == 0 && cg->stack_depth >= 1 && !is_ext) nparams = 1;

	/* 传参（Windows x64 ABI：前4个寄存器?+走栈?*/
	/* 计算栈空间：32字节shadow + ?+个参数的空间?6字节对齐 */
	int extra = nparams > 4 ? nparams - 4 : 0;
	int frame = 32 + extra * 8;
	if (frame % 16 != 0) frame += 8; /* 16字节对齐 */

	ir_sub_reg_imm(&cg->ir, REG_RSP, frame);

	/* 先放?+个参数到栈上（从最后一个开始pop?*/
	for (int i = nparams; i > 4; i--) {
		if (cg->stack_depth >= 1) {
			emit_pop_rax();
			if (cg->type_depth > 0) cg->type_depth--;
			ir_mov_mem_reg(&cg->ir, REG_RSP, 32 + (i - 5) * 8, REG_RAX);
		}
	}

	/* ?个参数走寄存?*/
	if (nparams >= 4 && cg->stack_depth >= 1) {
		emit_pop_rax();
		if (cg->type_depth > 0) cg->type_depth--;
		ir_mov_reg_reg(&cg->ir, REG_R9, REG_RAX);
	}
	if (nparams >= 3 && cg->stack_depth >= 1) {
		emit_pop_rax();
		if (cg->type_depth > 0) cg->type_depth--;
		ir_mov_reg_reg(&cg->ir, REG_R8, REG_RAX);
	}
	if (nparams >= 2 && cg->stack_depth >= 1) {
		emit_pop_rax();
		if (cg->type_depth > 0) cg->type_depth--;
		ir_mov_reg_reg(&cg->ir, REG_RDX, REG_RAX);
	}
	if (nparams >= 1 && cg->stack_depth >= 1) {
		emit_pop_rax();
		if (cg->type_depth > 0) cg->type_depth--;
		ir_mov_reg_reg(&cg->ir, REG_RCX, REG_RAX);
	}

	ir_call_extern(&cg->ir, sym);
	ir_add_reg_imm(&cg->ir, REG_RSP, frame);
	
	if (is_ext) {
		/* 外部 C 函数的返回值在 RAX，需要手动压?Mira 数据?*/
		emit_push_rax();
		cg->stack_depth++;
		cg->type_stack[cg->type_depth++] = TOS_INT;
	} else if (nparams > 0) {
		/* 用户函数的返回值已?Mira 数据栈上（R12），调整状?*/
		cg->stack_depth++;
		cg->type_stack[cg->type_depth++] = TOS_INT;
	}
}
