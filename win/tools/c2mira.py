#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# c2mira.py —— csmith 生成的 C 源码 → Mira 源码翻译器
#
# 用途:三方差分正确性测试(参考 gcc / Windows Mira / Linux Mira 三方输出一致)
# 用法:python3 c2mira.py <输入.c> [输出.mira]
#
# 设计要点(全部基于实测确认的 Mira 语义):
#  1. 值表示:有符号类型 → 64 位符号扩展;无符号类型 → 低 w 位掩码。
#     因此「截断到类型 T」统一为:
#       有符号 T_w(x) = ((x & M_w) ^ H_w) - H_w   (M_w=2^w-1, H_w=2^(w-1))
#       无符号 M_w(x) = x & M_w
#     任何源值截断只看低 w 位,对两种表示都安全。
#  2. Mira 的 return 词有缺陷(前缀式 return expr 的值序颠倒;postfix 触发
#     SSA 死块栈下溢),且函数返回值 = 函数体最后表达式的值。故所有 C return
#     语句翻译成「ret = <值>; done = 1;」,函数体末尾输出 ret;函数内一旦
#     出现 return,其后所有语句(含循环条件)用 done==0 守卫,循环出口加
#     done==1 检查,保证控制流与 C 等价。
#  3. 三目 / && / || / switch 等 C 特有结构:提升为临时变量 + if 语句,
#     求值顺序与短路语义逐条保持。循环条件的临时变量声明提到循环外,
#     continue 按 C 语义重写(跳到增量/条件检查)。
#  4. crc32 用逐位算法(poly 0xEDB88320,无数组——Mira 不支持数组字面量),
#     与 csmith 查表版数学等价;transparent_crc 固定喂 8 字节
#     (官方 csmith.h 的 crc32_8bytes 行为,默认平台无 NO_LONGLONG)。
#  5. 禁用特性(数组/指针/结构体/浮点/复合赋值/嵌入式赋值/自增自减/逗号
#     运算符)由 csmith 命令行选项关闭,翻译器对残余出现直接报错,绝不
#     静默生成错误代码。

import sys
import re
from pycparser import c_parser, c_ast
from pycparser import c_generator

MASK = {8: 255, 16: 65535, 32: 4294967295, 64: 18446744073709551615}
HALF = {8: 128, 16: 32768, 32: 2147483648, 64: 9223372036854775808}


class T:
    """类型信息:w=位宽, s=是否有符号"""
    __slots__ = ('w', 's')

    def __init__(self, w, s):
        self.w = w
        self.s = s

    def trunc(self, c):
        """把任意 64 位表示的值截断到本类型"""
        if self.s:
            return f"((({c}) & {MASK[self.w]}) ^ {HALF[self.w]}) - {HALF[self.w]}"
        return f"(({c}) & {MASK[self.w]})"

    def promote(self):
        """C 整型提升:char/short → int(有符号)"""
        return self if self.w >= 32 else T(32, True)


I32 = T(32, True)
U32 = T(32, False)

# C 类型名(空格连接)→ 类型信息。long 按 32 位处理:NO_LONGLONG 下
# csmith 只用 stdint 类型,裸 long 只出现在 __undefined(从不参与运算)。
TYPES = {
    'char': T(8, True), 'signed char': T(8, True),
    'int8_t': T(8, True), 'int8': T(8, True),
    'unsigned char': T(8, False),
    'uint8_t': T(8, False), 'uint8': T(8, False),
    'short': T(16, True), 'short int': T(16, True), 'signed short': T(16, True),
    'int16_t': T(16, True), 'int16': T(16, True),
    'unsigned short': T(16, False),
    'uint16_t': T(16, False), 'uint16': T(16, False),
    'int': T(32, True), 'signed': T(32, True), 'signed int': T(32, True),
    'long': T(32, True), 'long int': T(32, True), 'signed long': T(32, True),
    'int32_t': T(32, True), 'int32': T(32, True),
    'unsigned': T(32, False), 'unsigned int': T(32, False),
    'unsigned long': T(32, False),
    'uint32_t': T(32, False), 'uint32': T(32, False),
    'long long': T(64, True), 'signed long long': T(64, True),
    'int64_t': T(64, True), 'int64': T(64, True),
    'unsigned long long': T(64, False),
    'uint64_t': T(64, False), 'uint64': T(64, False),
}

# csmith runtime 中由翻译器特判的函数:调用被替换/丢弃,绝不翻译函数体
RUNTIME_DROP = ('crc32_gentab', 'crc32_initialize', 'crc32_finalize',
                'platform_main_begin')


class XError(Exception):
    pass


def indent(lines, n=1):
    pad = '    ' * n
    return [pad + ln for ln in lines]


TYPEDEF_INJECT = (
    'typedef signed char int8_t;\n'
    'typedef unsigned char uint8_t;\n'
    'typedef short int16_t;\n'
    'typedef unsigned short uint16_t;\n'
    'typedef int int32_t;\n'
    'typedef unsigned int uint32_t;\n'
    'typedef long long int64_t;\n'
    'typedef unsigned long long uint64_t;\n'
    'typedef int32_t int32;\n'
    'typedef uint32_t uint32;\n'
    'typedef int64_t int64;\n'
    'typedef uint64_t uint64;\n'
)


def preprocess(src, inject=True):
    """删掉 csmith 的 #include/#define 行与 __undefined 声明。
    注入 TYPEDEF_INJECT 供 pycparser 识别 stdint 类型名(两模式都需要,
    pycparser 2.21 必须先见 typedef 才能解析 int32_t 等标识符);
    inject=True 时额外注入 crc32_context 声明(翻译成 Mira 需要;
    csmith.h 自带它,参考 C 版注入会重复定义)。
    同时剥掉全部 /* */ 注释:本机 pycparser 2.21 的 lexer 注释规则缺失
    (实测 parse('/* x */ int a;') 报 before: /),而 csmith 字符串字面量
    不含注释记号,剥注释无副作用。"""
    src = re.sub(r'/\*.*?\*/', '', src, flags=re.DOTALL)
    lines = []
    for ln in src.splitlines():
        s = ln.strip()
        if s.startswith('#') and any(k in s for k in ('include', 'define', 'pragma')):
            continue
        if 'static long __undefined' in ln:
            continue
        lines.append(ln)
    out = TYPEDEF_INJECT
    if inject:
        # csmith.h 里声明的 crc32_context 随 #include 被删,补回来
        out += 'static uint32_t crc32_context = 0xFFFFFFFFUL;\n'
    return out + '\n'.join(lines)


def parse_int_const(v):
    """'0x8572U' / '1' / '-3' → 整数"""
    v = v.strip()
    neg = v.startswith('-')
    if neg:
        v = v[1:]
    v = re.sub(r'[uUlL]+$', '', v)
    if v.startswith(('0x', '0X')):
        n = int(v, 16)
    elif v.startswith(('0b', '0B')):
        n = int(v, 2)
    elif len(v) > 1 and v.startswith('0'):
        n = int(v, 8)
    else:
        n = int(v, 10)
    return -n if neg else n


class Xlat:
    def __init__(self):
        self.globals = {}      # 全局变量名 → T
        self.funcs = {}        # 函数名 → (返回类型 T|None, [形参 T])
        self.locals = {}       # 当前函数局部符号表(含形参)
        self.tmpc = 0          # 临时变量计数(函数级)
        self.fid = 0           # 函数编号(ret/done 变量名用)
        self.has_ret = False   # 当前函数是否含 return
        self.ret_name = None
        self.done_name = None
        self.cur_ret_t = None
        self.cont_rewrite = [] # 循环上下文栈(continue 重写)

    # ---------- 收集与类型 ----------

    def type_of(self, td):
        if isinstance(td, c_ast.Typename):
            # 无名字类型名(函数参数的 (void)、cast 的类型名)
            return self.type_of(td.type)
        if isinstance(td, c_ast.TypeDecl):
            it = td.type
            if isinstance(it, c_ast.IdentifierType):
                key = ' '.join(it.names)
                if key in TYPES:
                    return TYPES[key]
                if len(it.names) == 1 and it.names[0] in TYPES:
                    return TYPES[it.names[0]]
                raise XError(f'未知类型 {key}')
            if isinstance(it, c_ast.Enum):
                return I32
            raise XError(f'不支持的声明类型 {type(it).__name__}')
        if isinstance(td, c_ast.FuncDecl):
            return self.type_of(td.type) if td.type else None
        if isinstance(td, c_ast.PtrDecl):
            raise XError('指针(已在 csmith 选项禁用)')
        if isinstance(td, c_ast.ArrayDecl):
            raise XError('数组(已在 csmith 选项禁用)')
        raise XError(f'不支持的类型节点 {type(td).__name__}')

    def collect(self, ast):
        for d in ast.ext:
            if isinstance(d, c_ast.FuncDef):
                fd = d.decl.type
                rt = self.type_of(fd.type) if fd.type else None
                params = []
                if fd.args and fd.args.params:
                    for prm in fd.args.params:
                        if isinstance(prm, c_ast.Typename):
                            continue  # (void)
                        if prm.name is None:
                            continue
                        params.append(self.type_of(prm.type))
                self.funcs[d.decl.name] = (rt, params)
            elif isinstance(d, c_ast.Decl) and not isinstance(d.type, c_ast.FuncDecl):
                if d.name == '__undefined':
                    continue
                self.globals[d.name] = self.type_of(d.type)

    def lookup(self, name):
        if name in self.locals:
            return self.locals[name]
        if name in self.globals:
            return self.globals[name]
        raise XError(f'未声明变量 {name}')

    def common_t(self, lt, rt):
        """C usual arithmetic conversions:promote 后取公共类型。
        同有符号性 → 大宽度;异号同宽 → 无符号;异号不同宽 → 大宽度。"""
        lp, rp = lt.promote(), rt.promote()
        if lp.s == rp.s:
            return lp if lp.w >= rp.w else rp
        if lp.w == rp.w:
            return T(lp.w, False)
        return lp if lp.w > rp.w else rp

    def new_tmp(self):
        self.tmpc += 1
        return self.tmpc

    # ---------- 表达式 ----------

    def expr_inner(self, node, pre, target):
        """返回 (mira 代码, 表达式类型)。副作用语句追加到 pre"""
        if isinstance(node, c_ast.Constant):
            if node.type == 'string':
                raise XError('字符串常量只允许出现在 transparent_crc 实参')
            v = parse_int_const(node.value)
            uns = bool(re.search(r'[uU]', node.value))
            if v > 2**64 - 1:
                raise XError(f'字面量溢出 uint64: {node.value}')
            if v > 2**63 - 1:
                # 超有符号 64 位:回绕成补码负数。Mira 无无符号类型,补码
                # 位模式与 C 的 uint64 值相同(掩码/加减乘环绕都一致)。
                # 注意:uint64 高位值的比较/右移在 Mira 是符号语义,遇
                # 到此类参与运算会由差分测试暴露,另行处理。
                return f'{v - 2**64}', T(64, not uns)
            return f'{v}', T(32, not uns)
        if isinstance(node, c_ast.ID):
            return node.name, self.lookup(node.name)
        if isinstance(node, c_ast.UnaryOp):
            op = node.op
            if op in ('p++', '++p', 'p--', '--p', '&', '*'):
                raise XError(f'不支持的运算符 {op}')
            c, t = self.expr_inner(node.expr, pre, None)
            if op == '-':
                pt = t.promote()
                return pt.trunc(f'(0 - ({c}))'), pt
            if op == '~':
                pt = t.promote()
                return pt.trunc(f'(({c}) ^ -1)'), pt
            if op == '!':
                return f'(({c}) == 0)', I32
            if op == '+':
                return c, t.promote()
            raise XError(f'未知一元运算符 {op}')
        if isinstance(node, c_ast.BinaryOp):
            return self.binary(node, pre)
        if isinstance(node, c_ast.TernaryOp):
            return self.ternary(node, pre)
        if isinstance(node, c_ast.FuncCall):
            return self.call(node, pre)
        if isinstance(node, c_ast.Cast):
            c, t = self.expr_inner(node.expr, pre, None)
            dt = self.type_of(node.to_type)
            return dt.trunc(c), dt
        if isinstance(node, c_ast.ExprList):
            raise XError('逗号表达式(已在 csmith 选项禁用)')
        if isinstance(node, c_ast.ArrayRef):
            raise XError('数组下标(已在 csmith 选项禁用)')
        raise XError(f'未知表达式节点 {type(node).__name__}')

    def binary(self, node, pre):
        op = node.op
        if op in ('&&', '||'):
            return self.short_circuit(node, op, pre)
        if op == '=':
            raise XError('嵌入式赋值(已在 csmith 选项禁用)')
        if op in ('+=', '-=', '*=', '/=', '%=', '<<=', '>>=', '&=', '|=', '^='):
            raise XError(f'复合赋值 {op}(已在 csmith 选项禁用)')
        if op == ',':
            raise XError('逗号运算符(已在 csmith 选项禁用)')
        l, lt = self.expr_inner(node.left, pre, None)
        r, rt = self.expr_inner(node.right, pre, None)
        if op in ('==', '!=', '<', '>', '<=', '>='):
            # 比较:usual arithmetic conversion 后按公共类型语义比较
            ct = self.common_t(lt, rt)
            if ct.s:
                return f'(({l}) {op} ({r}))', I32
            # 无符号比较:两操作数掩码后比较(4+3 开,7 闭,必须配对)
            return (f'(((({l}) & {MASK[ct.w]}) {op} '
                    f'((({r}) & {MASK[ct.w]}))))', I32)
        if op in ('+', '-', '*', '/', '%', '&', '|', '^'):
            ct = self.common_t(lt, rt)
            if op in ('/', '%'):
                # 除零保护:除数 d → (d | (d == 0))。d==0 时除 1(商 n、余 0),
                # d!=0 时原值。按位或无溢出(比 d + (d==0) 安全:d 可为 INT_MAX)。
                # safediv/safemod 是 Mira 函数,参数只求值一次,与参考 C 的
                # __mira_* helper 语义完全一致。64 位表示下 Mira 的 / % 对
                # 符号扩展的负数与掩码后的正数都正确(与 C 的有/无符号一致)。
                fname = 'safediv' if op == '/' else 'safemod'
                return ct.trunc(f'{fname}({l}, {r})'), ct
            code = f'(({l}) {op} ({r}))'
            return ct.trunc(code), ct
        if op in ('<<', '>>'):
            # 移位:结果类型 = 左操作数提升后;右操作数只作移位量
            lp = lt.promote()
            if lp.s:
                return lp.trunc(f'(({l}) {op} ({r}))'), lp
            code = f'(((({l}) & {MASK[lp.w]}) {op} ({r})))'
            return lp.trunc(code), lp
        raise XError(f'未知二元运算符 {op}')

    def short_circuit(self, node, op, pre):
        l, lt = self.expr_inner(node.left, pre, None)
        tmp = self.new_tmp()
        pre.append(f'mut t{tmp} = 0;')
        if op == '&&':
            pre.append(f'if (({l}) != 0) {{')
            pr = []
            r, rt = self.expr_inner(node.right, pr, None)
            pre += indent(pr)
            pre.append(f'    t{tmp} = (({r}) != 0);')
            pre.append('}')
        else:
            pre.append(f'if (({l}) == 0) {{')
            pr = []
            r, rt = self.expr_inner(node.right, pr, None)
            pre += indent(pr)
            pre.append(f'    t{tmp} = (({r}) != 0);')
            pre.append('} else {')
            pre.append(f'    t{tmp} = 1;')
            pre.append('}')
        return f't{tmp}', I32

    def ternary(self, node, pre):
        c, ct = self.expr_inner(node.cond, pre, None)
        tmp = self.new_tmp()
        pre.append(f'mut t{tmp} = 0;')
        pa = []
        a, at = self.expr_inner(node.iftrue, pa, None)
        pb = []
        b, bt = self.expr_inner(node.iffalse, pb, None)
        ap, bp = at.promote(), bt.promote()
        common = U32 if not (ap.s and bp.s) else I32
        pre.append(f'if (({c}) != 0) {{')
        pre += indent(pa)
        pre.append(f'    t{tmp} = {common.trunc(a)};')
        pre.append('} else {')
        pre += indent(pb)
        pre.append(f'    t{tmp} = {common.trunc(b)};')
        pre.append('}')
        return f't{tmp}', common

    def call(self, node, pre):
        name = node.name.name
        if name in RUNTIME_DROP:
            return '0', I32
        if name == 'platform_main_end':
            raise XError('platform_main_end 只能在语句位置')
        if name == 'transparent_crc':
            # 官方 csmith.h 的 transparent_crc 固定喂 8 字节
            # (crc32_8bytes,仅 NO_LONGLONG/__SPLAT__ 时 4 字节;默认 x86-64 不定义)
            args = node.args.exprs
            c, t = self.expr_inner(args[0], pre, None)
            return f'crc32-feed-8({c})', I32
        if name not in self.funcs:
            raise XError(f'未知函数 {name}')
        rt, ptypes = self.funcs[name]
        args = node.args.exprs if node.args else []
        cargs = []
        for i, a in enumerate(args):
            pt = ptypes[i] if i < len(ptypes) else I32
            c, t = self.expr_inner(a, pre, None)
            cargs.append(pt.trunc(c))
        return f'{name}({", ".join(cargs)})', (rt if rt else I32)

    # ---------- 语句 ----------

    def trans_stmt(self, it):
        if isinstance(it, c_ast.Decl):
            return self.s_decl(it)
        if isinstance(it, c_ast.Assignment):
            return self.s_assign(it)
        if isinstance(it, c_ast.If):
            return self.s_if(it)
        if isinstance(it, c_ast.While):
            return self.s_while(it)
        if isinstance(it, c_ast.DoWhile):
            return self.s_dowhile(it)
        if isinstance(it, c_ast.For):
            return self.s_for(it)
        if isinstance(it, c_ast.Switch):
            return self.s_switch(it)
        if isinstance(it, c_ast.Break):
            return ['break;']
        if isinstance(it, c_ast.Continue):
            return self.s_continue()
        if isinstance(it, c_ast.Return):
            return self.s_return(it)
        if isinstance(it, c_ast.EmptyStatement):
            return []
        if isinstance(it, c_ast.ExprList):
            lines = []
            for e in it.exprs:
                lines += self.s_expr_stmt(e)
            return lines
        if isinstance(it, c_ast.FuncCall):
            # 裸函数调用语句(func_1();)
            return self.s_expr_stmt(it)
        if isinstance(it, c_ast.Compound):
            return self.trans_block(it.block_items or [], False)
        if isinstance(it, c_ast.Label):
            raise XError('goto/label(不在 csmith 输出内)')
        raise XError(f'未知语句节点 {type(it).__name__}')

    def s_decl(self, node):
        t = self.type_of(node.type)
        name = node.name
        self.locals[name] = t
        if node.init:
            p = []
            c, _ = self.expr_inner(node.init, p, None)
            return p + [f'mut {name} = {t.trunc(c)};']
        return [f'mut {name} = 0;']

    def s_assign(self, node):
        lv = node.lvalue
        if not isinstance(lv, c_ast.ID):
            raise XError('赋值左值必须是变量')
        t = self.lookup(lv.name)
        p = []
        c, _ = self.expr_inner(node.rvalue, p, None)
        return p + [f'{lv.name} = {t.trunc(c)};']

    def s_if(self, node):
        p = []
        c, t = self.expr_inner(node.cond, p, None)
        lines = p
        lines.append(f'if (({c}) != 0) {{')
        if isinstance(node.iftrue, c_ast.Compound):
            lines += indent(self.trans_block(node.iftrue.block_items or [], False))
        else:
            lines += indent(self.trans_stmt(node.iftrue))
        if node.iffalse:
            if isinstance(node.iffalse, c_ast.Compound):
                lines += ['} else {']
                lines += indent(self.trans_block(node.iffalse.block_items or [], False))
            else:
                # else if:翻译成嵌套块里的 if 语句
                lines += ['} else {']
                lines += indent(self.trans_stmt(node.iffalse))
            lines += ['}']
        else:
            lines += ['}']
        return lines

    def loop_cond(self, cond, pre_outer, pre_inner):
        """循环条件:临时变量声明提到循环外,其余语句放循环内"""
        p = []
        c, t = self.expr_inner(cond, p, None)
        for ln in p:
            if ln.startswith('mut t'):
                pre_outer.append(ln)
            else:
                pre_inner.append(ln)
        return c

    def s_while(self, node):
        pre_outer, pre_inner = [], []
        c = self.loop_cond(node.cond, pre_outer, pre_inner)
        lines = pre_outer
        lines += ['while (1) {']
        if self.has_ret:
            lines += [f'    if ({self.done_name} == 1) {{ break; }}']
        lines += indent(pre_inner)
        lines += [f'    if (({c}) == 0) {{ break; }}']
        body = self.trans_loop_body(node.stmt, ('while', pre_inner, c))
        lines += indent(body)
        lines += ['}']
        return lines

    def s_dowhile(self, node):
        pre_outer, pre_inner = [], []
        c = self.loop_cond(node.cond, pre_outer, pre_inner)
        lines = pre_outer
        lines += ['while (1) {']
        body = self.trans_loop_body(node.stmt, ('do', pre_inner, c))
        lines += indent(body)
        if self.has_ret:
            lines += [f'    if ({self.done_name} == 1) {{ break; }}']
        lines += indent(pre_inner)
        lines += [f'    if (({c}) == 0) {{ break; }}']
        lines += ['}']
        return lines

    def s_for(self, node):
        lines = []
        pre_outer, pre_inner = [], []
        if node.init:
            if isinstance(node.init, c_ast.Decl):
                lines += self.s_decl(node.init)
            elif isinstance(node.init, c_ast.Assignment):
                lines += self.s_assign(node.init)
            else:
                raise XError('for 初始化不支持')
        c = None
        if node.cond:
            c = self.loop_cond(node.cond, pre_outer, pre_inner)
        incr_lines = []
        if node.next:
            if isinstance(node.next, c_ast.Assignment):
                il = self.s_assign(node.next)
                decls, rest = self.split_decls(il)
                pre_outer += decls
                incr_lines += rest
            else:
                p = []
                code, t = self.expr_inner(node.next, p, None)
                decls, rest = self.split_decls(p)
                pre_outer += decls
                incr_lines += rest
                if isinstance(node.next, c_ast.FuncCall):
                    incr_lines.append(f'{code};')
        lines += pre_outer
        lines += ['while (1) {']
        if self.has_ret:
            lines += [f'    if ({self.done_name} == 1) {{ break; }}']
        lines += indent(pre_inner)
        if c is not None:
            lines += [f'    if (({c}) == 0) {{ break; }}']
        body = self.trans_loop_body(node.stmt, ('for', incr_lines))
        lines += indent(body)
        lines += indent(incr_lines)
        lines += ['}']
        return lines

    def split_decls(self, lines):
        decls, rest = [], []
        for ln in lines:
            if ln.startswith('mut t'):
                decls.append(ln)
            else:
                rest.append(ln)
        return decls, rest

    def s_switch(self, node):
        p = []
        c, t = self.expr_inner(node.cond, p, None)
        tmp = self.new_tmp()
        mtmp = self.new_tmp()
        lines = p
        lines.append(f'mut t{tmp} = 0;')
        lines.append(f't{tmp} = {c};')
        lines.append(f'mut m{mtmp} = 0;')
        # 收集 case / default(保持源码顺序)。pycparser 2.21 的 Case/Default
        # 节点自带 stmts 列表(语句不进 switch 的 block_items),直接取用;
        # else 分支兼容旧版 pycparser(裸语句散落在 block_items)。
        items = node.stmt.block_items or []
        cases = []
        cur = None
        for it in items:
            if isinstance(it, c_ast.Case):
                if cur is not None:
                    cases.append(cur)
                cur = (it.expr, list(it.stmts or []))
            elif isinstance(it, c_ast.Default):
                if cur is not None:
                    cases.append(cur)
                cur = (None, list(it.stmts or []))
            else:
                if cur is None:
                    cur = (None, [])
                cur[1].append(it)
        if cur is not None:
            cases.append(cur)
        # fallthrough 合并:无终止语句的 case 体追加下一个 case 的体
        for i in range(len(cases) - 2, -1, -1):
            if not self.terminated(cases[i][1]):
                cases[i] = (cases[i][0], cases[i][1] + cases[i + 1][1])
        lines += ['while (1) {']
        if self.has_ret:
            lines += [f'    if ({self.done_name} == 1) {{ break; }}']
        for val, stmts in cases:
            if val is not None:
                vcode, _ = self.expr_inner(val, [], None)
                body = self.trans_block(stmts, False)
                lines += [f'    if (t{tmp} == {vcode}) {{']
                lines += [f'        m{mtmp} = 1;']
                lines += indent(body, 2)
                lines += ['    }']
        # default 兜底(放在所有 case 检查之后)
        for val, stmts in cases:
            if val is None:
                body = self.trans_block(stmts, False)
                lines += [f'    if (m{mtmp} == 0) {{']
                lines += indent(body, 2)
                lines += ['    }']
        lines += ['    break;']
        lines += ['}']
        return lines

    def terminated(self, stmts):
        if not stmts:
            return False
        return isinstance(stmts[-1], (c_ast.Break, c_ast.Return, c_ast.Continue))

    def trans_loop_body(self, stmt, mode):
        self.cont_rewrite.append(mode)
        if isinstance(stmt, c_ast.Compound):
            lines = self.trans_block(stmt.block_items or [], False)
        else:
            lines = self.trans_stmt(stmt)
        self.cont_rewrite.pop()
        return lines

    def s_continue(self):
        if not self.cont_rewrite:
            raise XError('continue 不在循环内')
        mode = self.cont_rewrite[-1]
        kind = mode[0]
        if kind == 'for':
            return list(mode[1])  # 增量语句,循环顶自然检查条件
        if kind in ('while', 'do'):
            lines = []
            if self.has_ret:
                lines.append(f'if ({self.done_name} == 1) {{ break; }}')
            lines += list(mode[1])
            lines.append(f'if (({mode[2]}) == 0) {{ break; }}')
            return lines
        raise XError('switch 内 continue(罕见,未支持)')

    def s_return(self, node):
        lines = []
        if node.expr:
            p = []
            c, t = self.expr_inner(node.expr, p, None)
            lines += p
            if self.cur_ret_t is not None:
                lines.append(f'{self.ret_name} = {self.cur_ret_t.trunc(c)};')
        lines.append(f'{self.done_name} = 1;')
        return lines

    def s_expr_stmt(self, e):
        if isinstance(e, c_ast.FuncCall):
            name = e.name.name
            if name == 'transparent_crc':
                # 固定 8 字节,与官方 csmith.h 一致(见 call())
                p = []
                a0, t = self.expr_inner(e.args.exprs[0], p, None)
                return p + [f'crc32-feed-8({a0});']
            if name == 'platform_main_end':
                p = []
                a0, t = self.expr_inner(e.args.exprs[0], p, None)
                return p + [f'print({a0});']
            if name in RUNTIME_DROP:
                return []
        p = []
        code, t = self.expr_inner(e, p, None)
        if isinstance(e, c_ast.FuncCall):
            return p + [f'{code};']
        return p  # 纯表达式语句:只保留副作用(pre),值丢弃

    # ---------- 函数与输出 ----------

    def any_return(self, items):
        for it in items:
            if self.contains_return(it):
                return True
        return False

    def contains_return(self, node):
        if isinstance(node, c_ast.Return):
            return True
        for name, child in node.children():
            if isinstance(child, c_ast.Node) and self.contains_return(child):
                return True
        return False

    def trans_block(self, items, guard):
        lines = []
        for it in items:
            sub = self.trans_stmt(it)
            if guard and sub:
                sub = [f'if ({self.done_name} == 0) {{'] + indent(sub) + ['}']
            lines += sub
            if self.contains_return(it):
                guard = True
        return lines

    def translate_globals(self, ast):
        lines = []
        for d in ast.ext:
            if isinstance(d, c_ast.Decl) and not isinstance(d.type, c_ast.FuncDecl):
                if d.name == '__undefined':
                    continue
                t = self.type_of(d.type)
                if d.init:
                    p = []
                    c, _ = self.expr_inner(d.init, p, None)
                    if p:
                        raise XError(f'全局初始化 {d.name} 含副作用语句')
                    lines.append(f'var {d.name} = {t.trunc(c)};')
                else:
                    lines.append(f'var {d.name} = 0;')
        return lines

    def translate_func(self, fd):
        name = fd.decl.name
        ret_t, ptypes = self.funcs[name]
        params = []
        if fd.decl.type.args and fd.decl.type.args.params:
            for prm in fd.decl.type.args.params:
                if isinstance(prm, c_ast.Typename):
                    continue
                if prm.name:
                    params.append(prm.name)
        self.locals = {}
        for pname, pt in zip(params, ptypes):
            self.locals[pname] = pt
        self.tmpc = 0
        self.fid += 1
        self.has_ret = self.any_return(fd.body.block_items or [])
        self.ret_name = f'r{self.fid}'
        self.done_name = f'd{self.fid}'
        self.cur_ret_t = ret_t
        lines = [f'fn {name}({", ".join(params)}) {{']
        if self.has_ret:
            lines += [f'    mut {self.ret_name} = 0;',
                      f'    mut {self.done_name} = 0;']
        body = self.trans_block(fd.body.block_items or [], False)
        lines += indent(body)
        if self.has_ret:
            lines += [f'    {self.ret_name}']
        elif ret_t is None:
            lines += ['    0']
        lines += ['}']
        return lines

    def emit_crc32_runtime(self):
        # 逐位 CRC-32(poly 0xEDB88320 = 3988292384,初始 0xFFFFFFFF),
        # 与 csmith 查表版数学等价;transparent_crc 按类型宽度低位优先喂字节
        return [
            'fn crc32-feed-1(v) {',
            '    crc32_context = crc32_context ^ (v & 255);',
            '    mut i = 0;',
            '    while (i < 8) {',
            '        if (crc32_context & 1) {',
            '            crc32_context = (crc32_context >> 1) ^ 3988292384;',
            '        } else {',
            '            crc32_context = crc32_context >> 1;',
            '        }',
            '        i = i + 1;',
            '    }',
            '    0',
            '}',
            'fn crc32-feed-2(v) {',
            '    crc32-feed-1(v);',
            '    crc32-feed-1(v >> 8);',
            '    0',
            '}',
            'fn crc32-feed-4(v) {',
            '    crc32-feed-1(v);',
            '    crc32-feed-1(v >> 8);',
            '    crc32-feed-1(v >> 16);',
            '    crc32-feed-1(v >> 24);',
            '    0',
            '}',
            'fn crc32-feed-8(v) {',
            '    crc32-feed-4(v);',
            '    crc32-feed-4(v >> 32);',
            '    0',
            '}',
        ]

    def emit_safe_divmod(self):
        # 除零安全除法/取模:除数 d 非零化 (d | (d == 0))。d==0 时除 1,
        # 商 = n、余 = 0;d!=0 时原值。与参考 C 的 __mira_* helper 语义一致。
        # 无符号操作数调用前已掩码为正(64 位正表示),有符号负数符号扩展,
        # Mira 的 64 位 / % 对两者都给出与 C 一致的结果。
        return [
            'fn safediv(n, d) {',
            '    n / (d | (d == 0))',
            '}',
            'fn safemod(n, d) {',
            '    n % (d | (d == 0))',
            '}',
        ]

    def run(self, ast):
        self.collect(ast)
        lines = ['# 由 c2mira.py 翻译自 csmith 输出(三方差分正确性测试)']
        lines += self.translate_globals(ast)
        lines += self.emit_safe_divmod()
        lines += self.emit_crc32_runtime()
        for d in ast.ext:
            if isinstance(d, c_ast.FuncDef):
                lines += self.translate_func(d)
        return '\n'.join(lines) + '\n'


# 参考 C 版的除零安全 helper。32 位有符号的 INT_MIN %/÷ -1 是 C 的 UB
# (x86 idiv 溢出 SIGFPE),而 Mira 侧 64 位除法不溢出,故 32 位版必须特判
# 让两边一致;64 位版两边都是 64 位除法,溢出行为一致,不需要特判。
REF_HELPERS = '''static int __mira_sdiv32(int n, int d) { int dd = d | (d == 0); if (dd == -1) return (int)(0u - (unsigned)n); return n / dd; }
static int __mira_smod32(int n, int d) { int dd = d | (d == 0); if (dd == -1) return 0; return n % dd; }
static unsigned __mira_udiv32(unsigned n, unsigned d) { return n / (d | (d == 0)); }
static unsigned __mira_umod32(unsigned n, unsigned d) { return n % (d | (d == 0)); }
static long long __mira_sdiv64(long long n, long long d) { return n / (d | (d == 0)); }
static long long __mira_smod64(long long n, long long d) { return n % (d | (d == 0)); }
static unsigned long long __mira_udiv64(unsigned long long n, unsigned long long d) { return n / (d | (d == 0)); }
static unsigned long long __mira_umod64(unsigned long long n, unsigned long long d) { return n % (d | (d == 0)); }
'''


class RefC:
    """从同一 AST 生成 gcc 参考版:除/取模改写为 __mira_* 安全函数调用,
    除零语义与 Mira 侧 safediv/safemod 完全一致;其余结构原样保留,
    由 pycparser 的 CGenerator 输出(编译运行前 prepend csmith.h)。"""

    def __init__(self, xlat):
        self.xlat = xlat      # 复用其 funcs/globals 类型表
        self.locals = {}      # 当前函数局部符号表(遍历中维护)

    def lookup(self, name):
        if name in self.locals:
            return self.locals[name]
        if name in self.xlat.globals:
            return self.xlat.globals[name]
        if name in self.xlat.funcs:
            return self.xlat.funcs[name][0]
        raise XError(f'未声明变量 {name}')

    def infer(self, node):
        """推断表达式类型(用于 div/mod 的 usual conversion 分派)"""
        if isinstance(node, c_ast.Constant):
            v = parse_int_const(node.value)
            uns = bool(re.search(r'[uU]', node.value))
            if v > 2**63 - 1 and not uns:
                return T(64, True)
            if v > 2**31 - 1:
                # 无 U 后缀大值:十六进制按 unsigned(long long)语境处理
                return T(64, uns or v > 2**32 - 1)
            return T(32, not uns)
        if isinstance(node, c_ast.ID):
            return self.lookup(node.name)
        if isinstance(node, c_ast.UnaryOp):
            if node.op in ('-', '~'):
                return self.infer(node.expr).promote()
            return I32
        if isinstance(node, c_ast.BinaryOp):
            if node.op in ('==', '!=', '<', '>', '<=', '>=', '&&', '||'):
                return I32
            return self.common_t(self.infer(node.left), self.infer(node.right))
        if isinstance(node, c_ast.TernaryOp):
            return self.common_t(self.infer(node.iftrue), self.infer(node.iffalse))
        if isinstance(node, c_ast.Cast):
            return self.xlat.type_of(node.to_type)
        if isinstance(node, c_ast.FuncCall):
            name = node.name.name
            if name in self.xlat.funcs:
                return self.xlat.funcs[name][0]
            if name in ('transparent_crc', 'platform_main_end',
                        'platform_main_begin', 'crc32_gentab'):
                return I32
            raise XError(f'未知函数 {name}')
        if isinstance(node, c_ast.ExprList):
            return self.infer(node.exprs[-1])
        raise XError(f'类型推断失败 {type(node).__name__}')

    def common_t(self, lt, rt):
        lp, rp = lt.promote(), rt.promote()
        if lp.s == rp.s:
            return lp if lp.w >= rp.w else rp
        if lp.w == rp.w:
            return T(lp.w, False)
        return lp if lp.w > rp.w else rp

    def fix_expr(self, node):
        if isinstance(node, c_ast.BinaryOp):
            if node.op in ('/', '%'):
                l = self.fix_expr(node.left)
                r = self.fix_expr(node.right)
                ct = self.common_t(self.infer(node.left), self.infer(node.right))
                sig = 'u' if not ct.s else 's'
                w = ct.w if ct.w in (32, 64) else 32
                fname = f'__mira_{sig}{"div" if node.op == "/" else "mod"}{w}'
                return c_ast.FuncCall(c_ast.ID(fname),
                                      c_ast.ExprList([l, r]))
            return c_ast.BinaryOp(node.op, self.fix_expr(node.left),
                                  self.fix_expr(node.right))
        if isinstance(node, c_ast.UnaryOp):
            return c_ast.UnaryOp(node.op, self.fix_expr(node.expr))
        if isinstance(node, c_ast.TernaryOp):
            return c_ast.TernaryOp(self.fix_expr(node.cond),
                                   self.fix_expr(node.iftrue),
                                   self.fix_expr(node.iffalse))
        if isinstance(node, c_ast.Cast):
            return c_ast.Cast(node.to_type, self.fix_expr(node.expr))
        if isinstance(node, c_ast.FuncCall):
            if node.args:
                node.args.exprs = [self.fix_expr(a) for a in node.args.exprs]
            return node
        if isinstance(node, c_ast.ExprList):
            node.exprs = [self.fix_expr(e) for e in node.exprs]
            return node
        return node  # ID/Constant/ArrayRef/StructRef 原样

    def fix_stmt(self, stmt):
        if isinstance(stmt, c_ast.Compound):
            stmt.block_items = [self.fix_stmt(s)
                                for s in (stmt.block_items or [])]
            return stmt
        if isinstance(stmt, c_ast.Decl):
            if (stmt.name and stmt.type
                    and not isinstance(stmt.type, c_ast.FuncDecl)):
                t = self.xlat.type_of(stmt.type)
                if t is not None:
                    self.locals[stmt.name] = t
            if stmt.init:
                stmt.init = self.fix_expr(stmt.init)
            return stmt
        if isinstance(stmt, c_ast.Assignment):
            stmt.lvalue = self.fix_expr(stmt.lvalue)
            stmt.rvalue = self.fix_expr(stmt.rvalue)
            return stmt
        if isinstance(stmt, c_ast.If):
            stmt.cond = self.fix_expr(stmt.cond)
            stmt.iftrue = self.fix_stmt(stmt.iftrue)
            if stmt.iffalse:
                stmt.iffalse = self.fix_stmt(stmt.iffalse)
            return stmt
        if isinstance(stmt, c_ast.While):
            stmt.cond = self.fix_expr(stmt.cond)
            stmt.stmt = self.fix_stmt(stmt.stmt)
            return stmt
        if isinstance(stmt, c_ast.DoWhile):
            stmt.cond = self.fix_expr(stmt.cond)
            stmt.stmt = self.fix_stmt(stmt.stmt)
            return stmt
        if isinstance(stmt, c_ast.For):
            if stmt.init and isinstance(stmt.init, c_ast.Decl):
                stmt.init = self.fix_stmt(stmt.init)
            elif stmt.init:
                stmt.init = self.fix_expr(stmt.init)
            if stmt.cond:
                stmt.cond = self.fix_expr(stmt.cond)
            if stmt.next:
                stmt.next = self.fix_expr(stmt.next)
            stmt.stmt = self.fix_stmt(stmt.stmt)
            return stmt
        if isinstance(stmt, c_ast.Return):
            if stmt.expr:
                stmt.expr = self.fix_expr(stmt.expr)
            return stmt
        if isinstance(stmt, c_ast.Switch):
            stmt.cond = self.fix_expr(stmt.cond)
            if stmt.stmt.block_items:
                for i, c in enumerate(stmt.stmt.block_items):
                    stmt.stmt.block_items[i] = self.fix_stmt(c)
            return stmt
        if isinstance(stmt, c_ast.Case):
            if stmt.expr:
                stmt.expr = self.fix_expr(stmt.expr)
            if stmt.stmts:
                stmt.stmts = [self.fix_stmt(s) for s in stmt.stmts]
            return stmt
        if isinstance(stmt, c_ast.Default):
            if stmt.stmts:
                stmt.stmts = [self.fix_stmt(s) for s in stmt.stmts]
            return stmt
        if isinstance(stmt, c_ast.FuncCall):
            return self.fix_expr(stmt)
        if isinstance(stmt, c_ast.ExprList):
            stmt.exprs = [self.fix_expr(e) for e in stmt.exprs]
            return stmt
        return stmt  # Break/Continue/EmptyStatement/Label 原样


def main():
    if len(sys.argv) < 2:
        print('用法: python3 c2mira.py <输入.c> [输出.mira] | --ref [输出.c]',
              file=sys.stderr)
        sys.exit(2)
    src = open(sys.argv[1], encoding='utf-8', errors='replace').read()
    if len(sys.argv) > 2 and sys.argv[2] == '--ref':
        # 参考 C 版:同一 AST 改写除/取模后由 CGenerator 输出,gcc 编译运行
        text = preprocess(src, inject=False)
        ast_ref = c_parser.CParser().parse(text, filename='<csmith>')
        xlat = Xlat()
        xlat.collect(ast_ref)
        refc = RefC(xlat)
        for d in ast_ref.ext:
            if isinstance(d, c_ast.FuncDef):
                d.body.block_items = [refc.fix_stmt(s)
                                      for s in (d.body.block_items or [])]
        # 去掉注入的 typedef(csmith.h 链带 stdint.h,重复定义会冲突)
        ast_ref.ext = [d for d in ast_ref.ext
                       if not isinstance(d, c_ast.Typedef)]
        gen = c_generator.CGenerator()
        out = ('#include "csmith.h"\n\n' + REF_HELPERS + '\n'
               + gen.visit(ast_ref))
        dst = sys.argv[3] if len(sys.argv) > 3 else None
        if dst:
            open(dst, 'w', encoding='utf-8').write(out)
        else:
            sys.stdout.write(out)
        sys.exit(0)
    text = preprocess(src)
    ast = c_parser.CParser().parse(text, filename='<csmith>')
    out = Xlat().run(ast)
    if len(sys.argv) > 2:
        open(sys.argv[2], 'w', encoding='utf-8').write(out)
    else:
        sys.stdout.write(out)


if __name__ == '__main__':
    try:
        main()
    except XError as e:
        print(f'c2mira 错误: {e}', file=sys.stderr)
        sys.exit(1)
