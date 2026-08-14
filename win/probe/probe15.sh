#!/bin/bash
cd /tmp/mt/mira
echo "===== 调用次数临界点:2/3/4 次 ====="
for n in 2 3 4; do
cat > /tmp/a${n}b.mira <<EOF
fn long_cell(r, c, seed) {
    (r * 73856093) ^ (c * 19349663) ^ seed
}

fn f(rows, cols, rounds) {
    mut total = 0;
    mut round = 0;
    while (round < rounds) {
        mut r = 1;
        while (r < rows) {
            mut row = 0;
            mut c = 1;
            while (c < cols) {
                mut v = long_cell(r, c, round + 17);
                v = v + long_cell(r - 1, c, round + 17);
                v = v + long_cell(r + 1, c, round + 17);
$( [ "$n" -ge 4 ] && echo "                v = v + long_cell(r, c - 1, round + 17);" )
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
  ./mira /tmp/a${n}b.mira >/dev/null 2>&1
  t0=$(date +%s%N)
  timeout 5 ./a${n}b > /tmp/a${n}b_out.txt 2>&1
  rc=$?
  t1=$(date +%s%N)
  echo "a${n}b($n 次调用): $(( (t1-t0)/1000000 ))ms rc=$rc 输出=$(cat /tmp/a${n}b_out.txt)"
done
echo ""
echo "===== diff wh6(1 次)vs a2(5 次):prologue + 外层循环条件 ====="
objdump -d wh6 2>/dev/null | sed -n '/400180:/,/400240:/p' > /tmp/d_wh6.txt
objdump -d a2 2>/dev/null | sed -n '/400180:/,/4002a0:/p' > /tmp/d_a2.txt
diff /tmp/d_wh6.txt /tmp/d_a2.txt | head -60
