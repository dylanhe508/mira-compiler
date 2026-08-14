#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
c2mira.py —— csmith C 输出 → Mira 语法转换器(差分正确性测试用)

覆盖 csmith 受限子集(--no-argc --no-arrays --no-pointers --no-structs
--no-unions --no-bitfields --no-packed-struct --no-longlong --no-builtins
--no-math64 --no-const-pointers --no-volatile-pointers --no-volatiles
--no-int8 --no-float --no-safe-math --no-divs --no-comma-operators
--no-compound-assignment --no-embedded-assigns --no-post-incr-operator
--no-pre-incr-operator --no-post-decr-operator --no-pre-decr-operator):

类型: int32_t/uint32_t/int16_t/uint16_t/int/unsigned
运算: + - * & | ^ << >> 比较 逻辑 && || ! ~ 一元负 强制转换 三元
语句: 声明 赋值 调用 if/else while do-while for return break continue 块
main : 重写为 crc32 校验和输出(与 gcc 的 "checksum = N" 对照)

Mira 侧语义保真要点:
- 32 位截断:w32(x)/w16(x) 符号扩展,(x & 4294967295)/(x & 65535) 零扩展
- / % 已探针验证为 C 截断语义(Mira 原生一致)
- && || 三元 展开为临时变量 + if/else(保短路与顺序求值),temp 声明在函数头
- !x -> (x == 0)  ~x -> (x ^ 4294967295)  一元负 -> (0 - x)
- u32 参与比较/算术时按无符号掩码(usual arithmetic conversions)
"""
import sys, re

INT32_MAX = 2147483647
UINT32_MAX = 4294967295

S32, U32, S16, U16 = "s32", "u32", "s16", "u16"
TYPE_NAMES = {
    "int32_t": S32, "int": S32, "signed": S32,
    "uint32_t": U32, "unsigned": U32, "unsigned int": U32,
    "int16_t": S16, "short": S16,
    "uint16_t": U16, "unsigned short": U16, "unsigned short int": U16,
}

def promote(a, b):
    if a == U32 or b == U32:
        return U32
    return S32

def conv_to(v, t):
    """C 赋值转换:Mira 表达式字符串 v 转为类型 t 的存储形式"""
    if t == S32:
        return "w32(%s)" % v
    if t == U32:
        return "(%s & 4294967295)" % v
    if t == S16:
        return "w16(%s)" % v
    if t == U16:
        return "(%s & 65535)" % v
    return v

# ---------------- 词法 ----------------

class Tok:
    def __init__(self, kind, val, pos):
        self.kind = kind  # num id op lp rp lb rb lsq rsq semi comma
        self.val = val
        self.pos = pos

OPS2 = ["<<", ">>", "<=", ">=", "==", "!=", "&&", "||", "+=", "-=", "*=", "/=",
        "%=", "&=", "|=", "^=", "++", "--", "->"]

def tokenize(src):
    toks = []
    i, n = 0, len(src)
    while i < n:
        c = src[i]
        if c.isspace():
            i += 1
            continue
        if c == '/' and i + 1 < n and src[i+1] == '*':
            j = src.find('*/', i + 2)
            i = j + 2 if j >= 0 else n
            continue
        if c == '/' and i + 1 < n and src[i+1] == '/':
            j = src.find('\n', i)
            i = j if j >= 0 else n
            continue
        if c == '"':
            j = i + 1
            while j < n and src[j] != '"':
                if src[j] == '\\':
                    j += 1
                j += 1
            i = j + 1
            continue
        if c == "'":
            j = i + 1
            while j < n and src[j] != "'":
                if src[j] == '\\':
                    j += 1
                j += 1
            i = j + 1
            continue
        if c.isdigit():
            j = i
            while j < n and (src[j].isalnum() or src[j] == '_' or src[j] == '.'):
                j += 1
            toks.append(Tok("num", src[i:j], i))
            i = j
            continue
        if c.isalpha() or c == '_':
            j = i
            while j < n and (src[j].isalnum() or src[j] == '_'):
                j += 1
            toks.append(Tok("id", src[i:j], i))
            i = j
            continue
        two = src[i:i+2]
        if two in OPS2:
            toks.append(Tok("op", two, i)); i += 2; continue
        if c in "(){}[],;=+-*/%&|^~<>!?:.":
            k = {"(": "lp", ")": "rp", "{": "lb", "}": "rb", "[": "lsq", "]": "rsq",
                 ",": "comma", ";": "semi"}.get(c, "op")
            toks.append(Tok(k, c, i))
            i += 1
            continue
        raise ValueError("词法错误 @%d: %r" % (i, src[i:i+10]))
    return toks

# ---------------- 解析器 ----------------

class Parser:
    def __init__(self, src):
        self.toks = tokenize(src)
        self.pos = 0
        self.syms = {}          # 变量名 -> 类型(当前函数上下文)
        self.globals = {}       # 全局变量 名 -> (类型, init AST)
        self.funcs = {}         # 函数名 -> (返回类型, [参数类型])
        self.func_bodies = {}   # 函数名 -> (ret_t, param_ts, param_names, stmts, func_syms)
        self.static_locals = {} # 名 -> (类型, init AST) 静态局部提升为全局
        self.temp_counter = 0
        self.cur_temps = []
        self.loop_stack = []
        self.emit_loop_depth = 0
        self.cur_has_goto = False
        self.out_lines = []
        self.cur_ret_type = S32
        self.cur_func = ""

    # ---------------- 记号辅助 ----------------

    def peek(self):
        if self.pos < len(self.toks):
            return self.toks[self.pos]
        return Tok("eof", "", len(self.toks))

    def at(self, kind, val=None):
        t = self.peek()
        return t.kind == kind and (val is None or t.val == val)

    def eat(self, kind, val=None):
        if self.at(kind, val):
            self.pos += 1
            return True
        return False

    def expect(self, kind, val=None):
        if not self.eat(kind, val):
            raise ValueError("期望 %s%s,实际 %r @%d" % (kind, (" " + val) if val else "", self.peek().val, self.peek().pos))

    # ---------------- 表达式 ----------------

    def parse_expr(self):
        e = self.parse_ternary()
        # 顶层赋值(for 初始化/语句;--no-embedded-assigns 保证不在条件里)
        if e[0] == "id" and self.peek().kind == "op" and self.peek().val == "=":
            self.pos += 1
            rhs = self.parse_expr()
            return ("assign", e[1], rhs)
        # 顶层复合赋值(csmith 的 for 步进常写 g_2 -= 6)
        if e[0] == "id" and self.peek().kind == "op" and self.peek().val in \
                ("+=", "-=", "*=", "/=", "%=", "&=", "|=", "^="):
            op = self.peek().val
            self.pos += 1
            rhs = self.parse_expr()
            return ("cassign", e[1], op, rhs)
        return e

    def parse_ternary(self):
        c = self.parse_logor()
        if self.at("op", "?"):
            self.pos += 1
            a = self.parse_expr()
            self.expect("op", ":")
            b = self.parse_expr()
            return ("ternary", c, a, b)
        return c

    def parse_logor(self):
        e = self.parse_logand()
        while self.at("op", "||"):
            self.pos += 1
            e = ("logor", e, self.parse_logand())
        return e

    def parse_logand(self):
        e = self.parse_bitor()
        while self.at("op", "&&"):
            self.pos += 1
            e = ("logand", e, self.parse_bitor())
        return e

    def parse_bitor(self):
        e = self.parse_bitxor()
        while self.at("op", "|"):
            self.pos += 1
            e = ("bin", "|", e, self.parse_bitxor())
        return e

    def parse_bitxor(self):
        e = self.parse_bitand()
        while self.at("op", "^"):
            self.pos += 1
            e = ("bin", "^", e, self.parse_bitand())
        return e

    def parse_bitand(self):
        e = self.parse_eq()
        while self.at("op", "&"):
            self.pos += 1
            e = ("bin", "&", e, self.parse_eq())
        return e

    def parse_eq(self):
        e = self.parse_rel()
        while self.at("op", "==") or self.at("op", "!="):
            op = self.peek().val
            self.pos += 1
            e = ("bin", op, e, self.parse_rel())
        return e

    def parse_rel(self):
        e = self.parse_shift()
        while self.at("op", "<") or self.at("op", ">") or self.at("op", "<=") or self.at("op", ">="):
            op = self.peek().val
            self.pos += 1
            e = ("bin", op, e, self.parse_shift())
        return e

    def parse_shift(self):
        e = self.parse_add()
        while self.at("op", "<<") or self.at("op", ">>"):
            op = self.peek().val
            self.pos += 1
            e = ("bin", op, e, self.parse_add())
        return e

    def parse_add(self):
        e = self.parse_mul()
        while self.at("op", "+") or self.at("op", "-"):
            op = self.peek().val
            self.pos += 1
            e = ("bin", op, e, self.parse_mul())
        return e

    def parse_mul(self):
        e = self.parse_unary()
        while self.at("op", "*") or self.at("op", "/") or self.at("op", "%"):
            op = self.peek().val
            self.pos += 1
            e = ("bin", op, e, self.parse_unary())
        return e

    def parse_unary(self):
        t = self.peek()
        if t.kind == "op" and t.val in ("!", "~", "-", "+"):
            self.pos += 1
            if t.val == "+":
                return self.parse_unary()
            return ("un", t.val, self.parse_unary())
        if t.kind == "lp":
            save = self.pos
            self.pos += 1
            nt = self.peek()
            if nt.kind == "id" and nt.val in TYPE_NAMES:
                self.pos += 1
                if self.eat("rp"):
                    return ("cast", TYPE_NAMES[nt.val], self.parse_unary())
            self.pos = save
        return self.parse_postfix()

    def parse_postfix(self):
        e = self.parse_primary()
        while self.at("lp"):
            args = []
            self.pos += 1
            if not self.at("rp"):
                args.append(self.parse_expr())
                while self.eat("comma"):
                    args.append(self.parse_expr())
            self.expect("rp")
            e = ("call", e, args)
        return e

    def parse_primary(self):
        t = self.peek()
        if t.kind == "num":
            self.pos += 1
            return ("num", t.val)
        if t.kind == "id":
            self.pos += 1
            return ("id", t.val)
        if t.kind == "lp":
            self.pos += 1
            e = self.parse_expr()
            self.expect("rp")
            return e
        if t.kind == "eof":
            raise ValueError("表达式意外结束")
        raise ValueError("表达式错误 @%d: %r" % (t.pos, t.val))

    # ---------------- 类型与代码生成 ----------------

    def new_temp(self):
        n = "__t%d" % self.temp_counter
        self.temp_counter += 1
        self.cur_temps.append(n)
        return n

    def num_val(self, s):
        s2 = s.rstrip("uUlL")
        return int(s2, 0) if s2.lower().startswith("0x") else int(s2)

    def num_type(self, s):
        v = self.num_val(s)
        return U32 if (v > INT32_MAX or s[-1] in "uU") else S32

    def gen(self, node):
        """返回 (mira表达式串, 类型, [前置语句])。逻辑/三元在此展开。"""
        kind = node[0]
        if kind in ("logand", "logor", "ternary"):
            return self.gen_logic(node)
        if kind == "num":
            return str(self.num_val(node[1])), self.num_type(node[1]), []
        if kind == "id":
            t = self.syms.get(node[1])
            if t is None:
                raise ValueError("未知变量 %r" % node[1])
            return node[1], t, []
        if kind == "call":
            return self.gen_call(node)
        if kind == "un":
            return self.gen_unary(node)
        if kind == "cast":
            t = node[1]
            a, _, pre = self.gen(node[2])
            return conv_to(a, t), t, pre
        if kind == "bin":
            return self.gen_bin(node)
        if kind == "cassign":
            # x op= e -> x = x op (e)(嵌套时兜底展开)
            name, op, rhs = node[1], node[2], node[3]
            t = self.syms.get(name)
            if t is None:
                raise ValueError("复合赋值未知变量 %s" % name)
            s, _, pre = self.gen(rhs)
            # 表达式位置的赋值(Mira 的 = 仅语句级):提升为前置赋值语句,
            # 表达式值即赋值后的变量本身(C 语义)
            return name, t, pre + ["%s = %s;" % (name, conv_to("(%s %s (%s))" % (name, op[0], s), t))]
        if kind == "assign":
            # 兜底:x = e —— 表达式位置的赋值(如 for 步长、条件内的赋值;
            # Mira 的 = 仅语句级),提升为前置赋值语句,表达式值即变量本身
            name, rhs = node[1], node[2]
            t = self.syms.get(name)
            if t is None:
                raise ValueError("赋值未知变量 %s" % name)
            s, _, pre = self.gen(rhs)
            return name, t, pre + ["%s = %s;" % (name, conv_to(s, t))]
        raise ValueError("未知节点 %r" % (kind,))

    def gen_call(self, node):
        callee = node[1]
        if callee[0] != "id":
            raise ValueError("调用目标非标识符 %r" % (callee,))
        name = callee[1]
        if name not in self.funcs:
            raise ValueError("未知函数 %r" % name)
        ret_t, param_ts = self.funcs[name]
        if len(param_ts) != len(node[2]):
            raise ValueError("参数个数不符 %s" % name)
        args = []
        pre = []
        for a, pt in zip(node[2], param_ts):
            s, _, p = self.gen(a)
            pre.extend(p)
            args.append(conv_to(s, pt))
        return "%s(%s)" % (name, ", ".join(args)), ret_t, pre

    def gen_unary(self, node):
        op = node[1]
        a, at, pre = self.gen(node[2])
        if op == "!":
            return "(%s == 0)" % a, S32, pre
        if op == "~":
            return conv_to("(%s ^ 4294967295)" % a, at), at, pre
        if op == "-":
            return conv_to("(0 - %s)" % a, at), at, pre
        raise ValueError("不支持一元 %r" % op)

    def gen_bin(self, node):
        op = node[1]
        a, at, preA = self.gen(node[2])
        b, bt, preB = self.gen(node[3])
        pre = preA + preB
        if op in ("<<", ">>"):
            t = at
            a2 = conv_to(a, at)
            return conv_to("(%s %s %s)" % (a2, op, b), t), t, pre
        if op in ("==", "!=", "<", ">", "<=", ">="):
            if at == U32 or bt == U32:
                a2 = "(%s & 4294967295)" % a
                b2 = "(%s & 4294967295)" % b
            else:
                a2, b2 = a, b
            return "(%s %s %s)" % (a2, op, b2), S32, pre
        t = promote(at, bt)
        a2 = conv_to(a, t)
        b2 = conv_to(b, t)
        return conv_to("(%s %s %s)" % (a2, op, b2), t), t, pre

    def gen_logic(self, node):
        """&& || ?: 展开为临时变量 + if/else(短路,顺序求值)。temp 声明在函数头。"""
        kind = node[0]
        tname = self.new_temp()
        pre = []
        if kind == "logand":
            a, _, pa = self.gen(node[1])
            b, _, pb = self.gen(node[2])
            pre += pa
            pre.append("if ((%s) != 0) { %s = ((%s) != 0); } else { %s = 0; }" % (a, tname, b, tname))
            pre += pb
            return tname, S32, pre
        if kind == "logor":
            a, _, pa = self.gen(node[1])
            b, _, pb = self.gen(node[2])
            pre += pa
            pre.append("if ((%s) != 0) { %s = 1; } else { %s = ((%s) != 0); }" % (a, tname, tname, b))
            pre += pb
            return tname, S32, pre
        c, _, pc = self.gen(node[1])
        a, at, pa = self.gen(node[2])
        b, bt, pb = self.gen(node[3])
        t = promote(at, bt)
        pre += pc
        pre.append("if ((%s) != 0) { %s = %s; } else { %s = %s; }" % (c, tname, a, tname, b))
        pre += pa + pb
        return tname, t, pre

    # ---------------- 语句 ----------------

    def parse_statement(self):
        t = self.peek()
        if t.kind == "lb":
            self.pos += 1
            stmts = []
            while not self.at("rb"):
                stmts.append(self.parse_statement())
            self.expect("rb")
            return ("block", stmts)
        if t.kind == "semi":
            self.pos += 1
            return ("empty",)
        if t.kind == "id":
            v = t.val
            if v == "goto":
                self.pos += 1
                name_tok = self.peek()
                if name_tok.kind != "id":
                    raise ValueError("goto 缺目标 @%d" % t.pos)
                self.pos += 1
                self.expect("semi")
                return ("goto", name_tok.val)
            # label: ident ':'
            if self.pos + 1 < len(self.toks) and self.toks[self.pos+1].kind == "op" \
                    and self.toks[self.pos+1].val == ":":
                self.pos += 2
                return ("label", v)
            if v == "static":
                self.pos += 1
                if not (self.peek().kind == "id" and self.peek().val in TYPE_NAMES):
                    raise ValueError("static 后非类型 @%d" % t.pos)
                tn = self.peek().val
                self.pos += 1
                t2, name, init = self.parse_decl(tn)
                self.expect("semi")
                # 静态局部:提升为全局,函数内不再声明
                self.static_locals[name] = (t2, init)
                self.syms[name] = t2
                return ("empty",)
            if v == "if":
                self.pos += 1
                self.expect("lp")
                c = self.parse_expr()
                self.expect("rp")
                then_s = self.parse_statement()
                else_s = None
                if self.at("id", "else"):
                    self.pos += 1
                    else_s = self.parse_statement()
                return ("if", c, then_s, else_s)
            if v == "while":
                self.pos += 1
                self.expect("lp")
                c = self.parse_expr()
                self.expect("rp")
                body = self.parse_statement()
                return ("while", c, body)
            if v == "do":
                self.pos += 1
                body = self.parse_statement()
                if not self.at("id", "while"):
                    raise ValueError("do 后缺 while")
                self.pos += 1
                self.expect("lp")
                c = self.parse_expr()
                self.expect("rp")
                self.eat("semi")
                return ("do", body, c)
            if v == "for":
                self.pos += 1
                self.expect("lp")
                if self.at("semi"):
                    self.pos += 1
                    init = None
                elif self.peek().kind == "id" and self.peek().val in TYPE_NAMES:
                    tn = self.peek().val
                    self.pos += 1
                    t2, name, init_e = self.parse_decl(tn)
                    self.syms[name] = t2
                    init = ("decl", t2, name, init_e)
                    self.expect("semi")
                else:
                    init = ("expr", self.parse_expr())
                    self.expect("semi")
                cond = None
                if not self.at("semi"):
                    cond = self.parse_expr()
                self.expect("semi")
                step = None
                if not self.at("rp"):
                    step = self.parse_expr()
                self.expect("rp")
                body = self.parse_statement()
                return ("for", init, cond, step, body)
            if v == "return":
                self.pos += 1
                e = None
                if not self.at("semi"):
                    e = self.parse_expr()
                self.expect("semi")
                return ("return", e)
            if v == "break":
                self.pos += 1
                self.expect("semi")
                return ("break",)
            if v == "continue":
                self.pos += 1
                self.expect("semi")
                return ("continue",)
            if v == "else":
                raise ValueError("意外的 else")
            if v in TYPE_NAMES:
                self.pos += 1
                t2, name, init = self.parse_decl(v)
                self.expect("semi")
                return ("decl", t2, name, init)
            e = self.parse_expr()
            self.expect("semi")
            return ("expr", e)
        if t.kind == "eof":
            raise ValueError("语句意外结束")
        raise ValueError("语句错误 @%d: %r" % (t.pos, t.val))

    def parse_decl(self, type_name):
        t = TYPE_NAMES[type_name]
        name_tok = self.peek()
        if name_tok.kind != "id":
            raise ValueError("声明缺变量名 @%d" % name_tok.pos)
        self.pos += 1
        name = name_tok.val
        init = None
        if self.eat("op", "="):
            init = self.parse_expr()
        return t, name, init

    # ---------------- 语句发射 ----------------

    def emit_body(self, body, indent):
        """控制结构(while/do/for)的 body:必须展开为复合 body 内语句。
        Mira 里 if/while 分支位置的 { } 是复合语句,但语句位置独立出现的
        { } 是闭包字面量(创建不执行);转换结构里 body 块恰好处于独立语句
        位置,若输出 { } 会整块丢失,故直接展开内容。"""
        if body[0] == "block":
            for s2 in body[1]:
                self.emit_stmt(s2, indent)
        else:
            self.emit_stmt(body, indent, True)

    def emit_stmt(self, st, indent, in_control=False):
        kind = st[0]
        if kind == "block":
            if in_control:
                # 控制结构分支位置的块:Mira 复合语句,保留花括号
                self.out_lines.append(indent + "{")
                for s2 in st[1]:
                    self.emit_stmt(s2, indent + "    ")
                self.out_lines.append(indent + "}")
            else:
                # 独立语句位置的 C 复合块:必须顺序执行;Mira 语句位置 {} 是
                # 闭包字面量(创建不执行),故展开为顺序语句。块内 mut 声明展开后
                # 构成 shadow,与 C 块作用域语义等价(csmith 变量名全局唯一)。
                for s2 in st[1]:
                    self.emit_stmt(s2, indent)
        elif kind == "decl":
            t, name, init = st[1], st[2], st[3]
            if init is not None:
                s, _, pre = self.gen(init)
                for p in pre:
                    self.out_lines.append(indent + p)
                self.out_lines.append(indent + "mut %s = %s;" % (name, conv_to(s, t)))
            else:
                self.out_lines.append(indent + "mut %s = 0;" % name)
        elif kind == "assign":
            name, e = st[1], st[2]
            t = self.syms.get(name)
            if t is None:
                raise ValueError("赋值未知变量 %s" % name)
            s, _, pre = self.gen(e)
            for p in pre:
                self.out_lines.append(indent + p)
            self.out_lines.append(indent + "%s = %s;" % (name, conv_to(s, t)))
        elif kind == "expr":
            e = st[1]
            if e[0] == "assign":
                name, rhs = e[1], e[2]
                t = self.syms.get(name)
                if t is None:
                    raise ValueError("赋值未知变量 %s" % name)
                s, _, pre = self.gen(rhs)
                for p in pre:
                    self.out_lines.append(indent + p)
                self.out_lines.append(indent + "%s = %s;" % (name, conv_to(s, t)))
            elif e[0] == "cassign":
                # 复合赋值展开为普通赋值
                name, op, rhs = e[1], e[2], e[3]
                t = self.syms.get(name)
                if t is None:
                    raise ValueError("复合赋值未知变量 %s" % name)
                s, _, pre = self.gen(rhs)
                for p in pre:
                    self.out_lines.append(indent + p)
                self.out_lines.append(indent + "%s = %s;" % (name, conv_to("(%s %s (%s))" % (name, op[0], s), t)))
            else:
                s, _, pre = self.gen(e)
                for p in pre:
                    self.out_lines.append(indent + p)
                # 表达式语句:值无人消费会残留 vstack(if 分支深度检查报错),
                # 丢弃到临时变量(赋值消费值,零残留)
                t = self.new_temp()
                self.out_lines.append(indent + "%s = %s;" % (t, s))
        elif kind == "if":
            c, then_s, else_s = st[1], st[2], st[3]
            cs, _, pre = self.gen(c)
            for p in pre:
                self.out_lines.append(indent + p)
            self.out_lines.append(indent + "if ((%s) != 0) {" % cs)
            # 分支体展开:避免与 if 的 { 叠成双层块(旧编译器丢内层语句;
            # 新编译器执行但残留深度取决于块末语句)
            if then_s[0] == "block":
                for s2 in then_s[1]:
                    self.emit_stmt(s2, indent + "    ")
            else:
                self.emit_stmt(then_s, indent + "    ", True)
            if else_s is not None:
                self.out_lines.append(indent + "} else {")
                if else_s[0] == "block":
                    for s2 in else_s[1]:
                        self.emit_stmt(s2, indent + "    ")
                else:
                    self.emit_stmt(else_s, indent + "    ", True)
            self.out_lines.append(indent + "}")
        elif kind == "while":
            c, body = st[1], st[2]
            cs, _, pre = self.gen(c)
            self.out_lines.append(indent + "while (1) {")
            for p in pre:
                self.out_lines.append(indent + "    " + p)
            self.out_lines.append(indent + "    if ((%s) != 0) { } else { break; }" % cs)
            self.loop_stack.append(("while", None, None))
            self.emit_loop_depth += 1
            self.emit_body(body, indent + "    ")
            self.emit_loop_depth -= 1
            self.loop_stack.pop()
            self.out_lines.append(indent + "}")
        elif kind == "do":
            body, c = st[1], st[2]
            cs, _, pre = self.gen(c)
            self.out_lines.append(indent + "while (1) {")
            self.loop_stack.append(("while", None, None))
            self.emit_loop_depth += 1
            self.emit_body(body, indent + "    ")
            self.emit_loop_depth -= 1
            self.loop_stack.pop()
            for p in pre:
                self.out_lines.append(indent + "    " + p)
            self.out_lines.append(indent + "    if ((%s) != 0) { } else { break; }" % cs)
            self.out_lines.append(indent + "}")
        elif kind == "for":
            init, cond, step, body = st[1], st[2], st[3], st[4]
            step_t = self.new_temp()
            break_t = self.new_temp()
            if init is not None:
                self.emit_stmt(init, indent)
            self.out_lines.append(indent + "while (1) {")
            if cond is not None:
                cs, _, pre = self.gen(cond)
                for p in pre:
                    self.out_lines.append(indent + "    " + p)
                self.out_lines.append(indent + "    if ((%s) != 0) { } else { break; }" % cs)
            self.out_lines.append(indent + "    %s = 0;" % step_t)
            self.out_lines.append(indent + "    %s = 0;" % break_t)
            self.out_lines.append(indent + "    while (1) {")
            self.loop_stack.append(("for", step_t, break_t))
            self.emit_loop_depth += 1
            self.emit_body(body, indent + "        ")
            self.emit_loop_depth -= 1
            self.loop_stack.pop()
            self.out_lines.append(indent + "        break;")
            self.out_lines.append(indent + "    }")
            if self.cur_has_goto:
                # goto 传播:跳过 step 直接退出本层
                self.out_lines.append(indent + "    if (__gl_any != 0) { break; }")
            # break 标志:跳过步长直接退出;continue 与自然走完都执行步长
            self.out_lines.append(indent + "    if ((%s) != 0) { break; }" % break_t)
            if step is not None:
                if step[0] == "cassign":
                    name, op, rhs = step[1], step[2], step[3]
                    t = self.syms.get(name)
                    if t is None:
                        raise ValueError("复合赋值未知变量 %s" % name)
                    ss, _, spre = self.gen(rhs)
                    for p in spre:
                        self.out_lines.append(indent + "    " + p)
                    self.out_lines.append(indent + "    %s = %s;" % (name, conv_to("(%s %s (%s))" % (name, op[0], ss), t)))
                else:
                    ss, _, spre = self.gen(step)
                    for p in spre:
                        self.out_lines.append(indent + "    " + p)
                    # 步长表达式语句:丢弃到临时变量(零残留)
                    t = self.new_temp()
                    self.out_lines.append(indent + "    %s = %s;" % (t, ss))
            self.out_lines.append(indent + "}")
        elif kind == "return":
            e = st[1]
            if e is not None:
                s, _, pre = self.gen(e)
                for p in pre:
                    self.out_lines.append(indent + p)
                # 后缀式:Mira 的 return 是后缀词(pop 栈顶为返回值)。
                # 前置式 "return expr;" 中 return 先于 expr 执行,空栈不 pop,
                # 导致 expr 残留(if 分支深度检查出错)
                self.out_lines.append(indent + "%s return;" % conv_to(s, self.cur_ret_type))
            else:
                self.out_lines.append(indent + "0 return;")
        elif kind == "break":
            if self.loop_stack and self.loop_stack[-1][0] == "for":
                self.out_lines.append(indent + "%s = 1;" % self.loop_stack[-1][2])
                self.out_lines.append(indent + "break;")
            else:
                self.out_lines.append(indent + "break;")
        elif kind == "continue":
            if self.loop_stack and self.loop_stack[-1][0] == "for":
                self.out_lines.append(indent + "%s = 1;" % self.loop_stack[-1][1])
                self.out_lines.append(indent + "break;")
            else:
                self.out_lines.append(indent + "continue;")
        elif kind == "goto":
            # 段式 goto:置标志 + 一串 break(层数 = 当前循环转换深度,含段 while)
            self.out_lines.append(indent + "__gl_%s = 1;" % st[1])
            self.out_lines.append(indent + "__gl_any = 1;")
            for _ in range(self.emit_loop_depth):
                self.out_lines.append(indent + "break;")
        elif kind == "label":
            pass  # 段切分处理,这里不输出
        elif kind == "empty":
            pass
        else:
            raise ValueError("未知语句 %r" % (kind,))

    # ---------------- 顶层 ----------------

    def scan_top(self):
        toks = self.toks
        n = len(toks)
        i = 0
        while i < n:
            t = toks[i]
            if t.kind == "id" and t.val == "static" and i + 1 < n and \
               toks[i+1].kind == "id" and toks[i+1].val in TYPE_NAMES:
                j = i + 1
                k = j + 1
                # 多 token 类型:static unsigned int g_x —— 名字在 int 之后
                if toks[j].val == "unsigned" and k < n and toks[k].kind == "id" and toks[k].val == "int":
                    k += 1
                if k >= n or toks[k].kind != "id":
                    i += 1
                    continue
                name = toks[k].val
                m = k + 1
                if m < n and toks[m].kind == "lp":
                    # 函数:返回类型就是声明处的类型
                    ret_t = TYPE_NAMES[toks[j].val]
                    p = m
                    depth = 0
                    while p < n:
                        if toks[p].kind == "lp":
                            depth += 1
                        elif toks[p].kind == "rp":
                            depth -= 1
                            if depth == 0:
                                break
                        p += 1
                    q = p + 1
                    while q < n and toks[q].kind != "lb" and toks[q].kind != "semi":
                        q += 1
                    param_ts, param_names = [], []
                    if toks[m+1].kind != "rp":
                        pp = m + 1
                        while pp < p:
                            if toks[pp].kind == "id" and toks[pp].val in TYPE_NAMES:
                                # 多 token 类型:unsigned int right —— int 是类型不是参数名
                                tname = toks[pp].val
                                np = pp + 1
                                if tname == "unsigned" and np < p and toks[np].kind == "id" and toks[np].val == "int":
                                    tname = "unsigned int"
                                    np += 1
                                if np < p and toks[np].kind == "id":
                                    param_ts.append(TYPE_NAMES[tname])
                                    param_names.append(toks[np].val)
                                    pp = np
                            pp += 1
                    self.funcs[name] = (ret_t, param_ts)
                    if toks[q].kind == "semi":
                        # 前向声明
                        i = q + 1
                        continue
                    # 函数体
                    self.pos = q
                    self.expect("lb")
                    # main 直接跳过(重写)
                    if name == "main":
                        depth = 1
                        while depth > 0:
                            t2 = self.peek()
                            if t2.kind == "lb":
                                depth += 1
                            elif t2.kind == "rb":
                                depth -= 1
                            self.pos += 1
                        self.funcs.pop(name)
                        i = self.pos
                        continue
                    stmts = []
                    while not self.at("rb"):
                        stmts.append(self.parse_statement())
                    self.expect("rb")
                    fsyms = {}
                    for pn, pt in zip(param_names, param_ts):
                        fsyms[pn] = pt
                    self.collect_func_syms(stmts, fsyms)
                    for gname in self.globals:
                        fsyms[gname] = self.globals[gname][0]
                    for sln in self.static_locals:
                        if sln not in fsyms:
                            fsyms[sln] = self.static_locals[sln][0]
                    self.func_bodies[name] = (ret_t, param_ts, param_names, stmts, fsyms)
                    i = self.pos
                    continue
                else:
                    # 全局变量 static int32_t g_2 = ...; 名字在 k,先消费
                    self.pos = k + 1
                    init = None
                    if self.eat("op", "="):
                        init = self.parse_expr()
                    self.expect("semi")
                    self.globals[name] = (TYPE_NAMES[toks[j].val], init)
                    i = self.pos
                    continue
            i += 1

    def collect_func_syms(self, stmts, fsyms):
        for st in stmts:
            k = st[0]
            if k == "decl":
                fsyms[st[2]] = st[1]
            elif k == "block":
                self.collect_func_syms(st[1], fsyms)
            elif k == "if":
                self.collect_func_syms([st[2]], fsyms)
                if st[3] is not None:
                    self.collect_func_syms([st[3]], fsyms)
            elif k == "while":
                self.collect_func_syms([st[2]], fsyms)
            elif k == "do":
                self.collect_func_syms([st[1]], fsyms)
            elif k == "for":
                if st[1] is not None:
                    self.collect_func_syms([st[1]], fsyms)
                self.collect_func_syms([st[4]], fsyms)

    def find_gotos(self, stmts):
        for st in stmts:
            k = st[0]
            if k == "goto":
                self.cur_has_goto = True
            elif k == "block":
                self.find_gotos(st[1])
            elif k == "if":
                self.find_gotos([st[2]])
                if st[3] is not None:
                    self.find_gotos([st[3]])
            elif k == "while":
                self.find_gotos([st[2]])
            elif k == "do":
                self.find_gotos([st[1]])
            elif k == "for":
                if st[1] is not None:
                    self.find_gotos([st[1]])
                self.find_gotos([st[4]])

    def contains_label(self, stmts):
        for st in stmts:
            k = st[0]
            if k == "label":
                return True
            if k == "block":
                if self.contains_label(st[1]):
                    return True
            elif k == "if":
                if self.contains_label([st[2]]) or (st[3] is not None and self.contains_label([st[3]])):
                    return True
            elif k == "while":
                if self.contains_label([st[2]]):
                    return True
            elif k == "do":
                if self.contains_label([st[1]]):
                    return True
            elif k == "for":
                if (st[1] is not None and self.contains_label([st[1]])) or self.contains_label([st[4]]):
                    return True
        return False

    def split_segments(self, stmts):
        segments = []
        cur = []
        seg_names = []
        label_map = {}
        for st in stmts:
            if st[0] == "label":
                label_map[st[1]] = len(segments)
                seg_names.append(st[1])
                segments.append(cur)
                cur = []
            elif st[0] == "block" and self.contains_label(st[1]):
                raise ValueError("label 嵌套在块内,暂不支持")
            else:
                cur.append(st)
        segments.append(cur)
        return segments, seg_names, label_map

    def check_gotos(self, stmts, seg_idx, label_map):
        for st in stmts:
            k = st[0]
            if k == "goto":
                t = label_map.get(st[1])
                if t is None:
                    raise ValueError("goto 目标未定义 %s" % st[1])
                if t <= seg_idx:
                    raise ValueError("向后 goto %s 暂不支持" % st[1])
            elif k == "block":
                self.check_gotos(st[1], seg_idx, label_map)
            elif k == "if":
                self.check_gotos([st[2]], seg_idx, label_map)
                if st[3] is not None:
                    self.check_gotos([st[3]], seg_idx, label_map)
            elif k == "while":
                self.check_gotos([st[2]], seg_idx, label_map)
            elif k == "do":
                self.check_gotos([st[1]], seg_idx, label_map)
            elif k == "for":
                if st[1] is not None:
                    self.check_gotos([st[1]], seg_idx, label_map)
                self.check_gotos([st[4]], seg_idx, label_map)

    def run(self):
        self.scan_top()
        # 全局变量输出
        for name in self.globals:
            t, init = self.globals[name]
            self.syms = dict(self.globals)
            for sln in self.static_locals:
                self.syms[sln] = self.static_locals[sln][0]
            if init is not None:
                s, _, pre = self.gen(init)
                if pre:
                    raise ValueError("全局 init 含展开语句")
                self.out_lines.append("var %s = %s;" % (name, conv_to(s, t)))
            else:
                self.out_lines.append("var %s = 0;" % name)
        for name in self.static_locals:
            t, init = self.static_locals[name]
            if init is not None:
                s, _, pre = self.gen(init)
                self.out_lines.append("var %s = %s;" % (name, conv_to(s, t)))
            else:
                self.out_lines.append("var %s = 0;" % name)
        # 函数体
        for fname in self.func_bodies:
            ret_t, param_ts, param_names, stmts, fsyms = self.func_bodies[fname]
            self.cur_ret_type = ret_t
            self.syms = dict(fsyms)
            self.temp_counter = 0
            self.cur_temps = []
            self.loop_stack = []
            self.emit_loop_depth = 0
            self.cur_has_goto = False
            self.find_gotos(stmts)
            self.out_lines.append("fn %s(%s) {" % (fname, ", ".join(param_names)))
            body_start = len(self.out_lines)
            if self.cur_has_goto:
                segments, seg_names, label_map = self.split_segments(stmts)
                self.check_gotos(stmts, 0, label_map)
                for ln in seg_names:
                    self.out_lines.append("    mut __gl_%s = 0;" % ln)
                self.out_lines.append("    mut __gl_any = 0;")
                # 段 0:函数开头到第一个 label
                self.out_lines.append("    while (1) {")
                self.emit_loop_depth = 1
                self.emit_stmt_list(segments[0], "        ")
                self.out_lines.append("        break;")
                self.out_lines.append("    }")
                self.emit_loop_depth = 0
                # 段 i:入口检查对应 label 的标志
                for idx in range(1, len(segments)):
                    ln = seg_names[idx - 1]
                    self.out_lines.append("    if (__gl_%s != 0) {" % ln)
                    self.out_lines.append("        __gl_%s = 0;" % ln)
                    self.out_lines.append("        while (1) {")
                    self.emit_loop_depth = 1
                    self.emit_stmt_list(segments[idx], "            ")
                    self.out_lines.append("            break;")
                    self.out_lines.append("        }")
                    self.emit_loop_depth = 0
                    self.out_lines.append("    }")
            else:
                self.emit_stmt_list(stmts)
            self.out_lines.append("}")
            # temp 在 emit 过程中才产生,事后把 mut 声明插到函数头
            # 参数 shadow:Mira 参数只读,可被赋值的参数需要同名可变副本
            decls = ["    mut %s = %s;" % (p, p) for p in param_names]
            decls += ["    mut %s = 0;" % t for t in self.cur_temps]
            self.out_lines[body_start:body_start] = decls
        # main 重写
        self.emit_main()
        return "\n".join(self.out_lines)

    def emit_stmt_list(self, stmts, indent="    "):
        for st in stmts:
            self.emit_stmt(st, indent)

    def emit_main(self):
        entry = "func_1"
        if entry not in self.funcs:
            raise ValueError("csmith 程序缺 func_1")
        self.out_lines.append("fn main() {")
        self.out_lines.append("    %s();" % entry)
        for name in self.globals:
            self.out_lines.append("    transparent_crc(%s);" % name)
        # 先求低 32 位再打印(避免 64 位符号扩展污染);gcc 侧是 %u
        self.out_lines.append("    print((crc32_context ^ 4294967295) & 4294967295);")
        self.out_lines.append("}")

SAFE_FUNCS = {
    'safe_add_func_int16_t_s_s': 'static int16_t\nsafe_add_func_int16_t_s_s(int16_t si1, int16_t si2 )\n{\n \n  return\n    (si1 + si2);\n}',
    'safe_add_func_int32_t_s_s': 'static int32_t\nsafe_add_func_int32_t_s_s(int32_t si1, int32_t si2 )\n{\n \n  return\n    (((si1>0) && (si2>0) && (si1 > ((2147483647)-si2))) || ((si1<0) && (si2<0) && (si1 < ((-2147483647-1)-si2)))) ?\n    ((si1)) :\n    (si1 + si2);\n}',
    'safe_add_func_uint16_t_u_u': 'static uint16_t\nsafe_add_func_uint16_t_u_u(uint16_t ui1, uint16_t ui2 )\n{\n \n  return ui1 + ui2;\n}',
    'safe_add_func_uint32_t_u_u': 'static uint32_t\nsafe_add_func_uint32_t_u_u(uint32_t ui1, uint32_t ui2 )\n{\n \n  return ui1 + ui2;\n}',
    'safe_div_func_int16_t_s_s': 'static int16_t\nsafe_div_func_int16_t_s_s(int16_t si1, int16_t si2 )\n{\n \n  return\n    ((si2 == 0) || ((si1 == (-32767-1)) && (si2 == (-1)))) ?\n    ((si1)) :\n    (si1 / si2);\n}',
    'safe_div_func_int32_t_s_s': 'static int32_t\nsafe_div_func_int32_t_s_s(int32_t si1, int32_t si2 )\n{\n \n  return\n    ((si2 == 0) || ((si1 == (-2147483647-1)) && (si2 == (-1)))) ?\n    ((si1)) :\n    (si1 / si2);\n}',
    'safe_div_func_uint16_t_u_u': 'static uint16_t\nsafe_div_func_uint16_t_u_u(uint16_t ui1, uint16_t ui2 )\n{\n \n  return\n    (ui2 == 0) ?\n    ((ui1)) :\n    (ui1 / ui2);\n}',
    'safe_div_func_uint32_t_u_u': 'static uint32_t\nsafe_div_func_uint32_t_u_u(uint32_t ui1, uint32_t ui2 )\n{\n \n  return\n    (ui2 == 0) ?\n    ((ui1)) :\n    (ui1 / ui2);\n}',
    'safe_lshift_func_int16_t_s_s': 'static int16_t\nsafe_lshift_func_int16_t_s_s(int16_t left, int right )\n{\n \n  return\n    ((left < 0) || (((int)right) < 0) || (((int)right) >= 32) || (left > ((32767) >> ((int)right)))) ?\n    ((left)) :\n    (left << ((int)right));\n}',
    'safe_lshift_func_int16_t_s_u': 'static int16_t\nsafe_lshift_func_int16_t_s_u(int16_t left, unsigned int right )\n{\n \n  return\n    ((left < 0) || (((unsigned int)right) >= 32) || (left > ((32767) >> ((unsigned int)right)))) ?\n    ((left)) :\n    (left << ((unsigned int)right));\n}',
    'safe_lshift_func_int32_t_s_s': 'static int32_t\nsafe_lshift_func_int32_t_s_s(int32_t left, int right )\n{\n \n  return\n    ((left < 0) || (((int)right) < 0) || (((int)right) >= 32) || (left > ((2147483647) >> ((int)right)))) ?\n    ((left)) :\n    (left << ((int)right));\n}',
    'safe_lshift_func_int32_t_s_u': 'static int32_t\nsafe_lshift_func_int32_t_s_u(int32_t left, unsigned int right )\n{\n \n  return\n    ((left < 0) || (((unsigned int)right) >= 32) || (left > ((2147483647) >> ((unsigned int)right)))) ?\n    ((left)) :\n    (left << ((unsigned int)right));\n}',
    'safe_lshift_func_uint16_t_u_s': 'static uint16_t\nsafe_lshift_func_uint16_t_u_s(uint16_t left, int right )\n{\n \n  return\n    ((((int)right) < 0) || (((int)right) >= 32) || (left > ((65535) >> ((int)right)))) ?\n    ((left)) :\n    (left << ((int)right));\n}',
    'safe_lshift_func_uint16_t_u_u': 'static uint16_t\nsafe_lshift_func_uint16_t_u_u(uint16_t left, unsigned int right )\n{\n \n  return\n    ((((unsigned int)right) >= 32) || (left > ((65535) >> ((unsigned int)right)))) ?\n    ((left)) :\n    (left << ((unsigned int)right));\n}',
    'safe_lshift_func_uint32_t_u_s': 'static uint32_t\nsafe_lshift_func_uint32_t_u_s(uint32_t left, int right )\n{\n \n  return\n    ((((int)right) < 0) || (((int)right) >= 32) || (left > ((4294967295U) >> ((int)right)))) ?\n    ((left)) :\n    (left << ((int)right));\n}',
    'safe_lshift_func_uint32_t_u_u': 'static uint32_t\nsafe_lshift_func_uint32_t_u_u(uint32_t left, unsigned int right )\n{\n \n  return\n    ((((unsigned int)right) >= 32) || (left > ((4294967295U) >> ((unsigned int)right)))) ?\n    ((left)) :\n    (left << ((unsigned int)right));\n}',
    'safe_mod_func_int16_t_s_s': 'static int16_t\nsafe_mod_func_int16_t_s_s(int16_t si1, int16_t si2 )\n{\n \n  return\n    ((si2 == 0) || ((si1 == (-32767-1)) && (si2 == (-1)))) ?\n    ((si1)) :\n    (si1 % si2);\n}',
    'safe_mod_func_int32_t_s_s': 'static int32_t\nsafe_mod_func_int32_t_s_s(int32_t si1, int32_t si2 )\n{\n \n  return\n    ((si2 == 0) || ((si1 == (-2147483647-1)) && (si2 == (-1)))) ?\n    ((si1)) :\n    (si1 % si2);\n}',
    'safe_mod_func_uint16_t_u_u': 'static uint16_t\nsafe_mod_func_uint16_t_u_u(uint16_t ui1, uint16_t ui2 )\n{\n \n  return\n    (ui2 == 0) ?\n    ((ui1)) :\n    (ui1 % ui2);\n}',
    'safe_mod_func_uint32_t_u_u': 'static uint32_t\nsafe_mod_func_uint32_t_u_u(uint32_t ui1, uint32_t ui2 )\n{\n \n  return\n    (ui2 == 0) ?\n    ((ui1)) :\n    (ui1 % ui2);\n}',
    'safe_mul_func_int16_t_s_s': 'static int16_t\nsafe_mul_func_int16_t_s_s(int16_t si1, int16_t si2 )\n{\n \n  return\n    si1 * si2;\n}',
    'safe_mul_func_int32_t_s_s': 'static int32_t\nsafe_mul_func_int32_t_s_s(int32_t si1, int32_t si2 )\n{\n \n  return\n    (((si1 > 0) && (si2 > 0) && (si1 > ((2147483647) / si2))) || ((si1 > 0) && (si2 <= 0) && (si2 < ((-2147483647-1) / si1))) || ((si1 <= 0) && (si2 > 0) && (si1 < ((-2147483647-1) / si2))) || ((si1 <= 0) && (si2 <= 0) && (si1 != 0) && (si2 < ((2147483647) / si1)))) ?\n    ((si1)) :\n    si1 * si2;\n}',
    'safe_mul_func_uint16_t_u_u': 'static uint16_t\nsafe_mul_func_uint16_t_u_u(uint16_t ui1, uint16_t ui2 )\n{\n \n  return ((unsigned int)ui1) * ((unsigned int)ui2);\n}',
    'safe_mul_func_uint32_t_u_u': 'static uint32_t\nsafe_mul_func_uint32_t_u_u(uint32_t ui1, uint32_t ui2 )\n{\n \n  return ((unsigned int)ui1) * ((unsigned int)ui2);\n}',
    'safe_rshift_func_int16_t_s_s': 'static int16_t\nsafe_rshift_func_int16_t_s_s(int16_t left, int right )\n{\n \n  return\n    ((left < 0) || (((int)right) < 0) || (((int)right) >= 32))?\n    ((left)) :\n    (left >> ((int)right));\n}',
    'safe_rshift_func_int16_t_s_u': 'static int16_t\nsafe_rshift_func_int16_t_s_u(int16_t left, unsigned int right )\n{\n \n  return\n    ((left < 0) || (((unsigned int)right) >= 32)) ?\n    ((left)) :\n    (left >> ((unsigned int)right));\n}',
    'safe_rshift_func_int32_t_s_s': 'static int32_t\nsafe_rshift_func_int32_t_s_s(int32_t left, int right )\n{\n \n  return\n    ((left < 0) || (((int)right) < 0) || (((int)right) >= 32))?\n    ((left)) :\n    (left >> ((int)right));\n}',
    'safe_rshift_func_int32_t_s_u': 'static int32_t\nsafe_rshift_func_int32_t_s_u(int32_t left, unsigned int right )\n{\n \n  return\n    ((left < 0) || (((unsigned int)right) >= 32)) ?\n    ((left)) :\n    (left >> ((unsigned int)right));\n}',
    'safe_rshift_func_uint16_t_u_s': 'static uint16_t\nsafe_rshift_func_uint16_t_u_s(uint16_t left, int right )\n{\n \n  return\n    ((((int)right) < 0) || (((int)right) >= 32)) ?\n    ((left)) :\n    (left >> ((int)right));\n}',
    'safe_rshift_func_uint16_t_u_u': 'static uint16_t\nsafe_rshift_func_uint16_t_u_u(uint16_t left, unsigned int right )\n{\n \n  return\n    (((unsigned int)right) >= 32) ?\n    ((left)) :\n    (left >> ((unsigned int)right));\n}',
    'safe_rshift_func_uint32_t_u_s': 'static uint32_t\nsafe_rshift_func_uint32_t_u_s(uint32_t left, int right )\n{\n \n  return\n    ((((int)right) < 0) || (((int)right) >= 32)) ?\n    ((left)) :\n    (left >> ((int)right));\n}',
    'safe_rshift_func_uint32_t_u_u': 'static uint32_t\nsafe_rshift_func_uint32_t_u_u(uint32_t left, unsigned int right )\n{\n \n  return\n    (((unsigned int)right) >= 32) ?\n    ((left)) :\n    (left >> ((unsigned int)right));\n}',
    'safe_sub_func_int16_t_s_s': 'static int16_t\nsafe_sub_func_int16_t_s_s(int16_t si1, int16_t si2 )\n{\n \n  return\n    (si1 - si2);\n}',
    'safe_sub_func_int32_t_s_s': 'static int32_t\nsafe_sub_func_int32_t_s_s(int32_t si1, int32_t si2 )\n{\n \n  return\n    (((si1^si2) & (((si1 ^ ((si1^si2) & (~(2147483647))))-si2)^si2)) < 0) ?\n    ((si1)) :\n    (si1 - si2);\n}',
    'safe_sub_func_uint16_t_u_u': 'static uint16_t\nsafe_sub_func_uint16_t_u_u(uint16_t ui1, uint16_t ui2 )\n{\n \n  return ui1 - ui2;\n}',
    'safe_sub_func_uint32_t_u_u': 'static uint32_t\nsafe_sub_func_uint32_t_u_u(uint32_t ui1, uint32_t ui2 )\n{\n \n  return ui1 - ui2;\n}',
    'safe_unary_minus_func_int16_t_s': 'static int16_t\nsafe_unary_minus_func_int16_t_s(int16_t si )\n{\n \n  return\n    -si;\n}',
    'safe_unary_minus_func_int32_t_s': 'static int32_t\nsafe_unary_minus_func_int32_t_s(int32_t si )\n{\n \n  return\n    (si==(-2147483647-1)) ?\n    ((si)) :\n    -si;\n}',
    'safe_unary_minus_func_uint16_t_u': 'static uint16_t\nsafe_unary_minus_func_uint16_t_u(uint16_t ui )\n{\n \n  return -ui;\n}',
    'safe_unary_minus_func_uint32_t_u': 'static uint32_t\nsafe_unary_minus_func_uint32_t_u(uint32_t ui )\n{\n \n  return -ui;\n}',
}


def header():
    return """fn w32(x) {
    mut t = x & 4294967295;
    if (t >= 2147483648) { (t - 4294967296) } else { t }
}
fn w16(x) {
    mut t = x & 65535;
    if (t >= 32768) { (t - 65536) } else { t }
}
var crc32_context = 4294967295;
fn crc32_tab_at(i) {
    mut crc = i;
    mut j = 8;
    while (j > 0) {
        if ((crc & 1) != 0) {
            crc = (crc >> 1) ^ 3988292384;
        } else {
            crc = crc >> 1;
        }
        j = j - 1;
    }
    crc & 4294967295
}
fn crc32_byte(b) {
    crc32_context = ((crc32_context >> 8) & 16777215) ^ crc32_tab_at((crc32_context ^ b) & 255);
}
fn transparent_crc(val) {
    crc32_byte(val & 255);
    crc32_byte((val >> 8) & 255);
    crc32_byte((val >> 16) & 255);
    crc32_byte((val >> 24) & 255);
}
"""

def main():
    if len(sys.argv) < 3:
        print("用法: c2mira.py <输入.c> <输出.mira>")
        sys.exit(1)
    src = open(sys.argv[1], encoding="utf-8", errors="replace").read()
    src = re.sub(r"^\s*#.*$", "", src, flags=re.M)
    src = re.sub(r"static\s+long\s+__undefined\s*;", "", src)
    # safe-math:csmith 生成的 safe_* 调用需要函数定义;Mira 无 &&/|| 运算符,
    # 由 gen() 把检查表达式展开为短路 if,与 gcc 的 safe 语义一致
    used = set(re.findall(r"\bsafe_[a-z0-9_]+(?=\()", src))
    # 剥离 C 的身份转换 (int)/(unsigned int):Mira 无 cast 语法,32 位内恒等
    def strip_icast(txt):
        return re.sub(r"\(\s*(?:unsigned\s+)?int\s*\)", "", txt)
    inject = [strip_icast(SAFE_FUNCS[n]) for n in sorted(used) if n in SAFE_FUNCS]
    if inject:
        src += "\n\n" + "\n\n".join(inject)
    p = Parser(src)
    out = p.run()
    with open(sys.argv[2], "w", encoding="utf-8") as f:
        f.write(header() + "\n" + out + "\n")

if __name__ == "__main__":
    main()
