#!/usr/bin/env python3
# 递归支持 + 中部声明测试
import subprocess

MIRA = '/mnt/e/mira/mira/linux/mira'

def t(src, tag):
    with open('/tmp/rec.mira', 'w') as f:
        f.write(src)
    p = subprocess.run([MIRA, '-O0', '/tmp/rec.mira'],
                       capture_output=True, text=True, timeout=30)
    err = (p.stderr + p.stdout).replace('\x1b[0m', '').replace('\x1b[31m', '')
    uf = 'underflow' in err
    print('%s: rc=%d underflow=%s' % (tag, p.returncode, uf))
    if p.returncode != 0:
        print('   %s' % err.strip().split('\n')[-1][:120])
    return p.returncode == 0

# r1: 直接递归(带参数, 有终止)
t('''fn f(n) {
    if (n) {
        f(n - 1)
    } else {
        0
    }
}
fn main() {
    print(f(3));
}
''', 'r1 direct recursion')

# r2: 直接递归(无参数, 无限)
t('''fn f() {
    f()
}
fn main() {
    print(f());
}
''', 'r2 self recursion no-param')

# r3: 互相递归
t('''fn a() {
    b()
}
fn b() {
    a()
}
fn main() {
    print(a());
}
''', 'r3 mutual recursion')

# r4: 中部声明 (if/else 之后声明 mut, 声明后使用)
t('''fn main() {
    if (1) {
        print(1);
    } else {
        print(2);
    }
    mut v = 5;
    print(v);
}
''', 'r4 mid-decl after if/else')

# r5: 递归但有限深 + print (与 fuzz 结构接近)
t('''fn f() {
    if (1) {
        f()
    } else {
        0
    }
}
fn main() {
    print(f());
}
''', 'r5 recursion in if-then')

# r6: 回归 - SSA underflow ICE 最小复现
# 零参用户函数在调用点栈非空时被调用(条件表达式左侧值已在栈上),
# 旧启发式错误猜测 1 参数并把栈上值当参数 pop 掉 -> underflow
t('''fn f() {
    7
}
fn main() {
    if ((100 / (f() | 1))) {
        print(1);
    } else {
        print(0);
    }
}
''', 'r6 zero-arg call with nonempty stack (ICE regression)')

# r7: 同上变体 - else 分支调用 + 嵌套
t('''fn g() {
    3
}
fn main() {
    if (1) {
        if ((50 / (g() + 1))) {
            print(2);
        } else {
            print(3);
        }
    } else {
        print(g());
    }
}
''', 'r7 zero-arg call in else/nested if (ICE regression)')
