#!/bin/bash
cd /tmp/mt/mira
echo "===== 0x400180 反汇编(前 90 行)====="
objdump -d st_m15 | awk '/^0000000000400180/,/^$/' | head -90
echo "===== 0x4001ba 反汇编(前 40 行)====="
objdump -d st_m15 | awk '/^00000000004001ba/,/^$/' | head -40
echo "===== 0x4004d0 反汇编(前 30 行)====="
objdump -d st_m15 | awk '/^00000000004004d0/,/^$/' | head -30
echo "===== 各版本源码 diff ====="
ls -la /tmp/st_*.mira
diff /tmp/st_small.mira /tmp/st_ifeq.mira
echo "----- ifeq vs noif -----"
diff /tmp/st_ifeq.mira /tmp/st_noif.mira 2>/dev/null
echo "===== 重新计时(3 秒上限)====="
for v in st_ifeq st_noif st_m15; do
  if [ -x /tmp/mt/mira/$v ]; then
    t0=$(date +%s%N)
    timeout 3 /tmp/mt/mira/$v > /dev/null 2>&1
    rc=$?
    t1=$(date +%s%N)
    ms=$(( (t1-t0)/1000000 ))
    echo "$v: ${ms}ms rc=$rc"
  fi
done
