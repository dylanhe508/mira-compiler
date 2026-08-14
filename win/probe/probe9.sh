#!/bin/bash
cd /tmp/mt/mira
echo "===== st_args 输出(参数传递验证)====="
timeout 5 ./st_args
echo "rc=$?"
echo ""
echo "===== st_prog 完整反汇编:找 rbx 递增 ====="
objdump -d st_prog 2>/dev/null | grep -nE "add|inc|mov.*ebx|mov.*rbx|cmp.*rbx|rbx" | head -30
echo ""
echo "===== 生成一个超简单 while 测试 ====="
cat > /tmp/wh1.mira <<'EOF'
fn f(n) {
    mut i = 0;
    while (i < n) {
        i = i + 1;
    }
    i
}

fn main() { print(f(5)); }
EOF
./mira /tmp/wh1.mira >/dev/null 2>&1
echo "编译 rc=$?"
timeout 5 ./wh1
echo "运行 rc=$?"
echo ""
echo "===== 反汇编 wh1 ====="
objdump -d wh1 2>/dev/null | sed -n '/400180:/,/4001f0:/p' | head -50
