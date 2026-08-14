#!/bin/bash
# 第二组对决:matrix_stencil_long(mira vs gcc)+ 复核 fib
cd /tmp/mt/mira
cp /mnt/e/mira/mira/tests/matrix_stencil_long.c /tmp/ms.c
cp /mnt/e/mira/mira/tests/matrix_stencil_long.mira /tmp/ms.mira

echo "== matrix_stencil_long =="
/usr/bin/time -f "gcc -O3: %e s" gcc -O3 -o /tmp/ms_gcc /tmp/ms.c 2>&1
rm -f ms
/usr/bin/time -f "mira -O3: %e s" ./mira -O3 /tmp/ms.mira 2>&1
mv ms /tmp/ms_mira 2>/dev/null

echo "-- gcc 输出 --"
/tmp/ms_gcc
echo "-- mira 输出 --"
/tmp/ms_mira

echo "== fib 复核(各 5 次) =="
for exe in bf_gcc2 bf_gcc3 bf_mira3; do
  echo -n "/tmp/$exe: "
  for i in 1 2 3 4 5; do
    /tmp/$exe | grep -o 'elapsed_ns=[0-9]*' | cut -d= -f2
  done | sort -n | head -1
  echo -n "  ns (最优)"
done

echo "== 产物大小对比 =="
ls -l /tmp/ms_gcc /tmp/ms_mira | awk '{print $5"  "$9}'
