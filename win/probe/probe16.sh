#!/bin/bash
cd /tmp/mt/mira
echo "===== wh5 反汇编:round 循环条件(找 25 轮 bug)====="
objdump -d wh5 2>/dev/null | sed -n '/400180:/,/400230:/p'
echo ""
echo "===== a2 反汇编:所有写 rbp 负偏移的指令 ====="
objdump -d a2 2>/dev/null | grep -oE "mov.*%-0x[0-9a-f]+\(%rbp\),%r[0-9]+|mov.*%r[0-9]+,%-0x[0-9a-f]+\(%rbp\)" | sort | uniq -c
echo ""
echo "===== a2b print 版(验证是否无限)====="
cat > /tmp/a2b_p.mira <<'EOF'
fn long_cell(r, c, seed) {
    (r * 73856093) ^ (c * 19349663) ^ seed
}

fn f(rows, cols, rounds) {
    mut total = 0;
    mut round = 0;
    while (round < rounds) {
        print(round);
        mut r = 1;
        while (r < rows) {
            mut row = 0;
            mut c = 1;
            while (c < cols) {
                mut v = long_cell(r, c, round + 17);
                v = v + long_cell(r - 1, c, round + 17);
                row = row + v;
                c = c + 1;
            }
            total = total * 17 + row;
            r = r + 1;
        }
        round = round + 1;
    }
    total
}

fn main() { print(f(19, 25, 30)); }
EOF
./mira /tmp/a2b_p.mira >/dev/null 2>&1
timeout 4 ./a2b_p > /tmp/a2b_p_out.txt 2>&1
echo "a2b_p rc=$? 行数=$(wc -l < /tmp/a2b_p_out.txt) 最后2行:"
tail -2 /tmp/a2b_p_out.txt
