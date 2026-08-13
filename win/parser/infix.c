/* parser/infix.c a鈧????-??鈧? Shunting-Yard 猫搂锟???? */
#include "parser.h"

/* 杩愮畻绗︽爤缁熶竴 push:鍗曡?岃〃杈惧紡 token 鏁颁笉璁炬?讳笂闄?,arena 鍔ㄦ?佹墿瀹广??
 * IR_stack / IR_stack_len / IR_stack_cap 鐢变娇鐢ㄥ嚱鏁板０鏄?,comp 涓哄叏灞?瑙ｆ瀽涓婁笅鏂囥?? */
#define PUSH_OP_STACK(tok) do { \
	if (IR_stack_len >= IR_stack_cap) { \
		int new_cap = IR_stack_cap ? IR_stack_cap * 2 : 128; \
		Token *nb = arena_alloc(&comp->prog->ir_arena, (size_t)new_cap * sizeof(Token)); \
		if (IR_stack) memcpy(nb, IR_stack, (size_t)IR_stack_len * sizeof(Token)); \
		IR_stack = nb; IR_stack_cap = new_cap; \
	} \
	IR_stack[IR_stack_len++] = (tok); \
} while (0)
static IrNode *modern_negative_literal(const Token *t) {
	if (!t || t->kind != TOK_ID || t->len < 2 || t->start[0] != '-') return NULL;
	size_t first_digit = 0;
	while (first_digit < t->len && t->start[first_digit] == '-') first_digit++;
	if (first_digit == t->len) return NULL;
	int dot = 0;
	for (size_t i = first_digit; i < t->len; ++i) {
		if (t->start[i] == '.' && !dot) { dot = 1; continue; }
		if (!isdigit((unsigned char)t->start[i])) return NULL;
	}
	if (dot) {
		char buf[96]; size_t n = t->len - first_digit;
		if (n >= sizeof(buf)) n = sizeof(buf) - 1;
		memcpy(buf, t->start + first_digit, n); buf[n] = '\0';
		double value = strtod(buf, NULL);
		IrNode *o = new_ir(IR_FLOAT);
		o->u.d = (first_digit & 1) ? -value : value;
		return o;
	}
	uint64_t value = 0;
	for (size_t i = first_digit; i < t->len; ++i)
		value = value * 10u + (uint64_t)(t->start[i] - '0');
	IrNode *o = new_ir(IR_INT);
	o->u.i = (int64_t)((first_digit & 1) ? (UINT64_C(0) - value) : value);
	return o;
}

static IrNode *parse_eval_block(Program *prog) {
	lexer_expect(TOK_LPAREN);
	int paren_depth = 1;

	IrNode *out_queue_head = NULL;
	IrNode *out_queue_tail = NULL;
	
	// Operator stack for shunting yard
	Token *IR_stack = NULL;
	int IR_stack_cap = 0;
	int IR_stack_len = 0;

	// Helper to push to output queue
	void push_out(IrNode *node) {
		if (!out_queue_head) { out_queue_head = node; out_queue_tail = node; }
		else { out_queue_tail->next = node; out_queue_tail = node; }
		while (out_queue_tail->next) out_queue_tail = out_queue_tail->next;
	}

	while (!lexer_at(TOK_EOF)) {
		if (lexer_at(TOK_INT) || lexer_at(TOK_FLOAT) || lexer_at(TOK_STR) ||
		    lexer_at(TOK_LBRACKET)) {
			IrNode *node = parse_one(prog, false);
			if (node) push_out(node);
		} else if (lexer_at(TOK_LPAREN)) {
			paren_depth++;
			PUSH_OP_STACK(*lexer_cur());
			lexer_advance();
		} else if (lexer_at(TOK_RPAREN)) {
			paren_depth--;
			if (paren_depth == 0) {
				// This is actually the outer closing parenthesis of eval(...)
				break;
			}

			bool found_lparen = false;
			while (IR_stack_len > 0) {
				Token top = IR_stack[IR_stack_len - 1];
				if (top.kind == TOK_LPAREN) {
					found_lparen = true;
					IR_stack_len--; // Pop LPAREN
					break;
				}
				IR_stack_len--; // Pop operator
				char *top_op_str = arena_alloc(&comp->prog->ir_arena, top.len + 1);
				memcpy(top_op_str, top.start, top.len);
				top_op_str[top.len] = '\0';
				IrNode *IR_node = new_ir(IR_WORD);
				IR_node->u.word.name = top_op_str;
				IR_node->u.word.len = top.len;
				push_out(IR_node);
			}
			
			// Pop function if present
			if (found_lparen && IR_stack_len > 0) {
				Token top = IR_stack[IR_stack_len - 1];
				if (top.kind == TOK_ID) {
					char *top_op_str = arena_alloc(&comp->prog->ir_arena, top.len + 1);
					memcpy(top_op_str, top.start, top.len);
					top_op_str[top.len] = '\0';
					if (strcmp(top_op_str, "==") == 0) strcpy(top_op_str, "=");
					if (get_op_precedence(top_op_str) == 0) {
						IrNode *IR_node = new_ir(IR_WORD);
						IR_node->u.word.name = top_op_str;
						IR_node->u.word.len = top.len;
						push_out(IR_node);
						IR_stack_len--;
					}
				}
			}

			if (!found_lparen) {
				mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1, "Mismatched parentheses in eval()");
				return NULL;
			}
			lexer_advance();
		} else if (lexer_at(TOK_COMMA)) {
			/* A comma terminates the current function argument.  Shunting-yard
			 * requires every pending operator for that argument to be emitted up
			 * to (but not including) the nearest left parenthesis. */
			while (IR_stack_len > 0 && IR_stack[IR_stack_len - 1].kind != TOK_LPAREN) {
				Token top = IR_stack[--IR_stack_len];
				char *top_op_str = arena_alloc(&comp->prog->ir_arena, top.len + 1);
				memcpy(top_op_str, top.start, top.len);
				top_op_str[top.len] = '\0';
				if (strcmp(top_op_str, "==") == 0) strcpy(top_op_str, "=");
				IrNode *IR_node = new_ir(IR_WORD);
				IR_node->u.word.name = top_op_str;
				IR_node->u.word.len = top.len;
				push_out(IR_node);
			}
			lexer_advance();
		} else if (lexer_at(TOK_ID)) {
			Token *t = lexer_cur();
			IrNode *joined_negative = modern_negative_literal(t);
			if (joined_negative) {
				lexer_advance(); push_out(joined_negative); continue;
			}
			char *IR_str = arena_alloc(&comp->prog->ir_arena, t->len + 1);
			memcpy(IR_str, t->start, t->len);
			IR_str[t->len] = '\0';

			int prec = get_op_precedence(IR_str);
			
			if (prec > 0) { // It's an operator (+, -, etc.)
				while (IR_stack_len > 0) {
					Token top = IR_stack[IR_stack_len - 1];
					if (top.kind == TOK_LPAREN) break;
					
					char *top_op_str = arena_alloc(&comp->prog->ir_arena, top.len + 1);
					memcpy(top_op_str, top.start, top.len);
					top_op_str[top.len] = '\0';
					if (strcmp(top_op_str, "==") == 0) strcpy(top_op_str, "=");
					int top_prec = get_op_precedence(top_op_str);

					if (top_prec >= prec) {
						IrNode *IR_node = new_ir(IR_WORD);
						IR_node->u.word.name = top_op_str;
						IR_node->u.word.len = top.len;
						push_out(IR_node);
						IR_stack_len--;
					} else {
						break;
					}
				}
				PUSH_OP_STACK(*t);
				lexer_advance();
			} else if (lexer_at_peek(TOK_LPAREN)) { // Function call identifier
				char *dot = memchr(t->start, '.', t->len);
				if (dot && dot != t->start && dot + 1 < t->start + t->len) {
					size_t receiver_len = (size_t)(dot - t->start);
					size_t member_len = t->len - receiver_len - 1;
					int slot = prog_var_slot(prog, t->start, receiver_len);
					StructDef *owner = slot >= 0 ? prog->var_structs[slot] : NULL;
					extern StructDef *current_impl_owner;
					int is_self = receiver_len == 4 && memcmp(t->start, "self", 4) == 0;
					if (is_self) owner = current_impl_owner;
					if (!owner) {
						PUSH_OP_STACK(*t);
						lexer_advance();
						continue;
					}
					MethodDef *method = owner ? prog_find_method(prog, owner, dot + 1, member_len) : NULL;
					if (!method)
						mira_error(comp->src, comp->filename, t->line, t->col, 1,
							"unknown method '%.*s'", (int)t->len, t->start);
					extern int current_method_mut_self;
					if (method->mut_self &&
					    ((!is_self && !prog->var_mutable[slot]) ||
					     (is_self && !current_method_mut_self)))
						mira_error(comp->src, comp->filename, t->line, t->col, 1,
							"mutable method requires a mutable receiver");
					IrNode *receiver = is_self ? new_ir(IR_WORD) : new_ir(IR_VAR);
					if (is_self) { receiver->u.word.name = "self"; receiver->u.word.len = 4; }
					else receiver->u.var_slot = slot;
					push_out(receiver);
					if (!is_self) {
						IrNode *fetch = new_ir(IR_WORD); fetch->u.word.name = "@"; fetch->u.word.len = 1;
						push_out(fetch);
					}
					Token call = *t; call.start = method->qualified_name; call.len = method->qualified_name_len;
					PUSH_OP_STACK(call);
				} else PUSH_OP_STACK(*t);
				lexer_advance();
			} else { // Normal variable or postfix word
				IrNode *node = parse_one(prog, false);
				if (node) push_out(node);
			}
		} else {
			mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1, "Unexpected token in eval()");
			return NULL; // Or handle error appropriately
		}
	}
	
	while (IR_stack_len > 0) {
		Token top = IR_stack[--IR_stack_len];
		if (top.kind == TOK_LPAREN) {
			mira_error(comp->src, comp->filename, top.line, 0, 1, "Mismatched parentheses in eval()");
			return NULL; // Or handle error appropriately
		}
		char *top_op_str = arena_alloc(&comp->prog->ir_arena, top.len + 1);
		memcpy(top_op_str, top.start, top.len);
		top_op_str[top.len] = '\0';
		if (strcmp(top_op_str, "==") == 0) strcpy(top_op_str, "=");
		IrNode *IR_node = new_ir(IR_WORD);
		IR_node->u.word.name = top_op_str;
		IR_node->u.word.len = top.len;
		push_out(IR_node);
	}

	lexer_expect(TOK_RPAREN);
	return out_queue_head;
}

static IrNode *parse_infix_line(Program *prog) {
	IrNode *out_queue_head = NULL;
	IrNode *out_queue_tail = NULL;
	
	// Operator stack for shunting yard
	Token *IR_stack = NULL;
	int IR_stack_cap = 0;
	int IR_stack_len = 0;
	int paren_depth = 0;
	int expect_operand = 1;
	int saw_operand = 0;
	/* Adjacent operands (e.g. `sum @` or `1 2`) make the line a postfix
	 * chain, not an infix expression: operands never sit side by side in
	 * infix syntax.  Once seen, later operators are stack words evaluated
	 * left-to-right and are emitted directly instead of parked on the
	 * operator stack. */
	int postfix_seen = 0;

	// Helper to push to output queue
	void push_out_line(IrNode *node) {
		if (!out_queue_head) { out_queue_head = node; out_queue_tail = node; }
		else { out_queue_tail->next = node; out_queue_tail = node; }
		/* Walk to the end of the chain (parse_one may return multi-node chains) */
		while (out_queue_tail->next) out_queue_tail = out_queue_tail->next;
	}

	while (!lexer_at(TOK_EOF)) {
		if (lexer_at(TOK_LBRACE)) {
			// Stop infix parsing immediately when encountering a block {
			// This allows C-style blocks like `if (cond) { body }` to parse `body` correctly
			// instead of treating `{` as part of the infix expression.
			break;
		}

		if (paren_depth == 0 && lexer_at(TOK_NEWLINE)) {
			/* A binary operator at end of line explicitly continues the
			 * expression. A complete expression still terminates normally. */
			if (expect_operand && saw_operand) {
				lexer_advance();
				continue;
			}
			break;
		}
		if (paren_depth == 0 && (lexer_at(TOK_EOF) ||
		    (lexer_at(TOK_ID) && lexer_cur()->len == 1 && lexer_cur()->start[0] == ';'))) {
			break;
		}
		if (lexer_at(TOK_NEWLINE)) {
			lexer_advance();
			continue;
		}

		if (lexer_at(TOK_INT) || lexer_at(TOK_FLOAT) || lexer_at(TOK_STR) ||
		    lexer_at(TOK_LBRACKET)) {
			IrNode *node = parse_one(prog, true);
			if (node) {
				push_out_line(node);
				/* 链尾是运算符(@/! 等后缀词)时不触发 postfix 链模式:
				 * `left @ + 1` 中 @ 之后是 infix 运算符, 右操作数尚未读入,
				 * 直接发射会让 + 排到 1 之前, SSA Builder 弹栈 underflow.
				 * postfix 链(如 `1 2 +`、`sum @ i @ +`)由第二个操作数
				 * (链尾为操作数节点)置位, 不受影响. */
				IrNode *tail = node;
				while (tail->next) tail = tail->next;
				if (expect_operand == 0 && tail->kind != IR_WORD) postfix_seen = 1;
				expect_operand = 0;
				saw_operand = 1;
			}
		} else if (lexer_at(TOK_LPAREN)) {
			paren_depth++;
			PUSH_OP_STACK(*lexer_cur());
			expect_operand = 1;
			lexer_advance();
		} else if (lexer_at(TOK_RPAREN)) {
			paren_depth--;
			if (paren_depth < 0) {
				/* We hit a ) that belongs to an outer context (e.g. if(...) handler).
				   Stop parsing and leave it for the caller to consume. */
				break;
			}
			bool found_lparen = false;
			while (IR_stack_len > 0) {
				Token top = IR_stack[IR_stack_len - 1];
				if (top.kind == TOK_LPAREN) {
					found_lparen = true;
					IR_stack_len--; // Pop LPAREN
					break;
				}
				IR_stack_len--; // Pop operator
					char *top_op_str = arena_alloc(&comp->prog->ir_arena, top.len + 1);
					memcpy(top_op_str, top.start, top.len);
					top_op_str[top.len] = '\0';
					if (strcmp(top_op_str, "==") == 0) strcpy(top_op_str, "=");
					IrNode *IR_node = new_ir(IR_WORD);
				IR_node->u.word.name = top_op_str;
				IR_node->u.word.len = top.len;
				push_out_line(IR_node);
			}
			
			// Pop function if present
			if (found_lparen && IR_stack_len > 0) {
				Token top = IR_stack[IR_stack_len - 1];
				if (top.kind == TOK_ID) {
					char *top_op_str = arena_alloc(&comp->prog->ir_arena, top.len + 1);
					memcpy(top_op_str, top.start, top.len);
					top_op_str[top.len] = '\0';
					if (strcmp(top_op_str, "==") == 0) strcpy(top_op_str, "=");
					if (get_op_precedence(top_op_str) == 0) {
						IrNode *IR_node = new_ir(IR_WORD);
						IR_node->u.word.name = top_op_str;
						IR_node->u.word.len = top.len;
						push_out_line(IR_node);
						IR_stack_len--;
					}
				}
			}

			if (!found_lparen) {
				mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1, "Mismatched parentheses in infix line");
				return NULL;
			}
			expect_operand = 0;
			saw_operand = 1;
			lexer_advance();
		} else if (lexer_at(TOK_COMMA)) {
			/* Finish the current argument before starting the next one.  Merely
			 * skipping the comma lets operators leak across argument boundaries
			 * (for example f(a+b, a-b) became a b a + b - f). */
			while (IR_stack_len > 0 && IR_stack[IR_stack_len - 1].kind != TOK_LPAREN) {
				Token top = IR_stack[--IR_stack_len];
				char *top_op_str = arena_alloc(&comp->prog->ir_arena, top.len + 1);
				memcpy(top_op_str, top.start, top.len);
				top_op_str[top.len] = '\0';
				if (strcmp(top_op_str, "==") == 0) strcpy(top_op_str, "=");
				IrNode *IR_node = new_ir(IR_WORD);
				IR_node->u.word.name = top_op_str;
				IR_node->u.word.len = top.len;
				push_out_line(IR_node);
			}
			expect_operand = 1;
			lexer_advance();
		} else if (lexer_at(TOK_ID)) {
			Token *t = lexer_cur();
			IrNode *joined_negative = modern_negative_literal(t);
			if (joined_negative) {
				lexer_advance();
				push_out_line(joined_negative);
				expect_operand = 0;
				saw_operand = 1;
				continue;
			}
			/* Postfix range loop in infix lines:  start end for { body }.
			   Stop here so parse_block_content handles the for; treating it
			   as a word would leak IR_W[for] into the chain. */
			if (t->len == 3 && memcmp(t->start, "for", 3) == 0 &&
			    lexer_at_peek(TOK_LBRACE)) {
				break;
			}
			char *IR_str = arena_alloc(&comp->prog->ir_arena, t->len + 1);
			memcpy(IR_str, t->start, t->len);
			IR_str[t->len] = '\0';

			int prec = get_op_precedence(IR_str);

			int prefix_minus_run = expect_operand && t->len > 0;
			for (size_t mi = 0; prefix_minus_run && mi < t->len; ++mi)
				if (t->start[mi] != '-') prefix_minus_run = 0;
			if (prefix_minus_run) {
				/* Prefix minus is a unary postfix `neg`, not binary subtraction.
				 * A lexer token may contain a whole run such as `--(`.  Even
				 * runs cancel; odd runs leave exactly one negation. */
				if (t->len & 1) {
					Token unary = *t;
					unary.start = "neg";
					unary.len = 3;
					PUSH_OP_STACK(unary);
				}
				lexer_advance();
			} else if (t->len == 1 && t->start[0] == '=') {
				/* Statement assignment is handled by parse_block_content().
				 * Leaving '=' here would incorrectly turn it into equality. */
				break;
			} else if (prec > 0) {
				/* It's an infix operator (+, -, ==, !=, <, >, etc.) */
				if (postfix_seen) {
					/* Postfix chain: emit the operator in place so
					 * `sum @ i @ + sum !` stays `sum@ i@ + sum!` instead
					 * of treating `sum !` as the right operand of '+'. */
					char *op_str = arena_alloc(&comp->prog->ir_arena, t->len + 1);
					memcpy(op_str, t->start, t->len);
					op_str[t->len] = '\0';
					if (strcmp(op_str, "==") == 0) strcpy(op_str, "=");
					IrNode *IR_node = new_ir(IR_WORD);
					IR_node->u.word.name = op_str;
					IR_node->u.word.len = t->len;
					push_out_line(IR_node);
					lexer_advance();
					continue;
				}
				while (IR_stack_len > 0) {
					Token top = IR_stack[IR_stack_len - 1];
					if (top.kind == TOK_LPAREN) break;
					char *top_op_str = arena_alloc(&comp->prog->ir_arena, top.len + 1);
					memcpy(top_op_str, top.start, top.len);
					top_op_str[top.len] = '\0';
					if (strcmp(top_op_str, "==") == 0) strcpy(top_op_str, "=");
					int top_prec = get_op_precedence(top_op_str);
					if (top_prec >= prec) {
						IrNode *IR_node = new_ir(IR_WORD);
						IR_node->u.word.name = top_op_str;
						IR_node->u.word.len = top.len;
						push_out_line(IR_node);
						IR_stack_len--;
					} else {
						break;
					}
				}
				PUSH_OP_STACK(*t);
				expect_operand = 1;
				lexer_advance();
			} else if (lexer_at_peek(TOK_LPAREN) &&
			           (memchr(t->start, '.', t->len) != NULL ||
			            prog_var_slot(prog, t->start, t->len) < 0)) {
				/* Function or statically-resolved structure method call. */
				char *dot = memchr(t->start, '.', t->len);
				if (dot && dot != t->start && dot + 1 < t->start + t->len) {
					size_t receiver_len = (size_t)(dot - t->start);
					size_t member_len = t->len - receiver_len - 1;
					int slot = prog_var_slot(prog, t->start, receiver_len);
					StructDef *owner = slot >= 0 ? prog->var_structs[slot] : NULL;
					extern StructDef *current_impl_owner;
					int is_self = receiver_len == 4 && memcmp(t->start, "self", 4) == 0;
					if (is_self) owner = current_impl_owner;
					if (!owner) {
						PUSH_OP_STACK(*t);
						lexer_advance();
						continue;
					}
					MethodDef *method = owner ? prog_find_method(prog, owner, dot + 1, member_len) : NULL;
					if (!method)
						mira_error(comp->src, comp->filename, t->line, t->col, 1,
							"unknown method '%.*s'", (int)t->len, t->start);
					extern int current_method_mut_self;
					if (method->mut_self &&
					    ((!is_self && !prog->var_mutable[slot]) ||
					     (is_self && !current_method_mut_self)))
						mira_error(comp->src, comp->filename, t->line, t->col, 1,
							"mutable method requires a mutable receiver");
					IrNode *receiver = is_self ? new_ir(IR_WORD) : new_ir(IR_VAR);
					if (is_self) { receiver->u.word.name = "self"; receiver->u.word.len = 4; }
					else receiver->u.var_slot = slot;
					push_out_line(receiver);
					if (!is_self) {
						IrNode *fetch = new_ir(IR_WORD); fetch->u.word.name = "@"; fetch->u.word.len = 1;
						push_out_line(fetch);
					}
					Token call = *t; call.start = method->qualified_name; call.len = method->qualified_name_len;
					PUSH_OP_STACK(call);
				} else PUSH_OP_STACK(*t);
				lexer_advance();
				} else {
					/* Normal variable or postfix word */
					IrNode *node = parse_one(prog, true);
					if (node) {
						push_out_line(node);
						/* 同字面量分支: 链尾为运算符(后缀词)时不置 postfix_seen,
						 * 后续 infix 运算符(右操作数未读)走正常 Shunting-Yard 压栈 */
						IrNode *tail = node;
						while (tail->next) tail = tail->next;
						if (expect_operand == 0 && tail->kind != IR_WORD) postfix_seen = 1;
						expect_operand = 0;
						saw_operand = 1;
					}
				}
		} else {
			break; /* Unknown token 鈥? end of expression */
		}
	}
	
flush_and_return:; // Label for goto
	while (IR_stack_len > 0) {
		Token top = IR_stack[--IR_stack_len];
		if (top.kind == TOK_LPAREN) {
			mira_error(comp->src, comp->filename, top.line, 0, 1, "Mismatched parentheses in infix line");
			return NULL; // Or handle error appropriately
		}
		char *top_op_str = arena_alloc(&comp->prog->ir_arena, top.len + 1);
		memcpy(top_op_str, top.start, top.len);
		top_op_str[top.len] = '\0';
		if (strcmp(top_op_str, "==") == 0) strcpy(top_op_str, "=");
		IrNode *IR_node = new_ir(IR_WORD);
		IR_node->u.word.name = top_op_str;
		IR_node->u.word.len = top.len;
		push_out_line(IR_node);
	}

	return out_queue_head;
}

