#!/bin/bash
cd /tmp/mt/mira
echo "===== 0x4004b0(print 包装)完整反汇编 ====="
objdump -d st_prog 2>/dev/null | sed -n '/4004b0:/,/400590:/p' | head -60
echo ""
echo "===== 用当前编译器重新编译 st_noif 并计时 ====="
./mira /tmp/st_noif.mira >/dev/null 2>&1
echo "编译 rc=$?"
t0=$(date +%s%N)
timeout 10 ./st_noif
rc=$?
t1=$(date +%s%N)
echo "新编译 st_noif: $(( (t1-t0)/1000000 ))ms rc=$rc"
echo ""
echo "===== 同样重编 st_small ====="
./mira /tmp/st_small.mira >/dev/null 2>&1
t0=$(date +%s%N)
timeout 10 ./st_small
rc=$?
t1=$(date +%s%N)
echo "新编译 st_small: $(( (t1-t0)/1000000 ))ms rc=$rc"
