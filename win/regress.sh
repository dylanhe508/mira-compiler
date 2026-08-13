#!/bin/bash
# Mira 回归脚本:编译每个 tests/*.mira 并运行,记录编译/运行结果
# 用法: regress.sh <mira命令> <产物后缀> <输出文件>
mira_cmd="$1"
suffix="$2"
outfile="$3"

cd tests || exit 1
out="../$outfile"
rm -f "$out"

for f in *.mira; do
  b="${f%.mira}"
  err=$($mira_cmd "$f" 2>&1)
  crc=$?
  if [ -f "$b$suffix" ]; then
    run=$(timeout 30 ./"$b$suffix" 2>&1)
    rrc=$?
    echo "=== $f === COMPILE rc=$crc RUN rc=$rrc" >> "$out"
    echo "$run" >> "$out"
    rm -f "$b$suffix"
  else
    echo "=== $f === COMPILE rc=$crc NOPROD" >> "$out"
    echo "$err" >> "$out"
  fi
done
echo "DONE"
