/* parser/infix.c a€???-??€ Shunting-Yard è§￡??? */
#include "parser.h"
static IrNode *modern_negative_literal(const Token *t) {
	if (!t || t->kind != TOK_ID || t->len < 2 || t->start[0] != '-') return NULL;
	int dot = 0;
	for (size_t i = 1; i < t->len; ++i) {
		if (t->start[i] == '.' && !dot) { dot = 1; continue; }
		if (!isdigit((unsigned char)t->start[i])) return NULL;
	}
	if (dot) {
		char buf[96]; size_t n = t->len < sizeof(buf) - 1 ? t->len : sizeof(buf) - 1;
		memcpy(buf, t->start, n); buf[n] = '\0';
		IrNode *o = new_ir(IR_FLOAT); o->u.d = strtod(buf, NULL); return o;
	}
	int64_t value = 0;
	for (size_t i = 1; i < t->len; ++i) value = value * 10 + (t->start[i] - '0');
	IrNode *o = new_ir(IR_INT); o->u.i = -value; return o;
}

static IrNode *parse_eval_block(Program *prog) {
	lexer_expect(TOK_LPAREN);
	int paren_depth = 1;

	IrNode *out_queue_head = NULL;
	IrNode *out_queue_tail = NULL;
	
	// Operator stack for shunting yard
	#define MAX_OP_STACK 128
	Token IR_stack[MAX_OP_STACK];
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
			IR_stack[IR_stack_len++] = *lexer_cur();
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
				IR_stack[IR_stack_len++] = *t;
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
						IR_stack[IR_stack_len++] = *t;
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
					IR_stack[IR_stack_len++] = call;
				} else IR_stack[IR_stack_len++] = *t;
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
	#define MAX_OP_STACK_LINE 128
	Token IR_stack[MAX_OP_STACK_LINE];
	int IR_stack_len = 0;
	int paren_depth = 0;

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

		if (paren_depth == 0 && (lexer_at(TOK_NEWLINE) || lexer_at(TOK_EOF) ||
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
			if (node) push_out_line(node);
		} else if (lexer_at(TOK_LPAREN)) {
			paren_depth++;
			IR_stack[IR_stack_len++] = *lexer_cur();
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
			lexer_advance();
		} else if (lexer_at(TOK_ID)) {
			Token *t = lexer_cur();
			IrNode *joined_negative = modern_negative_literal(t);
			if (joined_negative) {
				lexer_advance(); push_out_line(joined_negative); continue;
			}
			char *IR_str = arena_alloc(&comp->prog->ir_arena, t->len + 1);
			memcpy(IR_str, t->start, t->len);
			IR_str[t->len] = '\0';

			int prec = get_op_precedence(IR_str);

			if (t->len == 1 && t->start[0] == '=') {
				/* Statement assignment is handled by parse_block_content().
				 * Leaving '=' here would incorrectly turn it into equality. */
				break;
			} else if (prec > 0) {
				/* It's an infix operator (+, -, ==, !=, <, >, etc.) */
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
				IR_stack[IR_stack_len++] = *t;
				lexer_advance();
			} else if (lexer_at_peek(TOK_LPAREN)) {
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
						IR_stack[IR_stack_len++] = *t;
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
					IR_stack[IR_stack_len++] = call;
				} else IR_stack[IR_stack_len++] = *t;
				lexer_advance();
			} else {
				/* Normal variable or postfix word */
				IrNode *node = parse_one(prog, true);
				if (node) push_out_line(node);
			}
		} else {
			break; /* Unknown token — end of expression */
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

