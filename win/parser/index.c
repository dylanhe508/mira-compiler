/* parser/index.c 閳?閸楀繗鐨熼崳顭掔窗鐎规矮绠熼崗鍙橀煩閻樿埖鈧降鈧礁绱╅崗銉ョ摍濡€虫健閵嗕垢arser_parse 閸忋儱褰?*/
#include "parser.h"

/* ---- 閸忓彉闊╅悩鑸碘偓?---- */
Compiler *comp;
int current_syntax_mode = 0;  /* 0=postfix, 1=infix */
int current_var_scope = 0;
int next_var_scope = 1;
int list_literal_id;
StructDef *current_impl_owner;
int current_method_mut_self;

static int token_is_followed_by_fn(const Token *token) {
	const char *next_word = token->start + token->len;
	while (*next_word == ' ' || *next_word == '\t' || *next_word == '\r') next_word++;
	return next_word[0] == 'f' && next_word[1] == 'n' &&
		!(isalnum((unsigned char)next_word[2]) || next_word[2] == '_');
}

/* ---- 瀵洖鍙嗙€涙劖膩閸?---- */
#include "helpers.c"
#include "infix.c"
#include "parse_one.c"
#include "blocks.c"

/* ---- parser_parse 姘撻垾尾銉冦儌寰?---- */
Program *parser_parse(Compiler *c) {
	comp = c;
	c->p = c->src;
	lexer_init(c);
	Program *prog = calloc(1, sizeof(Program));
	comp->prog = prog;
	module_table_init(&comp->modules, &prog->ir_arena);
	comp->current_module = module_intern_path(&comp->modules, "__root", 6);
	StructDef *impl_owner = NULL;
	current_impl_owner = NULL;
	current_method_mut_self = 0;
	current_var_scope = 0;
	next_var_scope = 1;

	while (!lexer_at(TOK_EOF)) {
		lexer_eat(TOK_NEWLINE);
		if (lexer_at(TOK_EOF)) break;

		if (impl_owner && lexer_at(TOK_RBRACE)) {
			lexer_advance();
			impl_owner = NULL;
			current_impl_owner = NULL;
			continue;
		}

		if (!impl_owner && lexer_at(TOK_ID) && lexer_cur()->len == 4 &&
		    memcmp(lexer_cur()->start, "impl", 4) == 0) {
			current_syntax_mode = 1;
			lexer_advance();
			if (!lexer_at(TOK_ID))
				mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col,
					1, "impl expects a structure name");
			impl_owner = prog_find_struct(prog, lexer_cur()->start, lexer_cur()->len);
			if (!impl_owner)
				mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col,
					1, "impl refers to an unknown structure");
			lexer_advance();
			current_impl_owner = impl_owner;
			while (lexer_eat(TOK_NEWLINE)) {}
			lexer_expect(TOK_LBRACE);
			continue;
		}

		/* Integer enums are syntax-level named constants.  Scoped variant
		 * names (`Color.Red`) lower into the existing constant table, so all
		 * current optimizers see ordinary integer values. */
		if (lexer_at(TOK_ID) && lexer_cur()->len == 4 &&
		    memcmp(lexer_cur()->start, "enum", 4) == 0) {
			current_syntax_mode = 1;
			lexer_advance();
			if (!lexer_at(TOK_ID))
				mira_error(comp->src, comp->filename, lexer_cur()->line,
					lexer_cur()->col, 1, "enum expects a name");
			char *enum_name = lexer_cur()->start;
			size_t enum_name_len = lexer_cur()->len;
			lexer_advance(); while (lexer_eat(TOK_NEWLINE)) {}
			lexer_expect(TOK_LBRACE);
			int64_t next_value = 0;
			while (!lexer_at(TOK_RBRACE) && !lexer_at(TOK_EOF)) {
				while (lexer_eat(TOK_NEWLINE)) {}
				if (lexer_at(TOK_RBRACE)) break;
				if (!lexer_at(TOK_ID))
					mira_error(comp->src, comp->filename, lexer_cur()->line,
						lexer_cur()->col, 1, "enum variant name expected");
				char *variant = lexer_cur()->start;
				size_t variant_len = lexer_cur()->len;
				lexer_advance();
				if (lexer_at(TOK_ID) && lexer_cur()->len == 1 &&
				    lexer_cur()->start[0] == '=') {
					lexer_advance();
					if (!lexer_at(TOK_INT))
						mira_error(comp->src, comp->filename, lexer_cur()->line,
							lexer_cur()->col, 1, "enum value must be an integer");
					next_value = lexer_cur()->val;
					lexer_advance();
				}
				size_t scoped_len = enum_name_len + 1 + variant_len;
				char *scoped = arena_alloc(&prog->ir_arena, scoped_len + 1);
				memcpy(scoped, enum_name, enum_name_len);
				scoped[enum_name_len] = '.';
				memcpy(scoped + enum_name_len + 1, variant, variant_len);
				scoped[scoped_len] = '\0';
				prog_add_const(prog, scoped, scoped_len, CONST_INT,
					next_value++, 0, NULL, 0);
				if (lexer_at(TOK_COMMA)) lexer_advance();
				else if (!lexer_at(TOK_NEWLINE) && !lexer_at(TOK_RBRACE))
					mira_error(comp->src, comp->filename, lexer_cur()->line,
						lexer_cur()->col, 1, "expected ',' or '}' after enum variant");
			}
			lexer_expect(TOK_RBRACE);
			continue;
		}

		/* Modern structure declaration.  Field types are accepted syntax and
		 * the names lower directly into the pre-existing StructDef table. */
		if (lexer_at(TOK_ID) && lexer_cur()->len == 6 &&
		    memcmp(lexer_cur()->start, "struct", 6) == 0) {
			current_syntax_mode = 1;
			lexer_advance();
			if (!lexer_at(TOK_ID))
				mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col,
					1, "struct expects a name");
			char *struct_name = lexer_cur()->start; size_t struct_name_len = lexer_cur()->len;
			lexer_advance(); while (lexer_eat(TOK_NEWLINE)) {}
			lexer_expect(TOK_LBRACE);
			int cap = 8, count = 0;
			char **fields = arena_alloc(&prog->ir_arena, (size_t)cap * sizeof(*fields));
			size_t *field_lens = arena_alloc(&prog->ir_arena, (size_t)cap * sizeof(*field_lens));
			while (!lexer_at(TOK_RBRACE) && !lexer_at(TOK_EOF)) {
				while (lexer_eat(TOK_NEWLINE)) {}
				if (lexer_at(TOK_RBRACE)) break;
				if (!lexer_at(TOK_ID))
					mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col,
						1, "struct field name expected");
				if (count == cap) {
					int next_cap = cap * 2;
					char **next_fields = arena_alloc(&prog->ir_arena, (size_t)next_cap * sizeof(*next_fields));
					size_t *next_lens = arena_alloc(&prog->ir_arena, (size_t)next_cap * sizeof(*next_lens));
					memcpy(next_fields, fields, (size_t)count * sizeof(*fields));
					memcpy(next_lens, field_lens, (size_t)count * sizeof(*field_lens));
					fields = next_fields; field_lens = next_lens; cap = next_cap;
				}
				fields[count] = lexer_cur()->start; field_lens[count] = lexer_cur()->len;
				count++; lexer_advance(); lexer_expect(TOK_COLON);
				if (!lexer_at(TOK_ID))
					mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col,
						1, "struct field type expected");
				lexer_advance();
				if (lexer_at(TOK_COMMA)) lexer_advance();
				else if (!lexer_at(TOK_NEWLINE) && !lexer_at(TOK_RBRACE))
					mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col,
						1, "expected ',' or '}' after struct field");
			}
			lexer_expect(TOK_RBRACE);
			prog_add_struct(prog, struct_name, struct_name_len, fields, field_lens, count);

			/* Generate a real constructor and typed field accessors over the
			 * existing managed list storage.  This keeps ownership visible to
			 * Static Reference and avoids a second object/runtime model. */
			IrNode *elements = NULL, *elements_tail = NULL;
			for (int fi = 0; fi < count; ++fi) {
				IrNode *field_value = new_ir(IR_WORD);
				field_value->u.word.name = fields[fi];
				field_value->u.word.len = field_lens[fi];
				if (!elements) elements = field_value;
				else elements_tail->next = field_value;
				elements_tail = field_value;
			}
			IrNode *constructor_body = new_ir(IR_LIST_LITERAL);
			constructor_body->u.list_literal.size = count;
			constructor_body->u.list_literal.count = count;
			constructor_body->u.list_literal.elements = elements;
			char *temp_name = arena_alloc(&prog->ir_arena, struct_name_len + 16);
			int temp_len = snprintf(temp_name, struct_name_len + 16,
				"__struct_%.*s", (int)struct_name_len, struct_name);
			constructor_body->u.list_literal.temp_slot =
				prog_add_var(prog, temp_name, (size_t)temp_len);
			Def *constructor = arena_alloc(&prog->ir_arena, sizeof(*constructor));
			memset(constructor, 0, sizeof(*constructor));
			constructor->name = struct_name; constructor->name_len = struct_name_len;
			constructor->params = fields; constructor->param_lens = field_lens;
			constructor->param_count = count; constructor->body = constructor_body;
			if (!prog->defs) prog->defs = constructor;
			else {
				Def *tail = prog->defs; while (tail->next) tail = tail->next;
				tail->next = constructor;
			}

			for (int fi = 0; fi < count; ++fi) {
				size_t getter_len = struct_name_len + 1 + field_lens[fi];
				char *getter_name = arena_alloc(&prog->ir_arena, getter_len + 1);
				memcpy(getter_name, struct_name, struct_name_len);
				getter_name[struct_name_len] = '.';
				memcpy(getter_name + struct_name_len + 1, fields[fi], field_lens[fi]);
				getter_name[getter_len] = '\0';
				char **getter_params = arena_alloc(&prog->ir_arena, sizeof(*getter_params));
				size_t *getter_lens = arena_alloc(&prog->ir_arena, sizeof(*getter_lens));
				getter_params[0] = "self"; getter_lens[0] = 4;
				IrNode *self = new_ir(IR_WORD);
				self->u.word.name = "self"; self->u.word.len = 4;
				IrNode *index = new_ir(IR_INT); index->u.i = fi;
				IrNode *get = new_ir(IR_WORD);
				get->u.word.name = "list-get"; get->u.word.len = 8;
				self->next = index; index->next = get;
				Def *getter = arena_alloc(&prog->ir_arena, sizeof(*getter));
				memset(getter, 0, sizeof(*getter));
				getter->name = getter_name; getter->name_len = getter_len;
				getter->params = getter_params; getter->param_lens = getter_lens;
				getter->param_count = 1; getter->body = self;
				Def *tail = prog->defs; while (tail->next) tail = tail->next;
				tail->next = getter;
			}
			continue;
		}

		/* Modern infix function syntax:
		 *   fn name(a, b) { ... }
		 * Seeing `fn` opts the file into infix mode; no pragma is required. */
		if (lexer_at(TOK_ID) && lexer_cur()->len == 2 &&
		    memcmp(lexer_cur()->start, "fn", 2) == 0) {
			current_syntax_mode = 1;
			lexer_advance();
			if (!lexer_at(TOK_ID)) {
				mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1,
					"'fn' expects a function name");
			}
			char *name = lexer_cur()->start;
			size_t name_len = lexer_cur()->len;
			lexer_advance();
			int source_is_main = name_len == 4 && memcmp(name, "main", 4) == 0;
			if (!impl_owner && comp->current_module != 0) {
				if (source_is_main)
					mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col,
						1, "imported module cannot define main");
				name = module_qualify_symbol(&comp->modules, comp->current_module, name, name_len);
				name_len = strlen(name);
			}
			char *source_method_name = name;
			size_t source_method_len = name_len;
			if (impl_owner) {
				size_t qualified_len = impl_owner->name_len + 1 + name_len;
				char *qualified = arena_alloc(&prog->ir_arena, qualified_len + 1);
				memcpy(qualified, impl_owner->name, impl_owner->name_len);
				qualified[impl_owner->name_len] = '.';
				memcpy(qualified + impl_owner->name_len + 1, name, name_len);
				qualified[qualified_len] = '\0';
				name = qualified;
				name_len = qualified_len;
			}
			lexer_expect(TOK_LPAREN);

			int param_cap = 4;
			int param_count = 0;
			bool method_mut_self = false;
			char **params = arena_alloc(&prog->ir_arena, (size_t)param_cap * sizeof(char *));
			size_t *param_lens = arena_alloc(&prog->ir_arena, (size_t)param_cap * sizeof(size_t));
			while (!lexer_at(TOK_RPAREN) && !lexer_at(TOK_EOF)) {
				if (impl_owner && param_count == 0 && lexer_at(TOK_ID) &&
				    lexer_cur()->len == 3 && memcmp(lexer_cur()->start, "mut", 3) == 0) {
					method_mut_self = true;
					lexer_advance();
				}
				if (!lexer_at(TOK_ID)) {
					mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1,
						"function parameter must be an identifier");
				}
				if (param_count == param_cap) {
					int new_cap = param_cap * 2;
					char **new_params = arena_alloc(&prog->ir_arena, (size_t)new_cap * sizeof(char *));
					size_t *new_lens = arena_alloc(&prog->ir_arena, (size_t)new_cap * sizeof(size_t));
					memcpy(new_params, params, (size_t)param_count * sizeof(char *));
					memcpy(new_lens, param_lens, (size_t)param_count * sizeof(size_t));
					params = new_params;
					param_lens = new_lens;
					param_cap = new_cap;
				}
				params[param_count] = lexer_cur()->start;
				param_lens[param_count] = lexer_cur()->len;
				param_count++;
				lexer_advance();
				/* Type annotations are syntax-layer metadata for now.  The
				 * existing scalar SSA remains the lowering target. */
				if (lexer_at(TOK_COLON)) {
					lexer_advance();
					if (!lexer_at(TOK_ID))
						mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col,
							1, "parameter type name expected after ':'");
					lexer_advance();
				}
				if (lexer_at(TOK_COMMA)) lexer_advance();
				else if (!lexer_at(TOK_RPAREN)) {
					mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1,
						"expected ',' or ')' after function parameter");
				}
			}
			lexer_expect(TOK_RPAREN);
			/* Optional modern return type: fn f(...) -> i64 { ... } */
			if (lexer_at(TOK_ID) && lexer_cur()->len == 2 &&
			    memcmp(lexer_cur()->start, "->", 2) == 0) {
				lexer_advance();
				if (!lexer_at(TOK_ID))
					mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col,
						1, "return type name expected after '->'");
				lexer_advance();
			}
			while (lexer_eat(TOK_NEWLINE)) {}
			lexer_expect(TOK_LBRACE);
			if (impl_owner) current_method_mut_self = method_mut_self ? 1 : 0;
			int saved_var_scope = current_var_scope;
			current_var_scope = prog_new_var_scope(prog, saved_var_scope);
			IrNode *body = parse_block_content(prog);
			current_var_scope = saved_var_scope;
			lexer_expect(TOK_RBRACE);
			if (impl_owner) current_method_mut_self = 0;

			if (!impl_owner && source_is_main) {
				if (param_count != 0) {
					mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1,
						"main function cannot have parameters");
				}
				/* ���� var/����ʽ��ʼ������׷�ӽ� main_block,fn main body ��Դ��˳��Ӻ�(�뾭�� main:{} һ��),���⸲�Ƕ�ʧȫ�ֳ�ʼ���� */
				if (prog->main_block) {
					IrNode *p = prog->main_block;
					while (p->next) p = p->next;
					p->next = body;
				} else {
					prog->main_block = body;
				}
			} else {
				Def *d = arena_alloc(&prog->ir_arena, sizeof(Def));
				memset(d, 0, sizeof(Def));
				d->name = name;
				d->name_len = name_len;
				d->params = params;
				d->param_lens = param_lens;
				d->param_count = param_count;
				d->body = body;
				if (impl_owner)
					prog_add_method(prog, impl_owner, source_method_name, source_method_len,
						name, name_len, method_mut_self);
				if (!prog->defs) prog->defs = d;
			else {
					Def *tail = prog->defs;
					while (tail->next) tail = tail->next;
					tail->next = d;
				}
			}
			continue;
		}

		/* Mira 6 modern module import: import std.math [as alias]; */
		if (lexer_at(TOK_ID) && lexer_cur()->len == 6 &&
		    memcmp(lexer_cur()->start, "import", 6) == 0 && lexer_at_peek(TOK_ID)) {
			lexer_advance();
			char logical[512];
			size_t logical_len = lexer_cur()->len;
			if (logical_len == 0 || logical_len >= sizeof(logical))
				mira_error(comp->src, comp->filename, lexer_cur()->line,
					lexer_cur()->col, 1, "module path is too long");
			memcpy(logical, lexer_cur()->start, logical_len);
			logical[logical_len] = '\0';
			for (size_t i = 0; i < logical_len; ++i)
				if (logical[i] == '.') logical[i] = '/';
			int is_std = logical_len > 4 && memcmp(logical, "std/", 4) == 0;
			lexer_advance();
			char alias[128] = {0};
			if (lexer_at(TOK_ID) && lexer_cur()->len == 2 &&
			    memcmp(lexer_cur()->start, "as", 2) == 0) {
				lexer_advance();
				if (!lexer_at(TOK_ID) || lexer_cur()->len >= sizeof(alias))
					mira_error(comp->src, comp->filename, lexer_cur()->line,
						lexer_cur()->col, 1, "module alias expected after 'as'");
				memcpy(alias, lexer_cur()->start, lexer_cur()->len);
				alias[lexer_cur()->len] = '\0';
				lexer_advance();
			}
			if (!(lexer_at(TOK_ID) && lexer_cur()->len == 1 &&
			      lexer_cur()->start[0] == ';'))
				mira_error(comp->src, comp->filename, lexer_cur()->line,
					lexer_cur()->col, 1, "modern import must end with ';'");
			parser_do_import(logical, alias[0] ? alias : NULL, is_std ? 2 : 3);
			continue;
		}
		if (lexer_at(TOK_PRAGMA)) {
			const char *pname = lexer_cur()->start;
			size_t plen = lexer_cur()->len;

			/* !import "path" or !import <libname> 鑺掗埀?鑾介垾閮濈柕蔚閾般儍尾寰蜂急銉傛偢寰濐煀浠胯秮銉⑩偓尾銉冿腹鈧挴鈧埗銇㈠?*/
			if ((plen == 6 && memcmp(pname, "import", 6) == 0)) {
				lexer_advance(); /* 姘撹伂鑼犲繖娌ら垾?"import" */
				/* 鐚悅鎷㈠繖鍟伂鐚矾鐐夋皳鎴垾? "..." 蹇欒棝?<...> */
				char import_path[512] = {0};
				int is_lib = 0;
				if (lexer_at(TOK_STR)) {
					size_t slen = lexer_cur()->str_len < 511 ? lexer_cur()->str_len : 511;
					memcpy(import_path, lexer_cur()->str, slen);
					import_path[slen] = '\0';
					lexer_advance();
				} else if (lexer_at(TOK_ID) && *lexer_cur()->start == '<') {
					/* 姘撹矾铏忚幗绂勮伀鐚瀯?lexer 姘撻檰閳ユ簫锔光偓鈷欑柕銇?<libname> 鐚倝閳ョ妴銉嬪枴?*/
					size_t slen = lexer_cur()->len < 511 ? lexer_cur()->len : 511;
					/* 姘撴菠绂勫繖娌ら垾鎳娾敒锔光偓鎾併儌濂?<> */
					if (slen > 2) { memcpy(import_path, lexer_cur()->start + 1, slen - 2); import_path[slen-2] = '\0'; }
					is_lib = 1;
					lexer_advance();
				} else {
					/* 鑾藉簮閳ь優銉?token 鐚矾鐐夋皳鎴垾鐏活嚪濡撳枹尾鐚溾埗蔚鎾偓鎳娿儌灏栤偓鈷氥儌寰涜顕峰皷鈧?*/
					size_t slen = lexer_cur()->len < 511 ? lexer_cur()->len : 511;
					memcpy(import_path, lexer_cur()->start, slen);
					import_path[slen] = '\0';
					is_lib = 1;
					lexer_advance();
				}
				if (lexer_at(TOK_NEWLINE)) {
					/* Keep cur = NEWLINE: read_token already advanced p to
					 * the next line start. Pre-reading here would make
					 * lexer_push_file save a resume point inside the following
					 * !import pragma; after pop, <lib> is read as a standalone
					 * TOK_ID and the module never loads. */
				} else {
					lexer_eat(TOK_NEWLINE);
				}
				/* 鐚悅鎷㈠繖鍟伂姘撹伀鐐夎寘閳ь兘鈧噴顬ｂ檧鈧?as alias */
				char alias[64] = {0};
				if (lexer_at(TOK_ID) && lexer_cur()->len == 2 && memcmp(lexer_cur()->start, "as", 2) == 0) {
					lexer_advance();
					if (lexer_at(TOK_ID)) {
						size_t al = lexer_cur()->len < 63 ? lexer_cur()->len : 63;
						memcpy(alias, lexer_cur()->start, al);
						alias[al] = '\0';
						lexer_advance();
					}
				}
				/* 鐚悅鎷㈠繖鍟伂鑾借墮楦ユ皳搴愬暘鐚矾鐐夋皳鎴垾鐏活嚪濡撳枹銉傃€鈧緷锔光偓鍏簫顬犵儵鍔?main.c 鑾芥嫥閳ョ伝銉⑩偓尾顭嬨儌鎵佸亾鐚埉閳ノ炽儏鐘呪敓銉⑩偓梅鐭腹鈧挋鎳婎嚪?/
				/* 鐚矾鐐夋皳鎴垾鐏活煀顬狅迹蔚闊兎顬狀厸鈧拋褉鈧挋? !import "file" 鑾介垾閮濊銉傤嚪濮戙儌瑙ｂ偓婧嶃儮鈧ゥ宓滐腹鈧挴鈧埗銇㈠鐫寡€鈧儩顔氥儌瑙ｂ偓? !import <lib> 鑾介垾閮濊銉傤嚪?libs-mira/ */
				parser_do_import(import_path, alias[0] ? alias : NULL, is_lib);
				continue;
			}

			if ((plen == 6 && memcmp(pname, "syntax", 6) == 0)) {
				lexer_advance(); /* 姘撹伂鑼犲繖娌ら垾?"syntax" */
				if (lexer_at(TOK_ID)) {
					if (lexer_cur()->len == 5 && memcmp(lexer_cur()->start, "infix", 5) == 0) {
						current_syntax_mode = 1;
					} else if (lexer_cur()->len == 7 && memcmp(lexer_cur()->start, "postfix", 7) == 0) {
						current_syntax_mode = 0;
					} else {
						mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1, "Unknown syntax mode, expected 'infix' or 'postfix'");
					}
					lexer_advance();
				} else {
					mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1, "Expected syntax mode 'infix' or 'postfix' after !syntax");
				}
				continue;
			}

			/* 姘撻垾尾鐫广仮鐑┾偓?pragma 鑺掗埀?蹇欓鎷㈡皳璧傝祩姘撻钘存皳閳ノ层儍鈹锯偓婧屼箙顭娒?*/
			Pragma *p = arena_alloc(&comp->prog->ir_arena, sizeof(Pragma));
			memset(p, 0, sizeof(Pragma));
			p->name = (char *)pname;
			p->name_len = plen;
			lexer_advance();
			if (!lexer_at(TOK_NEWLINE) && !lexer_at(TOK_EOF)) {
				p->arg = lexer_cur()->start;
				while (!lexer_at(TOK_NEWLINE) && !lexer_at(TOK_EOF)) lexer_advance();
			}
			p->next = prog->pragmas;
			prog->pragmas = p;
			continue;
		}



		/* extern id: { args } (姘撻檱閳ユ拋鈹㈡崠顭嬨儮鈧风煫锔光偓鈷欐噴銉傦饥鎳娢绘簯鐭嚪瀵傛崡锔光偓鏂?body) */
		if (lexer_at(TOK_ID) && lexer_cur()->len == 6 && memcmp(lexer_cur()->start, "extern", 6) == 0 &&
		    !token_is_followed_by_fn(lexer_cur())) {
			lexer_advance();
			if (!lexer_at(TOK_ID)) { mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1, "'extern' expects an identifier name"); }
			char *ename = lexer_cur()->start;
			size_t elen = lexer_cur()->len;
			lexer_advance();
			lexer_expect(TOK_COLON);
			lexer_expect(TOK_LBRACE);
			char **params = NULL;
			size_t *param_lens = NULL;
			int nparam = 0;
			/* 鐚┐閳┾挌鈹锯偓鈭ㄦ崡銇㈠槑宓溿剢鎹栫煫褉鈧?parse_param_list鑼傚綍鑹楁皳閳ラ儩鐘嗐仮鍢庡ソ銉傤啙鎹椼儌鍢幬澄垫挴鈧?LBRACE 蹇欒伀鑱皳鑱垾鎾侇嚪瀵傛崡位鍡忊偓妯忋仮瀹︻優锔光偓鎵斥偓濮戙儏鐘咁煁尾寰涙兎銉傚繆鈧拋銉傚繆鈧锔光偓?*/
			int cap = 4;
			params = arena_alloc(&comp->prog->ir_arena, (size_t)cap * sizeof(char *));
			param_lens = arena_alloc(&comp->prog->ir_arena, (size_t)cap * sizeof(size_t));
			while (lexer_at(TOK_ID)) {
				if (nparam >= cap) {
					cap *= 2;
					char **np = arena_alloc(&comp->prog->ir_arena, (size_t)cap * sizeof(char *));
					size_t *nl = arena_alloc(&comp->prog->ir_arena, (size_t)cap * sizeof(size_t));
					memcpy(np, params, nparam * sizeof(char *));
					memcpy(nl, param_lens, nparam * sizeof(size_t));
					params = np; param_lens = nl;
				}
				params[nparam] = lexer_cur()->start;
				param_lens[nparam] = lexer_cur()->len;
				nparam++;
				lexer_advance();
			}
			lexer_expect(TOK_RBRACE);
			Def *d = arena_alloc(&comp->prog->ir_arena, sizeof(Def));
			memset(d, 0, sizeof(Def));
			d->name = ename;
			d->name_len = elen;
			d->params = params;
			d->param_lens = param_lens;
			d->param_count = nparam;
			d->is_extern = true;
			d->body = NULL;
			d->next = NULL;
			if (!prog->defs) prog->defs = d;
			else {
				Def *tail = prog->defs;
				while (tail->next) tail = tail->next;
				tail->next = d;
			}
			continue;
		}

		/* const id : value */
		if (lexer_at(TOK_ID) && lexer_cur()->len == 5 && memcmp(lexer_cur()->start, "const", 5) == 0) {
			lexer_advance();
			if (!lexer_at(TOK_ID)) { mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1, "'const' expects an identifier name"); }
			char *cname = lexer_cur()->start;
			size_t clen = lexer_cur()->len;
			lexer_advance();
			if (current_syntax_mode == 1) {
				if (lexer_at(TOK_COLON)) {
					lexer_advance();
					if (!lexer_at(TOK_ID))
						mira_error(comp->src, comp->filename, lexer_cur()->line,
							lexer_cur()->col, 1, "constant type name expected after ':'");
					lexer_advance();
				}
				if (!(lexer_at(TOK_ID) && lexer_cur()->len == 1 && lexer_cur()->start[0] == '='))
					mira_error(comp->src, comp->filename, lexer_cur()->line,
						lexer_cur()->col, 1, "modern const declaration expects '='");
				lexer_advance();
			} else {
				lexer_expect(TOK_COLON);
			}
			if (lexer_at(TOK_INT)) {
				prog_add_const(prog, cname, clen, CONST_INT, lexer_cur()->val, 0, NULL, 0);
				lexer_advance();
			} else if (lexer_at(TOK_FLOAT)) {
				prog_add_const(prog, cname, clen, CONST_DOUBLE, 0, lexer_cur()->dbl, NULL, 0);
				lexer_advance();
			} else if (lexer_at(TOK_STR)) {
				prog_add_const(prog, cname, clen, CONST_STR, 0, 0, lexer_cur()->str, lexer_cur()->str_len);
				lexer_advance();
			} else {
				mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1, "'const' expects a number or string after ':'");
			}
			continue;
		}

		/* Mira 6 typed foreign declaration: extern fn symbol(a: i64) -> i64; */
		if (lexer_at(TOK_ID) && lexer_cur()->len == 6 &&
		    memcmp(lexer_cur()->start, "extern", 6) == 0 && token_is_followed_by_fn(lexer_cur())) {
			lexer_advance();
			if (!(lexer_at(TOK_ID) && lexer_cur()->len == 2 &&
			      memcmp(lexer_cur()->start, "fn", 2) == 0))
				mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col,
					1, "modern extern expects 'fn'");
			lexer_advance();
			if (!lexer_at(TOK_ID))
				mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col,
					1, "extern fn expects a symbol name");
			char *name = lexer_cur()->start;
			size_t name_len = lexer_cur()->len;
			if (!comp->current_is_stdlib && name_len >= 7 && memcmp(name, "__mira_", 7) == 0)
				mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col,
					1, "reserved runtime symbol '%.*s' is not available to user code",
					(int)name_len, name);
			lexer_advance();
			lexer_expect(TOK_LPAREN);
			int cap = 4, count = 0;
			char **params = arena_alloc(&prog->ir_arena, (size_t)cap * sizeof(char *));
			size_t *param_lens = arena_alloc(&prog->ir_arena, (size_t)cap * sizeof(size_t));
			while (!lexer_at(TOK_RPAREN) && !lexer_at(TOK_EOF)) {
				if (!lexer_at(TOK_ID))
					mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col,
						1, "extern parameter name expected");
				if (count == cap) {
					int next_cap = cap * 2;
					char **next_params = arena_alloc(&prog->ir_arena, (size_t)next_cap * sizeof(char *));
					size_t *next_lens = arena_alloc(&prog->ir_arena, (size_t)next_cap * sizeof(size_t));
					memcpy(next_params, params, (size_t)count * sizeof(char *));
					memcpy(next_lens, param_lens, (size_t)count * sizeof(size_t));
					params = next_params; param_lens = next_lens; cap = next_cap;
				}
				params[count] = lexer_cur()->start;
				param_lens[count++] = lexer_cur()->len;
				lexer_advance();
				lexer_expect(TOK_COLON);
				if (!lexer_at(TOK_ID))
					mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col,
						1, "extern parameter type expected");
				lexer_advance();
				if (lexer_at(TOK_COMMA)) lexer_advance();
				else if (!lexer_at(TOK_RPAREN))
					mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col,
						1, "expected ',' or ')' in extern declaration");
			}
			lexer_expect(TOK_RPAREN);
			if (lexer_at(TOK_ID) && lexer_cur()->len == 2 &&
			    memcmp(lexer_cur()->start, "->", 2) == 0) {
				lexer_advance();
				if (!lexer_at(TOK_ID))
					mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col,
						1, "extern return type expected");
				lexer_advance();
			}
			if (!(lexer_at(TOK_ID) && lexer_cur()->len == 1 && lexer_cur()->start[0] == ';'))
				mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col,
					1, "extern fn declaration must end with ';'");

			Def *d = arena_alloc(&prog->ir_arena, sizeof(*d));
			memset(d, 0, sizeof(*d));
			d->name = name; d->name_len = name_len;
			d->params = params; d->param_lens = param_lens; d->param_count = count;
			d->is_extern = true;
			if (!prog->defs) prog->defs = d;
			else { Def *tail = prog->defs; while (tail->next) tail = tail->next; tail->next = d; }
			continue;
		}
		/* extern name: { params } 鑺掗埀?姘撻檱閳ユ拋鈹㈡崠顭嬨儮鈧风煫锔光偓鈷欐噴銉傦饥鎳娢绘簯?*/
		if (lexer_at(TOK_ID) && lexer_cur()->len == 6 && memcmp(lexer_cur()->start, "extern", 6) == 0) {
			lexer_advance(); /* 姘撹伂鑼犲繖娌ら垾?"extern" */
			if (!lexer_at(TOK_ID)) {
				mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1, "'extern' expects a function name");
			}
			char *name = lexer_cur()->start;
			size_t name_len = lexer_cur()->len;
			lexer_advance(); /* 姘撹伂鑼犲繖娌ら垾鎳娿儮鈧风煫锔光偓鈷欐噴銉?*/
			lexer_expect(TOK_COLON); /* 姘撹伂鑼犲繖娌ら垾?: */
			lexer_eat(TOK_NEWLINE);

			Def *d = arena_alloc(&comp->prog->ir_arena, sizeof(Def));
			memset(d, 0, sizeof(Def));
			d->name = name;
			d->name_len = name_len;
			d->is_extern = true;
			d->body = NULL;
			d->next = NULL;

			/* 鐚悅鎷㈠繖鍟伂姘撹伀閳ユ锔光偓鈷欐噴銉嬪棌鈧枂顭娒?{ a b c } */
			if (lexer_at(TOK_LBRACE)) {
				char **params = NULL;
				size_t *param_lens = NULL;
				int nparam = 0;
				parse_param_list(&params, &param_lens, &nparam);
				d->params = params;
				d->param_lens = param_lens;
				d->param_count = nparam;
			}

			/* 鐚┐闄嗘皳鑹╄伣姘撹棝?defs 鑼呴垾婧屼箙顭娒?*/
			if (!prog->defs) prog->defs = d;
			else {
				Def *tail = prog->defs;
				while (tail->next) tail = tail->next;
				tail->next = d;
			}
			continue;
		}

		/* import "filename" (Legacy syntax) */
		if (lexer_at(TOK_ID) && lexer_cur()->len == 6 && memcmp(lexer_cur()->start, "import", 6) == 0) {
			lexer_advance(); /* Eat "import" */
			if (lexer_at(TOK_STR)) {
				char import_path[512] = {0};
				size_t slen = lexer_cur()->str_len < 511 ? lexer_cur()->str_len : 511;
				memcpy(import_path, lexer_cur()->str, slen);
				import_path[slen] = '\0';
				lexer_advance();
				parser_do_import(import_path, NULL, 0); /* Parse content directly into IR */
			}
			continue;
		}

		/* : name ... ; (Classic concatenative syntax) */
		if (lexer_at(TOK_COLON)) {
			lexer_advance(); /* Eat : */
			if (!lexer_at(TOK_ID)) { mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1, "':' expects a function name"); }
			char *name = lexer_cur()->start;
			size_t name_len = lexer_cur()->len;
			lexer_advance(); /* Eat name */
			
			Def *d = arena_alloc(&comp->prog->ir_arena, sizeof(Def));
			memset(d, 0, sizeof(Def));
			d->name = name;
			d->name_len = name_len;
			d->next = NULL;
			
			IrNode *expr_head = NULL;
			/* 娴ｈ法鏁?parse_block_content 娴犮儲鏁幐?if/while/for 缁涘鎮楃紓鈧幒褍鍩楀ù浣稿彠闁款喖鐡?*/
			int saved_var_scope = current_var_scope;
			current_var_scope = prog_new_var_scope(prog, saved_var_scope);
			expr_head = parse_block_content(prog);
			current_var_scope = saved_var_scope;
			/* 閸氬啯甯€ ; 缂佸牊顒涚粭?*/
			if (lexer_at(TOK_ID) && lexer_cur()->len == 1 && lexer_cur()->start[0] == ';') {
				lexer_advance();
			}
			d->body = expr_head;
			if (!prog->defs) prog->defs = d;
			else {
				Def *tail = prog->defs;
				while (tail->next) tail = tail->next;
				tail->next = d;
			}
			continue;
		}

		/* name: ... 鑺掗埀?main: { ... } 蹇欒棝?x: 123 / y: "hello" */
		if (lexer_at(TOK_ID) && lexer_at_peek(TOK_COLON)) {
			char *name = lexer_cur()->start;
			size_t name_len = lexer_cur()->len;

			lexer_advance();
			/* 閸氬啯甯€閸愭帒褰?*/
			lexer_advance();
			lexer_eat(TOK_NEWLINE);

			/* 缁犫偓閸楁洝绁撮崐纭风窗x: 123 閹?y: "hello" */
			if (lexer_at(TOK_INT) || lexer_at(TOK_STR)) {
				IrNode *v;
				if (lexer_at(TOK_INT)) {
					v = new_ir(IR_INT);
					v->u.i = lexer_cur()->val;
					lexer_advance();
				} else {
					v = new_ir(IR_STR);
					v->u.str.s = lexer_cur()->str;
					v->u.str.len = lexer_cur()->str_len;
					lexer_advance();
				}
				IrNode *chain = make_assign_chain(prog, name, name_len, v);
				if (!prog->init_ops) prog->init_ops = chain;
				else {
					IrNode *t = prog->init_ops;
					while (t->next) t = t->next;
					t->next = chain;
				}
				continue;
			}

			/* main: { ... } */
			if (name_len == 4 && memcmp(name, "main", 4) == 0 && lexer_at(TOK_LBRACE)) {
				lexer_advance();            /* 閸氬啯甯€ { */
				int saved_var_scope = current_var_scope;
				current_var_scope = prog_new_var_scope(prog, saved_var_scope);
				IrNode *main_block = parse_block_content(prog);
				current_var_scope = saved_var_scope;
				lexer_expect(TOK_RBRACE);
				/* 婵″倹鐏夊鍙夋箒 main_block 鐏忓崬鎮庨獮璁圭礄鏉╄棄濮為崚鐗堟汞鐏忔拝绱濇俊?async 閸ョ偠鐨熸潻钘夊閿?*/
				if (prog->main_block) {
					IrNode *p = prog->main_block;
					while (p->next) p = p->next;
					p->next = main_block;
				} else {
					prog->main_block = main_block;
				}
				continue;
			}

			/* 閸氾箑鍨弰顖氬毐閺佹澘鐣炬稊澶涚窗name: { params } body 閹?name: { body } */
			Def *d = arena_alloc(&comp->prog->ir_arena, sizeof(Def));
			memset(d, 0, sizeof(Def));
			d->name = name;
			d->name_len = name_len;
			d->next = NULL;
			if (!prog->defs) prog->defs = d;
			else {
				Def *tail = prog->defs;
				while (tail->next) tail = tail->next;
				tail->next = d;
			}

			if (lexer_at(TOK_LBRACE)) {
				/* 閸掋倖鏌?{ a b } 閸氬酣娼扮捄鐔烘畱閺勵垰寮弫?+ body  鏉╂ɑ妲? { body } 閳?闁俺绻冮崑椋庢箙娑撳绔存稉?token */
				if (lexer_at_peek(TOK_ID) || lexer_at_peek(TOK_RBRACE)) {
					char **params = NULL;
					size_t *param_lens = NULL;
					int nparam = 0;
					parse_param_list(&params, &param_lens, &nparam);

					/* 妫€鏌ユ槸鍚︽槸 struct 璇硶锛歂ame: {fields} struct */
					if (lexer_at(TOK_ID) && lexer_cur()->len == 6 && memcmp(lexer_cur()->start, "struct", 6) == 0) {
						lexer_advance();
						prog_add_struct(prog, name, name_len, params, param_lens, nparam);
					/* 娴?defs 闁炬崘銆冩稉顓犘╅梽銈呭嚒濞ｈ濮為惃?d */
						if (prog->defs == d) { prog->defs = d->next; }
						else { Def *prev = prog->defs; while (prev && prev->next != d) prev = prev->next; if (prev) prev->next = d->next; }
						free(d);
						continue;
					}

					/* 甯﹀弬鏁扮殑鍑芥暟锛歛dd: { a b } a b + */
					d->params = params;
					d->param_lens = param_lens;
					d->param_count = nparam;
					int saved_var_scope = current_var_scope;
					current_var_scope = prog_new_var_scope(prog, saved_var_scope);
					d->body = parse_body_until_newline(prog);
					current_var_scope = saved_var_scope;
				} else {
					/* 濞屸剝婀侀崣鍌涙殶閿涘瞼鍑?body 閸?*/
					lexer_expect(TOK_LBRACE);
					int saved_var_scope = current_var_scope;
					current_var_scope = prog_new_var_scope(prog, saved_var_scope);
					d->body = parse_block_content(prog);
					current_var_scope = saved_var_scope;
					lexer_expect(TOK_RBRACE);
				}
			} else {
				mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1, "function/definition expects { ... }");
			}
			continue;
		}

		/* 妞よ泛鐪扮悰銊ㄦ彧瀵骏绱欐俊?"Hello" print 缁涘绱氶敍灞炬暪闂嗗棗鍩?main_block */
		if (lexer_at(TOK_ID) && lexer_cur()->len == 1 && lexer_cur()->start[0] == ';') {
			lexer_advance(); /* ���� var ����/����ʽ��� ';' ������ս��,ֱ������(�� blocks.c һ��),����ᱻ���� word ���� call ; */
			continue;
		}
		/* Top-level bare assignment `g = 1;` is invalid: globals can only
		 * be introduced via `var g = 1;`.  Intercept before parse_one so
		 * the failure is a clear syntax error instead of a link-time one
		 * (the '=' would otherwise be parsed as a word and emitted as a
		 * call to a symbol named "="). */
		if (lexer_at(TOK_ID) && lexer_at_peek(TOK_ID) &&
		    comp->peek.len == 1 && comp->peek.start[0] == '=' &&
		    !(lexer_cur()->len == 3 && memcmp(lexer_cur()->start, "var", 3) == 0)) {
			mira_error(comp->src, comp->filename, lexer_cur()->line,
				lexer_cur()->col, 1,
				"top-level assignment is not allowed; declare it with 'var %.*s = value'",
				(int)lexer_cur()->len, lexer_cur()->start);
		}
		IrNode *o = (current_syntax_mode == 0 && lexer_at(TOK_ID) && lexer_at_peek(TOK_LPAREN)) ?
		            parse_infix_line(prog) : parse_one(prog, false);
		if (o) {
			/* Top-level postfix: { cond } { body } while 閹存牞鈧?{ cond } { body } if */
			if (o->kind == IR_BLOCK) {
				while (lexer_eat(TOK_NEWLINE)) {}
				if (lexer_at(TOK_LBRACE)) {
					IrNode *second = parse_one(prog, false);
					if (second && second->kind == IR_BLOCK) {
						while (lexer_eat(TOK_NEWLINE)) {}
						if (lexer_at(TOK_ID) && lexer_cur()->len == 5 &&
						    memcmp(lexer_cur()->start, "while", 5) == 0) {
							lexer_advance();
							IrNode *wh = new_ir(IR_WHILE_COND);
							wh->u.while_cond.cond = o;
							wh->u.while_cond.body = second;
							o = wh;
						} else if (lexer_at(TOK_ID) && lexer_cur()->len == 2 &&
						           memcmp(lexer_cur()->start, "if", 2) == 0) {
							lexer_advance();
							IrNode *iff = new_ir(IR_IF);
							iff->u.iff.cond = o;
							iff->u.iff.then_b = second;
							iff->u.iff.else_b = NULL; /* top-level simple if */
							o = iff;
						} else {
							if (!prog->main_block) prog->main_block = o;
							else { IrNode *tt = prog->main_block; while (tt->next) tt = tt->next; tt->next = o; }
							o = second;
						}
					} else if (second) {
						if (!prog->main_block) prog->main_block = o;
						else { IrNode *tt = prog->main_block; while (tt->next) tt = tt->next; tt->next = o; }
						o = second;
					}
				} else if (lexer_at(TOK_ID) && lexer_cur()->len == 2 &&
				           memcmp(lexer_cur()->start, "if", 2) == 0) {
					/* {body} if */
					lexer_advance();
					IrNode *iff = new_ir(IR_IF);
					iff->u.iff.cond = NULL; /* or just treat as truthy, though usually Mira expects cond on stack */
					iff->u.iff.then_b = o;
					iff->u.iff.else_b = NULL;
					o = iff;
				}
			}
			if (!prog->main_block) prog->main_block = o;
			else {
				IrNode *tt = prog->main_block;
				while (tt->next) tt = tt->next;
				tt->next = o;
			}
			continue;
		}

		/* 閺冪姵纭剁憴锝嗙€介惃?Token 閳?鐠哄疇绻?*/
		lexer_advance();
	}

	/* 婵″倹鐏夐張?x: 123 娴溠呮晸閻ㄥ嫬鍨垫慨瀣娴狅絿鐖滈敍灞芥値楠炶泛鍩?main 閸忋儱褰涢惃鍕闂?*/
	if (prog->init_ops && prog->main_block) {
		IrNode *t = prog->init_ops;
		while (t->next) t = t->next;
		t->next = prog->main_block;
		prog->main_block = prog->init_ops;
		prog->init_ops = NULL;
	} else if (prog->init_ops) {
		prog->main_block = prog->init_ops;
		prog->init_ops = NULL;
	}

	return prog;
}


