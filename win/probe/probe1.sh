#!/bin/bash
cd /tmp/mt/mira
objdump -d st_m15 > /tmp/d_m15.txt 2>&1
objdump -d st_ifeq > /tmp/d_ifeq.txt 2>&1
echo "===== diff 快慢两版反汇编 ====="
diff /tmp/d_ifeq.txt /tmp/d_m15.txt
echo "===== 慢版所有跳转 ====="
grep -nE "je |jne |jmp |call " /tmp/d_m15.txt | head -60
