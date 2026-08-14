#!/bin/bash
cd /tmp/mt/mira
echo "===== CPU 频率 ====="
grep -E "model name|cpu MHz" /proc/cpuinfo | head -4
echo "===== 启动 st_noif 并采样 15 秒 ====="
./st_noif > /dev/null 2>&1 &
PID=$!
echo "PID=$PID"
for i in $(seq 1 30); do
  S=$(awk '{print $14, $15, $22}' /proc/$PID/stat 2>/dev/null)
  if [ -z "$S" ]; then echo "进程已退出"; break; fi
  echo "t=${i}: utime_stime_vsize=${S}"
  sleep 0.5
done
echo "===== 上下文切换 ====="
grep -E "voluntary|nonvoluntary" /proc/$PID/status 2>/dev/null
kill $PID 2>/dev/null
