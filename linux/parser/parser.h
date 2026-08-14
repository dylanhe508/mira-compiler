/* parser 妯″潡鍐呴儴鍏变韩澶存枃浠?*/
#ifndef MIRA_PARSER_H
#define MIRA_PARSER_H

#include "../mira.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void module_table_init(ModuleTable *table, Arena *arena);
ModuleId module_intern_path(ModuleTable *table, const char *path, size_t path_len);
bool module_add_import(ModuleTable *table, ModuleId owner, ModuleId target,
	const char *alias, size_t alias_len, ModuleImport **out_import);
char *module_qualify_symbol(ModuleTable *table, ModuleId module,
	const char *name, size_t name_len);
void module_finish_loading(ModuleTable *table, ModuleId module);
ModuleId module_register_import(Compiler *compiler, const char *logical, const char *alias);
char *module_resolve_dotted(ModuleTable *table, ModuleId owner,
	const char *name, size_t name_len, size_t *resolved_len, int *error_kind);
/* ---- 鍏变韩鐘舵€侊紙瀹氫箟鍦?index.c 涓級 ---- */
extern Compiler *comp;
extern int current_syntax_mode;  /* 0=postfix, 1=infix */
extern int current_var_scope;
extern int next_var_scope;
extern int list_literal_id;

/* ---- 鍓嶅悜澹版槑 ---- */
static IrNode *parse_block_content(Program *prog);
static IrNode *parse_body_until_newline(Program *prog);
static IrNode *parse_block_until_loop(Program *prog);
static IrNode *parse_one(Program *prog, bool is_infix_recurse);

/* ---- 宸ュ叿鍑芥暟锛坔elpers.c锛?---- */
static IrNode *pop_lIR_n(IrNode **head, IrNode **tail, int n);
static int64_t IR_to_int(IrNode *o, Program *prog);
int prog_add_var(Program *prog, char *name, size_t len);
static int prog_var_slot(Program *prog, const char *name, size_t len);
static int prog_add_struct(Program *prog, char *name, size_t name_len, char **fields, size_t *field_lens, int field_count);
static StructDef *prog_find_struct(Program *prog, const char *name, size_t len);
static int prog_field_offset(StructDef *sd, const char *fname, size_t flen);
static MethodDef *prog_find_method(Program *prog, StructDef *owner, const char *name, size_t len);
static MethodDef *prog_add_method(Program *prog, StructDef *owner, char *name, size_t len,
	char *qualified_name, size_t qualified_len, bool mut_self);
static void prog_set_var_struct(Program *prog, int slot, StructDef *sd, bool is_mutable);
static int prog_add_const(Program *prog, char *name, size_t len, ConstKind k, int64_t vi, double vd, char *vs, size_t vslen);
static int prog_const_slot(Program *prog, const char *name, size_t len);
static IrNode *new_ir(IrKind k);
static IrNode *make_assign_chain(Program *prog, char *name, size_t len, IrNode *value_op);
static int is_mira_builtin(const char *name, size_t len);
static int get_op_precedence(const char *IrNode);

/* ---- 涓紑瑙ｆ瀽锛坕nfix.c锛?---- */
static IrNode *parse_eval_block(Program *prog);
static IrNode *parse_infix_line(Program *prog);

/* ---- 鍙傛暟鍒楄〃锛坆locks.c锛?---- */
static bool parse_param_list(char ***out_names, size_t **out_lens, int *out_count);

#endif /* MIRA_PARSER_H */
