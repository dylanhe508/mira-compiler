/* emit.c �?IR-based emit functions (替代�?NASM 文本输出)
 *
 * 提供 emit_push_rax / emit_pop_rax / emit_push_imm 等便利函数，
 * 内部使用 ir_* API �?cg->ir 发射指令�?
 */
#include "codegen.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* Mira stack 操作：r12 用作数据栈指�?*/

void emit_push_rax(void) {
	ir_mov_mem_reg(&cg->ir, REG_R12, 0, REG_RAX);  /* mov [r12], rax */
	ir_add_reg_imm(&cg->ir, REG_R12, 8);            /* add r12, 8 */
	cg->stack_depth++;
}

void emit_pop_rax(void) {
	ir_sub_reg_imm(&cg->ir, REG_R12, 8);            /* sub r12, 8 */
	ir_mov_reg_mem(&cg->ir, REG_RAX, REG_R12, 0);   /* mov rax, [r12] */
	cg->stack_depth--;
}

void emit_push_imm(int64_t v) {
	ir_mov_mem_imm(&cg->ir, REG_R12, 0, v);         /* mov qword [r12], imm */
	ir_add_reg_imm(&cg->ir, REG_R12, 8);            /* add r12, 8 */
	cg->stack_depth++;
}

/* 将标识符中的 - 替换�?_ 生成合法符号�?*/
static char sanitized_buf[256];

void emit_sanitized_name(const char *name, size_t len) {
	/* 将结果存�?sanitized_buf 供后续使�?*/
	if (len >= sizeof(sanitized_buf)) len = sizeof(sanitized_buf) - 1;
	for (size_t i = 0; i < len; i++) {
		sanitized_buf[i] = (name[i] == '-') ? '_' : name[i];
	}
	sanitized_buf[len] = '\0';
}

int new_label(void) {
	return ++cg->comp->label_id;
}

int param_slot(const char *name, size_t len) {
	if (!cg->current_def) return -1;
	for (int i = 0; i < cg->current_def->param_count; i++)
		if (cg->current_def->param_lens[i] == len && memcmp(cg->current_def->params[i], name, len) == 0)
			return i;
	return -1;
}

int prog_var_slot(Program *prog, const char *name, size_t len) {
	if (!prog) return -1;
	for (int i = 0; i < prog->var_count; i++)
		if (prog->var_lens[i] == len && memcmp(prog->var_names[i], name, len) == 0)
			return i;
	return -1;
}
