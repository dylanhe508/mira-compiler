#!/bin/bash
cd /tmp/mt/mira
echo "===== 双层嵌套 while ====="
cat > /tmp/wh2.mira <<'EOF'
fn f(rows, cols) {
    mut r = 1;
    mut total = 0;
    while (r < rows) {
        mut c = 1;
        while (c < cols) {
            total = total + 1;
            c = c + 1;
        }
        r = r + 1;
    }
    total
}

fn main() { print(f(5, 7)); }
EOF
./mira /tmp/wh2.mira >/dev/null 2>&1 && timeout 5 ./wh2
echo "rc=$? (期望 24)"
echo ""
echo "===== 三层嵌套 while(带参数,模拟 stencil)====="
cat > /tmp/wh3.mira <<'EOF'
fn f(rows, cols, rounds) {
    mut total = 0;
    mut round = 0;
    while (round < rounds) {
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
./mira /tmp/wh3.mira >/dev/null 2>&1 && timeout 5 ./wh3
echo "rc=$?"
echo ""
echo "===== 无参数版本(常量 19/25/30)====="
cat > /tmp/wh4.mira <<'EOF'
fn f() {
    mut total = 0;
    mut round = 0;
    while (round < 30) {
        mut r = 1;
        while (r < 19) {
            mut row = 0;
            mut c = 1;
            while (c < 25) {
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

fn main() { print(f()); }
EOF
./mira /tmp/wh4.mira >/dev/null 2>&1 && timeout 5 ./wh4
echo "rc=$?"
