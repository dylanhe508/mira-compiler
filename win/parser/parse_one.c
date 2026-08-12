/* parser/parse_one.c 鈥?鏍稿績 token 璋冨害鍣?*/
#include "parser.h"
IrNode *parse_block_content(Program *prog);

static IrNode *parse_one(Program *prog, bool is_infix_recurse) {
	Token *t = lexer_cur();

	/* --- LBRACE BLOCK --- */
	if (lexer_at(TOK_LBRACE)) {
		lexer_advance(); /* Eat "{" */
		IrNode *b = parse_block_content(prog);
		if (lexer_at(TOK_RBRACE)) lexer_advance(); /* Eat "}" */
		IrNode *o = new_ir(IR_BLOCK);
		o->u.block = b;
		return o;
	}

	/* --- INFIX MODE C-STYLE FLOW CONTROL INTERCEPTION --- */
	if (current_syntax_mode == 1 && lexer_at(TOK_ID)) {
		/* C-style "if (cond) { body } else { body }" */
		if (lexer_cur()->len == 2 && memcmp(lexer_cur()->start, "if", 2) == 0 && lexer_at_peek(TOK_LPAREN)) {
			lexer_advance(); /* Eat "if" */
			
			lexer_advance(); /* Eat "(" */
			
			IrNode *cond_block = parse_infix_line(prog);
			
			/* 
			   parse_infix_line is now smart enough to halt at \n or } or RPAREN at paren_depth == 0.
			   Because it halted at RPAREN, we just check and consume it.
			*/
			if (lexer_at(TOK_RPAREN)) lexer_advance();


			if (!lexer_at(TOK_LBRACE)) {
				mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1, "'if' expects a { block } after condition");
			}
			
			IrNode *then_block = parse_one(prog, false);
			
			IrNode *else_block = NULL;
			while (lexer_eat(TOK_NEWLINE)) {}
			if (lexer_at(TOK_ID) && lexer_cur()->len == 4 && memcmp(lexer_cur()->start, "else", 4) == 0) {
				lexer_advance(); /* Eat "else" */
				else_block = parse_one(prog, false);
			}
			
			IrNode *if_op = new_ir(IR_IF);
			if_op->u.iff.cond = cond_block;
			if_op->u.iff.then_b = then_block;
			if_op->u.iff.else_b = else_block;
			return if_op;
		}
		
		/* C-style "while (cond) { body }" */
		if (lexer_cur()->len == 5 && memcmp(lexer_cur()->start, "while", 5) == 0 && lexer_at_peek(TOK_LPAREN)) {
			lexer_advance(); /* Eat "while" */
			
			lexer_advance(); /* Eat "(" */
			
			/* Check for while(true) or while(1) 芒鈥?infinite loop */
			int is_infinite = 0;
			if (lexer_at(TOK_ID) && lexer_cur()->len == 4 && memcmp(lexer_cur()->start, "true", 4) == 0 && lexer_at_peek(TOK_RPAREN)) {
				is_infinite = 1;
				lexer_advance(); /* Eat "true" */
			} else if (lexer_at(TOK_INT) && lexer_cur()->val != 0 && lexer_at_peek(TOK_RPAREN)) {
				is_infinite = 1;
				lexer_advance(); /* Eat the nonzero int */
			}
			
			if (is_infinite) {
				if (lexer_at(TOK_RPAREN)) lexer_advance();
				IrNode *body_block = parse_one(prog, false);
				IrNode *wh_op = new_ir(IR_WHILE_INF);
				wh_op->u.while_inf.body = body_block;
				return wh_op;
			}
			
			IrNode *cond_block = parse_infix_line(prog);
			
			if (lexer_at(TOK_RPAREN)) lexer_advance();


			if (!lexer_at(TOK_LBRACE)) {
				mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1, "'while' expects a { block } after condition");
			}
			IrNode *body_block = parse_one(prog, false);
			
			IrNode *wh_op = new_ir(IR_WHILE_COND);
			wh_op->u.while_cond.cond = cond_block;
			wh_op->u.while_cond.body = body_block;
			return wh_op;
		}

		/* C-style "for (start, end) { body }" */
		if (lexer_cur()->len == 3 && memcmp(lexer_cur()->start, "for", 3) == 0 && lexer_at_peek(TOK_LPAREN)) {
			lexer_advance(); /* Eat "for" */
			lexer_advance(); /* Eat "("   */
			
			int64_t start = 0, end = 0;
			if (lexer_at(TOK_INT)) {
				start = lexer_cur()->val;
				lexer_advance();
			}
			if (lexer_at(TOK_COMMA)) lexer_advance(); // Skip comma
			if (lexer_at(TOK_INT)) {
				end = lexer_cur()->val;
				lexer_advance();
			}
			
			if (!lexer_at(TOK_RPAREN)) {
				mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1, "Expected ')' after for(start, end)");
			}
			lexer_advance(); /* Eat ")" */
			
			/* Inject 'i' into the symbol table so the body can reference it */
			int var_slot = prog_add_var(prog, "i", 1);
			IrNode *body_block = parse_one(prog, false);
			
			IrNode *fo = new_ir(IR_FOR_RANGE);
			fo->u.for_range.start = start;
			fo->u.for_range.end = end;
			fo->u.for_range.body = body_block;
			fo->u.for_range.var = NULL;
			fo->u.for_range.var_len = 0;
			fo->u.for_range.var_slot = var_slot;
			return fo;
		}
		/* break / continue / return 鈥?鐩存帴鐢熸垚 IR_WORD锛屼笉璧?infix 璺敱 */
		if ((lexer_cur()->len == 5 && memcmp(lexer_cur()->start, "break", 5) == 0) ||
		    (lexer_cur()->len == 8 && memcmp(lexer_cur()->start, "continue", 8) == 0) ||
		    (lexer_cur()->len == 6 && memcmp(lexer_cur()->start, "return", 6) == 0)) {
			char *name = lexer_cur()->start;
			size_t len = lexer_cur()->len;
			lexer_advance();
			IrNode *o = new_ir(IR_WORD);
			o->u.word.name = name;
			o->u.word.len = len;
			return o;
		}

		/* C-style "switch (expr) { 1: { ... } default: { ... } }" */
		if (lexer_cur()->len == 6 && memcmp(lexer_cur()->start, "switch", 6) == 0 && lexer_at_peek(TOK_LPAREN)) {
			lexer_advance(); /* Eat "switch" */
			lexer_advance(); /* Eat "(" */
			IrNode *val_expr = parse_infix_line(prog);
			if (lexer_at(TOK_RPAREN)) lexer_advance();
			
			if (!lexer_at(TOK_LBRACE)) {
				mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1, "Expected '{' after switch expression");
			}
			lexer_advance(); /* Eat "{" */
			
			IrNode *cases_head = NULL;
			IrNode *cases_tail = NULL;
			IrNode *default_block = NULL;
			
			while (!lexer_at(TOK_EOF) && !lexer_at(TOK_RBRACE)) {
				if (lexer_at(TOK_NEWLINE)) { lexer_advance(); continue; }
				
				if (lexer_cur()->len == 7 && memcmp(lexer_cur()->start, "default", 7) == 0 && lexer_at_peek(TOK_COLON)) {
					lexer_advance(); /* Eat "default" */
					lexer_advance(); /* Eat ":" */
					default_block = parse_one(prog, false);
				} else {
					IrNode *pattern = parse_infix_line(prog);
					if (lexer_at(TOK_COLON)) lexer_advance();
					IrNode *body = parse_one(prog, false);
					
					/* Link pattern -> body -> next_pattern */
					if (pattern) {
						pattern->next = body;
						if (!cases_head) { cases_head = pattern; cases_tail = body; }
						else { cases_tail->next = pattern; cases_tail = body; }
					}
				}
			}
			if (lexer_at(TOK_RBRACE)) lexer_advance();
			
			IrNode *sw = new_ir(IR_SWITCH);
			sw->u.switch_.value = val_expr;
			sw->u.switch_.cases = cases_head;
			sw->u.switch_.default_block = default_block;
			return sw;
		}

		/* C-style "try { body }" */
		if (lexer_cur()->len == 3 && memcmp(lexer_cur()->start, "try", 3) == 0 && lexer_at_peek(TOK_LBRACE)) {
			lexer_advance(); /* Eat "try" */
			IrNode *body = parse_one(prog, false);
			IrNode *try_op = new_ir(IR_TRY);
			try_op->u.try_block.body = body;
			return try_op;
		}

		/* C-style "each (list_expr) { body }" */
		if (lexer_cur()->len == 4 && memcmp(lexer_cur()->start, "each", 4) == 0 && lexer_at_peek(TOK_LPAREN)) {
			lexer_advance(); /* Eat "each" */
			lexer_advance(); /* Eat "(" */
			IrNode *list_expr = parse_infix_line(prog);
			if (lexer_at(TOK_RPAREN)) lexer_advance();
			IrNode *body = parse_one(prog, false);
			IrNode *ea = new_ir(IR_EACH);
			ea->u.each.list = list_expr;
			ea->u.each.body = body;
			return ea;
		}
	}
	/* --- END INFIX C-STYLE FLOW CONTROL INTERCEPTION --- */

	/* If we're not inside a C-style flow structure but we ARE in infix mode, 
	   route the entire line out to Shunting-Yard automatically UNLESS it's { } */
	if (current_syntax_mode == 1 && !lexer_at(TOK_LBRACE) && !lexer_at(TOK_LBRACKET) && !is_infix_recurse
	    && !(lexer_at(TOK_INT) && lexer_at_peek(TOK_LBRACKET))) {
		return parse_infix_line(prog);
	}

	/* --- POSTFIX C-STYLE LAMBDA OR LIST LITERAL WITHOUT SIZE --- */
	if (lexer_at(TOK_LBRACKET)) {
		lexer_advance(); // Eat `[`
		
		IrNode *elem_head = NULL, *elem_tail = NULL;
		int count = 0;
		int all_identifiers = 1;
		
		/* We'll collect up to 32 param names for a lambda check. If more, it's just a list. */
		char *pnames[32];
		size_t plens[32];

		while (!lexer_at(TOK_RBRACKET) && !lexer_at(TOK_EOF)) {
			if (count < 32 && lexer_at(TOK_ID)) {
				pnames[count] = lexer_cur()->start;
				plens[count] = lexer_cur()->len;
			} else {
				all_identifiers = 0;
			}
			IrNode *e = parse_one(prog, true); /* true: parse individual tokens, not full infix lines */
			if (!e) break;
			if (!elem_head) elem_head = elem_tail = e;
			else { elem_tail->next = e; elem_tail = e; }
			while (elem_tail->next) elem_tail = elem_tail->next;
			count++;
		}
		lexer_expect(TOK_RBRACKET);
		
		/* 濡傛灉鍚庨潰璺熺殑鏄?{锛屼笖鍒氭墠鎷彿閲屽叏閮芥槸鏍囪瘑绗︼紙鎴栦负绌猴級锛岃鏄庡畠鏄悗缂€ Lambda: [ x y ] { ... } */
		if (lexer_at(TOK_LBRACE) && all_identifiers) {
			lexer_advance(); // Eat '{'
			int saved_var_scope = current_var_scope;
			current_var_scope = prog_new_var_scope(prog, saved_var_scope);
			IrNode *body = parse_block_content(prog);
			current_var_scope = saved_var_scope;
			lexer_expect(TOK_RBRACE);
			IrNode *lam = new_ir(IR_LAMBDA);
			
			char **lam_pnames = NULL;
			size_t *lam_plens = NULL;
			if (count > 0) {
				lam_pnames = arena_alloc(&comp->prog->ir_arena, count * sizeof(char*));
				lam_plens = arena_alloc(&comp->prog->ir_arena, count * sizeof(size_t));
				for (int i=0; i<count; i++) { lam_pnames[i] = pnames[i]; lam_plens[i] = plens[i]; }
			}
			
			lam->u.lambda.params = lam_pnames;
			lam->u.lambda.param_lens = lam_plens;
			lam->u.lambda.param_count = count;
			lam->u.lambda.body = body;
			return lam;
		}
		
		/* Otherwise it's just an array literal like `[1, 2, 3]` (size inferred from count) */
		char *tmp_name = arena_alloc(&comp->prog->ir_arena, 16);
		if (!tmp_name) { perror("malloc"); exit(1); }
		snprintf(tmp_name, 16, "__L%d", list_literal_id++);
		int slot = prog_add_var(prog, tmp_name, (size_t)strlen(tmp_name));
		IrNode *lit = new_ir(IR_LIST_LITERAL);
		lit->u.list_literal.size = count;
		lit->u.list_literal.elements = elem_head;
		lit->u.list_literal.count = count;
		lit->u.list_literal.temp_slot = slot;
		return lit;
	}

	if (lexer_at(TOK_INT)) {
		int64_t size_val = t->val;
		/* Legacy: size [ elem ... ] */
		if (lexer_at_peek(TOK_LBRACKET)) {
			lexer_advance(); // size
			lexer_advance(); // [
			IrNode *elem_head = NULL, *elem_tail = NULL;
			int count = 0;
			while (!lexer_at(TOK_RBRACKET) && !lexer_at(TOK_EOF)) {
				IrNode *e = parse_one(prog, true);
				if (!e) break;
				if (!elem_head) elem_head = elem_tail = e;
				else { elem_tail->next = e; elem_tail = e; }
				while (elem_tail->next) elem_tail = elem_tail->next;
				count++;
			}
			lexer_expect(TOK_RBRACKET);
			if (count != (int)size_val) {
				mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1, "list literal: size %lld does not match element count %d", (long long)size_val, count);
			}
			char *tmp_name = arena_alloc(&comp->prog->ir_arena, 16);
			snprintf(tmp_name, 16, "__L%d", list_literal_id++);
			int slot = prog_add_var(prog, tmp_name, (size_t)strlen(tmp_name));
			IrNode *lit = new_ir(IR_LIST_LITERAL);
			lit->u.list_literal.size = (int)size_val;
			lit->u.list_literal.elements = elem_head;
			lit->u.list_literal.count = count;
			lit->u.list_literal.temp_slot = slot;
			return lit;
		}
		IrNode *o = new_ir(IR_INT);
		o->u.i = size_val;
		lexer_advance();
		return o;
	}
	if (lexer_at(TOK_FLOAT)) {
		IrNode *o = new_ir(IR_FLOAT);
		o->u.d = t->dbl;
		lexer_advance();
		return o;
	}
	if (lexer_at(TOK_STR)) {
		char *s = t->str;
		size_t slen = t->str_len;
		lexer_advance();

		/* 妫€鏌ュ瓧绗︿覆鍐呮湁鏃?{expr} 鎻掑€?*/
		int has_interp = 0;
		for (size_t i = 0; i < slen; i++) {
			if (s[i] == '{' && (i == 0 || s[i-1] != '\\')) { has_interp = 1; break; }
		}
		if (!has_interp) {
			IrNode *o = new_ir(IR_STR);
			o->u.str.s = s;
			o->u.str.len = slen;
			return o;
		}

		/* 閫愭鎷嗗垎鎷兼帴锛?abc{x}def" 鍙樻垚 "abc" x @ to-str str-concat "def" str-concat */
		IrNode *chain_head = NULL, *chain_tail = NULL;
		int seg_count = 0;
		size_t i = 0;

		#define APPEND_OP(IrNode) do { \
			if (!chain_head) { chain_head = chain_tail = (IrNode); } \
			else { chain_tail->next = (IrNode); chain_tail = (IrNode); } \
		} while(0)

		while (i <= slen) {
			size_t seg_start = i;
			while (i < slen && !(s[i] == '{' && (i == 0 || s[i-1] != '\\'))) i++;

			/* 绾枃鏈墖娈?*/
			if (i > seg_start) {
				size_t seg_len = i - seg_start;
				char *seg = arena_alloc(&comp->prog->ir_arena, seg_len + 1);
				memcpy(seg, s + seg_start, seg_len);
				seg[seg_len] = '\0';
				IrNode *str_op = new_ir(IR_STR);
				str_op->u.str.s = seg;
				str_op->u.str.len = seg_len;
				APPEND_OP(str_op);
				if (seg_count > 0) {
					IrNode *cat = new_ir(IR_WORD);
					cat->u.word.name = "str-concat"; cat->u.word.len = 10;
					APPEND_OP(cat);
				}
				seg_count++;
			}
			if (i >= slen) break;

			/* 璺宠繃 { */
			i++;
			size_t expr_start = i;
			int depth = 1;
			while (i < slen && depth > 0) {
				if (s[i] == '{') depth++;
				else if (s[i] == '}') depth--;
				if (depth > 0) i++;
			}
			size_t expr_len = i - expr_start;
				if (i < slen) i++; /* 璺宠繃 } */

			if (expr_len > 0) {
				char *expr = s + expr_start;
				/* 瑙ｆ瀽 expr 鐨勭被鍨?*/
				if (isdigit((unsigned char)expr[0]) || (expr[0] == '-' && expr_len > 1 && isdigit((unsigned char)expr[1]))) {
					/* 鏁板瓧瀛楅潰閲?*/
					int64_t val = 0;
					int neg = 0;
					size_t j = 0;
					if (expr[0] == '-') { neg = 1; j = 1; }
					for (; j < expr_len && isdigit((unsigned char)expr[j]); j++)
						val = val * 10 + (expr[j] - '0');
					if (neg) val = -val;
					IrNode *iop = new_ir(IR_INT);
					iop->u.i = val;
					APPEND_OP(iop);
				} else {
					/* 鍙橀噺鍚嶆煡鎵?*/
					int vslot = prog_var_slot(prog, expr, expr_len);
					if (vslot >= 0) {
						IrNode *var_op = new_ir(IR_VAR);
						var_op->u.var_slot = vslot;
						APPEND_OP(var_op);
						/* 鑷姩 fetch 鍙栧€?*/
						IrNode *fetch = new_ir(IR_WORD);
						fetch->u.word.name = "@"; fetch->u.word.len = 1;
						APPEND_OP(fetch);
					} else {
						/* 甯搁噺鏌ユ壘 */
						int cslot = prog_const_slot(prog, expr, expr_len);
						if (cslot >= 0) {
							IrNode *cop = new_ir(IR_CONST);
							cop->u.const_slot = cslot;
							APPEND_OP(cop);
						} else {
							/* 鍥為€€涓烘櫘閫氱殑 word 璋冪敤 */
							char *wname = arena_alloc(&comp->prog->ir_arena, expr_len + 1);
							memcpy(wname, expr, expr_len);
							wname[expr_len] = '\0';
							IrNode *wop = new_ir(IR_WORD);
							wop->u.word.name = wname;
							wop->u.word.len = expr_len;
							APPEND_OP(wop);
						}
					}
				}
				/* to-str 淇濊瘉鎵€鏈夋彃鍊肩粨鏋滈兘杞负瀛楃涓?*/
				IrNode *tostr = new_ir(IR_WORD);
				tostr->u.word.name = "to-str"; tostr->u.word.len = 6;
				APPEND_OP(tostr);
				/* 鎷兼帴 */
				if (seg_count > 0) {
					IrNode *cat = new_ir(IR_WORD);
					cat->u.word.name = "str-concat"; cat->u.word.len = 10;
					APPEND_OP(cat);
				}
				seg_count++;
			}
		}
		#undef APPEND_OP
		return chain_head ? chain_head : new_ir(IR_STR);
	}
	if (lexer_at(TOK_ID)) {
		char *name = t->start;
		size_t len = t->len;
		char *dot = memchr(name, '.', len);
		if (current_syntax_mode == 1 && dot && dot != name && dot + 1 < name + len &&
		    !lexer_at_peek(TOK_LPAREN)) {
			size_t receiver_len = (size_t)(dot - name);
			char *member = dot + 1;
			size_t member_len = len - receiver_len - 1;
			int receiver_slot = prog_var_slot(prog, name, receiver_len);
			StructDef *owner = receiver_slot >= 0 ? prog->var_structs[receiver_slot] : NULL;
			extern StructDef *current_impl_owner;
			int is_self = receiver_len == 4 && memcmp(name, "self", 4) == 0;
			if (is_self) owner = current_impl_owner;
			if (!owner) goto normal_identifier;
			if (prog_field_offset(owner, member, member_len) < 0)
				mira_error(comp->src, comp->filename, t->line, t->col, 1,
					"unknown field '%.*s.%.*s'", (int)owner->name_len, owner->name,
					(int)member_len, member);
			lexer_advance();
			IrNode *value;
			if (is_self) {
				value = new_ir(IR_WORD);
				value->u.word.name = "self"; value->u.word.len = 4;
			} else {
				value = new_ir(IR_VAR); value->u.var_slot = receiver_slot;
				IrNode *fetch = new_ir(IR_WORD); fetch->u.word.name = "@"; fetch->u.word.len = 1;
				value->next = fetch;
			}
			IrNode *tail = value; while (tail->next) tail = tail->next;
			size_t getter_len = owner->name_len + 1 + member_len;
			char *getter_name = arena_alloc(&prog->ir_arena, getter_len + 1);
			memcpy(getter_name, owner->name, owner->name_len);
			getter_name[owner->name_len] = '.';
			memcpy(getter_name + owner->name_len + 1, member, member_len);
			getter_name[getter_len] = '\0';
			IrNode *getter = new_ir(IR_WORD);
			getter->u.word.name = getter_name; getter->u.word.len = getter_len;
			tail->next = getter;
			return value;
		}
normal_identifier:
		if (len == 4 && memcmp(name, "eval", 4) == 0) {
			lexer_advance(); // Consume 'eval'
			return parse_eval_block(prog);
		}
		if (len == 3 && memcmp(name, "var", 3) == 0) {
			lexer_advance(); // Consume 'var'
			if (!lexer_at(TOK_ID)) {
				mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1, "Expected identifier after 'var'");
			}
			char *vname = lexer_cur()->start;
			size_t vlen = lexer_cur()->len;
			int vslot = prog_add_var(prog, vname, vlen);
			lexer_advance(); // Consume identifier
			
			IrNode *o = new_ir(IR_VAR);
			o->u.var_slot = vslot;
			
			IrNode *store = new_ir(IR_WORD);
			store->u.word.name = "!";
			store->u.word.len = 1;
			o->next = store;
			
			/* var x = expr 初始化。infix 模式下 parse_infix_line 遇到
			 * 单独 '=' 会直接 break(把赋值留给 parse_block_content),
			 * 导致顶层/行内声明的初始化表达式被静默丢弃。这里在
			 * 声明处直接消费 '=' 并解析到行尾,生成 value var ! 链。 */
			if (lexer_at(TOK_ID) && lexer_cur()->len == 1 &&
			    lexer_cur()->start[0] == '=') {
				lexer_advance(); /* Consume '=' */
				IrNode *init = parse_infix_line(prog);
				if (init) {
					IrNode *tail = init;
					while (tail->next) tail = tail->next;
					tail->next = o;   /* value ... VAR(slot) ! */
					return init;
				}
			}
			/* �޳�ֵ var ����:��ѹ 0 �� store,�����ջ store ���� Stack underflow,�������г�ֵһ��(��ʼΪ 0) */
			IrNode *zero = new_ir(IR_INT);
			zero->u.i = 0;
			zero->next = o;
			return zero;
		}
		lexer_advance();
		int cslot = prog_const_slot(prog, name, len);
		if (cslot >= 0) {
			IrNode *o = new_ir(IR_CONST);
			o->u.const_slot = cslot;
			return o;
		}
		int vslot = prog_var_slot(prog, name, len);
		if (vslot >= 0) {
			IrNode *o = new_ir(IR_VAR);
			o->u.var_slot = vslot;
			/* In infix mode (called from Shunting-Yard), auto-fetch the value.
			   In postfix mode the user writes "var @" explicitly.  A variable
			   followed by '!' is a store target, never a fetch; a variable
			   already followed by '@' needs no second fetch. */
			if (is_infix_recurse) {
				if (!(lexer_at(TOK_ID) && lexer_cur()->len == 1 &&
				      (lexer_cur()->start[0] == '@' ||
				       lexer_cur()->start[0] == '!'))) {
					IrNode *fetch = new_ir(IR_WORD);
					fetch->u.word.name = "@";
					fetch->u.word.len = 1;
					o->next = fetch;
				}
			}
			return o;
		}
		IrNode *o = new_ir(IR_WORD);
		o->u.word.name = name;
		o->u.word.len = len;
		return o;
	}
	if (lexer_at(TOK_LBRACE)) {
		lexer_advance();
		IrNode *block = new_ir(IR_BLOCK);
		block->u.block = parse_block_content(prog);
		lexer_expect(TOK_RBRACE);
		return block;
	}
	return NULL;
}
