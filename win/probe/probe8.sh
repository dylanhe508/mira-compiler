#!/bin/bash
cd /tmp/mt/mira
echo "===== st_prog 完整输出最后 10 行(看是否结束)====="
timeout 8 ./st_prog > /tmp/prog_out.txt 2>&1
echo "rc=$?"
tail -5 /tmp/prog_out.txt
echo "总行数: $(wc -l < /tmp/prog_out.txt)"
echo ""
echo "===== st_prog 反汇编:prologue + 外层循环条件 ====="
objdump -d st_prog 2>/dev/null | sed -n '/4001ba:/,/400210:/p' | head -45
echo "--- 外层循环条件区域 ---"
objdump -d st_prog 2>/dev/null | grep -A6 -B2 "40023e\|40024[0-9a-f]:" | head -30
echo ""
echo "===== 打印参数值验证 ====="
cat > /tmp/st_args.mira <<'EOF'
fn long_stencil(rows, cols, rounds) {
    print(rows);
    print(cols);
    print(rounds);
    rows + cols + rounds
}

fn main() { print(long_stencil(19, 25, 30)); }
EOF
./mira /tmp/st_args.mira >/dev/null 2>&1
echo "编译 rc=$?"
timeout 5 ./st_args
echo "运行 rc=$?"
