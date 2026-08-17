/* interpreter.c �?终极无敌雷霆爆炸神圣牛福解释�? * 完全独立�?Mira 解释器，不依赖编译器�?parser/codegen/SSA/linker�? * 自带迷你词法分析器，直接执行 Token 流�? * 支持后缀 (1 2 +) 和中缀 (1 + 2 * 3) 双模式�? * 用法: mira -i [file.mira]   (无文件则进入 REPL)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <math.h>
#include <stdarg.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* ========== 颜色输出 ========== */
#ifdef _WIN32
static void itp_color_red(void)   { SetConsoleTextAttribute(GetStdHandle(STD_ERROR_HANDLE), FOREGROUND_RED|FOREGROUND_INTENSITY); }
static void itp_color_green(void) { SetConsoleTextAttribute(GetStdHandle(STD_ERROR_HANDLE), FOREGROUND_GREEN|FOREGROUND_INTENSITY); }
static void itp_color_cyan(void)  { SetConsoleTextAttribute(GetStdHandle(STD_ERROR_HANDLE), FOREGROUND_GREEN|FOREGROUND_BLUE|FOREGROUND_INTENSITY); }
static void itp_color_white(void) { SetConsoleTextAttribute(GetStdHandle(STD_ERROR_HANDLE), FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE|FOREGROUND_INTENSITY); }
static void itp_color_reset(void) { SetConsoleTextAttribute(GetStdHandle(STD_ERROR_HANDLE), 7); }
#else
static void itp_color_red(void)   { fprintf(stderr, "\033[1;31m"); }
static void itp_color_green(void) { fprintf(stderr, "\033[1;32m"); }
static void itp_color_cyan(void)  { fprintf(stderr, "\033[1;36m"); }
static void itp_color_white(void) { fprintf(stderr, "\033[1;37m"); }
static void itp_color_reset(void) { fprintf(stderr, "\033[0m"); }
#endif

/* ========== 值类�?========== */
typedef enum { V_INT, V_FLOAT, V_STR, V_PTR, V_LAMBDA } ValType;

typedef struct {
    ValType type;
    union {
        int64_t  i;
        double   f;
        char    *s;  /* 字符�?(strdup 的堆内存) */
        void    *p;
        struct { char *body; char **params; int param_count; } lam;
    } u;
} Val;

/* ========== 迷你词法分析�?========== */
typedef enum {
    TK_EOF, TK_INT, TK_FLOAT, TK_STR, TK_ID,
    TK_COLON, TK_SEMI,
    TK_LBRACE, TK_RBRACE,
    TK_LPAREN, TK_RPAREN,
    TK_LBRACKET, TK_RBRACKET,
    TK_COMMA, TK_NEWLINE,
    TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH, TK_PERCENT,
    TK_LT, TK_GT, TK_LE, TK_GE, TK_EQ, TK_NEQ,
    TK_BANG, TK_AT, TK_HASH
} TkKind;

typedef struct {
    TkKind kind;
    int64_t ival;
    double  fval;
    char   *sval;   /* 堆分配的字符串副�?*/
    char    id[256]; /* 标识符名 */
    int     line, col;
} Tk;

typedef struct {
    const char *src;   /* 源代码全�?*/
    const char *p;     /* 当前读取位置 */
    const char *line_start;
    int cur_line;
    Tk  cur;           /* 当前 Token */
} Lexer;

static int is_id_char(char c) {
    return isalnum((unsigned char)c) || c=='_' || c=='-' || c=='?' || c=='.';
}

static void lex_skip_ws(Lexer *L) {
    while (*L->p == ' ' || *L->p == '\t' || *L->p == '\r') L->p++;
}

static void lex_next(Lexer *L) {
    lex_skip_ws(L);
    Tk *t = &L->cur;
    memset(t, 0, sizeof(Tk));
    t->line = L->cur_line;
    t->col  = (int)(L->p - L->line_start) + 1;

    char c = *L->p;
    if (!c) { t->kind = TK_EOF; return; }

    /* 换行 */
    if (c == '\n') {
        t->kind = TK_NEWLINE;
        L->p++;
        L->cur_line++;
        L->line_start = L->p;
        return;
    }
    /* 注释 # */
    if (c == '#') {
        while (*L->p && *L->p != '\n') L->p++;
        lex_next(L);
        return;
    }
    /* 数字 */
    if (isdigit((unsigned char)c)) {
        const char *start = L->p;
        int64_t v = 0;
        while (isdigit((unsigned char)*L->p)) {
            v = v * 10 + (*L->p - '0');
            L->p++;
        }
        if (*L->p == '.' && isdigit((unsigned char)L->p[1])) {
            L->p++;
            while (isdigit((unsigned char)*L->p)) L->p++;
            /* 科学计数�?*/
            if ((*L->p == 'e' || *L->p == 'E') &&
                (isdigit((unsigned char)L->p[1]) || ((L->p[1]=='+' || L->p[1]=='-') && isdigit((unsigned char)L->p[2])))) {
                L->p++;
                if (*L->p == '+' || *L->p == '-') L->p++;
                while (isdigit((unsigned char)*L->p)) L->p++;
            }
            char buf[64];
            size_t n = (size_t)(L->p - start);
            if (n >= sizeof buf) n = sizeof buf - 1;
            memcpy(buf, start, n); buf[n] = '\0';
            t->kind = TK_FLOAT;
            t->fval = strtod(buf, NULL);
        } else {
            t->kind = TK_INT;
            t->ival = v;
        }
        return;
    }
    /* 字符�?*/
    if (c == '"') {
        L->p++;
        const char *start = L->p;
        /* 先计算长度（处理转义�?*/
        size_t cap = 256;
        char *buf = malloc(cap);
        size_t len = 0;
        while (*L->p && *L->p != '"') {
            char ch = *L->p;
            if (ch == '\\' && L->p[1]) {
                L->p++;
                switch (*L->p) {
                    case 'n': ch = '\n'; break;
                    case 't': ch = '\t'; break;
                    case 'r': ch = '\r'; break;
                    case '\\': ch = '\\'; break;
                    case '"': ch = '"'; break;
                    case '0': ch = '\0'; break;
                    default: ch = *L->p; break;
                }
            }
            if (len + 1 >= cap) { cap *= 2; buf = realloc(buf, cap); }
            buf[len++] = ch;
            L->p++;
        }
        buf[len] = '\0';
        if (*L->p == '"') L->p++;
        t->kind = TK_STR;
        t->sval = buf;
        return;
    }
    /* 单字�?Token */
    switch (c) {
        case ':': t->kind = TK_COLON;    L->p++; return;
        case ';': t->kind = TK_SEMI;     L->p++; return;
        case '{': t->kind = TK_LBRACE;   L->p++; return;
        case '}': t->kind = TK_RBRACE;   L->p++; return;
        case '(': t->kind = TK_LPAREN;   L->p++; return;
        case ')': t->kind = TK_RPAREN;   L->p++; return;
        case '[': t->kind = TK_LBRACKET; L->p++; return;
        case ']': t->kind = TK_RBRACKET; L->p++; return;
        case ',': t->kind = TK_COMMA;    L->p++; return;
    }
    /* 多字符运算符和标识符 */
    if (c == '<' && L->p[1] == '=') { t->kind = TK_LE; L->p += 2; strcpy(t->id, "<="); return; }
    if (c == '>' && L->p[1] == '=') { t->kind = TK_GE; L->p += 2; strcpy(t->id, ">="); return; }
    if (c == '=' && L->p[1] == '=') { t->kind = TK_EQ; L->p += 2; strcpy(t->id, "=="); return; }
    if (c == '!' && L->p[1] == '=') { t->kind = TK_NEQ; L->p += 2; strcpy(t->id, "!="); return; }

    /* 单字符运算符（也作为 TOK_ID�?*/
    if (c == '+')  { t->kind = TK_PLUS;    L->p++; strcpy(t->id, "+"); return; }
    if (c == '-')  { t->kind = TK_MINUS;   L->p++; strcpy(t->id, "-"); return; }
    if (c == '*')  { t->kind = TK_STAR;    L->p++; strcpy(t->id, "*"); return; }
    if (c == '/')  { t->kind = TK_SLASH;   L->p++; strcpy(t->id, "/"); return; }
    if (c == '%')  { t->kind = TK_PERCENT; L->p++; strcpy(t->id, "%"); return; }
    if (c == '<')  { t->kind = TK_LT;      L->p++; strcpy(t->id, "<"); return; }
    if (c == '>')  { t->kind = TK_GT;      L->p++; strcpy(t->id, ">"); return; }
    if (c == '=')  { t->kind = TK_EQ;      L->p++; strcpy(t->id, "="); return; }
    if (c == '!')  { t->kind = TK_BANG;    L->p++; strcpy(t->id, "!"); return; }
    if (c == '@')  { t->kind = TK_AT;      L->p++; strcpy(t->id, "@"); return; }

    /* 普通标识符 */
    if (is_id_char(c)) {
        t->kind = TK_ID;
        int i = 0;
        while (is_id_char(*L->p) && i < 255) {
            t->id[i++] = *L->p;
            L->p++;
        }
        t->id[i] = '\0';
        return;
    }

    /* 跳过不认识的字符 */
    L->p++;
    lex_next(L);
}

static void lex_init(Lexer *L, const char *src) {
    L->src = src;
    L->p = src;
    L->line_start = src;
    L->cur_line = 1;
    lex_next(L);
}

/* ========== 解释器状�?========== */
#define MAX_STACK  4096
#define MAX_VARS   1024
#define MAX_FUNCS  512
#define MAX_DEPTH  256

typedef struct {
    char name[128];
    Val  val;
} Var;

typedef struct {
    char  name[128];
    char *body;  /* 函数体源码副�?*/
    char **params;
    int   param_count;
} Func;

typedef struct LoopCtx {
    const char *loop_start;  /* 循环体起始位�?*/
    int break_flag;
    int continue_flag;
} LoopCtx;

typedef struct Interp {
    /* 数据�?*/
    Val  stack[MAX_STACK];
    int  sp;
    /* 变量 */
    Var  vars[MAX_VARS];
    int  var_count;
    /* 函数 */
    Func funcs[MAX_FUNCS];
    int  func_count;
    /* 循环上下文栈 */
    LoopCtx loops[MAX_DEPTH];
    int  loop_depth;
    /* 错误处理 */
    int  had_error;
    int  is_repl;
    /* 中缀模式标志 (!syntax infix) */
    int  infix_mode;
    /* 当前源文件名 */
    const char *filename;
} Interp;

static Interp *G;  /* 全局解释器实�?*/

/* ========== 值栈操作 ========== */
static void vpush(Interp *I, Val v) {
    if (I->sp >= MAX_STACK) {
        fprintf(stderr, "error: stack overflow\n");
        I->had_error = 1;
        return;
    }
    I->stack[I->sp++] = v;
}

static Val vpop(Interp *I) {
    if (I->sp <= 0) {
        fprintf(stderr, "error: stack underflow\n");
        I->had_error = 1;
        Val v; v.type = V_INT; v.u.i = 0; return v;
    }
    return I->stack[--I->sp];
}

static Val vpeek(Interp *I) {
    if (I->sp <= 0) {
        Val v; v.type = V_INT; v.u.i = 0; return v;
    }
    return I->stack[I->sp - 1];
}

static Val make_int(int64_t i) { Val v; v.type = V_INT; v.u.i = i; return v; }
static Val make_float(double f) { Val v; v.type = V_FLOAT; v.u.f = f; return v; }
static Val make_str(const char *s) { Val v; v.type = V_STR; v.u.s = strdup(s); return v; }

/* 值转 double (隐式提升) */
static double val_as_float(Val v) {
    if (v.type == V_FLOAT) return v.u.f;
    return (double)v.u.i;
}

static int val_truthy(Val v) {
    switch (v.type) {
        case V_INT:   return v.u.i != 0;
        case V_FLOAT: return v.u.f != 0.0;
        case V_STR:   return v.u.s && v.u.s[0];
        default:      return 0;
    }
}

/* ========== 变量系统 ========== */
static Var *find_var(Interp *I, const char *name) {
    for (int i = 0; i < I->var_count; i++) {
        if (strcmp(I->vars[i].name, name) == 0) return &I->vars[i];
    }
    return NULL;
}

static Var *create_var(Interp *I, const char *name, Val val) {
    Var *v = find_var(I, name);
    if (v) { v->val = val; return v; }
    if (I->var_count >= MAX_VARS) {
        fprintf(stderr, "error: too many variables\n");
        I->had_error = 1;
        return NULL;
    }
    v = &I->vars[I->var_count++];
    strncpy(v->name, name, 127); v->name[127] = '\0';
    v->val = val;
    return v;
}

/* ========== 函数系统 ========== */
static Func *find_func(Interp *I, const char *name) {
    for (int i = 0; i < I->func_count; i++) {
        if (strcmp(I->funcs[i].name, name) == 0) return &I->funcs[i];
    }
    return NULL;
}

/* ========== 前向声明 ========== */
static void exec_block(Interp *I, Lexer *L);
static void exec_token_stream(Interp *I, Lexer *L);
static Val  eval_infix_expr(Interp *I, Lexer *L);

/* ========== 跳过大括号块 ========== */
static void skip_block(Lexer *L) {
    int depth = 1;
    while (depth > 0 && L->cur.kind != TK_EOF) {
        lex_next(L);
        if (L->cur.kind == TK_LBRACE) depth++;
        else if (L->cur.kind == TK_RBRACE) depth--;
    }
    if (L->cur.kind == TK_RBRACE) lex_next(L); /* 跳过 } */
}

/* ========== 中缀表达式求�?(Shunting-yard) ========== */

static int IR_precedence(TkKind k) {
    switch (k) {
        case TK_PLUS: case TK_MINUS: return 1;
        case TK_STAR: case TK_SLASH: case TK_PERCENT: return 2;
        case TK_LT: case TK_GT: case TK_LE: case TK_GE: return 0;
        case TK_EQ: case TK_NEQ: return 0;
        default: return -1;
    }
}

static int is_op_token(TkKind k) {
    return IR_precedence(k) >= 0;
}

static Val apply_op(TkKind IrNode, Val a, Val b) {
    /* 如果任意一个是浮点则提�?*/
    if (a.type == V_FLOAT || b.type == V_FLOAT) {
        double fa = val_as_float(a), fb = val_as_float(b);
        switch (IrNode) {
            case TK_PLUS:    return make_float(fa + fb);
            case TK_MINUS:   return make_float(fa - fb);
            case TK_STAR:    return make_float(fa * fb);
            case TK_SLASH:   return fb != 0.0 ? make_float(fa / fb) : make_float(0.0);
            case TK_PERCENT: return make_float(fmod(fa, fb));
            case TK_LT:      return make_int(fa < fb);
            case TK_GT:      return make_int(fa > fb);
            case TK_LE:      return make_int(fa <= fb);
            case TK_GE:      return make_int(fa >= fb);
            case TK_EQ:      return make_int(fa == fb);
            case TK_NEQ:     return make_int(fa != fb);
            default: return make_int(0);
        }
    }
    int64_t ia = a.u.i, ib = b.u.i;
    switch (IrNode) {
        case TK_PLUS:    return make_int(ia + ib);
        case TK_MINUS:   return make_int(ia - ib);
        case TK_STAR:    return make_int(ia * ib);
        case TK_SLASH:   return ib != 0 ? make_int(ia / ib) : make_int(0);
        case TK_PERCENT: return ib != 0 ? make_int(ia % ib) : make_int(0);
        case TK_LT:      return make_int(ia < ib);
        case TK_GT:      return make_int(ia > ib);
        case TK_LE:      return make_int(ia <= ib);
        case TK_GE:      return make_int(ia >= ib);
        case TK_EQ:      return make_int(ia == ib);
        case TK_NEQ:     return make_int(ia != ib);
        default: return make_int(0);
    }
}

/* 解析中缀表达式原�?*/
static Val infix_atom(Interp *I, Lexer *L) {
    Tk *t = &L->cur;
    if (t->kind == TK_INT)   { Val v = make_int(t->ival);   lex_next(L); return v; }
    if (t->kind == TK_FLOAT) { Val v = make_float(t->fval); lex_next(L); return v; }
    if (t->kind == TK_STR)   { Val v = make_str(t->sval);   lex_next(L); return v; }
    if (t->kind == TK_MINUS) {
        /* 一元负�?*/
        lex_next(L);
        Val v = infix_atom(I, L);
        if (v.type == V_FLOAT) return make_float(-v.u.f);
        return make_int(-v.u.i);
    }
    if (t->kind == TK_LPAREN) {
        lex_next(L); /* 吃掉 ( */
        Val v = eval_infix_expr(I, L);
        if (L->cur.kind == TK_RPAREN) lex_next(L); /* 吃掉 ) */
        return v;
    }
    if (t->kind == TK_ID) {
        char name[256];
        strncpy(name, t->id, 255); name[255] = '\0';
        lex_next(L);

        /* 检查是否是函数调用 func(a, b) */
        if (L->cur.kind == TK_LPAREN) {
            lex_next(L); /* 吃掉 ( */
            /* 收集参数 */
            Val args[16]; int argc = 0;
            while (L->cur.kind != TK_RPAREN && L->cur.kind != TK_EOF) {
                args[argc++] = eval_infix_expr(I, L);
                if (L->cur.kind == TK_COMMA) lex_next(L);
            }
            if (L->cur.kind == TK_RPAREN) lex_next(L); /* 吃掉 ) */
            /* 查找函数并执�?*/
            Func *fn = find_func(I, name);
            if (fn) {
                /* 保存参数为局部变�?*/
                Var saved[16]; int saved_count = 0;
                for (int i = 0; i < fn->param_count && i < argc; i++) {
                    Var *existing = find_var(I, fn->params[i]);
                    if (existing) { saved[saved_count++] = *existing; }
                    create_var(I, fn->params[i], args[i]);
                }
                /* 执行函数�?*/
                Lexer fnL;
                lex_init(&fnL, fn->body);
                exec_token_stream(I, &fnL);
                /* 恢复参数 */
                for (int i = 0; i < saved_count; i++) {
                    Var *v = find_var(I, saved[i].name);
                    if (v) *v = saved[i];
                }
                if (I->sp > 0) return vpop(I);
                return make_int(0);
            }
            /* 内置函数 */
            if (strcmp(name, "print") == 0 && argc >= 1) {
                Val v = args[0];
                if (v.type == V_INT)   printf("%lld", (long long)v.u.i);
                else if (v.type == V_FLOAT) printf("%g", v.u.f);
                else if (v.type == V_STR)   printf("%s", v.u.s);
                return make_int(0);
            }
            if (strcmp(name, "abs") == 0 && argc >= 1) {
                if (args[0].type == V_FLOAT) return make_float(fabs(args[0].u.f));
                return make_int(args[0].u.i < 0 ? -args[0].u.i : args[0].u.i);
            }
            if (strcmp(name, "sqrt") == 0 && argc >= 1) {
                return make_float(sqrt(val_as_float(args[0])));
            }
            fprintf(stderr, "error: unknown function '%s'\n", name);
            I->had_error = 1;
            return make_int(0);
        }

        /* 查找变量 */
        if (strcmp(name, "true") == 0) return make_int(1);
        if (strcmp(name, "false") == 0) return make_int(0);
        Var *var = find_var(I, name);
        if (var) return var->val;
        fprintf(stderr, "error: undefined variable '%s'\n", name);
        I->had_error = 1;
        return make_int(0);
    }
    fprintf(stderr, "error: unexpected token in expression (line %d)\n", t->line);
    I->had_error = 1;
    lex_next(L);
    return make_int(0);
}

/* 中缀表达式求值（优先级爬升法�?*/
static Val infix_expr_bp(Interp *I, Lexer *L, int min_bp) {
    Val lhs = infix_atom(I, L);
    for (;;) {
        int prec = IR_precedence(L->cur.kind);
        if (prec < 0 || prec < min_bp) break;
        TkKind IrNode = L->cur.kind;
        lex_next(L); /* 吃掉运算�?*/
        Val rhs = infix_expr_bp(I, L, prec + 1);
        lhs = apply_op(IrNode, lhs, rhs);
    }
    return lhs;
}

static Val eval_infix_expr(Interp *I, Lexer *L) {
    return infix_expr_bp(I, L, 0);
}

/* ========== 打印�?========== */
static void print_val(Val v) {
    switch (v.type) {
        case V_INT:   printf("%lld", (long long)v.u.i); break;
        case V_FLOAT: printf("%g", v.u.f); break;
        case V_STR:   printf("%s", v.u.s); break;
        case V_LAMBDA: printf("<lambda>"); break;
        default:      printf("<??>"); break;
    }
}

/* ========== 执行 Token �?========== */

/* 读取一个大括号块的源码文本（含嵌套大括号） */
static char *capture_block(Lexer *L) {
    /* L->cur 应该已经�?{ 之后的位�?*/
    const char *start = L->p;
    int depth = 1;
    while (depth > 0 && *L->p) {
        if (*L->p == '{') depth++;
        else if (*L->p == '}') { depth--; if (depth == 0) break; }
        else if (*L->p == '"') { L->p++; while (*L->p && *L->p != '"') { if (*L->p == '\\') L->p++; L->p++; } }
        else if (*L->p == '#') { while (*L->p && *L->p != '\n') L->p++; if (*L->p) { continue; } }
        if (*L->p) L->p++;
    }
    size_t len = (size_t)(L->p - start);
    char *body = malloc(len + 1);
    memcpy(body, start, len);
    body[len] = '\0';
    if (*L->p == '}') L->p++;
    lex_next(L);
    return body;
}

static void exec_token_stream(Interp *I, Lexer *L) {
    while (L->cur.kind != TK_EOF && !I->had_error) {
        Tk *t = &L->cur;

        /* 检�?break/continue */
        if (I->loop_depth > 0) {
            LoopCtx *lc = &I->loops[I->loop_depth - 1];
            if (lc->break_flag || lc->continue_flag) return;
        }

        /* 跳过换行 */
        if (t->kind == TK_NEWLINE) { lex_next(L); continue; }

        /* === 字面量：直接压栈 === */
        if (t->kind == TK_INT) {
            vpush(I, make_int(t->ival));
            lex_next(L);

            /* 中缀检测：数字后跟运算符，且运算符后跟数字�?�?             * 例如 "3 + 4" 是中缀, �?"3 4 + print" 中的 + 不触发中缀�?*/
            if (is_op_token(L->cur.kind)) {
                /* 偷瞄运算符后面一�?Token：是数字还是 ( �?*/
                const char *saved_p = L->p;
                int saved_line = L->cur_line;
                Tk peek;
                memset(&peek, 0, sizeof(peek));
                /* 手动预读 */
                const char *pp = L->p;
                while (*pp == ' ' || *pp == '\t' || *pp == '\r') pp++;
                int rhs_is_value = (isdigit((unsigned char)*pp) || *pp == '(' || *pp == '"' || *pp == '-');
                if (rhs_is_value) {
                    Val lhs = vpop(I);
                    int prec = IR_precedence(L->cur.kind);
                    TkKind IrNode = L->cur.kind;
                    lex_next(L);
                    Val rhs = infix_expr_bp(I, L, prec + 1);
                    vpush(I, apply_op(IrNode, lhs, rhs));
                }
            }
            continue;
        }
        if (t->kind == TK_FLOAT) {
            vpush(I, make_float(t->fval));
            lex_next(L);
            if (is_op_token(L->cur.kind)) {
                const char *pp = L->p;
                while (*pp == ' ' || *pp == '\t' || *pp == '\r') pp++;
                int rhs_is_value = (isdigit((unsigned char)*pp) || *pp == '(' || *pp == '"' || *pp == '-');
                if (rhs_is_value) {
                    Val lhs = vpop(I);
                    TkKind IrNode = L->cur.kind;
                    int prec = IR_precedence(IrNode);
                    lex_next(L);
                    Val rhs = infix_expr_bp(I, L, prec + 1);
                    vpush(I, apply_op(IrNode, lhs, rhs));
                }
            }
            continue;
        }
        if (t->kind == TK_STR) {
            vpush(I, make_str(t->sval));
            lex_next(L);
            continue;
        }

        /* === 括号表达�?(中缀) === */
        if (t->kind == TK_LPAREN) {
            Val v = eval_infix_expr(I, L);
            vpush(I, v);
            continue;
        }

        /* === Lambda: [ params ] { body } === */
        if (t->kind == TK_LBRACKET) {
            lex_next(L); /* �?[ */
            char *lam_params[16]; int lam_pc = 0;
            while (L->cur.kind != TK_RBRACKET && L->cur.kind != TK_EOF) {
                if (L->cur.kind == TK_ID) {
                    lam_params[lam_pc++] = strdup(L->cur.id);
                }
                lex_next(L);
            }
            if (L->cur.kind == TK_RBRACKET) lex_next(L); /* �?] */
            while (L->cur.kind == TK_NEWLINE) lex_next(L);
            if (L->cur.kind == TK_LBRACE) {
                /* L->p 已经�?{ 后面了，capture_block �?L->p 开始读 */
                /* �?lex_next 还没调用，所�?L->p 其实指向 { 后面 */
                /* 不需要额�?lex_next，直接让 L->p 前进�?{ */
                L->p = L->p; /* L->p 已在 { 之后 (lexer 消费�?{) */
                /* 等等 �?L->cur is {, 这意味着 L->p 指向 { 之后的内�?*/
                char *body = capture_block(L);
                Val lv; lv.type = V_LAMBDA;
                lv.u.lam.body = body;
                lv.u.lam.params = malloc(sizeof(char*) * lam_pc);
                memcpy(lv.u.lam.params, lam_params, sizeof(char*) * lam_pc);
                lv.u.lam.param_count = lam_pc;
                vpush(I, lv);
            }
            continue;
        }

        /* === 大括号块压栈 (Forth风格: { code } 压入栈作为匿名块) === */
        if (t->kind == TK_LBRACE) {
            /* L->cur �?{, L->p 指向 { 之后的内�?*/
            char *body = capture_block(L);
            Val bv; bv.type = V_LAMBDA;
            bv.u.lam.body = body;
            bv.u.lam.params = NULL;
            bv.u.lam.param_count = 0;
            vpush(I, bv);
            continue;
        }

        /* === 后缀算术运算�?=== */
        if (t->kind == TK_PLUS)    { lex_next(L); Val b = vpop(I); Val a = vpop(I); vpush(I, apply_op(TK_PLUS, a, b)); continue; }
        if (t->kind == TK_MINUS)   { lex_next(L); Val b = vpop(I); Val a = vpop(I); vpush(I, apply_op(TK_MINUS, a, b)); continue; }
        if (t->kind == TK_STAR)    { lex_next(L); Val b = vpop(I); Val a = vpop(I); vpush(I, apply_op(TK_STAR, a, b)); continue; }
        if (t->kind == TK_SLASH)   { lex_next(L); Val b = vpop(I); Val a = vpop(I); vpush(I, apply_op(TK_SLASH, a, b)); continue; }
        if (t->kind == TK_PERCENT) { lex_next(L); Val b = vpop(I); Val a = vpop(I); vpush(I, apply_op(TK_PERCENT, a, b)); continue; }

        /* 比较运算�?(后缀) */
        if (t->kind == TK_LT)  { lex_next(L); Val b = vpop(I); Val a = vpop(I); vpush(I, apply_op(TK_LT, a, b)); continue; }
        if (t->kind == TK_GT)  { lex_next(L); Val b = vpop(I); Val a = vpop(I); vpush(I, apply_op(TK_GT, a, b)); continue; }
        if (t->kind == TK_LE)  { lex_next(L); Val b = vpop(I); Val a = vpop(I); vpush(I, apply_op(TK_LE, a, b)); continue; }
        if (t->kind == TK_GE)  { lex_next(L); Val b = vpop(I); Val a = vpop(I); vpush(I, apply_op(TK_GE, a, b)); continue; }
        if (t->kind == TK_EQ)  { lex_next(L); Val b = vpop(I); Val a = vpop(I); vpush(I, apply_op(TK_EQ, a, b)); continue; }
        if (t->kind == TK_NEQ) { lex_next(L); Val b = vpop(I); Val a = vpop(I); vpush(I, apply_op(TK_NEQ, a, b)); continue; }

        /* 内存操作: val varname ! */
        if (t->kind == TK_BANG) {
            lex_next(L); /* 吃掉 ! */
            /* !syntax infix 指令 */
            if (L->cur.kind == TK_ID && strcmp(L->cur.id, "syntax") == 0) {
                lex_next(L); /* �?syntax */
                if (L->cur.kind == TK_ID && strcmp(L->cur.id, "infix") == 0) {
                    I->infix_mode = 1;
                    lex_next(L);
                }
                continue;
            }
            continue;
        }
        if (t->kind == TK_AT)  { lex_next(L); continue; }

        /* === 标识�?=== */
        if (t->kind == TK_ID) {
            char word[256];
            strncpy(word, t->id, 255); word[255] = '\0';
            lex_next(L);

            /* == !syntax infix 指令 == */
            if (word[0] == '!' && strcmp(word, "!syntax") == 0) {
                if (L->cur.kind == TK_ID && strcmp(L->cur.id, "infix") == 0) {
                    I->infix_mode = 1;
                    lex_next(L);
                }
                continue;
            }

            /* == 中缀模式执行 == */
            if (I->infix_mode) {
                /* print(expr) / cr() 等函数调�?*/
                if (strcmp(word, "print") == 0 && L->cur.kind == TK_LPAREN) {
                    lex_next(L); /* �?( */
                    Val v = eval_infix_expr(I, L);
                    if (L->cur.kind == TK_RPAREN) lex_next(L);
                    print_val(v);
                    continue;
                }
                if ((strcmp(word, "cr") == 0 || strcmp(word, "nl") == 0) && L->cur.kind == TK_LPAREN) {
                    lex_next(L); /* �?( */
                    if (L->cur.kind == TK_RPAREN) lex_next(L);
                    printf("\n"); fflush(stdout);
                    continue;
                }
                /* if (cond) { ... } */
                if (strcmp(word, "if") == 0) goto handle_if;
                /* while (cond) { ... } */
                if (strcmp(word, "while") == 0) goto handle_while;
                /* var x */
                if (strcmp(word, "var") == 0) goto handle_var;
                if (strcmp(word, "const") == 0) goto handle_const;
                if (strcmp(word, "for") == 0) goto handle_for;
                if (strcmp(word, "break") == 0) { if (I->loop_depth > 0) I->loops[I->loop_depth-1].break_flag = 1; return; }
                if (strcmp(word, "continue") == 0) { if (I->loop_depth > 0) I->loops[I->loop_depth-1].continue_flag = 1; return; }
                if (strcmp(word, "return") == 0) return;

                /* x = expr (中缀赋�? */
                Var *iv = find_var(I, word);
                if (iv && L->cur.kind == TK_EQ) {
                    lex_next(L); /* �?= */
                    iv->val = eval_infix_expr(I, L);
                    continue;
                }
                if (iv) {
                    vpush(I, iv->val);
                    continue;
                }
                /* 查找用户函数 */
                Func *ifn = find_func(I, word);
                if (ifn) {
                    if (L->cur.kind == TK_LPAREN) {
                        lex_next(L);
                        if (L->cur.kind == TK_RPAREN) lex_next(L);
                    }
                    Lexer fnL; lex_init(&fnL, ifn->body);
                    exec_token_stream(I, &fnL);
                    continue;
                }
                fprintf(stderr, "error: unknown word '%s' (line %d)\n", word, t->line);
                I->had_error = 1;
                continue;
            }

            /* == 内置关键�?(后缀模式) == */

            /* print */
            if (strcmp(word, "print") == 0) {
                Val v = vpop(I);
                print_val(v);
                continue;
            }
            /* cr / nl */
            if (strcmp(word, "cr") == 0 || strcmp(word, "nl") == 0) {
                printf("\n"); fflush(stdout);
                continue;
            }
            /* true / false */
            if (strcmp(word, "true") == 0) { vpush(I, make_int(1)); continue; }
            if (strcmp(word, "false") == 0) { vpush(I, make_int(0)); continue; }

            /* 栈操�?*/
            if (strcmp(word, "dup") == 0) {
                Val v = vpeek(I); vpush(I, v); continue;
            }
            if (strcmp(word, "drop") == 0) { vpop(I); continue; }
            if (strcmp(word, "swap") == 0) {
                Val a = vpop(I), b = vpop(I);
                vpush(I, a); vpush(I, b);
                continue;
            }
            if (strcmp(word, "over") == 0) {
                if (I->sp >= 2) vpush(I, I->stack[I->sp - 2]);
                continue;
            }
            if (strcmp(word, "rot") == 0) {
                if (I->sp >= 3) {
                    Val c = vpop(I), b = vpop(I), a = vpop(I);
                    vpush(I, b); vpush(I, c); vpush(I, a);
                }
                continue;
            }
            if (strcmp(word, "nip") == 0) {
                Val top = vpop(I); vpop(I); vpush(I, top);
                continue;
            }

            /* 逻辑 */
            if (strcmp(word, "and") == 0) {
                Val b = vpop(I), a = vpop(I);
                vpush(I, make_int(val_truthy(a) && val_truthy(b)));
                continue;
            }
            if (strcmp(word, "or") == 0) {
                Val b = vpop(I), a = vpop(I);
                vpush(I, make_int(val_truthy(a) || val_truthy(b)));
                continue;
            }
            if (strcmp(word, "not") == 0) {
                Val a = vpop(I);
                vpush(I, make_int(!val_truthy(a)));
                continue;
            }

            /* 类型转换 */
            if (strcmp(word, "to-float") == 0 || strcmp(word, "i2f") == 0) {
                Val a = vpop(I);
                vpush(I, make_float(val_as_float(a)));
                continue;
            }
            if (strcmp(word, "f2i") == 0 || strcmp(word, "to-int") == 0) {
                Val a = vpop(I);
                vpush(I, make_int((int64_t)val_as_float(a)));
                continue;
            }
            if (strcmp(word, "to-str") == 0) {
                Val a = vpop(I);
                char buf[64];
                if (a.type == V_INT) snprintf(buf, 64, "%lld", (long long)a.u.i);
                else if (a.type == V_FLOAT) snprintf(buf, 64, "%g", a.u.f);
                else strncpy(buf, a.u.s ? a.u.s : "", 63);
                vpush(I, make_str(buf));
                continue;
            }

            /* 数学 */
            if (strcmp(word, "abs") == 0) {
                Val a = vpop(I);
                if (a.type == V_FLOAT) vpush(I, make_float(fabs(a.u.f)));
                else vpush(I, make_int(a.u.i < 0 ? -a.u.i : a.u.i));
                continue;
            }
            if (strcmp(word, "neg") == 0) {
                Val a = vpop(I);
                if (a.type == V_FLOAT) vpush(I, make_float(-a.u.f));
                else vpush(I, make_int(-a.u.i));
                continue;
            }
            if (strcmp(word, "mod") == 0) {
                Val b = vpop(I), a = vpop(I);
                vpush(I, apply_op(TK_PERCENT, a, b));
                continue;
            }

            /* 字符串操�?*/
            if (strcmp(word, "str-len") == 0) {
                Val a = vpop(I);
                vpush(I, make_int(a.type == V_STR && a.u.s ? (int64_t)strlen(a.u.s) : 0));
                continue;
            }
            if (strcmp(word, "str-cat") == 0 || strcmp(word, "str-concat") == 0) {
                Val b = vpop(I), a = vpop(I);
                char *sa = a.type == V_STR ? a.u.s : "";
                char *sb = b.type == V_STR ? b.u.s : "";
                char *r = malloc(strlen(sa) + strlen(sb) + 1);
                strcpy(r, sa); strcat(r, sb);
                Val v; v.type = V_STR; v.u.s = r;
                vpush(I, v);
                continue;
            }
            if (strcmp(word, "str-eq") == 0) {
                Val b = vpop(I), a = vpop(I);
                int eq = (a.type == V_STR && b.type == V_STR) ? (strcmp(a.u.s, b.u.s) == 0) : 0;
                vpush(I, make_int(eq));
                continue;
            }

            /* sleep */
            if (strcmp(word, "sleep") == 0) {
                Val a = vpop(I);
#ifdef _WIN32
                Sleep((DWORD)a.u.i);
#endif
                continue;
            }

            /* break / continue */
            if (strcmp(word, "break") == 0) {
                if (I->loop_depth > 0) I->loops[I->loop_depth - 1].break_flag = 1;
                return;
            }
            if (strcmp(word, "continue") == 0) {
                if (I->loop_depth > 0) I->loops[I->loop_depth - 1].continue_flag = 1;
                return;
            }
            /* return */
            if (strcmp(word, "return") == 0) { return; }

            /* == var 声明: "var name" �?"value var name !" == */
            handle_var:
            if (strcmp(word, "var") == 0) {
                /* 读变量名 */
                if (L->cur.kind == TK_ID) {
                    char vname[128];
                    strncpy(vname, L->cur.id, 127); vname[127] = '\0';
                    lex_next(L);
                    Val init = make_int(0);
                    /* var name: value */
                    if (L->cur.kind == TK_COLON) {
                        lex_next(L);
                        init = eval_infix_expr(I, L);
                    } else if (I->sp > 0) {
                        /* 后缀: value var name */
                        init = vpop(I);
                    }
                    create_var(I, vname, init);
                }
                continue;
            }

            /* == const 声明: "const NAME : value" == */
            handle_const:
            if (strcmp(word, "const") == 0) {
                if (L->cur.kind == TK_ID) {
                    char cname[128];
                    strncpy(cname, L->cur.id, 127); cname[127] = '\0';
                    lex_next(L);
                    Val init = make_int(0);
                    if (L->cur.kind == TK_COLON) {
                        lex_next(L);
                        init = eval_infix_expr(I, L);
                    }
                    create_var(I, cname, init);
                }
                continue;
            }

            /* == Forth风格: { cond } { body } if  (块先行式) == */
            handle_if:
            if (strcmp(word, "if") == 0) {
                Val cond;
                if (L->cur.kind == TK_LPAREN) {
                    /* C风格: if (cond) { ... } */
                    cond = eval_infix_expr(I, L);
                } else if (I->sp >= 2 && I->stack[I->sp-1].type == V_LAMBDA && I->stack[I->sp-2].type == V_LAMBDA) {
                    /* Forth风格: { cond } { then } if  �? { cond } { then } { else } if */
                    Val else_body = vpop(I);
                    Val then_body = vpop(I);
                    /* 执行条件�?*/
                    Lexer cL; lex_init(&cL, then_body.u.lam.body);
                    /* 等等, Forth风格�?{ cond } { body } if */
                    /* then_body 其实�?cond_block, else_body 其实�?then_block */
                    Lexer condL; lex_init(&condL, then_body.u.lam.body);
                    exec_token_stream(I, &condL);
                    cond = vpop(I);
                    if (val_truthy(cond)) {
                        Lexer bL; lex_init(&bL, else_body.u.lam.body);
                        exec_token_stream(I, &bL);
                    }
                    continue;
                } else if (I->sp >= 1 && I->stack[I->sp-1].type == V_LAMBDA) {
                    /* 单块: cond_val { body } if */
                    Val body_block = vpop(I);
                    cond = vpop(I);
                    if (val_truthy(cond)) {
                        Lexer bL; lex_init(&bL, body_block.u.lam.body);
                        exec_token_stream(I, &bL);
                    }
                    continue;
                } else {
                    /* 后缀: 条件已在栈上, 后面�?{ } */
                    cond = vpop(I);
                }
                if (L->cur.kind == TK_LBRACE) {
                    lex_next(L); /* �?{ */
                    if (val_truthy(cond)) {
                        char *body = capture_block(L);
                        Lexer bL; lex_init(&bL, body);
                        exec_token_stream(I, &bL);
                        free(body);
                    } else {
                        int depth = 1;
                        while (depth > 0 && L->cur.kind != TK_EOF) {
                            if (L->cur.kind == TK_LBRACE) depth++;
                            else if (L->cur.kind == TK_RBRACE) depth--;
                            if (depth > 0) lex_next(L);
                        }
                        if (L->cur.kind == TK_RBRACE) lex_next(L);
                    }
                    /* 检�?else */
                    while (L->cur.kind == TK_NEWLINE) lex_next(L);
                    if (L->cur.kind == TK_ID && strcmp(L->cur.id, "else") == 0) {
                        lex_next(L);
                        if (L->cur.kind == TK_LBRACE) {
                            lex_next(L);
                            if (!val_truthy(cond)) {
                                char *body = capture_block(L);
                                Lexer bL; lex_init(&bL, body);
                                exec_token_stream(I, &bL);
                                free(body);
                            } else {
                                int depth = 1;
                                while (depth > 0 && L->cur.kind != TK_EOF) {
                                    if (L->cur.kind == TK_LBRACE) depth++;
                                    else if (L->cur.kind == TK_RBRACE) depth--;
                                    if (depth > 0) lex_next(L);
                                }
                                if (L->cur.kind == TK_RBRACE) lex_next(L);
                            }
                        }
                    }
                }
                continue;
            }

            /* == while == */
            handle_while:
            if (strcmp(word, "while") == 0) {
                if (L->cur.kind == TK_LPAREN) {
                    /* C风格: while (cond) { body } */
                    /* 捕获条件文本 */
                    const char *cond_start = L->p; /* 指向 ( 之后 */
                    /* 先跳过条件表达式以找�?{ */
                    Val first_cond = eval_infix_expr(I, L);
                    while (L->cur.kind == TK_NEWLINE) lex_next(L);
                    /* 现在 L->cur 应该�?{ */
                    char *body_text = NULL;
                    if (L->cur.kind == TK_LBRACE) {
                        body_text = capture_block(L);
                    }
                    if (!body_text) { continue; }
                    /* 如果第一次条件就为假 */
                    if (!val_truthy(first_cond)) { free(body_text); continue; }
                    /* 执行第一次循环体 */
                    I->loops[I->loop_depth].break_flag = 0;
                    I->loops[I->loop_depth].continue_flag = 0;
                    I->loop_depth++;
                    Lexer bL; lex_init(&bL, body_text);
                    exec_token_stream(I, &bL);
                    I->loop_depth--;
                    if (I->loops[I->loop_depth].break_flag) { free(body_text); continue; }
                    /* 后续迭代 */
                    for (;;) {
                        Lexer condL; lex_init(&condL, cond_start);
                        Val cv = eval_infix_expr(I, &condL);
                        if (!val_truthy(cv)) break;
                        I->loops[I->loop_depth].break_flag = 0;
                        I->loops[I->loop_depth].continue_flag = 0;
                        I->loop_depth++;
                        Lexer bL2; lex_init(&bL2, body_text);
                        exec_token_stream(I, &bL2);
                        I->loop_depth--;
                        if (I->loops[I->loop_depth].break_flag || I->had_error) break;
                    }
                    free(body_text);
                } else if (I->sp >= 2 && I->stack[I->sp-1].type == V_LAMBDA && I->stack[I->sp-2].type == V_LAMBDA) {
                    /* Forth风格: { cond } { body } while */
                    Val body_block = vpop(I);
                    Val cond_block = vpop(I);
                    for (;;) {
                        Lexer cL; lex_init(&cL, cond_block.u.lam.body);
                        exec_token_stream(I, &cL);
                        if (I->sp <= 0 || !val_truthy(vpop(I))) break;
                        I->loops[I->loop_depth].break_flag = 0;
                        I->loops[I->loop_depth].continue_flag = 0;
                        I->loop_depth++;
                        Lexer bL; lex_init(&bL, body_block.u.lam.body);
                        exec_token_stream(I, &bL);
                        I->loop_depth--;
                        if (I->loops[I->loop_depth].break_flag || I->had_error) break;
                    }
                } else {
                    Val cv = vpop(I);
                    if (L->cur.kind == TK_LBRACE) {
                        lex_next(L);
                        char *body = capture_block(L);
                        while (val_truthy(cv) && !I->had_error) {
                            I->loops[I->loop_depth].break_flag = 0;
                            I->loops[I->loop_depth].continue_flag = 0;
                            I->loop_depth++;
                            Lexer bL; lex_init(&bL, body);
                            exec_token_stream(I, &bL);
                            I->loop_depth--;
                            if (I->loops[I->loop_depth].break_flag) break;
                            if (I->sp > 0) cv = vpop(I); else break;
                        }
                        free(body);
                    }
                }
                continue;
            }

            /* == loop { ... } (无限循环) == */
            if (strcmp(word, "loop") == 0) {
                if (L->cur.kind == TK_LBRACE) {
                    lex_next(L);
                    char *body = capture_block(L);
                    for (;;) {
                        I->loops[I->loop_depth].break_flag = 0;
                        I->loops[I->loop_depth].continue_flag = 0;
                        I->loop_depth++;
                        Lexer bL;
                        lex_init(&bL, body);
                        exec_token_stream(I, &bL);
                        I->loop_depth--;
                        if (I->loops[I->loop_depth].break_flag || I->had_error) break;
                    }
                    free(body);
                }
                continue;
            }

            /* == for range: for i 0 10 { ... } == */
            handle_for:
            if (strcmp(word, "for") == 0) {
                if (L->cur.kind == TK_ID) {
                    char iname[128];
                    strncpy(iname, L->cur.id, 127); iname[127] = '\0';
                    lex_next(L);
                    Val start_v = eval_infix_expr(I, L);
                    Val end_v = eval_infix_expr(I, L);
                    int64_t s = start_v.u.i, e = end_v.u.i;
                    if (L->cur.kind == TK_LBRACE) {
                        lex_next(L);
                        char *body = capture_block(L);
                        for (int64_t i = s; i < e && !I->had_error; i++) {
                            create_var(I, iname, make_int(i));
                            I->loops[I->loop_depth].break_flag = 0;
                            I->loops[I->loop_depth].continue_flag = 0;
                            I->loop_depth++;
                            Lexer bL; lex_init(&bL, body);
                            exec_token_stream(I, &bL);
                            I->loop_depth--;
                            if (I->loops[I->loop_depth].break_flag) break;
                        }
                        free(body);
                    }
                }
                continue;
            }

            /* == 函数定义�? main: { ... } �?name: value (变量声明) == */
            if (L->cur.kind == TK_COLON) {
                lex_next(L); /* 吃掉 : */
                while (L->cur.kind == TK_NEWLINE) lex_next(L);
                if (L->cur.kind == TK_LBRACE) {
                    /* name: { body } �?函数定义�?main 入口 */
                    char *body = capture_block(L);
                    if (strcmp(word, "main") == 0) {
                        /* 立刻执行 main */
                        Lexer bL;
                        lex_init(&bL, body);
                        exec_token_stream(I, &bL);
                        free(body);
                    } else {
                        /* 注册函数 */
                        if (I->func_count < MAX_FUNCS) {
                            Func *fn = &I->funcs[I->func_count++];
                            strncpy(fn->name, word, 127); fn->name[127] = '\0';
                            fn->body = body;
                            fn->params = NULL;
                            fn->param_count = 0;
                        }
                    }
                } else {
                    /* name: value �?变量声明 (例如 x: 12) */
                    Val init = eval_infix_expr(I, L);
                    create_var(I, word, init);
                }
                }
                continue;

            /* == exec0 / exec2 �?执行 Lambda == */
            if (strcmp(word, "exec0") == 0) {
                Val lv = vpop(I);
                if (lv.type == V_LAMBDA) {
                    Lexer fnL; lex_init(&fnL, lv.u.lam.body);
                    exec_token_stream(I, &fnL);
                }
                continue;
            }
            if (strcmp(word, "exec1") == 0) {
                Val lv = vpop(I);
                Val a1 = vpop(I);
                if (lv.type == V_LAMBDA && lv.u.lam.param_count >= 1) {
                    create_var(I, lv.u.lam.params[0], a1);
                    Lexer fnL; lex_init(&fnL, lv.u.lam.body);
                    exec_token_stream(I, &fnL);
                }
                continue;
            }
            if (strcmp(word, "exec2") == 0) {
                Val lv = vpop(I);
                Val a2 = vpop(I), a1 = vpop(I);
                if (lv.type == V_LAMBDA && lv.u.lam.param_count >= 2) {
                    create_var(I, lv.u.lam.params[0], a1);
                    create_var(I, lv.u.lam.params[1], a2);
                    Lexer fnL; lex_init(&fnL, lv.u.lam.body);
                    exec_token_stream(I, &fnL);
                }
                continue;
            }

            /* == Forth 风格函数定义: : name ... ; == */
            if (strcmp(word, ":") == 0 || t->kind == TK_COLON) {
                continue;
            }

            /* == 查找用户自定义函�?== */
            Func *fn = find_func(I, word);
            if (fn) {
                Lexer fnL; lex_init(&fnL, fn->body);
                exec_token_stream(I, &fnL);
                continue;
            }

            /* == 查找变量 (后缀读取) == */
            Var *var = find_var(I, word);
            if (var) {
                if (L->cur.kind == TK_BANG) {
                    lex_next(L); /* �?! */
                    var->val = vpop(I);
                } else {
                    vpush(I, var->val);
                }
                continue;
            }

            /* 未知单词 */
            fprintf(stderr, "error: unknown word '%s' (line %d)\n", word, t->line);
            I->had_error = 1;
            continue;
        }

        /* 冒号（独立的函数定义开�?`: name ... ;`�?*/
        if (t->kind == TK_COLON) {
            lex_next(L);
            if (L->cur.kind == TK_ID) {
                char fname[128];
                strncpy(fname, L->cur.id, 127); fname[127] = '\0';
                lex_next(L);
                /* 收集源码直到 ; */
                const char *body_start = L->p;
                while (L->cur.kind != TK_SEMI && L->cur.kind != TK_EOF) lex_next(L);
                size_t blen = (size_t)(L->p - body_start);
                /* 回退一点，不包�?; */
                if (L->cur.kind == TK_SEMI) {
                    const char *semi_pos = L->p - 1;
                    while (semi_pos > body_start && *semi_pos != ';') semi_pos--;
                    blen = (size_t)(semi_pos - body_start);
                }
                char *body = malloc(blen + 1);
                memcpy(body, body_start, blen);
                body[blen] = '\0';
                if (I->func_count < MAX_FUNCS) {
                    Func *fn = &I->funcs[I->func_count++];
                    strncpy(fn->name, fname, 127); fn->name[127] = '\0';
                    fn->body = body;
                    fn->params = NULL;
                    fn->param_count = 0;
                }
                if (L->cur.kind == TK_SEMI) lex_next(L);
            }
            continue;
        }

        /* 右大括号 �?块结�?*/
        if (t->kind == TK_RBRACE) {
            lex_next(L);
            return;
        }

        /* 跳过不认识的 Token */
        lex_next(L);
    }
}

/* ========== 公共 API ========== */

/* 执行一�?Mira 源码 */
void mira_interpret(const char *source, const char *filename) {
    Interp interp;
    memset(&interp, 0, sizeof(interp));
    interp.filename = filename ? filename : "<input>";
    G = &interp;

    Lexer L;
    lex_init(&L, source);
    exec_token_stream(&interp, &L);

    if (interp.had_error) {
        /* 解释器中报错不用 exit */
    }
}

/* 执行一�?.mira 文件 */
void mira_interpret_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "error: cannot open '%s'\n", path);
        return;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)sz + 1);
    fread(buf, 1, (size_t)sz, f);
    buf[sz] = '\0';
    fclose(f);

    mira_interpret(buf, path);
    free(buf);
}

/* REPL 交互模式 */
void mira_repl(void) {
    Interp interp;
    memset(&interp, 0, sizeof(interp));
    interp.is_repl = 1;
    interp.filename = "<repl>";
    G = &interp;

    itp_color_cyan();
    fprintf(stderr, "Mira Interpreter (REPL) v5.14.0\n");
    fprintf(stderr, "Type expressions to evaluate. Type 'exit' or 'quit' to leave.\n");
    itp_color_reset();
    fprintf(stderr, "\n");

    char line[4096];
    for (;;) {
        itp_color_green();
        fprintf(stderr, "mira> ");
        itp_color_reset();
        fflush(stderr);

        if (!fgets(line, sizeof line, stdin)) break;
        /* 去掉末尾换行 */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';
        if (len == 0) continue;

        /* 退出命�?*/
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) break;

        /* 解释执行 */
        interp.had_error = 0;
        Lexer L;
        lex_init(&L, line);
        int old_sp = interp.sp;
        exec_token_stream(&interp, &L);

        if (!interp.had_error) {
            /* 如果栈上有新的值，自动打印 */
            if (interp.sp > old_sp) {
                itp_color_cyan();
                fprintf(stderr, "=> ");
                itp_color_reset();
                print_val(interp.stack[interp.sp - 1]);
                printf("\n");
                fflush(stdout);
            }
        }
    }

    itp_color_cyan();
    fprintf(stderr, "Bye!\n");
    itp_color_reset();
}
