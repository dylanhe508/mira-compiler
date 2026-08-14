/* codegen 各模块共享：状态、TOS 常量、CodegenState 结构�?*/
#ifndef CODEGEN_CODEGEN_H
#define CODEGEN_CODEGEN_H

#include "../mira.h"
#include "ir.h"
#include "../hash.h"
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

/* word handler: IrNode �?void（从哈希表分发） */
typedef void (*word_handler_t)(IrNode *o);

/* 栈顶类型�?=int, 1=string(ptr), 2=float, 3=bool */
#define TOS_INT    0
#define TOS_STR    1
#define TOS_FLOAT  2
#define TOS_BOOL   3

/* --- CodegenState：编译器代码生成状�?--- */
typedef struct {
	Compiler *comp;
	Program *prog;
	Def *current_def;
	int stack_depth;
	int *type_stack;
	int type_stack_cap;
	int type_depth;
	int *var_types;
	int var_types_cap;
	int lIR_var_slot;
	/* break/continue：循环上下文�?*/
	int *loop_end_stack;
	int *loop_cont_stack;
	int loop_depth;
	int loop_cap;
	/* switch 上下文栈 */
	int *sw_end_stack;
	int *sw_cont_stack;
	int *sw_cont_kind;
	int sw_depth;
	int sw_cap;
	/* word 哈希分发�?*/
	HashTable word_ht;
	/* IR 缓冲�?*/
	IrBuffer ir;
} CodegenState;

/* 全局 codegen 状态指�?*/
extern CodegenState *cg;

/* 状态管�?(state.c) */
void codegen_state_init(Compiler *c, Program *p);
void codegen_state_free(void);

/* emit 与标�?(emit.c) �?现在发射 IR 而非 NASM 文本 */
void emit_push_rax(void);
void emit_pop_rax(void);
void emit_push_imm(int64_t v);
void emit_sanitized_name(const char *name, size_t len);
int new_label(void);
int param_slot(const char *name, size_t len);
int prog_var_slot(Program *prog, const char *name, size_t len);
int prog_add_var(Program *prog, char *name, size_t len);

/* 循环�?switch 上下�?*/
void push_loop_context(int Lend, int Lcontinue);
void pop_loop_context(void);
int get_loop_end(void);
int get_loop_continue(void);

void push_switch_context(int Lend, int Lcontinue, int cont_kind);
void pop_switch_context(void);
int get_switch_end(void);
int get_switch_continue(void);
int get_switch_continue_kind(void);

/* IrNode 生成 */
void gen_ops(IrNode *list);
void gen_op(IrNode *o, IrNode *next);

/* words 子模�?*/
bool gen_word_debug(IrNode *o, const char *name, size_t len);
bool gen_word_arith(IrNode *o, const char *name, size_t len);
bool gen_word_compare(IrNode *o, const char *name, size_t len);
bool gen_word_memory(IrNode *o, const char *name, size_t len);
bool gen_word_io(IrNode *o, const char *name, size_t len);
bool gen_word_data(IrNode *o, const char *name, size_t len);
bool gen_word_string(IrNode *o, const char *name, size_t len);
bool gen_word_file(IrNode *o, const char *name, size_t len);
bool gen_word_math(IrNode *o, const char *name, size_t len);
bool gen_word_async(IrNode *o, const char *name, size_t len);
bool gen_word_winapi(IrNode *o, const char *name, size_t len);

/* word 注册 */
void register_word(const char *name, word_handler_t handler);
void words_init(void);

/* struct 字段偏移计算 */
static inline int prog_field_offset(StructDef *sd, const char *fname, size_t flen) {
	for (int i = 0; i < sd->field_count; i++)
		if (sd->field_lens[i] == flen && memcmp(sd->field_names[i], fname, flen) == 0)
			return i * 8;
	return -1;
}

#endif
