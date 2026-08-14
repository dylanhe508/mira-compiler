#!/usr/bin/env python3
# fuzz_mira.py - mira 编译器模糊差分器
# 差分策略: 同一随机程序, O0/O1/O2/O3 编译产物输出必须完全一致
#   (覆盖项目核心规则"O0-O3 语义一致", 发现优化 pass 的语义破坏 bug)
# 发现不一致/部分崩溃 -> 保存用例到 fail_<seed>_<idx>.mira 并报告
#
# 用法:
#   python3 fuzz_mira.py [--iters N] [--seed S] [--mira PATH] [--keep]
#     --iters 生成程序数(默认 200)
#     --seed  随机种子(默认 0xMIRA; 可复现)
#     --mira  编译器路径(默认 ../linux/mira)
#     --keep  保留全部用例到 work 目录(默认只留失败用例)

import argparse
import os
import random
import re
import shutil
import subprocess
import sys
import tempfile
import time

# ---------------- 生成器 ----------------
# 表达式: 二目 + - * / % & | ^ << >>, 一元 -, 字面量, 变量, 函数调用
# 语句: mut 声明/赋值/if-else/while(计数器模式, 保证终止)/print

class Gen:
    def __init__(self, rng):
        self.rng = rng
        self.readable = []          # 可读: 参数 + mut 变量(mira 函数级作用域)
        self.writable = []          # 可写: 仅 mut 变量(参数只读)
        self.counters = set()       # 活跃 while 计数器, 禁止赋值覆盖(防死循环)
        self.funcs = []             # (名字, 参数个数)
        self.call_stack = set()     # 生成中的调用链, 防递归
        self.whiles_left = 1        # 每函数最多 1 个 while
        self.depth = 0
        self.max_depth = 3
        self.top = True             # 是否在必然执行位置(函数顶层)
        self.bd = 0                 # 块嵌套深度(防无限 if/while 递归)
        self.max_bd = 4

    def literal(self):
        r = self.rng
        c = r.random()
        if c < 0.5:
            return str(r.randint(-2 ** 40, 2 ** 40))
        if c < 0.8:
            return str(r.randint(-(2 ** 63) + 1, 2 ** 63 - 1))
        return str(r.getrandbits(64))          # 大无符号字面量(bench 同款)

    def expr(self):
        r = self.rng
        if self.depth >= self.max_depth or r.random() < 0.35:
            self.depth += 1
            e = self.leaf()
            self.depth -= 1
            return e
        self.depth += 1
        op = r.choice(['+', '-', '*', '/', '%', '&', '|', '^', '<<', '>>'])
        a = self.expr()
        if op in ('/', '%'):
            b = '(%s | 1)' % self.expr()       # 除零保险: 或 1 非零
        elif op in ('<<', '>>'):
            b = str(r.randint(0, 55))          # 移位量 0..55, 避免语义边界
        else:
            b = self.expr()
        self.depth -= 1
        e = '(%s %s %s)' % (a, op, b)
        if r.random() < 0.15:
            e = '(-%s)' % e
        return e

    def leaf(self):
        r = self.rng
        c = r.random()
        if c < 0.45:
            return self.literal()
        if c < 0.8 and self.readable:
            return self.rng.choice(self.readable)
        if c < 0.95 and self.funcs and self.depth < self.max_depth:
            cand = [f for f in self.funcs if f[0] not in self.call_stack]
            if cand:
                name, nargs = self.rng.choice(cand)
                args = [self.expr() for _ in range(nargs)]
                return '%s(%s)' % (name, ', '.join(args))
        return '(%s)' % self.literal()

    def stmt(self, indent, func_name=None):
        r = self.rng
        pad = '    ' * indent
        c = r.random()
        if c < 0.30 and self.top:
            # mut 声明只允许在必然执行位置(函数顶层):
            # mira 函数级作用域允许跨块引用, 但条件块内声明+块外使用
            # 会在分支未走时读取未初始化变量(UB), O 级别间行为不同
            v = 'v%d' % r.randint(0, 9999)
            e = self.expr()                      # 先求值, 再入池(防自引用)
            self.readable.append(v)
            self.writable.append(v)
            return '%smut %s = %s;\n' % (pad, v, e)
        if c < 0.45 and self.writable:
            cand = [v for v in self.writable if v not in self.counters]
            if cand:
                v = r.choice(cand)
                return '%s%s = %s;\n' % (pad, v, self.expr())
        if c < 0.60 and self.bd < self.max_bd:
            cond = self.expr()
            saved_top = self.top
            saved_bd = self.bd
            self.top = False
            self.bd += 1
            body = self.block(indent + 1)
            if r.random() < 0.5:
                els = self.block(indent + 1)
                self.top = saved_top
                self.bd = saved_bd
                return '%sif (%s) {\n%s} else {\n%s}\n' % (pad, cond, body, els)
            self.top = saved_top
            self.bd = saved_bd
            return '%sif (%s) {\n%s}\n' % (pad, cond, body)
        if c < 0.75 and self.whiles_left > 0 and self.bd < self.max_bd and self.top:
            self.whiles_left -= 1
            # while 计数器模式: 保证终止; 计数器声明同 mut, 只能在顶层
            # (否则块外使用会读到未初始化值 -> UB/ICE)
            v = 'w%d' % r.randint(0, 9999)
            n = r.randint(1, 16)
            self.readable.append(v)
            self.writable.append(v)
            self.counters.add(v)
            saved_top = self.top
            saved_bd = self.bd
            self.top = False
            self.bd += 1
            body = '%s%s = %s + 1;\n' % (pad + '    ', v, v)
            body += self.block(indent + 1)
            self.counters.discard(v)
            self.top = saved_top
            self.bd = saved_bd
            return ('%smut %s = 0;\n%swhile (%s < %d) {\n%s}\n'
                    % (pad, v, pad, v, n, body))
        return '%sprint(%s);\n' % (pad, self.expr())

    def block(self, indent):
        n = self.rng.randint(1, 4)
        return ''.join(self.stmt(indent) for _ in range(n))

    def function(self):
        r = self.rng
        name = 'f%d' % r.randint(0, 9999)
        nargs = r.randint(0, 3)
        params = ['p%d' % i for i in range(nargs)]
        saved_pool = list(self.var_pool)
        saved_wl = self.whiles_left
        self.var_pool = list(params)
        self.whiles_left = 1
        self.call_stack.add(name)
        body = self.block(1)
        ret = self.expr()
        self.call_stack.discard(name)
        self.var_pool = saved_pool
        self.whiles_left = saved_wl
        src = 'fn %s(%s) {\n%s    %s\n}\n' % (
            name, ', '.join(params), body, ret)
        self.funcs.append((name, nargs))
        return src

    def program(self):
        r = self.rng
        parts = []
        # 两遍法: 先登记全部函数签名, 再生成函数体(无前向引用/递归)
        nfuncs = r.randint(1, 4)
        sigs = []
        for _ in range(nfuncs):
            name = 'f%d' % r.randint(0, 9999)
            nargs = r.randint(0, 3)
            self.funcs.append((name, nargs))
            sigs.append((name, ['p%d' % i for i in range(nargs)]))
        for name, params in sigs:
            saved_r = list(self.readable)
            saved_w = list(self.writable)
            saved_wl = self.whiles_left
            self.readable = list(params)         # 参数只读: 仅进 readable
            self.writable = []
            self.whiles_left = 1
            self.call_stack.add(name)
            body = self.block(1)
            ret = self.expr()
            self.call_stack.discard(name)
            self.readable = saved_r
            self.writable = saved_w
            self.whiles_left = saved_wl
            parts.append('fn %s(%s) {\n%s    %s\n}\n' % (
                name, ', '.join(params), body, ret))
        main = ['fn main() {\n']
        self.readable = []
        self.writable = []
        self.whiles_left = 2
        main.append(self.block(1))
        for _ in range(r.randint(1, 4)):
            main.append('    print(%s);\n' % self.expr())
        main.append('}\n')
        parts.append(''.join(main))
        return ''.join(parts)

# ---------------- 差分执行 ----------------

def has_call_cycle(src):
    """调用图环检测(防互递归)。

    call_stack 防递归只拦直接自递归: 两遍法先登记全部函数签名再逐个
    生成函数体, 生成 fA 时 fB 尚未入栈, 生成 fB 时 fA 已出栈,
    于是 fA <-> fB 互调用可成环 -> 运行期无限递归栈溢出(SIGSEGV)。
    环上调用若被条件分支挡住则不触发(程序正常), 故只拦"必有环"。
    """
    cur, g = None, {}
    for line in src.splitlines():
        m = re.match(r'\s*fn\s+(f\d+)\s*\(', line)
        if m:
            cur = m.group(1)
            g.setdefault(cur, set())
        elif cur:
            for c in re.finditer(r'\b(f\d+)\s*\(', line):
                g[cur].add(c.group(1))
    WHITE, GRAY, BLACK = 0, 1, 2
    color = {n: WHITE for n in g}
    def dfs(n):
        color[n] = GRAY
        for m in g.get(n, ()):
            if color[m] == GRAY:
                return True
            if color[m] == WHITE and dfs(m):
                return True
        color[n] = BLACK
        return False
    return any(dfs(n) for n in g if color[n] == WHITE)

def run(cmd, timeout=10, cwd=None):
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout,
                           cwd=cwd)
        return p.returncode, p.stdout, p.stderr
    except subprocess.TimeoutExpired:
        return -1, '', 'TIMEOUT'
    except OSError as e:
        return -2, '', 'OSERROR: %s' % e

def main():
    ap = argparse.ArgumentParser(description='mira 编译器 O0-O3 模糊差分器')
    ap.add_argument('--iters', type=int, default=200)
    ap.add_argument('--seed', type=int, default=0x4D495241)  # "MIRA"
    ap.add_argument('--mira', default='../linux/mira')
    ap.add_argument('--keep', action='store_true')
    args = ap.parse_args()

    work = 'fuzz_work'
    os.makedirs(work, exist_ok=True)
    rng = random.Random(args.seed)
    args.mira = os.path.abspath(args.mira)
    print('seed=%d (0x%X)' % (args.seed, args.seed))

    passed = failed = crashed = compile_fail = ices = 0
    t0 = time.time()
    for idx in range(args.iters):
        # 互递归防护: 生成器防递归只拦直接自递归, 两遍法下函数可互调成环
        # (fA <-> fB) -> 运行期无限递归栈溢出。生成后检测调用图, 有环则
        # 换新 Gen 重试(继续消耗随机序列, 保持用例多样性)。
        while True:
            src = Gen(rng).program()
            if not has_call_cycle(src):
                break
        case = 'fuzz_%05d' % idx
        d = os.path.abspath(os.path.join(work, case))
        os.makedirs(d, exist_ok=True)
        with open(os.path.join(d, 'case.mira'), 'w') as f:
            f.write(src)

        outs = {}
        for opt in (0, 1, 2, 3):
            shutil.rmtree(os.path.join(d, 'o%d' % opt), ignore_errors=True)
            od = os.path.join(d, 'o%d' % opt)
            os.makedirs(od)
            rc, so, se = run([args.mira, '-O%d' % opt,
                              os.path.join(d, 'case.mira')],
                             timeout=30, cwd=od)
            if rc != 0:
                # 区分 ICE(编译器内部崩溃)与普通编译失败(error: ...)
                if se.strip() and 'error' not in se.split('\n')[0][:80].lower():
                    ices += 1
                    failed += 1
                    print('[%s] O%d ICE: %s' % (case, opt, se.strip()[:140]))
                else:
                    compile_fail += 1
                    failed += 1
                    print('[%s] O%d 编译失败: %s' % (case, opt, se.strip()[:120]))
                break
            executable = (os.path.join(od, 'case.exe')
                          if os.name == 'nt' else './case')
            rc, so, se = run([executable], timeout=15, cwd=od)
            outs[opt] = (rc, so)
        else:
            sigs = {outs[o][0] for o in outs}    # rc 集合(必须 set, dict 恒为 4)
            outs_set = {outs[o][1] for o in outs}
            if len(sigs) != 1:
                crashed += 1
                failed += 1
                print('[%s] 崩溃差异: %s' % (case, {o: outs[o][0] for o in outs}))
            elif any(s != 0 for s in sigs):
                # 四级同 rc 且非 0: 全部崩溃(如无限递归栈溢出)也应判失败,
                # 否则输出恰好一致会被误判 pass
                crashed += 1
                failed += 1
                print('[%s] 全级别崩溃: rc=%s' %
                      (case, {o: outs[o][0] for o in outs}))
            elif len(outs_set) > 1:
                failed += 1
                print('[%s] 输出不一致!' % case)
                base = outs[0][1]
                for o in (1, 2, 3):
                    if outs[o][1] != base:
                        print('    O%d != O0: rc=%d out=%r' %
                              (o, outs[o][0], outs[o][1][:100]))
            else:
                passed += 1
                if not args.keep:
                    shutil.rmtree(d)

        if (idx + 1) % 50 == 0:
            print('... %d/%d  %ds  pass=%d fail=%d' % (
                idx + 1, args.iters, int(time.time() - t0), passed, failed))

    print('---- 完成: %d 程序, %d pass / %d fail'
          % (args.iters, passed, failed))
    print('    细分: %d ICE / %d 编译失败 / %d 崩溃差异 / %d 输出不一致, %.1fs'
          % (ices, compile_fail, crashed, failed - ices - compile_fail - crashed,
             time.time() - t0))
    if failed:
        print('失败用例保留在 %s/fuzz_* (最小化后可提交报告)' % work)
    return 1 if failed else 0

if __name__ == '__main__':
    sys.exit(main())
