/* parser/blocks.c �??閸ф楠囩憴锝嗙€介崳?*/
#include "parser.h"
/* 鐟欙絾鐎介崸妤€鍞�?�€�?櫢绱濋惄鏉戝煂闁�?洤鍩?loop 閸忔娊鏁€?*/
static IrNode *parse_block_until_loop(Program *prog) {
	IrNode *head = NULL, *tail = NULL;
	unsigned iter = 0;
	const unsigned max_iter = 100000;
	while (!lexer_at(TOK_EOF)) {
		if (++iter > max_iter) {
			mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1, "too many tokens before 'loop' (possible infinite loop)");
		}
		while (lexer_eat(TOK_NEWLINE)) {
			if (lexer_at(TOK_EOF)) break;
		}
		if (lexer_at(TOK_EOF)) break;
		/* 闁洤鍩?loop 鐏忚�?�?ㄩ弶鐔恍掗弸?*/
		if (lexer_at(TOK_ID) && lexer_cur()->len == 4 && memcmp(lexer_cur()->start, "loop", 4) == 0) {
			lexer_advance();
			break;
		}
		if (lexer_at(TOK_ID) && lexer_at_peek(TOK_COLON)) {
			char *name = lexer_cur()->start;
			size_t len = lexer_cur()->len;
			lexer_advance();
			lexer_advance();
			IrNode *expr_head = NULL, *expr_tail = NULL;
			while (!lexer_at(TOK_NEWLINE) && !lexer_at(TOK_EOF) &&
			       !(lexer_at(TOK_ID) && lexer_cur()->len == 4 && memcmp(lexer_cur()->start, "loop", 4) == 0)) {
				IrNode *o = parse_one(prog, false);
				if (!o) break;
				if (!expr_head) expr_head = expr_tail = o; else { expr_tail->next = o; expr_tail = o; }
			}
			if (expr_head) {
				int slot = prog_add_var(prog, name, len);
				IrNode *var_op = new_ir(IR_VAR);
				var_op->u.var_slot = slot;
				IrNode *store_op = new_ir(IR_WORD);
				store_op->u.word.name = "!";
				store_op->u.word.len = 1;
				for (expr_tail = expr_head; expr_tail->next; expr_tail = expr_tail->next);
				expr_tail->next = var_op;
				var_op->next = store_op;
				if (!head) head = tail = expr_head; else { tail->next = expr_head; }
				while (tail->next) tail = tail->next;
				continue;
			}
		}
		IrNode *o = parse_one(prog, false);
		if (!o) break;
		if (!head) head = tail = o; else { tail->next = o; tail = o; }
	}
	return head;
}

/* 濡偓閺屻儱缍�??�??token 閺勵垰鎯�?弰�?�氬瀻閸?(`;` �??TOK_ID, len=1, start[0]=';') */
#define IS_SEMICOLON() (lexer_at(TOK_ID) && lexer_cur()->len == 1 && lexer_cur()->start[0] == ';')

/* 鐟欙絾鐎介崸妤€鍞�?�€�?櫢绱濋惄鏉戝煂闁�?洤鍩?} �??; �??EOF */
static IrNode *parse_block_content(Program *prog) {
	IrNode *head = NULL, *tail = NULL;
	unsigned iter = 0;
	const unsigned max_iter = 100000;
	while (!lexer_at(TOK_RBRACE) && !lexer_at(TOK_EOF) && !IS_SEMICOLON()) {
		if (++iter > max_iter) {
			mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1, "too many tokens in block (possible infinite loop)");
		}
		while (lexer_eat(TOK_NEWLINE)) {
			if (lexer_at(TOK_RBRACE) || lexer_at(TOK_EOF) || IS_SEMICOLON()) break;
		}
		if (lexer_at(TOK_RBRACE) || lexer_at(TOK_EOF) || IS_SEMICOLON()) break;

		if (lexer_at(TOK_RBRACE) || lexer_at(TOK_EOF)) break;

		/* Modern constant range loop, lowered directly to IR_FOR_RANGE. */
		if (current_syntax_mode == 1 && lexer_at(TOK_ID) &&
	    lexer_cur()->len == 3 && memcmp(lexer_cur()->start, "for", 3) == 0 &&
	    lexer_at_peek(TOK_ID)) {
			lexer_advance();
			if (!lexer_at(TOK_ID))
				mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col,
					1, "for expects an induction variable");
			char *var_name = lexer_cur()->start; size_t var_len = lexer_cur()->len;
			int var_slot = prog_add_var(prog, var_name, var_len); lexer_advance();
			if (!(lexer_at(TOK_ID) && lexer_cur()->len == 2 &&
			      memcmp(lexer_cur()->start, "in", 2) == 0))
				mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col,
					1, "for induction variable must be followed by 'in'");
			lexer_advance();
			IrNode *start_expr = parse_infix_line(prog);
			if (!start_expr)
				mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col,
					1, "range start expression expected");
			lexer_expect(TOK_DOTDOT);
			IrNode *end_expr = parse_infix_line(prog);
			if (!end_expr)
				mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col,
					1, "range end expression expected");
			while (lexer_eat(TOK_NEWLINE)) {}
			lexer_expect(TOK_LBRACE);
			IrNode *body_ops = parse_block_content(prog);
			lexer_expect(TOK_RBRACE);
			IrNode *body = new_ir(IR_BLOCK); body->u.block = body_ops;
			IrNode *loop;
			if (start_expr->kind == IR_INT && !start_expr->next &&
			    end_expr->kind == IR_INT && !end_expr->next) {
				/* Preserve the compact, highly optimized literal fast path. */
				loop = new_ir(IR_FOR_RANGE);
				loop->u.for_range.start = start_expr->u.i;
				loop->u.for_range.end = end_expr->u.i;
				loop->u.for_range.var = var_name; loop->u.for_range.var_len = var_len;
				loop->u.for_range.var_slot = var_slot;
				loop->u.for_range.body = body;
			} else {
				/* Dynamic ranges lower to the existing C-style loop IR:
				 * init; while (index < end) { body; index += 1; } */
				IrNode *init_tail = start_expr;
				while (init_tail->next) init_tail = init_tail->next;
				IrNode *init_var = new_ir(IR_VAR); init_var->u.var_slot = var_slot;
				IrNode *init_store = new_ir(IR_WORD);
				init_store->u.word.name = "!"; init_store->u.word.len = 1;
				init_tail->next = init_var; init_var->next = init_store;
				IrNode *init = new_ir(IR_BLOCK); init->u.block = start_expr;

				IrNode *cond_var = new_ir(IR_VAR); cond_var->u.var_slot = var_slot;
				IrNode *cond_fetch = new_ir(IR_WORD);
				cond_fetch->u.word.name = "@"; cond_fetch->u.word.len = 1;
				cond_var->next = cond_fetch; cond_fetch->next = end_expr;
				IrNode *end_tail = end_expr;
				while (end_tail->next) end_tail = end_tail->next;
				IrNode *less = new_ir(IR_WORD); less->u.word.name = "<"; less->u.word.len = 1;
				end_tail->next = less;
				IrNode *cond = new_ir(IR_BLOCK); cond->u.block = cond_var;

				IrNode *step_var = new_ir(IR_VAR); step_var->u.var_slot = var_slot;
				IrNode *step_fetch = new_ir(IR_WORD);
				step_fetch->u.word.name = "@"; step_fetch->u.word.len = 1;
				IrNode *one = new_ir(IR_INT); one->u.i = 1;
				IrNode *add = new_ir(IR_WORD); add->u.word.name = "+"; add->u.word.len = 1;
				IrNode *step_dst = new_ir(IR_VAR); step_dst->u.var_slot = var_slot;
				IrNode *step_store = new_ir(IR_WORD);
				step_store->u.word.name = "!"; step_store->u.word.len = 1;
				step_var->next = step_fetch; step_fetch->next = one; one->next = add;
				add->next = step_dst; step_dst->next = step_store;
				IrNode *step = new_ir(IR_BLOCK); step->u.block = step_var;

				loop = new_ir(IR_FOR_CSTYLE);
				loop->u.for_cstyle.init = init;
				loop->u.for_cstyle.cond = cond;
				loop->u.for_cstyle.step = step;
				loop->u.for_cstyle.body = body;
			}
			if (!head) head = tail = loop; else { tail->next = loop; tail = loop; }
			continue;
		}

		/* Modern return is written in prefix form (`return expression;`) but
		 * the existing IR is postfix and expects `expression return`.  Lower it
		 * here so every expression kind, including a typed memory place, follows
		 * the same return path. */
		if (current_syntax_mode == 1 && lexer_at(TOK_ID) &&
		    lexer_cur()->len == 6 && memcmp(lexer_cur()->start, "return", 6) == 0) {
			Token return_token = *lexer_cur();
			lexer_advance();
			IrNode *value = NULL;
			if (!IS_SEMICOLON() && !lexer_at(TOK_NEWLINE) && !lexer_at(TOK_RBRACE))
				value = parse_infix_line(prog);
			IrNode *ret = new_ir(IR_WORD);
			ret->u.word.name = return_token.start;
			ret->u.word.len = return_token.len;
			if (value) {
				IrNode *value_tail = value;
				while (value_tail->next) value_tail = value_tail->next;
				value_tail->next = ret;
				if (!head) head = value;
				else tail->next = value;
				tail = ret;
			} else {
				if (!head) head = tail = ret;
				else { tail->next = ret; tail = ret; }
			}
			if (IS_SEMICOLON()) lexer_advance();
			continue;
		}

		/* Infix declarations and assignments:
		 *   let x: i64 = expression;
		 *   mut x: i64 = expression;
		 *       x = expression;
		 * Lower both forms to the existing postfix chain: expression x ! */
		if (current_syntax_mode == 1 && lexer_at(TOK_ID)) {
			int is_mut = lexer_cur()->len == 3 && memcmp(lexer_cur()->start, "mut", 3) == 0;
			int is_let = lexer_cur()->len == 3 && memcmp(lexer_cur()->start, "let", 3) == 0;
			int is_decl = is_mut || is_let;
			char *name = NULL;
			size_t name_len = 0;
			MiraType declared_type = MIRA_TYPE_UNKNOWN;
			bool declared_type_explicit = false;
			char compound_op = 0;
			StructDef *decl_struct = NULL;
			StructDef *field_owner = NULL;
			int field_receiver_slot = -1;
			int field_is_self = 0;
			char *field_name = NULL;
			size_t field_name_len = 0;

			/* Postfix increment/decrement statements.  The legacy lexer keeps
			 * dashes inside identifiers, so accept both `x++`/`x--` and the
			 * spaced `x ++`/`x --` forms without changing tokenization. */
			size_t update_name_len = 0;
			char update_op = 0;
			if (lexer_cur()->len > 2 &&
			    lexer_cur()->start[lexer_cur()->len - 1] ==
			    lexer_cur()->start[lexer_cur()->len - 2] &&
			    (lexer_cur()->start[lexer_cur()->len - 1] == '+' ||
			     lexer_cur()->start[lexer_cur()->len - 1] == '-')) {
				update_name_len = lexer_cur()->len - 2;
				update_op = lexer_cur()->start[lexer_cur()->len - 1];
			} else if (lexer_at_peek(TOK_ID) && comp->peek.len == 2 &&
			           comp->peek.start[0] == comp->peek.start[1] &&
			           (comp->peek.start[0] == '+' || comp->peek.start[0] == '-')) {
				update_name_len = lexer_cur()->len;
				update_op = comp->peek.start[0];
			}
			if (update_name_len) {
				int slot = prog_var_slot(prog, lexer_cur()->start, update_name_len);
				if (slot < 0)
					mira_error(comp->src, comp->filename, lexer_cur()->line,
						lexer_cur()->col, 1, "increment/decrement of unknown variable");
				lexer_advance();
				if (lexer_at(TOK_ID) && lexer_cur()->len == 2 &&
				    lexer_cur()->start[0] == update_op && lexer_cur()->start[1] == update_op)
					lexer_advance();
				IrNode *load = new_ir(IR_VAR); load->u.var_slot = slot;
				IrNode *fetch = new_ir(IR_WORD); fetch->u.word.name = "@"; fetch->u.word.len = 1;
				IrNode *one = new_ir(IR_INT); one->u.i = 1;
				IrNode *arith = new_ir(IR_WORD);
				arith->u.word.name = update_op == '+' ? "+" : "-"; arith->u.word.len = 1;
				IrNode *store_var = new_ir(IR_VAR); store_var->u.var_slot = slot;
				IrNode *store = new_ir(IR_WORD); store->u.word.name = "!"; store->u.word.len = 1;
				load->next = fetch; fetch->next = one; one->next = arith;
				arith->next = store_var; store_var->next = store;
				if (!head) head = load; else tail->next = load;
				tail = store;
				if (IS_SEMICOLON()) lexer_advance();
				continue;
			}

			if (is_decl) {
				lexer_advance();
				if (!lexer_at(TOK_ID)) {
					mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1,
						"declaration expects an identifier");
				}
				name = lexer_cur()->start;
				name_len = lexer_cur()->len;
				lexer_advance();
				if (lexer_at(TOK_COLON)) {
					lexer_advance();
					if (!lexer_at(TOK_ID))
						mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col,
							1, "variable type name expected after ':'");
					declared_type = parse_declared_type(false);
					declared_type_explicit = true;
				}
			} else if (lexer_at_peek(TOK_ID) &&
			    ((comp->peek.len == 1 && comp->peek.start[0] == '=') ||
			     (comp->peek.len == 2 && comp->peek.start[1] == '=' &&
			      strchr("+-*/%&|^", comp->peek.start[0]) != NULL))) {
				name = lexer_cur()->start;
				name_len = lexer_cur()->len;
				if (comp->peek.len == 2) compound_op = comp->peek.start[0];
				lexer_advance();
			}

			if (name) {
				char *dot = memchr(name, '.', name_len);
				if (!is_decl && dot && dot != name && dot + 1 < name + name_len) {
					size_t receiver_len = (size_t)(dot - name);
					field_name = dot + 1;
					field_name_len = name_len - receiver_len - 1;
					field_receiver_slot = prog_var_slot(prog, name, receiver_len);
					field_owner = field_receiver_slot >= 0 ? prog->var_structs[field_receiver_slot] : NULL;
					field_is_self = receiver_len == 4 && memcmp(name, "self", 4) == 0;
					if (field_is_self) {
						extern StructDef *current_impl_owner;
						field_owner = current_impl_owner;
					}
					if (!field_owner || prog_field_offset(field_owner, field_name, field_name_len) < 0)
						mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1,
							"assignment to unknown structure field");
				}
				if (!(lexer_at(TOK_ID) &&
				      ((lexer_cur()->len == 1 && lexer_cur()->start[0] == '=') ||
				       (!is_decl && lexer_cur()->len == 2 &&
				        lexer_cur()->start[0] == compound_op && lexer_cur()->start[1] == '=')))) {
					mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1,
						"assignment expects '='");
				}
				lexer_advance();
				if (is_decl && lexer_at(TOK_ID) && lexer_at_peek(TOK_LPAREN))
					decl_struct = prog_find_struct(prog, lexer_cur()->start, lexer_cur()->len);
				IrNode *expr = parse_infix_line(prog);
				if (!expr) {
					mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1,
						"assignment expects an expression");
				}

				if (field_owner) {
					extern int current_method_mut_self;
					if ((!field_is_self && !prog->var_mutable[field_receiver_slot]) ||
					    (field_is_self && !current_method_mut_self))
						mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col,
							1, "cannot modify a field through a read-only receiver");
					int field_index = prog_field_offset(field_owner, field_name, field_name_len) / 8;
					IrNode *obj = field_is_self ? new_ir(IR_WORD) : new_ir(IR_VAR);
					if (field_is_self) {
						obj->u.word.name = "self"; obj->u.word.len = 4;
					} else obj->u.var_slot = field_receiver_slot;
					IrNode *cursor = obj;
					if (!field_is_self) {
						IrNode *fetch = new_ir(IR_WORD); fetch->u.word.name = "@"; fetch->u.word.len = 1;
						cursor->next = fetch; cursor = fetch;
					}
					IrNode *index = new_ir(IR_INT); index->u.i = field_index;
					cursor->next = index; cursor = index;
					if (compound_op) {
						IrNode *obj2 = field_is_self ? new_ir(IR_WORD) : new_ir(IR_VAR);
						if (field_is_self) {
							obj2->u.word.name = "self"; obj2->u.word.len = 4;
						} else obj2->u.var_slot = field_receiver_slot;
						cursor->next = obj2; cursor = obj2;
						if (!field_is_self) {
							IrNode *fetch2 = new_ir(IR_WORD); fetch2->u.word.name = "@"; fetch2->u.word.len = 1;
							cursor->next = fetch2; cursor = fetch2;
						}
						IrNode *index2 = new_ir(IR_INT); index2->u.i = field_index;
						IrNode *get = new_ir(IR_WORD); get->u.word.name = "list-get"; get->u.word.len = 8;
						cursor->next = index2; index2->next = get; cursor = get;
					}
					cursor->next = expr;
					while (cursor->next) cursor = cursor->next;
					if (compound_op) {
						const char *op_name = compound_op == '+' ? "+" : compound_op == '-' ? "-" :
							compound_op == '*' ? "*" : compound_op == '/' ? "/" :
							compound_op == '%' ? "%" : compound_op == '&' ? "&" :
							compound_op == '|' ? "|" : "^";
						IrNode *arith = new_ir(IR_WORD);
						arith->u.word.name = (char *)op_name; arith->u.word.len = 1;
						cursor->next = arith; cursor = arith;
					}
					IrNode *set = new_ir(IR_WORD); set->u.word.name = "list-set"; set->u.word.len = 8;
					cursor->next = set;
					if (!head) head = obj; else tail->next = obj;
					tail = set;
					if (IS_SEMICOLON()) lexer_advance();
					continue;
				}

				int slot = is_decl ? prog_add_var(prog, name, name_len) : prog_var_slot(prog, name, name_len);
				if (slot < 0) {
					mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1,
						"assignment to unknown variable");
				}
				if (is_decl && decl_struct)
					prog_set_var_struct(prog, slot, decl_struct, is_mut);
				if (is_decl && declared_type_explicit)
					prog_set_var_type(prog, slot, declared_type, true);
				/* Syntax-only lowering: x += rhs becomes x @ rhs + x ! and
				 * therefore uses the existing SSA and optimization pipeline. */
				if (compound_op) {
					IrNode *load_var = new_ir(IR_VAR);
					load_var->u.var_slot = slot;
					IrNode *fetch = new_ir(IR_WORD);
					fetch->u.word.name = "@";
					fetch->u.word.len = 1;
					load_var->next = fetch;
					fetch->next = expr;
					expr = load_var;
				}
				IrNode *expr_tail = expr;
				while (expr_tail->next) expr_tail = expr_tail->next;
				if (compound_op) {
					const char *op_name = compound_op == '+' ? "+" : compound_op == '-' ? "-" :
						compound_op == '*' ? "*" : compound_op == '/' ? "/" :
						compound_op == '%' ? "%" : compound_op == '&' ? "&" :
						compound_op == '|' ? "|" : "^";
					IrNode *arith = new_ir(IR_WORD);
					arith->u.word.name = (char *)op_name;
					arith->u.word.len = 1;
					expr_tail->next = arith;
					expr_tail = arith;
				}
				IrNode *var_op = new_ir(IR_VAR);
				var_op->u.var_slot = slot;
				IrNode *store_op = new_ir(IR_WORD);
				store_op->u.word.name = "!";
				store_op->u.word.len = 1;
				expr_tail->next = var_op;
				var_op->next = store_op;

				if (!head) head = expr; else tail->next = expr;
				tail = store_op;
				if (IS_SEMICOLON()) lexer_advance();
				continue;
			}
		}
		

		/* const id : value �??閸ф鍞�?�敮鎼佸櫤鐎规矮�??*/
		if (lexer_at(TOK_ID) && lexer_cur()->len == 5 && memcmp(lexer_cur()->start, "const", 5) == 0) {
			lexer_advance();
			if (!lexer_at(TOK_ID)) { mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1, "'const' expects an identifier name"); }
			char *cname = lexer_cur()->start;
			size_t clen = lexer_cur()->len;
			MiraType declared_type = MIRA_TYPE_UNKNOWN;
			bool declared_type_explicit = false;
			int const_slot = -1;
			lexer_advance();
			if (current_syntax_mode == 1) {
				if (lexer_at(TOK_COLON)) {
					lexer_advance();
					if (!lexer_at(TOK_ID))
						mira_error(comp->src, comp->filename, lexer_cur()->line,
							lexer_cur()->col, 1, "constant type name expected after ':'");
					declared_type = parse_declared_type(false);
					declared_type_explicit = true;
				}
				if (!(lexer_at(TOK_ID) && lexer_cur()->len == 1 && lexer_cur()->start[0] == '='))
					mira_error(comp->src, comp->filename, lexer_cur()->line,
						lexer_cur()->col, 1, "modern const declaration expects '='");
				lexer_advance();
			} else {
				lexer_expect(TOK_COLON);
			}
			if (lexer_at(TOK_INT)) {
				const_slot = prog_add_const(prog, cname, clen, CONST_INT, lexer_cur()->val, 0, NULL, 0);
				lexer_advance();
			} else if (lexer_at(TOK_FLOAT)) {
				const_slot = prog_add_const(prog, cname, clen, CONST_DOUBLE, 0, lexer_cur()->dbl, NULL, 0);
				lexer_advance();
			} else if (lexer_at(TOK_STR)) {
				const_slot = prog_add_const(prog, cname, clen, CONST_STR, 0, 0, lexer_cur()->str, lexer_cur()->str_len);
				lexer_advance();
			} else {
				mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1, "'const' expects a number or string after ':'");
			}
			if (declared_type_explicit)
				prog_set_const_type(prog, const_slot, declared_type, true);
			continue;
		}

		/* id : 123 �??id : "hello" �??id : expr �??閸欐﹢鍣虹挧瀣�??*/
		if (lexer_at(TOK_ID) && lexer_at_peek(TOK_COLON)) {
			char *name = lexer_cur()->start;
			size_t len = lexer_cur()->len;
			lexer_advance();
			lexer_advance();
			/* 閸掋倖鏌囨稉瀣╃娑?token 閺�?�儱鍠呯�?规俺绁撮崐鍏兼煙瀵骏绱版俊鍌涚�?閺勵垳鐣濋崡鏇熸殻閺�?绗�?崥搴ㄦ桨閺勵垱宕�?悰灞芥皑閻╁瓨甯寸挧瀣�??*/
			if (lexer_at(TOK_INT) && (lexer_at_peek(TOK_NEWLINE) || lexer_at_peek(TOK_RBRACE) || lexer_at_peek(TOK_EOF))) {
				IrNode *v = new_ir(IR_INT);
				v->u.i = lexer_cur()->val;
				lexer_advance();
				IrNode *chain = make_assign_chain(prog, name, len, v);
				if (!head) head = tail = chain; else { tail->next = chain; }
				while (tail->next) tail = tail->next;
				continue;
			}
			/* 閸氬奔绗傞敍灞肩稻閸掋�?�鏌�?弰�?�氭儊閺勵垳鐣濋崡鏇炵摟缁�?�缚瑕嗙挧�?��??*/
			if (lexer_at(TOK_STR) && (lexer_at_peek(TOK_NEWLINE) || lexer_at_peek(TOK_RBRACE) || lexer_at_peek(TOK_EOF))) {
				IrNode *v = new_ir(IR_STR);
				v->u.str.s = lexer_cur()->str;
				v->u.str.len = lexer_cur()->str_len;
				lexer_advance();
				IrNode *chain = make_assign_chain(prog, name, len, v);
				if (!head) head = tail = chain; else { tail->next = chain; }
				while (tail->next) tail = tail->next;
				continue;
			}
			/* x: 鐞涖劏鎻?�??闁氨鏁ょ挧瀣偓纭风�?�鐏忓�?�銆冩潏�?х�?缂佹挻鐏夌挧瀣舶閸欐﹢鍣洪敍�?��??var !) */
			{
				IrNode *expr_head = NULL, *expr_tail = NULL;
				while (!lexer_at(TOK_NEWLINE) && !lexer_at(TOK_RBRACE) && !lexer_at(TOK_EOF)) {
					IrNode *o = parse_one(prog, false);
					if (!o) break;
					if (!expr_head) expr_head = expr_tail = o; else { expr_tail->next = o; expr_tail = o; }
					/* walk to real tail - parse_one may return multi-node chains in infix mode */
					while (expr_tail->next) expr_tail = expr_tail->next;
				}
				if (!expr_head) {
					mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1, "variable assignment expects a value or expression after ':'");
				}
				int slot = prog_add_var(prog, name, len);
				IrNode *var_op = new_ir(IR_VAR);
				var_op->u.var_slot = slot;
				IrNode *store_op = new_ir(IR_WORD);
				store_op->u.word.name = "!";
				store_op->u.word.len = 1;
				expr_tail->next = var_op;
				var_op->next = store_op;
				if (!head) head = tail = expr_head; else { tail->next = expr_head; }
				while (tail->next) tail = tail->next;
				continue;
			}
		}

		/* In infix mode, skip postfix flow control interception entirely 鑺掗�??
		   parse_one() has its own C-style if/while/for interceptors */
		if (current_syntax_mode == 1) {
			/* Structured error handling:
			 *   try { ... } catch (error) { ... }
			 * The error variable receives the thrown message pointer. */
			if (lexer_at(TOK_ID) && lexer_cur()->len == 3 &&
			    memcmp(lexer_cur()->start, "try", 3) == 0 &&
			    lexer_at_peek(TOK_LBRACE)) {
				lexer_advance();
				lexer_expect(TOK_LBRACE);
				IrNode *try_body_ops = parse_block_content(prog);
				lexer_expect(TOK_RBRACE);
				while (lexer_eat(TOK_NEWLINE)) {}
				IrNode *catch_body = NULL;
				int error_slot = -1;
				if (lexer_at(TOK_ID) && lexer_cur()->len == 5 &&
				    memcmp(lexer_cur()->start, "catch", 5) == 0) {
					lexer_advance();
					bool parenthesized = lexer_eat(TOK_LPAREN);
					if (!lexer_at(TOK_ID))
						mira_error(comp->src, comp->filename, lexer_cur()->line,
							lexer_cur()->col, 1, "catch expects an error variable");
					error_slot = prog_add_var(prog, lexer_cur()->start, lexer_cur()->len);
					lexer_advance();
					if (parenthesized) lexer_expect(TOK_RPAREN);
					while (lexer_eat(TOK_NEWLINE)) {}
					lexer_expect(TOK_LBRACE);
					IrNode *catch_ops = parse_block_content(prog);
					lexer_expect(TOK_RBRACE);
					catch_body = new_ir(IR_BLOCK);
					catch_body->u.block = catch_ops;
				}
				IrNode *try_body = new_ir(IR_BLOCK);
				try_body->u.block = try_body_ops;
				IrNode *try_op = new_ir(IR_TRY);
				try_op->u.try_block.body = try_body;
				try_op->u.try_block.catch_body = catch_body;
				try_op->u.try_block.error_slot = error_slot;
				if (!head) head = try_op; else tail->next = try_op;
				tail = try_op;
				continue;
			}
			/* Reuse the existing switch parser/codegen for match expressions. */
			if (lexer_at(TOK_ID) && lexer_cur()->len == 5 &&
			    memcmp(lexer_cur()->start, "match", 5) == 0 &&
			    lexer_at_peek(TOK_LPAREN)) {
				lexer_cur()->start = "switch";
				lexer_cur()->len = 6;
			}
			/* Postfix for in fn bodies: start end for { body } or { body } for.
			   Skip the generic parse_one so the unified postfix-for branch below
			   can consume the for (it builds IR_FOR_RANGE / IR_FOR_CSTYLE). */
			bool postfix_for_pending =
			    (lexer_at(TOK_ID) && lexer_cur()->len == 3 &&
			     memcmp(lexer_cur()->start, "for", 3) == 0 &&
			     (lexer_at_peek(TOK_LBRACE) || (tail && tail->kind == IR_BLOCK)));
			if (!postfix_for_pending) {
				IrNode *o = parse_one(prog, false);
				if (o && lexer_at(TOK_ID) && lexer_cur()->len == 1 &&
				    lexer_cur()->start[0] == '=') {
					IrNode *prev = NULL;
					IrNode *last = o;
					while (last->next) { prev = last; last = last->next; }
					int byte_place = last->kind == IR_WORD && last->u.word.len == 3 &&
						memcmp(last->u.word.name, "m8@", 3) == 0;
					int word_place = last->kind == IR_WORD && last->u.word.len == 4 &&
						memcmp(last->u.word.name, "m64@", 4) == 0;
					if (!prev || (!word_place && !byte_place))
						mira_error(comp->src, comp->filename, lexer_cur()->line,
							lexer_cur()->col, 1,
							"left side of memory assignment must be a typed memory place");
					prev->next = NULL;
					lexer_advance();
					IrNode *value = parse_infix_line(prog);
					if (!value)
						mira_error(comp->src, comp->filename, lexer_cur()->line,
							lexer_cur()->col, 1, "memory assignment expects a value");
					IrNode *value_tail = value;
					while (value_tail->next) value_tail = value_tail->next;
					value_tail->next = o;
					IrNode *place_tail = o;
					while (place_tail->next) place_tail = place_tail->next;
					IrNode *store = new_ir(IR_WORD);
					store->u.word.name = byte_place ? "m8!" : "m64!";
					store->u.word.len = byte_place ? 3 : 4;
					place_tail->next = store;
					o = value;
				}
				if (!o) break;
				if (!head) head = tail = o; else { tail->next = o; tail = o; }
				while (tail->next) tail = tail->next;
				if (IS_SEMICOLON()) lexer_advance();
				continue;
			}
		}

		/* { init } { cond } { step } { body } for */
		if (lexer_at(TOK_ID) && lexer_cur()->len == 3 && memcmp(lexer_cur()->start, "for", 3) == 0) {
			int n = 0;
			for (IrNode *p = head; p; p = p->next) n++;
			if (n >= 4) {
				IrNode *p = head;
				for (int i = 0; i < n - 4; i++) p = p->next;
				IrNode *a = p, *b = p ? p->next : NULL, *c = b ? b->next : NULL, *d = c ? c->next : NULL;
				if (a && b && c && d && a->kind == IR_BLOCK && b->kind == IR_BLOCK &&
				    c->kind == IR_BLOCK && d->kind == IR_BLOCK) {
					lexer_advance();
					IrNode *removed = pop_lIR_n(&head, &tail, 4);
					IrNode *fo = new_ir(IR_FOR_CSTYLE);
					fo->u.for_cstyle.init = removed;
					fo->u.for_cstyle.cond = removed->next;
					fo->u.for_cstyle.step = removed->next ? removed->next->next : NULL;
					fo->u.for_cstyle.body = removed->next && removed->next->next ? removed->next->next->next : NULL;
					if (!head) head = tail = fo; else { tail->next = fo; tail = fo; }
					continue;
				}
			}
		}
		/* 閸氬海绱?3 閸欏倹鏆?for: var start end for { body } �??Python 妞�??孩鐗?range 瀵邦亞骞?*/
		if (lexer_at(TOK_ID) && lexer_cur()->len == 3 && memcmp(lexer_cur()->start, "for", 3) == 0 &&
		    lexer_at_peek(TOK_LBRACE)) {
			IrNode *lIR3 = NULL, *lIR2 = NULL, *lIR1 = NULL;
			for (IrNode *p = head; p; p = p->next) { lIR3 = lIR2; lIR2 = lIR1; lIR1 = p; }
			if (lIR1 && lIR2 && lIR3 && (lIR1->kind == IR_INT || lIR1->kind == IR_CONST) &&
			    (lIR2->kind == IR_INT || lIR2->kind == IR_CONST) && lIR3->kind == IR_WORD &&
			    !is_mira_builtin(lIR3->u.word.name, lIR3->u.word.len)) {
				/* 鐟欙絾鐎介崣姗€鍣洪崥�?�冣偓浣稿絿 block 娴ｆ粈璐熷顏嗗箚娴ｆ挶鈧焦鏁為崗?body 閸欐﹢鍣�?妴鍌濆閸欐﹢鍣�?崥�?�嗘Ц unknown word 鐏忚精鍤滈崝銊﹀潑閸?*/
				char *var = lIR3->u.word.name;
				size_t var_len = lIR3->u.word.len;
				int var_slot = prog_var_slot(prog, var, var_len);
				if (var_slot < 0)
					var_slot = prog_add_var(prog, var, var_len);
				lexer_advance(); /* for */
				IrNode *block = parse_one(prog, false);
				if (!block || block->kind != IR_BLOCK) {
					mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1, "'for' expects a { block } after it");
				}
				if (!head) head = tail = block; else { tail->next = block; tail = block; }
				IrNode *removed = pop_lIR_n(&head, &tail, 4);
				/* removed = var, start, end, block 閸欏秴鎮滄惔蹇撳�? */
				int64_t start = removed->next && (removed->next->kind == IR_INT || removed->next->kind == IR_CONST) ?
					(removed->next->kind == IR_INT ? removed->next->u.i : IR_to_int(removed->next, prog)) : 0;
				int64_t end = removed->next && removed->next->next && (removed->next->next->kind == IR_INT || removed->next->next->kind == IR_CONST) ?
					(removed->next->next->kind == IR_INT ? removed->next->next->u.i : IR_to_int(removed->next->next, prog)) : 0;
				IrNode *fo = new_ir(IR_FOR_RANGE);
				fo->u.for_range.start = start;
				fo->u.for_range.end = end;
				fo->u.for_range.body = removed->next && removed->next->next ? removed->next->next->next : NULL;
				fo->u.for_range.var = var;
				fo->u.for_range.var_len = var_len;
				fo->u.for_range.var_slot = var_slot;
				if (!head) head = tail = fo; else { tail->next = fo; tail = fo; }
				continue;
			}
		}
		/* 閸氬海绱?2 閸欏倹鏆?for: start end for { body } �??閺冪姴褰夐柌蹇撴倳閻?range 瀵邦亞骞?*/
		if (lexer_at(TOK_ID) && lexer_cur()->len == 3 && memcmp(lexer_cur()->start, "for", 3) == 0 &&
		    lexer_at_peek(TOK_LBRACE)) {
			IrNode *lIR3 = NULL, *lIR2 = NULL, *lIR1 = NULL;
			for (IrNode *p = head; p; p = p->next) { lIR3 = lIR2; lIR2 = lIR1; lIR1 = p; }
			if (lIR1 && lIR2 && (lIR1->kind == IR_INT || lIR1->kind == IR_CONST) &&
			    (lIR2->kind == IR_INT || lIR2->kind == IR_CONST)) {
				lexer_advance(); /* for */
				IrNode *block = parse_one(prog, false);
				if (!block || block->kind != IR_BLOCK) {
					mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1, "'for' expects a { block } after it");
				}
				if (!head) head = tail = block; else { tail->next = block; tail = block; }
				IrNode *removed = pop_lIR_n(&head, &tail, 3);
				int64_t start = removed->kind == IR_INT ? removed->u.i : IR_to_int(removed, prog);
				int64_t end = removed->next ? (removed->next->kind == IR_INT ? removed->next->u.i : IR_to_int(removed->next, prog)) : 0;
				IrNode *fo = new_ir(IR_FOR_RANGE);
				fo->u.for_range.start = start;
				fo->u.for_range.end = end;
				fo->u.for_range.body = removed->next ? removed->next->next : NULL;
				fo->u.for_range.var = NULL;
				fo->u.for_range.var_len = 0;
				fo->u.for_range.var_slot = prog_add_var(prog, "i", 1);
				if (!head) head = tail = fo; else { tail->next = fo; tail = fo; }
				continue;
			}
		}
		/* 閸氬海绱?each { body } */
		if (lexer_at(TOK_ID) && lexer_cur()->len == 4 && memcmp(lexer_cur()->start, "each", 4) == 0) {
			if (tail && tail->kind == IR_LIST_LITERAL) {
				lexer_advance();
				IrNode *o = parse_one(prog, false);
				if (!o) {
					mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1, "'each' expects a { block } after it");
				}
				if (!head) head = tail = o; else { tail->next = o; tail = o; }
				IrNode *removed = pop_lIR_n(&head, &tail, 2);
				IrNode *ea = new_ir(IR_EACH);
				ea->u.each.list = removed;
				ea->u.each.body = removed->next;
				if (!head) head = tail = ea; else { tail->next = ea; tail = ea; }
				continue;
			}
		}

		/* { body } try �??鐏忓棗澧犳稉鈧�??block 閸栧懓顥婃稉?IR_TRY */
		if (lexer_at(TOK_ID) && lexer_cur()->len == 3 && memcmp(lexer_cur()->start, "try", 3) == 0) {
			IrNode *lIR1 = NULL;
			for (IrNode *p = head; p; p = p->next) { lIR1 = p; }
			if (lIR1 && lIR1->kind == IR_BLOCK) {
				lexer_advance();
				IrNode *removed = pop_lIR_n(&head, &tail, 1);
				IrNode *try_op = new_ir(IR_TRY);
				try_op->u.try_block.body = removed;
				if (!head) head = tail = try_op; else { tail->next = try_op; tail = try_op; }
				continue;
			}
		}

		/* { cond } { body } while */
		if (lexer_at(TOK_ID) && lexer_cur()->len == 5 && memcmp(lexer_cur()->start, "while", 5) == 0) {
			IrNode *lIR2 = NULL, *lIR1 = NULL;
			for (IrNode *p = head; p; p = p->next) { lIR2 = lIR1; lIR1 = p; }
			if (lIR1 && lIR2 && lIR1->kind == IR_BLOCK && lIR2->kind == IR_BLOCK) {
				lexer_advance();
				IrNode *removed = pop_lIR_n(&head, &tail, 2);
				IrNode *wh = new_ir(IR_WHILE_COND);
				wh->u.while_cond.cond = removed;
				wh->u.while_cond.body = removed->next;
				if (!head) head = tail = wh; else { tail->next = wh; tail = wh; }
				continue;
			}
		}

		/* 閺�?�€叉 { then } { else } if �??閺�?�€叉 { then } if �??�??else 閸掑棙鏁�?崣�?�炩�??*/
		if (lexer_at(TOK_ID) && lexer_cur()->len == 2 && memcmp(lexer_cur()->start, "if", 2) == 0) {
			IrNode *lIR2 = NULL, *lIR1 = NULL;
			for (IrNode *p = head; p; p = p->next) { lIR2 = lIR1; lIR1 = p; }
			if (lIR1 && lIR2 && lIR1->kind == IR_BLOCK && lIR2->kind == IR_BLOCK) {
				lexer_advance();
				IrNode *prev = NULL, *p = head;
				for (; p && p->kind != IR_BLOCK; prev = p, p = p->next) {}
				if (!p || !p->next || p->next->kind != IR_BLOCK) continue;
				IrNode *cond_head = head;
				IrNode *then_b = p;
				IrNode *else_b = p->next;
				then_b->next = NULL;
				if (prev) {
					prev->next = NULL;
					tail = prev;
				} else {
					head = NULL;
					tail = NULL;
				}
				IrNode *iff = new_ir(IR_IF);
				iff->u.iff.cond = cond_head;
				iff->u.iff.then_b = then_b;
				iff->u.iff.else_b = else_b;
				head = tail = iff;
				continue;
			}
			/* 閺�?�€叉 { then } if �??�??else */
			if (lIR1 && lIR1->kind == IR_BLOCK) {
				lexer_advance();
				IrNode *prev = NULL, *p = head;
				for (; p && p->kind != IR_BLOCK; prev = p, p = p->next) {}
				if (!p || !prev) continue;  /* 闂団偓鐟?cond �??block */
				IrNode *cond_head = head;
				IrNode *then_b = p;
				if (prev) prev->next = NULL; else head = NULL;
				tail = prev;
				IrNode *iff = new_ir(IR_IF);
				iff->u.iff.cond = cond_head;
				iff->u.iff.then_b = then_b;
				iff->u.iff.else_b = NULL;
				head = tail = iff;
				continue;
			}
		}
		/* switch 鐠囶厽纭堕敍姘�? default 閸滃奔绗夌敮?default 娑撱倗顫掕ぐ銏犵�? */
		if (lexer_at(TOK_ID) && lexer_cur()->len == 6 && memcmp(lexer_cur()->start, "switch", 6) == 0) {
			IrNode *lIR2 = NULL, *lIR1 = NULL;
			for (IrNode *p = head; p; p = p->next) { lIR2 = lIR1; lIR1 = p; }
			/* �??default閿涙�?IR2 �??"default" 閺嶅洩鐦戠粭?*/
			if (lIR1 && lIR2 && lIR1->kind == IR_BLOCK &&
			    lIR2->kind == IR_WORD && lIR2->u.word.len == 7 &&
			    memcmp(lIR2->u.word.name, "default", 7) == 0) {
				lexer_advance();
				int len = 0;
				for (IrNode *p = head; p; p = p->next) len++;
				if (len < 4) continue;
				/* 閺€�?曟肠閹�?�偓�??(pattern, block) 鐎电櫢绱皃attern �??block 娴溿倖娴涢崙铏瑰�? */
				IrNode *first_block = NULL;
				for (IrNode *p = head; p; p = p->next)
					if (p->kind == IR_BLOCK) { first_block = p; break; }
				if (!first_block) continue;
				IrNode *p1 = NULL;
				for (IrNode *p = head; p != first_block; p1 = p, p = p->next) {}
				/* n_pop = �??p1 �??default_block 閻ㄥ�?濡悙瑙�?�? */
				int n_fixed = 0;
				for (IrNode *p = p1; p; p = p->next) n_fixed++;
				/* value 閸︺劍娓堕崜宥�?�桨閿涙�?1 閹�?�洤�?滅粭顑跨娑?op閿涘牆褰查懗鑺ユЦ value 娑旂喎褰查懗鑺ユЦ @ 閸欐牕鈧�?�绱氶敍瀵乤lue 闂€�?闁艾鐖舵稉?1 */
				int val_len = 1;
				if (p1) {
					IrNode *before_p1 = NULL;
					for (IrNode *p = head; p != p1; before_p1 = p, p = p->next) {}
					if (before_p1 && before_p1->kind == IR_WORD &&
					    before_p1->u.word.len == 1 && before_p1->u.word.name[0] == '@')
						val_len = 2;
				}
				int n_pop = val_len + n_fixed;
				if (n_pop > len) n_pop = n_fixed;
				IrNode *removed = pop_lIR_n(&head, &tail, n_pop);
				if (!removed) continue;
				IrNode *default_block = removed;
				for (; default_block->next; default_block = default_block->next) {}
				IrNode *prev = removed;
				for (; prev->next && !(prev->next->kind == IR_WORD && prev->next->u.word.len == 7 &&
				    memcmp(prev->next->u.word.name, "default", 7) == 0); prev = prev->next) {}
				if (!prev->next) continue;
				prev->next = NULL;
				/* removed �??value + cases + default 闁炬拝绱濋�?�鈧憰浣�?��?�缁?value �??cases */
				IrNode *value = removed;
				IrNode *cases = value->next;
				IrNode *first_blk = NULL;
				for (IrNode *p = removed; p; p = p->next)
					if (p->kind == IR_BLOCK) { first_blk = p; break; }
				if (first_blk) {
					IrNode *first_pat = NULL;
					for (IrNode *p = removed; p != first_blk; first_pat = p, p = p->next) {}
					if (first_pat) {
						IrNode *val_lIR = NULL;
						for (IrNode *p = removed; p != first_pat; val_lIR = p, p = p->next) {}
						if (val_lIR) { val_lIR->next = NULL; value = removed; cases = first_pat; }
					}
				}
				IrNode *sw = new_ir(IR_SWITCH);
				sw->u.switch_.value = value;
				sw->u.switch_.cases = cases;
				sw->u.switch_.default_block = default_block;
				if (!head) head = tail = sw; else { tail->next = sw; tail = sw; }
				continue;
			}
			/* �??default�?? { one } 2 { two } switch */
			if (lIR1 && lIR2 && lIR1->kind == IR_BLOCK &&
			    (lIR2->kind == IR_INT || lIR2->kind == IR_CONST)) {
				lexer_advance();
				int len = 0;
				for (IrNode *p = head; p; p = p->next) len++;
				if (len < 3) continue;  /* 閼峰啿鐨�?�鈧憰?value + pat + block */
				IrNode *first_block = NULL;
				for (IrNode *p = head; p; p = p->next)
					if (p->kind == IR_BLOCK) { first_block = p; break; }
				if (!first_block) continue;
				IrNode *p1 = NULL;
				for (IrNode *p = head; p != first_block; p1 = p, p = p->next) {}
				int n_fixed = 0;
				for (IrNode *p = p1; p; p = p->next) n_fixed++;
				int val_len = 1;
				if (p1) {
					IrNode *before_p1 = NULL;
					for (IrNode *p = head; p != p1; before_p1 = p, p = p->next) {}
					if (before_p1 && before_p1->kind == IR_WORD &&
					    before_p1->u.word.len == 1 && before_p1->u.word.name[0] == '@')
						val_len = 2;
				}
				int n_pop = val_len + n_fixed;
				if (n_pop > len) n_pop = n_fixed;
				IrNode *removed = pop_lIR_n(&head, &tail, n_pop);
				if (!removed) continue;
				IrNode *value = removed;
				IrNode *cases = value->next;
				IrNode *first_blk = NULL;
				for (IrNode *p = removed; p; p = p->next)
					if (p->kind == IR_BLOCK) { first_blk = p; break; }
				if (first_blk) {
					IrNode *first_pat = NULL;
					for (IrNode *p = removed; p != first_blk; first_pat = p, p = p->next) {}
					if (first_pat) {
						IrNode *val_lIR = NULL;
						for (IrNode *p = removed; p != first_pat; val_lIR = p, p = p->next) {}
						if (val_lIR) { val_lIR->next = NULL; value = removed; cases = first_pat; }
					}
				}
				IrNode *sw = new_ir(IR_SWITCH);
				sw->u.switch_.value = value;
				sw->u.switch_.cases = cases;
				sw->u.switch_.default_block = NULL;
				if (!head) head = tail = sw; else { tail->next = sw; tail = sw; }
				continue;
			}
			/* { 濡�?崇�??1 { 閸掑棙鏁担? } 濡�?崇�??2 { 閸掑棙鏁担? } default { 姒涙�?��?�崚鍡樻�? } } switch �??婢堆�?�?閸欏嘲�?��?�憗鐟拌埌�???*/
			if (lIR1 && lIR2 && lIR1->kind == IR_BLOCK) {
				lexer_advance();
				IrNode *prev = NULL, *p = head;
				for (; p && p->kind != IR_BLOCK; prev = p, p = p->next) {}
				if (!p) continue;
				IrNode *value = prev ? head : NULL;
				IrNode *block = p;
				if (prev) prev->next = NULL; else head = NULL;
				tail = prev;
				IrNode *content = block->u.block;
				if (!content) continue;
				IrNode *lIR = content;
				for (; lIR->next; lIR = lIR->next) {}
				if (lIR->kind != IR_BLOCK) continue;
				IrNode *default_block = lIR;
				IrNode *prev2 = content;
				for (; prev2->next && !(prev2->next->kind == IR_WORD && prev2->next->u.word.len == 7 &&
				    memcmp(prev2->next->u.word.name, "default", 7) == 0); prev2 = prev2->next) {}
				if (!prev2->next) continue;
				prev2->next = NULL;
				IrNode *cases = content;
				IrNode *sw = new_ir(IR_SWITCH);
				sw->u.switch_.value = value;
				sw->u.switch_.cases = cases;
				sw->u.switch_.default_block = default_block;
				head = tail = sw;
				continue;
			}
		}
		/* { cond } { body } while �??缁�?�兛绨╃�??while */
		if (lexer_at(TOK_ID) && lexer_cur()->len == 5 && memcmp(lexer_cur()->start, "while", 5) == 0) {
			IrNode *lIR2 = NULL, *lIR1 = NULL;
			for (IrNode *p = head; p; p = p->next) { lIR2 = lIR1; lIR1 = p; }
			if (lIR1 && lIR2 && lIR1->kind == IR_BLOCK && lIR2->kind == IR_BLOCK) {
				lexer_advance();
				IrNode *removed = pop_lIR_n(&head, &tail, 2);
				IrNode *wo = new_ir(IR_WHILE_COND);
				wo->u.while_cond.cond = removed;
				wo->u.while_cond.body = removed->next;
				if (!head) head = tail = wo; else { tail->next = wo; tail = wo; }
				continue;
			}
			/* while ... loop 閺冪�?妾哄�?�嗗�? */
			lexer_advance();
			lexer_eat(TOK_NEWLINE);
			IrNode *body = parse_block_until_loop(prog);
			IrNode *wo = new_ir(IR_WHILE_INF);
			wo->u.while_inf.body = body;
			if (!head) head = tail = wo; else { tail->next = wo; tail = wo; }
			continue;
		}

		/* 閸氬海绱?2 閸欏倹鏆?for: start end for { body } �??閺冪姴褰夐柌蹇撴�? range 瀵邦亞骞?*/
		if (lexer_at(TOK_ID) && lexer_cur()->len == 3 && memcmp(lexer_cur()->start, "for", 3) == 0) {
			IrNode *lIR2 = NULL, *lIR1 = NULL;
			for (IrNode *p = head; p; p = p->next) { lIR2 = lIR1; lIR1 = p; }
			bool body_after = lexer_at_peek(TOK_LBRACE);
			bool body_before = lIR1 && lIR1->kind == IR_BLOCK;
			if (body_after || body_before) {
				lexer_advance(); /* 'for' */
				int i_slot = prog_add_var(prog, "i", 1);
				IrNode *block;
				if (body_after) {
					block = parse_one(prog, false);
					if (!block || block->kind != IR_BLOCK) {
						mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1, "'for' expects a { block } after it");
					}
					if (!head) head = tail = block; else { tail->next = block; tail = block; }
				}
				block = pop_lIR_n(&head, &tail, 1); /* pops { body }, before or after 'for' */
				/* The remaining chain holds [start? end] preceding the body. */
				IrNode *b2 = NULL, *b1 = NULL;
				for (IrNode *p = head; p; p = p->next) { b2 = b1; b1 = p; }
				if (b1 && b2 && (b1->kind == IR_INT || b1->kind == IR_CONST) &&
				    (b2->kind == IR_INT || b2->kind == IR_CONST)) {
					IrNode *removed = pop_lIR_n(&head, &tail, 2);
					int64_t start = removed->kind == IR_INT ? removed->u.i : IR_to_int(removed, prog);
					int64_t end = removed->next ? (removed->next->kind == IR_INT ? removed->next->u.i : IR_to_int(removed->next, prog)) : 0;
					IrNode *fo = new_ir(IR_FOR_RANGE);
					fo->u.for_range.start = start;
					fo->u.for_range.end = end;
					/* Loop variable on the stack at body entry: prefix 'i @' */
					IrNode *bi = new_ir(IR_VAR); bi->u.var_slot = i_slot;
					IrNode *bf = new_ir(IR_WORD); bf->u.word.name = "@"; bf->u.word.len = 1;
					bi->next = bf; bf->next = block->u.block;
					block->u.block = bi;
					fo->u.for_range.body = block;
					fo->u.for_range.var = NULL;
					fo->u.for_range.var_len = 0;
					fo->u.for_range.var_slot = i_slot;
					if (!head) head = tail = fo; else { tail->next = fo; tail = fo; }
					continue;
				}
				if (b1) {
					/* Dynamic range: the end expression is the trailing statement.
					 * Find the last store ('!' word) and take everything after it,
					 * so earlier statements (e.g. var init) stay on the main chain
					 * and run before the loop.  Lower to IR_FOR_CSTYLE
					 * (i = 0; i < end; i++), mirroring the named-for path. */
					IrNode *last_store = NULL;
					for (IrNode *p = head; p; p = p->next)
						if (p->kind == IR_WORD && p->u.word.len == 1 &&
						    p->u.word.name[0] == '!')
							last_store = p;
					IrNode *end_expr;
					if (last_store) {
						end_expr = last_store->next;
						last_store->next = NULL;
						tail = last_store;
					} else {
						end_expr = head;
						head = NULL; tail = NULL;
					}
				
					/* init: 0 i ! */
					IrNode *zero = new_ir(IR_INT); zero->u.i = 0;
					IrNode *init_var = new_ir(IR_VAR); init_var->u.var_slot = i_slot;
					IrNode *init_store = new_ir(IR_WORD);
					init_store->u.word.name = "!"; init_store->u.word.len = 1;
					zero->next = init_var; init_var->next = init_store;
					IrNode *init = new_ir(IR_BLOCK); init->u.block = zero;
				
					/* cond: i @ end_expr < */
					IrNode *cond_var = new_ir(IR_VAR); cond_var->u.var_slot = i_slot;
					IrNode *cond_fetch = new_ir(IR_WORD);
					cond_fetch->u.word.name = "@"; cond_fetch->u.word.len = 1;
					IrNode *eend = end_expr;
					while (eend->next) eend = eend->next;
					IrNode *less = new_ir(IR_WORD); less->u.word.name = "<"; less->u.word.len = 1;
					eend->next = less;
					cond_var->next = cond_fetch; cond_fetch->next = end_expr;
					IrNode *cond = new_ir(IR_BLOCK); cond->u.block = cond_var;
				
					/* step: i @ 1 + i ! */
					IrNode *step_var = new_ir(IR_VAR); step_var->u.var_slot = i_slot;
					IrNode *step_fetch = new_ir(IR_WORD);
					step_fetch->u.word.name = "@"; step_fetch->u.word.len = 1;
					IrNode *one = new_ir(IR_INT); one->u.i = 1;
					IrNode *add = new_ir(IR_WORD); add->u.word.name = "+"; add->u.word.len = 1;
					IrNode *step_dst = new_ir(IR_VAR); step_dst->u.var_slot = i_slot;
					IrNode *step_store = new_ir(IR_WORD);
					step_store->u.word.name = "!"; step_store->u.word.len = 1;
					step_var->next = step_fetch; step_fetch->next = one; one->next = add;
					add->next = step_dst; step_dst->next = step_store;
					IrNode *step = new_ir(IR_BLOCK); step->u.block = step_var;
				
						/* Loop variable on the stack at body entry: prefix 'i @' */
					IrNode *bi = new_ir(IR_VAR); bi->u.var_slot = i_slot;
					IrNode *bf = new_ir(IR_WORD); bf->u.word.name = "@"; bf->u.word.len = 1;
					bi->next = bf; bf->next = block->u.block;
					block->u.block = bi;
				IrNode *loop = new_ir(IR_FOR_CSTYLE);
					loop->u.for_cstyle.init = init;
					loop->u.for_cstyle.cond = cond;
					loop->u.for_cstyle.step = step;
					loop->u.for_cstyle.body = block;
					if (!head) head = tail = loop; else { tail->next = loop; tail = loop; }
					continue;
				}
				/* No start/end expression precedes 'for' */
				mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1, "'for' expects a start/end expression before it");
			}
		}

		IrNode *o = current_syntax_mode == 1 ? parse_infix_line(prog) : parse_one(prog, false);
		if (!o) break;
		if (!head) head = tail = o; else { tail->next = o; tail = o; }
		while (tail->next) tail = tail->next;
	}
	return head;
}

/* 鐟欙絾鐎介崙鑺ユ殶娴ｆ挾娲块崚浼翠海閸掔増宕�?悰灞惧灗 EOF */
static IrNode *parse_body_until_newline(Program *prog) {
	IrNode *head = NULL, *tail = NULL;
	unsigned iter_nl = 0;
	for (;;) {
		/* Multi-line definition body: blank lines are skipped; the body
		 * continues while the next line is indented (col > 1) and ends
		 * at a top-level line (col == 1) or EOF.  Single-line bodies
		 * (`name: { p } a b +`) are parsed exactly as before. */
		if (lexer_at(TOK_NEWLINE)) {
			while (lexer_at(TOK_NEWLINE)) lexer_advance();
			if (lexer_at(TOK_EOF)) break;
			if (lexer_cur()->col <= 1) break;
		}
		while (!lexer_at(TOK_NEWLINE) && !lexer_at(TOK_EOF)) {
		if (++iter_nl > 100000) {
			mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1, "too many tokens in definition body (possible infinite loop)");
		}

		/* 閸氬海绱?2 閸欏倹鏆?for: start end for { body } �??閺冪姴褰夐柌蹇撴�? range 瀵邦亞骞?*/
		if (lexer_at(TOK_ID) && lexer_cur()->len == 3 && memcmp(lexer_cur()->start, "for", 3) == 0) {
			IrNode *lIR2 = NULL, *lIR1 = NULL;
			for (IrNode *p = head; p; p = p->next) { lIR2 = lIR1; lIR1 = p; }
			bool body_after = lexer_at_peek(TOK_LBRACE);
			bool body_before = lIR1 && lIR1->kind == IR_BLOCK;
			if (body_after || body_before) {
				lexer_advance(); /* 'for' */
				int i_slot = prog_add_var(prog, "i", 1);
				IrNode *block;
				if (body_after) {
					block = parse_one(prog, false);
					if (!block || block->kind != IR_BLOCK) {
						mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1, "'for' expects a { block } after it");
					}
					if (!head) head = tail = block; else { tail->next = block; tail = block; }
				}
				block = pop_lIR_n(&head, &tail, 1); /* pops { body }, before or after 'for' */
				/* The remaining chain holds [start? end] preceding the body. */
				IrNode *b2 = NULL, *b1 = NULL;
				for (IrNode *p = head; p; p = p->next) { b2 = b1; b1 = p; }
				if (b1 && b2 && (b1->kind == IR_INT || b1->kind == IR_CONST) &&
				    (b2->kind == IR_INT || b2->kind == IR_CONST)) {
					IrNode *removed = pop_lIR_n(&head, &tail, 2);
					int64_t start = removed->kind == IR_INT ? removed->u.i : IR_to_int(removed, prog);
					int64_t end = removed->next ? (removed->next->kind == IR_INT ? removed->next->u.i : IR_to_int(removed->next, prog)) : 0;
					IrNode *fo = new_ir(IR_FOR_RANGE);
					fo->u.for_range.start = start;
					fo->u.for_range.end = end;
					/* Loop variable on the stack at body entry: prefix 'i @' */
					IrNode *bi = new_ir(IR_VAR); bi->u.var_slot = i_slot;
					IrNode *bf = new_ir(IR_WORD); bf->u.word.name = "@"; bf->u.word.len = 1;
					bi->next = bf; bf->next = block->u.block;
					block->u.block = bi;
					fo->u.for_range.body = block;
					fo->u.for_range.var = NULL;
					fo->u.for_range.var_len = 0;
					fo->u.for_range.var_slot = i_slot;
					if (!head) head = tail = fo; else { tail->next = fo; tail = fo; }
					continue;
				}
				if (b1) {
					/* Dynamic range: the end expression is the trailing statement.
					 * Find the last store ('!' word) and take everything after it,
					 * so earlier statements (e.g. var init) stay on the main chain
					 * and run before the loop.  Lower to IR_FOR_CSTYLE
					 * (i = 0; i < end; i++), mirroring the named-for path. */
					IrNode *last_store = NULL;
					for (IrNode *p = head; p; p = p->next)
						if (p->kind == IR_WORD && p->u.word.len == 1 &&
						    p->u.word.name[0] == '!')
							last_store = p;
					IrNode *end_expr;
					if (last_store) {
						end_expr = last_store->next;
						last_store->next = NULL;
						tail = last_store;
					} else {
						end_expr = head;
						head = NULL; tail = NULL;
					}
				
					/* init: 0 i ! */
					IrNode *zero = new_ir(IR_INT); zero->u.i = 0;
					IrNode *init_var = new_ir(IR_VAR); init_var->u.var_slot = i_slot;
					IrNode *init_store = new_ir(IR_WORD);
					init_store->u.word.name = "!"; init_store->u.word.len = 1;
					zero->next = init_var; init_var->next = init_store;
					IrNode *init = new_ir(IR_BLOCK); init->u.block = zero;
				
					/* cond: i @ end_expr < */
					IrNode *cond_var = new_ir(IR_VAR); cond_var->u.var_slot = i_slot;
					IrNode *cond_fetch = new_ir(IR_WORD);
					cond_fetch->u.word.name = "@"; cond_fetch->u.word.len = 1;
					IrNode *eend = end_expr;
					while (eend->next) eend = eend->next;
					IrNode *less = new_ir(IR_WORD); less->u.word.name = "<"; less->u.word.len = 1;
					eend->next = less;
					cond_var->next = cond_fetch; cond_fetch->next = end_expr;
					IrNode *cond = new_ir(IR_BLOCK); cond->u.block = cond_var;
				
					/* step: i @ 1 + i ! */
					IrNode *step_var = new_ir(IR_VAR); step_var->u.var_slot = i_slot;
					IrNode *step_fetch = new_ir(IR_WORD);
					step_fetch->u.word.name = "@"; step_fetch->u.word.len = 1;
					IrNode *one = new_ir(IR_INT); one->u.i = 1;
					IrNode *add = new_ir(IR_WORD); add->u.word.name = "+"; add->u.word.len = 1;
					IrNode *step_dst = new_ir(IR_VAR); step_dst->u.var_slot = i_slot;
					IrNode *step_store = new_ir(IR_WORD);
					step_store->u.word.name = "!"; step_store->u.word.len = 1;
					step_var->next = step_fetch; step_fetch->next = one; one->next = add;
					add->next = step_dst; step_dst->next = step_store;
					IrNode *step = new_ir(IR_BLOCK); step->u.block = step_var;
				
						/* Loop variable on the stack at body entry: prefix 'i @' */
					IrNode *bi = new_ir(IR_VAR); bi->u.var_slot = i_slot;
					IrNode *bf = new_ir(IR_WORD); bf->u.word.name = "@"; bf->u.word.len = 1;
					bi->next = bf; bf->next = block->u.block;
					block->u.block = bi;
				IrNode *loop = new_ir(IR_FOR_CSTYLE);
					loop->u.for_cstyle.init = init;
					loop->u.for_cstyle.cond = cond;
					loop->u.for_cstyle.step = step;
					loop->u.for_cstyle.body = block;
					if (!head) head = tail = loop; else { tail->next = loop; tail = loop; }
					continue;
				}
				/* No start/end expression precedes 'for' */
				mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1, "'for' expects a start/end expression before it");
			}
		}
		if (lexer_at(TOK_ID) && lexer_cur()->len == 5 && memcmp(lexer_cur()->start, "const", 5) == 0) {
			lexer_advance();
			if (!lexer_at(TOK_ID)) { mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1, "'const' expects an identifier name"); }
			char *cname = lexer_cur()->start;
			size_t clen = lexer_cur()->len;
			lexer_advance();
			lexer_expect(TOK_COLON);
			if (lexer_at(TOK_INT)) { prog_add_const(prog, cname, clen, CONST_INT, lexer_cur()->val, 0, NULL, 0); lexer_advance(); }
			else if (lexer_at(TOK_FLOAT)) { prog_add_const(prog, cname, clen, CONST_DOUBLE, 0, lexer_cur()->dbl, NULL, 0); lexer_advance(); }
			else if (lexer_at(TOK_STR)) { prog_add_const(prog, cname, clen, CONST_STR, 0, 0, lexer_cur()->str, lexer_cur()->str_len); lexer_advance(); }
			else { mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1, "'const' expects a number or string after ':'"); }
			continue;
		}
		if (lexer_at(TOK_ID) && lexer_at_peek(TOK_COLON)) {
			char *name = lexer_cur()->start;
			size_t len = lexer_cur()->len;
			lexer_advance();
			lexer_advance();
			if (lexer_at(TOK_INT) && (lexer_at_peek(TOK_NEWLINE) || lexer_at_peek(TOK_EOF))) {
				IrNode *v = new_ir(IR_INT);
				v->u.i = lexer_cur()->val;
				lexer_advance();
				IrNode *chain = make_assign_chain(prog, name, len, v);
				if (!head) head = tail = chain; else { tail->next = chain; }
				while (tail->next) tail = tail->next;
				continue;
			}
			if (lexer_at(TOK_STR) && (lexer_at_peek(TOK_NEWLINE) || lexer_at_peek(TOK_EOF))) {
				IrNode *v = new_ir(IR_STR);
				v->u.str.s = lexer_cur()->str;
				v->u.str.len = lexer_cur()->str_len;
				lexer_advance();
				IrNode *chain = make_assign_chain(prog, name, len, v);
				if (!head) head = tail = chain; else { tail->next = chain; }
				while (tail->next) tail = tail->next;
				continue;
			}
			/* id : 闁氨鏁ょ悰銊ㄦ彧�???*/
			IrNode *expr_head = NULL, *expr_tail = NULL;
			while (!lexer_at(TOK_NEWLINE) && !lexer_at(TOK_EOF)) {
				IrNode *o = parse_one(prog, false);
				if (!o) break;
				if (!expr_head) expr_head = expr_tail = o; else { expr_tail->next = o; expr_tail = o; }
			}
			if (expr_head) {
				int slot = prog_add_var(prog, name, len);
				IrNode *var_op = new_ir(IR_VAR);
				var_op->u.var_slot = slot;
				IrNode *store_op = new_ir(IR_WORD);
				store_op->u.word.name = "!"; store_op->u.word.len = 1;
				expr_tail->next = var_op; var_op->next = store_op;
				if (!head) head = tail = expr_head; else { tail->next = expr_head; }
				while (tail->next) tail = tail->next;
				continue;
			}
		}
		IrNode *o = parse_one(prog, false);
		if (!o) break;
		
		/* Intercept postfix `=>` for infix Lambdas: `(x) => { body }` becomes `x { body } =>` */
		if (o->kind == IR_WORD && o->u.word.len == 2 && memcmp(o->u.word.name, "=>", 2) == 0) {
			if (!head || head == tail) {
				mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1, "'=>' expects arguments and a body block");
			}
			
			/* Find the node right before `tail` using a loop */
			IrNode *prev_tail = NULL;
			IrNode *curr = head;
			while (curr && curr->next && curr->next != tail && curr != tail) {
				prev_tail = curr;
				curr = curr->next;
			}
			/* If tail is the second element, prev_tail is head */
			if (head->next == tail) prev_tail = head;
			if (!prev_tail) {
				mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1, "'=>' expects arguments and a body block");
			}

			IrNode *body = tail;
			IrNode *args_head = head; // Arguments actually span from head to prev_tail. Note: This means Lambda binds strictly at the top level of this sub-block.
			
			/* Pop them from the list */
			prev_tail->next = NULL; // Now args_head is an isolated list `a -> b -> c`
			head = tail = NULL; // Reset to start fresh, lambda is the first thing in the current chain
			
			/* Construct IR_LAMBDA */
			IrNode *lam = new_ir(IR_LAMBDA);
			lam->u.lambda.body = body;
			
			/* Gather params */
			int pcount = 0;
			IrNode *p = args_head;
			while (p) { pcount++; p = p->next; }
			
			char **pnames = arena_alloc(&comp->prog->ir_arena, pcount * sizeof(char*));
			size_t *plens = arena_alloc(&comp->prog->ir_arena, pcount * sizeof(size_t));
			
			p = args_head;
			int idx = 0;
			while (p) {
				if (p->kind == IR_WORD || p->kind == IR_VAR) {
					/* Infix parser will pass variables as IR_WORD or IR_VAR */
					if (p->kind == IR_WORD) {
						pnames[idx] = p->u.word.name;
						plens[idx] = p->u.word.len;
					} else { // IR_VAR (if it resolved existing variable, though usually it's just WORD in params)
						// mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1, "Invalid lambda parameter type");
					}
				}
				idx++;
				p = p->next;
			}
			
			lam->u.lambda.params = pnames;
			lam->u.lambda.param_lens = plens;
			lam->u.lambda.param_count = pcount;
			
			/* Push Lambda node */
			head = tail = lam;
			continue;
		}

		if (!head) { head = tail = o; } 
		else { tail->next = o; tail = o; }
		}
		if (!lexer_at(TOK_NEWLINE) && !lexer_at(TOK_EOF)) break; /* parse_one gave up */
		if (lexer_at(TOK_EOF)) break;
	}
	return head;
}

/* 瑙ｆ瀽鍙傛暟鍒�?��?? { ... }锛屾敹闆嗙敱绌烘牸鍒嗛�?�鐨�?爣璇嗙 */
static bool parse_param_list(char ***out_names, size_t **out_lens, int *out_count) {
	if (!lexer_at(TOK_LBRACE)) return false;
	lexer_advance();
	char **names = NULL;
	size_t *lens = NULL;
	int n = 0, cap = 4;
	names = arena_alloc(&comp->prog->ir_arena, (size_t)cap * sizeof(char *));
	lens = arena_alloc(&comp->prog->ir_arena, (size_t)cap * sizeof(size_t));
	while (lexer_at(TOK_ID)) {
		if (n >= cap) {
			cap *= 2;
			char **nn = arena_alloc(&comp->prog->ir_arena, (size_t)cap * sizeof(char *));
			size_t *nl = arena_alloc(&comp->prog->ir_arena, (size_t)cap * sizeof(size_t));
			memcpy(nn, names, n * sizeof(char *));
			memcpy(nl, lens, n * sizeof(size_t));
			names = nn; lens = nl;
		}
		names[n] = lexer_cur()->start;
		lens[n] = lexer_cur()->len;
		n++;
		lexer_advance();
	}
	lexer_expect(TOK_RBRACE);
	*out_names = names;
	*out_lens = lens;
	*out_count = n;
	return true;
}

