#!/usr/bin/env python3
# mira2c.py - fuzz 生成的 mira 程序 -> 等价 C(供 gcc 耗时对比 + 语义对拍)
# 规则: fn -> long long 函数; mut -> long long 局部; print -> printf("%lld\n")
#       ret 表达式(缩进4且无分号) -> return; >INT64_MAX 字面量补 ULL
import re

FN = re.compile(r'^fn\s+(\w+)\s*\((.*?)\)\s*\{\s*$')
BIG_NUM = re.compile(r'\b(\d{19,20})\b')

def _fix_literals(s):
    # mira 字面量=位模式(实验: 18446744073709551615 输出 -1), 运算全按有符号。
    # C 里 ULL 字面量会提升表达式为无符号(除法语义不同), 故转成位模式等价的
    # 有符号写法; LLONG_MIN 用惯用式避免被解析为 ULL 取负。
    def rep(m):
        v = int(m.group(1))
        if v == 9223372036854775808:
            return '(-9223372036854775807LL - 1)'
        if v > 9223372036854775807:
            return str(v - 18446744073709551616)
        return m.group(1)
    return BIG_NUM.sub(rep, s)

def mira2c(src):
    lines = src.splitlines()
    # C 要求先声明后使用: 先扫全部函数定义输出原型(mira 允许调用后定义函数)
    protos = []
    for line in lines:
        m = FN.match(line)
        if m and m.group(1) != 'main':
            n = len([p for p in m.group(2).split(',') if p.strip()])
            protos.append('long long %s(%s);' % (m.group(1), ', '.join(['long long'] * n) or 'void'))
    out = ['#include <stdio.h>', ''] + protos + ['']
    cur = None          # [name, params]
    body = []           # 当前函数体原始行
    depth = 0           # 当前函数大括号深度(fn 行起为 1)
    for line in lines:
        m = FN.match(line)
        if m:
            if cur:
                _flush(out, cur, body)
            cur = [m.group(1), m.group(2).strip()]
            body = []
            depth = 1
            continue
        if cur is None:
            continue
        # fuzz 生成的 } 全部顶格, 嵌套块结束也会是顶格 } —— 用深度判定函数结束
        depth += line.count('{') - line.count('}')
        if depth == 0 and line.strip() == '}':
            if cur[0] != 'main':
                _flush(out, cur, body)      # 内部已含收尾 '}'
            else:
                # main 无 ret 表达式, 补 return 0
                out.append('int main(void) {')
                seen = set()
                for b in body:
                    out.append(_conv_stmt(b, seen))
                out.append('    return 0;')
                out.append('}')
            cur = None
        else:
            body.append(line)
    if cur:
        _flush(out, cur, body)
    return '\n'.join(out) + '\n'

def _flush(out, cur, body):
    name, params = cur
    plist = params.split(',') if params else []
    ps = ', '.join('long long ' + p.strip() for p in plist if p.strip())
    out.append('long long %s(%s) {' % (name, ps))
    # ret 行 = 缩进4、无分号、非控制流的最后一行
    ret_i = None
    for i, b in enumerate(body):
        if (b.startswith('    ') and not b.startswith('    if ')
                and not b.startswith('    while ')
                and b.strip() and not b.rstrip().endswith(';')
                and not b.strip().startswith('}')):
            ret_i = i
    seen = set()
    for i, b in enumerate(body):
        if i == ret_i:
            out.append('    return %s;' % _fix_literals(b.strip()))
        else:
            out.append(_conv_stmt(b, seen))
    out.append('}')

def _conv_stmt(b, seen):
    s = b.strip()
    if s.startswith('mut '):
        mm = re.match(r'^mut\s+(\w+)\s*=\s*(.*);$', s)
        var, rhs = mm.group(1), mm.group(2)
        if var in seen:
            s = '%s = %s;' % (var, _fix_literals(rhs))   # 覆盖声明 -> 赋值
        else:
            seen.add(var)
            s = 'long long %s = %s;' % (var, _fix_literals(rhs))
    elif s.startswith('print(') and s.endswith(');'):
        inner = s[6:-2]
        s = 'printf("%lld\\n", (long long)(' + _fix_literals(inner) + '));'
    else:
        s = _fix_literals(s)
    return s

if __name__ == '__main__':
    import sys
    with open(sys.argv[1]) as f:
        print(mira2c(f.read()))
