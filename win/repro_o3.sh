#!/bin/sh
# 精确复现 runall_o3.sh 中 modern_go_fast exit=3 的触发条件
cd /root/mira
echo "== 阶段A: 跑完单测段(t1..tadd2,含 tadd 段错误) =="
for t in t1 t2 t3 t4 t5 ct ct2 d1 t42 loop bf bf2 xf params c3 hello tadd tadd2; do
    ./mira -O3 /tmp/$t.mira > /tmp/c.log 2>&1
    [ $? -ne 0 ] && echo "$t: COMPILE_FAIL" && continue
    timeout 5 ./$t > /tmp/r.log 2>&1
    echo "$t: rc=$?"
done
echo "== 阶段B: 编译并运行 modern_go_channel =="
./mira -O3 tests/modern_go_channel.mira > /tmp/c.log 2>&1
timeout 5 ./modern_go_channel > /tmp/r.log 2>&1
echo "ch: rc=$? out=[$(tr -d '\n' < /tmp/r.log)]"
echo "== 阶段C: 编译并运行 modern_go_fast =="
./mira -O3 tests/modern_go_fast.mira > /tmp/c.log 2>&1
md5sum modern_go_fast
timeout 5 ./modern_go_fast > /tmp/r.log 2>&1
echo "go_fast: rc=$? out=[$(tr -d '\n' < /tmp/r.log)]"
echo "== 阶段D: 再跑一次 modern_go_fast(不重编) =="
timeout 5 ./modern_go_fast > /tmp/r.log 2>&1
echo "go_fast2: rc=$? out=[$(tr -d '\n' < /tmp/r.log)]"
