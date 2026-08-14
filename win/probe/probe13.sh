#!/bin/bash
cd /tmp/mt/mira
echo "===== wh5:三层循环 + print(round),无函数调用 ====="
cat > /tmp/wh5.mira <<'EOF'
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
                row = row + 1;
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
./mira /tmp/wh5.mira >/dev/null 2>&1 && timeout 5 ./wh5 | wc -l
echo "wh5 输出行数 rc=$?(期望 30 行)"
echo ""
echo "===== wh6:三层循环 + long_cell 调用,无 print ====="
cat > /tmp/wh6.mira <<'EOF'
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
./mira /tmp/wh6.mira >/dev/null 2>&1
t0=$(date +%s%N)
timeout 6 ./wh6
rc=$?
t1=$(date +%s%N)
echo "wh6: $(( (t1-t0)/1000000 ))ms rc=$rc"
echo ""
echo "===== wh7:三层循环 + long_cell 调用但被调函数为空 ====="
cat > /tmp/wh7.mira <<'EOF'
fn long_cell(r, c, seed) {
    seed
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
./mira /tmp/wh7.mira >/dev/null 2>&1
t0=$(date +%s%N)
timeout 6 ./wh7
rc=$?
t1=$(date +%s%N)
echo "wh7: $(( (t1-t0)/1000000 ))ms rc=$rc"
