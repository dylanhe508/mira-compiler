#!/bin/bash
cd /tmp/mt/mira
echo "===== C 期望值 ====="
cat > /tmp/wh3_c.c <<'EOF'
#include <stdio.h>
int main(void) {
    long long total = 0;
    for (long long round = 0; round < 30; round++)
        for (long long r = 1; r < 19; r++) {
            long long row = 0;
            for (long long c = 1; c < 25; c++) row++;
            total = total * 17 + row;
        }
    printf("%lld\n", total);
    return 0;
}
EOF
gcc -O2 -o /tmp/wh3_c /tmp/wh3_c.c && /tmp/wh3_c
echo "===== wh3 反汇编(循环控制流)====="
objdump -d wh3 2>/dev/null | sed -n '/400180:/,/4002a0:/p'
echo ""
echo "===== 复制 wh3.mira 到 Windows 侧 ====="
cp /tmp/wh3.mira /mnt/e/mira/mira/wh3_win.mira 2>/dev/null && echo "已复制"
