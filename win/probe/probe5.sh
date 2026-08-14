#!/bin/bash
cd /tmp/mt/mira
echo "===== strace -c 30 秒(统计信号与系统调用)====="
timeout 30 strace -c ./st_noif 2>&1 | head -30
echo ""
echo "===== /proc/PID/stack 是否可用 ====="
./st_noif > /dev/null 2>&1 &
PID=$!
sleep 2
cat /proc/$PID/stack 2>&1 | head -5
kill $PID 2>/dev/null
wait 2>/dev/null
echo "===== 编译 C 等价程序 ====="
cat > /tmp/st_c.c <<'EOF'
#include <stdio.h>
static long long long_cell(long long r, long long c, long long seed) {
    return (r * 73856093LL) ^ (c * 19349663LL) ^ seed;
}
static long long long_stencil(long long rows, long long cols, long long rounds) {
    long long total = 0;
    long long round;
    for (round = 0; round < rounds; round++) {
        long long r;
        for (r = 1; r < rows - 1; r++) {
            long long row = 0;
            long long c;
            for (c = 1; c < cols - 1; c++) {
                long long value = long_cell(r, c, round + 17) * 4;
                value += long_cell(r-1, c, round + 17);
                value += long_cell(r+1, c, round + 17);
                value += long_cell(r, c-1, round + 17);
                value += long_cell(r, c+1, round + 17);
                value /= 8;
                row = row + value;
            }
            total = total * 17 + row;
        }
    }
    return total;
}
int main(void) { printf("%lld\n", long_stencil(19, 25, 30)); return 0; }
EOF
gcc -O2 -o /tmp/st_c /tmp/st_c.c && echo "C 编译成功"
t0=$(date +%s%N)
timeout 10 /tmp/st_c
rc=$?
t1=$(date +%s%N)
echo "C 版耗时 $(( (t1-t0)/1000000 ))ms rc=$rc"
