#!/bin/bash
cd /tmp/mt/mira
echo "===== st_small.mira 完整源码 ====="
cat /tmp/st_small.mira
echo ""
echo "===== st_noif.mira 完整源码 ====="
cat /tmp/st_noif.mira
echo ""
echo "===== st_noif 精确计时 ====="
timeout 120 ./st_noif > /dev/null 2>&1
echo "st_noif rc=$?"
echo "===== ltrace st_noif(前 20 行)====="
timeout 10 ltrace -c ./st_noif 2>&1 | head -25
