#!/bin/bash
# gcc vs mira 三项对决:编译时间 / 产物大小 / 运行性能
cd /tmp/mt/mira
cp /mnt/e/mira/mira/bench_fib_linux.c /tmp/bf.c
cp /mnt/e/mira/mira/bench_fib.mira /tmp/bf.mira

echo "== 编译时间 =="
gcc -O2 -o /tmp/bf_gcc2 /tmp/bf.c
echo "gcc -O2:  ${TIMEFORMAT_REAL}"
/usr/bin/time -f "gcc -O2:  %e s" gcc -O2 -o /tmp/bf_gcc2 /tmp/bf.c 2>&1
/usr/bin/time -f "gcc -O3:  %e s" gcc -O3 -o /tmp/bf_gcc3 /tmp/bf.c 2>&1
rm -f bf
/usr/bin/time -f "mira -O2: %e s" ./mira /tmp/bf.mira 2>&1
mv bf /tmp/bf_mira2 2>/dev/null
rm -f bf
/usr/bin/time -f "mira -O3: %e s" ./mira -O3 /tmp/bf.mira 2>&1
mv bf /tmp/bf_mira3 2>/dev/null

echo "== 产物大小 =="
ls -l /tmp/bf_gcc2 /tmp/bf_gcc3 /tmp/bf_mira2 /tmp/bf_mira3 2>/dev/null | awk '{print $5"  "$9}'

echo "== 运行时间(1e8 次迭代,各跑 3 次取最优,ns) =="
for exe in bf_gcc2 bf_gcc3 bf_mira2 bf_mira3; do
  best=999999999999
  for i in 1 2 3; do
    t=$(/tmp/$exe | tail -1)
    if [ "$t" -lt "$best" ]; then best=$t; fi
  done
  echo "/tmp/$exe: $best ns"
done
