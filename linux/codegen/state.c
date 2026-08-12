/* codegen 全局状�?�?使用 CodegenState 结构�?*/
#include "codegen.h"
#include <stdlib.h>
#include <string.h>

CodegenState *cg = NULL;

void codegen_state_init(Compiler *c, Program *p) {
	cg = calloc(1, sizeof(CodegenState));
	if (!cg) { fprintf(stderr, "mira: out of memory\n"); exit(1); }
	cg->comp = c;
	cg->prog = p;
	cg->current_def = NULL;
	cg->stack_depth = 0;
	/* 类型�?*/
	cg->type_stack_cap = 512;
	cg->type_stack = calloc((size_t)cg->type_stack_cap, sizeof(int));
	cg->type_depth = 0;
	/* 变量类型�?*/
	cg->var_types_cap = 256;
	cg->var_types = calloc((size_t)cg->var_types_cap, sizeof(int));
	cg->lIR_var_slot = -1;
	/* 循环上下文栈 */
	cg->loop_cap = 16;
	cg->loop_end_stack = calloc((size_t)cg->loop_cap, sizeof(int));
	cg->loop_cont_stack = calloc((size_t)cg->loop_cap, sizeof(int));
	cg->loop_depth = 0;
	/* switch 上下文栈 */
	cg->sw_cap = 8;
	cg->sw_end_stack = calloc((size_t)cg->sw_cap, sizeof(int));
	cg->sw_cont_stack = calloc((size_t)cg->sw_cap, sizeof(int));
	cg->sw_cont_kind = calloc((size_t)cg->sw_cap, sizeof(int));
	cg->sw_depth = 0;
	/* word 哈希分发�?*/
	ht_init(&cg->word_ht, 256);
	words_init();
	/* IR 缓冲�?*/
	ir_init(&cg->ir);
}

void codegen_state_free(void) {
	if (!cg) return;
	free(cg->type_stack);
	free(cg->var_types);
	free(cg->loop_end_stack);
	free(cg->loop_cont_stack);
	free(cg->sw_end_stack);
	free(cg->sw_cont_stack);
	free(cg->sw_cont_kind);
	ht_free(&cg->word_ht);
	ir_free(&cg->ir);
	free(cg);
	cg = NULL;
}

void register_word(const char *name, word_handler_t handler) {
	ht_set(&cg->word_ht, name, (void *)handler);
}

/* break/continue：循环上下文 */
void push_loop_context(int Lend, int Lcontinue) {
	if (cg->loop_depth >= cg->loop_cap) {
		cg->loop_cap *= 2;
		cg->loop_end_stack = realloc(cg->loop_end_stack, (size_t)cg->loop_cap * sizeof(int));
		cg->loop_cont_stack = realloc(cg->loop_cont_stack, (size_t)cg->loop_cap * sizeof(int));
	}
	cg->loop_end_stack[cg->loop_depth] = Lend;
	cg->loop_cont_stack[cg->loop_depth] = Lcontinue;
	cg->loop_depth++;
}
void pop_loop_context(void) { if (cg->loop_depth > 0) cg->loop_depth--; }
int get_loop_end(void) { return cg->loop_depth > 0 ? cg->loop_end_stack[cg->loop_depth - 1] : -1; }
int get_loop_continue(void) { return cg->loop_depth > 0 ? cg->loop_cont_stack[cg->loop_depth - 1] : -1; }

/* switch 上下�?*/
void push_switch_context(int Lend, int Lcontinue, int cont_kind) {
	if (cg->sw_depth >= cg->sw_cap) {
		cg->sw_cap *= 2;
		cg->sw_end_stack = realloc(cg->sw_end_stack, (size_t)cg->sw_cap * sizeof(int));
		cg->sw_cont_stack = realloc(cg->sw_cont_stack, (size_t)cg->sw_cap * sizeof(int));
		cg->sw_cont_kind = realloc(cg->sw_cont_kind, (size_t)cg->sw_cap * sizeof(int));
	}
	cg->sw_end_stack[cg->sw_depth] = Lend;
	cg->sw_cont_stack[cg->sw_depth] = Lcontinue;
	cg->sw_cont_kind[cg->sw_depth] = cont_kind;
	cg->sw_depth++;
}
void pop_switch_context(void) { if (cg->sw_depth > 0) cg->sw_depth--; }
int get_switch_end(void) { return cg->sw_depth > 0 ? cg->sw_end_stack[cg->sw_depth - 1] : -1; }
int get_switch_continue(void) { return cg->sw_depth > 0 ? cg->sw_cont_stack[cg->sw_depth - 1] : -1; }
int get_switch_continue_kind(void) { return cg->sw_depth > 0 ? cg->sw_cont_kind[cg->sw_depth - 1] : -1; }
