/* Mira lexer 鈥?# 娉ㄩ噴  ! 缂栬瘧鎸囦护  : 瀹氫箟  { } 鍧? 鍚庣疆璇?*/
#include "mira.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct LexerState LexerState;

static Compiler *comp;

static void next_char(void) { if (*comp->p) comp->p++; }

static int is_ident_char(char c) {
	return isalnum((unsigned char)c) || c == '_' || c == '-' || c == '?' || c == '=' || c == '.';
}
const char *intern_string(const char *str, size_t len);

/* Token 莽卤禄氓啪鈥姑ヂ?芒鈥?氓聫炉猫炉禄氓颅鈥斆γぢ?*/
const char *token_kind_name(TokenKind k) {
	switch (k) {
	case TOK_EOF:      return "end of file";
	case TOK_NEWLINE:  return "newline";
	case TOK_INT:      return "integer";
	case TOK_FLOAT:    return "float";
	case TOK_STR:      return "string";
	case TOK_ID:       return "identifier";
	case TOK_PRAGMA:   return "pragma";
	case TOK_COLON:    return "':'";
	case TOK_LBRACE:   return "'{'";
	case TOK_RBRACE:   return "'}'";
	case TOK_LBRACKET: return "'['";
	case TOK_RBRACKET: return "']'";
	case TOK_LPAREN:   return "'('";
	case TOK_RPAREN:   return "')'";
	case TOK_COMMA:    return "','";
	case TOK_DOTDOT:   return "'..'";
	}
	return "unknown";
}

void read_token(Token *t) {
	const char *s = comp->p;
	while (*s == ' ' || *s == '\t' || *s == '\r') s++;
	comp->p = (char *)s;
	if (*s == '\n') {
		t->kind = TOK_NEWLINE;
		t->len = 1;
		t->line = comp->cur_line;
		t->col  = (int)(s - comp->line_start) + 1;
		comp->cur_line++;
		comp->p = (char *)s + 1;
		comp->line_start = comp->p;
		return;
	}
	if (*s == '#') {
		while (*s && *s != '\n') s++;
		comp->p = (char *)s;
		read_token(t);
		return;
	}
	t->start = (char *)s;
	t->len = 0;
	t->str = NULL;
	t->str_len = 0;
	t->line = comp->cur_line;
	t->col  = (int)(s - comp->line_start) + 1;
	if (!*s) { t->kind = TOK_EOF; comp->p = (char *)s; return; }

	/* 莽录鈥撁€樏ε掆€∶ぢ宦?!target !stack 莽颅鈥懊尖€?= 忙藴炉盲赂聧莽颅鈥懊β€澝酒捗尖€好ヂ嶁€⒚р€孤?! 忙藴?氓鈥犫劉氓鈥犫€γヂ?猫炉?*/
	if (s[0] == '.' && s[1] == '.') {
		t->kind = TOK_DOTDOT; t->len = 2; next_char(); next_char(); return;
	}
	if (*s == '!') {
		if (s[1] == '=') {
			/* != 盲赂聧莽颅鈥懊β€澝酒?芒鈥?忙鈥⒙疵ぢ解€溍ぢ脚撁ぢ嘎?TOK_ID */
			t->kind = TOK_ID;
			t->len = 2;
			next_char(); next_char();
			return;
		}
		if (isalpha((unsigned char)s[1]) || isdigit((unsigned char)s[1]) || s[1] == '-' || s[1] == '_') {
			next_char();
			s = comp->p;
			while (is_ident_char(*comp->p)) next_char();
			t->kind = TOK_PRAGMA;
			t->len = (size_t)(comp->p - s);
			t->start = (char *)s;
			return;
		}
		t->kind = TOK_ID;
		t->len = 1;
		next_char();
		return;
	}

	/* 忙鈥⒙懊ヂ€斆妓喢︹€⒙疵︹€⒙懊λ嗏€撁ヂ奥徝︹€⒙懊寂捗︹€澛ε捖伱р€樏ヂγ∶︹€⒙懊β斥€?1e-3茂录?*/
	if (isdigit((unsigned char)*s)) {
		t->val = 0;
		while (isdigit((unsigned char)*comp->p)) {
			t->val = t->val * 10 + (*comp->p - '0');
			next_char();
		}
		if (*comp->p == '.' && isdigit((unsigned char)comp->p[1])) {
			next_char();
			while (isdigit((unsigned char)*comp->p)) next_char();
			if ((*comp->p == 'e' || *comp->p == 'E') &&
			    (isdigit((unsigned char)comp->p[1]) || ((comp->p[1] == '+' || comp->p[1] == '-') && isdigit((unsigned char)comp->p[2])))) {
				next_char();
				if (*comp->p == '+' || *comp->p == '-') next_char();
				while (isdigit((unsigned char)*comp->p)) next_char();
			}
			char buf[64];
			size_t n = (size_t)(comp->p - s);
			if (n >= sizeof buf) n = sizeof buf - 1;
			memcpy(buf, s, n);
			buf[n] = '\0';
			t->dbl = strtod(buf, NULL);
			t->kind = TOK_FLOAT;
		} else {
			t->kind = TOK_INT;
		}
		t->len = (size_t)(comp->p - t->start);
		return;
	}

	/* 氓颅鈥斆γぢ?"..." */
	if (*s == '"') {
		next_char();
		s = comp->p;
		t->kind = TOK_STR;
		while (*comp->p && *comp->p != '"') {
			if (*comp->p == '\n') { comp->cur_line++; comp->line_start = comp->p + 1; }
			if (*comp->p == '\\') next_char();
			next_char();
		}
		size_t len = (size_t)(comp->p - s);
		t->str = (char *)intern_string(s, len);
		t->str_len = len;
		if (*comp->p == '"') next_char();
		return;
	}

	/* : { } [ ] */
	if (*s == ':') { t->kind = TOK_COLON; t->len = 1; next_char(); return; }
	if (*s == '{') { t->kind = TOK_LBRACE; t->len = 1; next_char(); return; }
	if (*s == '}') { t->kind = TOK_RBRACE; t->len = 1; next_char(); return; }
	if (*s == '[') { t->kind = TOK_LBRACKET; t->len = 1; next_char(); return; }
	if (*s == ']') { t->kind = TOK_RBRACKET; t->len = 1; next_char(); return; }
	if (*s == '(') { t->kind = TOK_LPAREN; t->len = 1; next_char(); return; }
	if (*s == ')') { t->kind = TOK_RPAREN; t->len = 1; next_char(); return; }
	if (*s == ',') { t->kind = TOK_COMMA;  t->len = 1; next_char(); return; }
	if (*s == ';') { t->kind = TOK_ID; t->len = 1; next_char(); return; }

	/* 忙聽鈥∶€犆?猫炉?*/
	if (is_ident_char(*s) || *s == '+' || *s == '-' || *s == '*' || *s == '/' || *s == '%' ||
	    *s == '@' || *s == '<' || *s == '>' || *s == '=' || *s == '!' ||
	    *s == '&' || *s == '|' || *s == '^') {
		t->kind = TOK_ID;
		next_char();
		while (is_ident_char(*comp->p) || *comp->p == '+' || *comp->p == '-' || *comp->p == '*' ||
		       *comp->p == '/' || *comp->p == '%' || *comp->p == '@' || *comp->p == '<' ||
		       *comp->p == '>' || *comp->p == '=' || *comp->p == '!' || *comp->p == '&' ||
		       *comp->p == '|' || *comp->p == '^')
			next_char();
		t->len = (size_t)(comp->p - t->start);
		return;
	}

	t->kind = TOK_EOF;
}

void lexer_init(Compiler *c) {
	comp = c;
	comp->p = comp->src;
	comp->cur_line = 1;
	comp->cur_col = 1;
	comp->line_start = comp->src;
	read_token(&comp->cur);
}

void lexer_advance(void) {
	comp->has_peek = false;
	read_token(&comp->cur);
	/* 氓陆鈥溍モ€奥嵜︹€撯€∶ぢ宦睹ニ喡懊韭?EOF茂录拧猫鈥÷ヅ犅ヂ悸姑モ€号久ぢ概犆ヂ扁€毭︹€撯€∶ぢ宦睹β德?*/
	while (comp->cur.kind == TOK_EOF && comp->lex_state) {
		lexer_pop_file();
	}
}


bool lexer_at(TokenKind k) { return comp->cur.kind == k; }
bool lexer_at_peek(TokenKind k) {
	if (!comp->has_peek) {
		char *saved_p = comp->p;
		int saved_line = comp->cur_line;
		read_token(&comp->peek);
		comp->p = saved_p;
		comp->cur_line = saved_line;
		comp->has_peek = true;
	}
	return comp->peek.kind == k;
}
bool lexer_eat(TokenKind k) {
	if (comp->cur.kind != k) return false;
	lexer_advance();
	return true;
}

void lexer_expect(TokenKind k) {
	if (comp->cur.kind != k) {
		if (comp->cur.kind == TOK_ID || comp->cur.kind == TOK_PRAGMA) {
			mira_error(comp->src, comp->filename, comp->cur.line, comp->cur.col, 1,
				"expected %s, got %s '%.*s'",
				token_kind_name(k), token_kind_name(comp->cur.kind),
				(int)comp->cur.len, comp->cur.start);
		} else {
			mira_error(comp->src, comp->filename, comp->cur.line, comp->cur.col, 1,
				"expected %s, got %s",
				token_kind_name(k), token_kind_name(comp->cur.kind));
		}
	}
	lexer_advance();
}
Token *lexer_cur(void) { return &comp->cur; }

/* 闆舵嫹璐?Lexer 娴佸紡妯″潡鏍?*/
bool lexer_push_file(const char *path, const char *alias) {
	/* 璇诲彇鐩爣鏂囦欢 */
	FILE *f = fopen(path, "rb");
	if (!f) return false;
	fseek(f, 0, SEEK_END);
	long fsz = ftell(f);
	fseek(f, 0, SEEK_SET);
	char *buf = (char *)malloc((size_t)fsz + 1);
	if (!buf) { fclose(f); return false; }
	fread(buf, 1, (size_t)fsz, f);
	buf[fsz] = '\0';
	fclose(f);

	/* 淇濆瓨褰撳墠 Lexer 鐘舵€佸埌鏍?*/
	LexerState *state = (LexerState *)malloc(sizeof(LexerState));
	if (!state) { free(buf); return false; }
	state->src = comp->src;
	state->p = comp->p;
	state->filename = (char *)comp->filename;
	state->cur_line = comp->cur_line;
	if (alias) {
		strncpy(state->alias_prefix, alias, sizeof(state->alias_prefix) - 1);
		state->alias_prefix[sizeof(state->alias_prefix) - 1] = '\0';
	} else {
		state->alias_prefix[0] = '\0';
	}
	state->prev = comp->lex_state;
	comp->lex_state = state;

	/* 鍒囨崲鍒版柊鏂囦欢 */
	comp->src = buf;
	comp->p = buf;
	comp->filename = path;
	comp->cur_line = 1;
	comp->has_peek = false;
	read_token(&comp->cur);
	return true;
}

/* import 的子文件缓冲区可能仍被 Def/IR 指针引用（token 的 start 直接指向
 * src 内部），不能在 pop 时立即 free，否则 word 定义名、参数、字符串等在
 * codegen 阶段变成悬垂指针。延迟到编译结束统一回收（编译器为短生命周期进程）。 */
static char **deferred_frees = NULL;
static size_t deferred_count = 0, deferred_cap = 0;

static void defer_free_src(char *p) {
	if (!p) return;
	if (deferred_count >= deferred_cap) {
		deferred_cap = deferred_cap ? deferred_cap * 2 : 32;
		deferred_frees = (char **)realloc(deferred_frees, deferred_cap * sizeof(char *));
	}
	deferred_frees[deferred_count++] = p;
}

void lexer_pop_file(void) {
	if (!comp->lex_state) return;
	LexerState *state = comp->lex_state;

	/* 延迟释放当前文件的缓冲，而不是立即 free */
	defer_free_src(comp->src);

	/* 鎭㈠涓婁竴灞?Lexer 鐘舵€?*/
	comp->src = state->src;
	comp->p = state->p;
	comp->filename = state->filename;
	comp->cur_line = state->cur_line;
	comp->has_peek = false;
	comp->lex_state = state->prev;
	free(state);

	/* 缁х画璇诲彇涓婂眰鏂囦欢鐨勪笅涓€涓?token */
	read_token(&comp->cur);
}
