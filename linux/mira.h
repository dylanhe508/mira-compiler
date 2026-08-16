/* Mira 锟?閺嶅牆锟?閸氬海鐤嗙拠顓熺《閿涘本锟?鐠囶叀鈻堥弽鐓庣础.txt 鐟欏嫯锟?*/
#ifndef MIRA_H
#define MIRA_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>

#include "typecheck.h"

/* --- Arena Allocator --- */
typedef struct ArenaBlock {
	struct ArenaBlock *next;
	size_t size;
	size_t used;
	char data[];
} ArenaBlock;

typedef struct {
	ArenaBlock *head;
} Arena;

/* --- Token 鐎规矮锟?--- */
typedef enum {
	TOK_EOF, TOK_NEWLINE,
	TOK_INT, TOK_FLOAT, TOK_STR, TOK_ID,
	TOK_PRAGMA,   /* !target !stack 锟?*/
	TOK_COLON, TOK_LBRACE, TOK_RBRACE,
	TOK_LBRACKET, TOK_RBRACKET,
	TOK_LPAREN, TOK_RPAREN, TOK_COMMA, TOK_DOTDOT
} TokenKind;

typedef struct Token {
	TokenKind kind;
	int64_t   val;
	double    dbl;    /* TOK_FLOAT 閺冩湹濞囬悽?*/
	char     *start;
	size_t    len;
	char     *str;    /* TOK_STR 閺冭埖瀵氶崥鎴濆嚒閸掑棝鍘ょ€涙顑佹稉?*/
	size_t    str_len;
	int       line;   /* 閹碘偓閸︺劏顢戦崣鍑ょ礄1-based锟?*/
	int       col;    /* 閹碘偓閸︺劌鍨崣鍑ょ礄1-based锟?*/
	unsigned char has_call_arity;
	unsigned char call_receiver_count;
	int       call_argc;
} Token;

/* --- IR锛氭娊璞¤娉曟爲 --- */
typedef enum {
	IR_INT, IR_FLOAT, IR_STR, IR_WORD, IR_VAR, IR_CONST, IR_BLOCK, IR_IF, IR_SWITCH,
	IR_LIST_LITERAL, IR_FOR_EXT, IR_WHILE_INF, IR_FOR_CSTYLE, IR_FOR_RANGE, IR_EACH, IR_WHILE_COND,
	IR_TRY, IR_LAMBDA
} IrKind;

typedef struct IrNode IrNode;
struct IrNode {
	IrKind kind;
	int line;
	int col;
	union {
		int64_t  i;
		double   d;     /* IR_FLOAT */
		struct { char *s; size_t len; } str;
		struct {
			char *name;
			size_t len;
			unsigned char has_call_arity;
			unsigned char call_boundary_mode;
			unsigned char logical_booleanize;
			unsigned char call_receiver_count;
			int call_argc;
			size_t call_close_offset;
		} word;
		int      var_slot;   /* IR_VAR锛氬彉閲忔Ы浣嶄笅??*/
		int      const_slot; /* IR_CONST锛氬父閲忔Ы浣嶄笅??*/
		IrNode *block;   /* 閸ф鍞寸粭顑跨閺夆槄绱濋悽?next 娑撹尪锟?*/
		struct { IrNode *cond; IrNode *then_b; IrNode *else_b; } iff;
		struct { int size; IrNode *elements; int count; int temp_slot; } list_literal;
		struct { int64_t start, end, step; char *var; size_t var_len; IrNode *body; } for_ext;
		struct { IrNode *cond; IrNode *body; } while_inf;
		struct { IrNode *init; IrNode *cond; IrNode *step; IrNode *body; } for_cstyle;
		struct { int64_t start, end; IrNode *body; char *var; size_t var_len; int var_slot; } for_range;
		struct { IrNode *list; IrNode *body; } each;
		struct { IrNode *cond; IrNode *body; } while_cond;
		struct { IrNode *value; IrNode *cases; IrNode *default_block; } switch_;
		struct { IrNode *body; IrNode *catch_body; int error_slot; } try_block;
		struct { char **params; size_t *param_lens; int param_count; IrNode *body; } lambda;
	} u;
	IrNode *next;
	const char *source;
	const char *source_filename;
	size_t source_offset;
	uint32_t source_module;
	MiraOwnership checked_ownership;
	const char *checked_free_func_name;
	unsigned char ownership_checked;
};

typedef struct Def {
	char *name;
	size_t name_len;
	char **params;   /* 閸欏倹鏆熼崥宥忕礉param_count 锟?*/
	size_t *param_lens;
	MiraType *param_types;
	unsigned char *param_type_explicit;
	int param_count;
	MiraType return_type;
	unsigned char return_type_explicit;
	MiraOwnership return_ownership;
	const char *return_free_func_name;
	unsigned char ownership_checked;
	unsigned char *param_may_escape;
	int line;
	int col;
	IrNode *body;
	bool is_extern;  /* 閺勵垰鎯侀弰?extern 婢圭増妲戦敍鍫熸￥ body锟?*/
	struct Def *next;
} Def;

typedef struct Pragma {
	char *name;
	size_t name_len;
	char *arg;      /* 閺佺顢戦崣鍌涙殶鐎涙顑佹稉璇х礉閸欘垶锟?*/
	struct Pragma *next;
} Pragma;

/* 缁撴瀯浣撳畾??*/
typedef struct StructDef {
	char *name;
	size_t name_len;
	char **field_names;
	size_t *field_lens;
	int field_count;
} StructDef;

typedef struct MethodDef {
	char *owner;
	size_t owner_len;
	char *name;
	size_t name_len;
	char *qualified_name;
	size_t qualified_name_len;
	bool mut_self;
	struct MethodDef *next;
} MethodDef;

/* 鐢悂鍣虹猾璇诧拷?*/
typedef enum { CONST_INT, CONST_DOUBLE, CONST_STR } ConstKind;

typedef uint32_t ModuleId;

typedef enum {
	MODULE_UNSEEN,
	MODULE_LOADING,
	MODULE_LOADED
} ModuleState;

typedef struct {
	char *path;
	size_t path_len;
	ModuleState state;
} ModuleRecord;

typedef struct ModuleImport {
	ModuleId owner;
	ModuleId target;
	char *alias;
	size_t alias_len;
} ModuleImport;

typedef struct {
	Arena *arena;
	ModuleRecord *modules;
	size_t module_count;
	size_t module_cap;
	ModuleImport *imports;
	size_t import_count;
	size_t import_cap;
} ModuleTable;

typedef struct MiraSourceInfo {
	const char *source;
	size_t source_len;
	const char *filename;
	const char *module_path;
	size_t module_path_len;
	struct MiraSourceInfo *next;
} MiraSourceInfo;

typedef struct Program {
	Arena ir_arena;
	MiraSourceInfo *source_infos;
	Pragma *pragmas;
	Def *defs;
	MiraType main_return_type;
	unsigned char main_return_type_explicit;
	int main_line;
	int main_col;
	IrNode *main_block;   /* main: { ... } 閻ㄥ嫬锟?*/
	IrNode *init_ops;     /* 妞よ泛鐪伴惃?x: 123 / y: "hello" 娴溠呮晸閻ㄥ嫬鍨垫慨瀣鎼村繐锟?*/
	/* 閸忋劌鐪崣姗€锟?*/
	char **var_names;
	size_t *var_lens;
	MiraType *var_types;
	unsigned char *var_type_explicit;
	int *var_scopes;
	int var_count;
	int var_cap;
	int *scope_parents;
	int scope_count;
	int scope_cap;
	/* 鐢悂锟?*/
	char **const_names;
	size_t *const_lens;
	MiraType *const_types;
	unsigned char *const_type_explicit;
	IrNode **const_origins;
	ConstKind *const_kinds;
	int64_t *const_ints;
	double *const_doubles;
	char **const_strs;
	size_t *const_str_lens;
	int const_count;
	int const_cap;
	/* 缂佹挻鐎担?*/
	StructDef *structs;
	int struct_count;
	int struct_cap;
	StructDef **var_structs;
	unsigned char *var_mutable;
	MethodDef *methods;
} Program;

typedef struct LexerState {
	char *src;
	char *p;
	char *filename;
	int cur_line;
	const char *line_start;
	char alias_prefix[64];
	ModuleId module_id;
	bool is_stdlib;
	struct LexerState *prev;
} LexerState;

/* --- Compiler 缂栬瘧鍣ㄧ姸??--- */
typedef struct Compiler {
	char *src;
	char *p;
	Token cur;
	Token peek;
	bool has_peek;
	FILE *out;
	const char *out_path;
	const char *filename;  /* 婧愭枃浠跺悕锛堢敤浜庨敊璇俊鎭級 */
	struct Program *prog;
	int label_id;
	int cur_line;   /* 瑜版挸澧犵悰灞藉娇锟?-based锟?*/
	int cur_col;    /* 瑜版挸澧犻崚妤€褰块敍?-based锟?*/
	const char *line_start; /* 瑜版挸澧犵悰宀勵浕閹稿洭锟?*/
	LexerState *lex_state;
	ModuleTable modules;
	ModuleId current_module;
	bool current_is_stdlib; /* 婢舵岸鍣搁弬鍥︽锟?Lexer 锟?*/

	/* 缂傛牞鐦ч柅澶愶拷?*/
	char *opt_target;
	int opt_stack;
	int opt_heap;
} Compiler;

/* --- Lexer --- */
void lexer_init(Compiler *c);
void lexer_advance(void);
bool lexer_at(TokenKind k);
bool lexer_at_peek(TokenKind k);
bool lexer_eat(TokenKind k);
void lexer_expect(TokenKind k);
Token *lexer_cur(void);
void read_token(Token *t);

/* Token 缁鐎烽崥宥忕礄閻劋绨柨娆掝嚖娣団剝浼呴敍?*/
const char *token_kind_name(TokenKind k);

/* Lexer 婢舵岸鍣搁弬鍥︽閸栧懎锟?缂傛挸鍟跨捄瀹犵┈ */
bool lexer_push_file(const char *path, const char *alias);
ModuleId module_register_import(Compiler *compiler, const char *logical, const char *alias);
void lexer_pop_file(void);

/* --- Parser / Codegen --- */
Program *parser_parse(Compiler *c);
void codegen(Compiler *c, Program *prog);

/* Import 鐠侯垰绶炵憴锝嗙€介崶鐐剁殶閿涘牏锟?main.c 鐎圭偟骞囬敍灞肩返 parser.c 鐠嬪啰鏁ら敍?*/
void parser_do_import(const char *path, const char *alias, int is_lib);


/* --- 閸愬懎鐡ㄧ粻锛勬倞 --- */
void *arena_alloc(Arena *a, size_t size);
void arena_free(Arena *a);

void IR_free(IrNode *list);
void program_free(Program *prog);

/* --- 闁挎瑨顕ら幎銉ユ啞 --- */
void mira_error(const char *src, const char *filename, int line, int col, int exit_code, const char *fmt, ...);
void mira_error_simple(int exit_code, const char *fmt, ...);

/* --- 浼樺寲绛夌骇 (0-3, 榛樿 2) --- */
extern int mira_opt_level;

#endif
