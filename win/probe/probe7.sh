#!/bin/bash
cd /tmp/mt/mira
# 加进度打印:每轮 round 打印
cat > /tmp/st_prog.mira <<'EOF'
fn long_cell(r, c, seed) {
    (r * 73856093) ^ (c * 19349663) ^ seed
}

fn long_stencil(rows, cols, rounds) {
    mut total = 0;
    mut round = 0;
    while (round < rounds) {
        print(round);
        mut r = 1;
        while (r < rows - 1) {
            mut row = 0;
            mut c = 1;
            while (c < cols - 1) {
                mut center = long_cell(r, c, round + 17);
                mut value = center * 4;
                value = value + long_cell(r - 1, c, round + 17);
                value = value + long_cell(r + 1, c, round + 17);
                value = value + long_cell(r, c - 1, round + 17);
                value = value + long_cell(r, c + 1, round + 17);
                value = value / 8;
                row = row + value;
                c = c + 1;
            }
            total = total * 17 + row;
            r = r + 1;
        }
        round = round + 1;
    }
    total
}

fn main() { print(long_stencil(19, 25, 30)); }
EOF
./mira /tmp/st_prog.mira >/dev/null 2>&1
echo "编译 rc=$?"
t0=$(date +%s%N)
timeout 25 ./st_prog 2>&1 | while read line; do
  t1=$(date +%s%N)
  echo "round=$line 耗时 $(( (t1-t0)/1000000 ))ms"
done
echo "程序结束 rc=$?"
