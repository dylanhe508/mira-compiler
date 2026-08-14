#!/bin/sh
# 退出码语义测试:main 的最后一条语句返回值是否成为退出码
cd /root/mira
for t in t_void t_void2 t_gojoin t_print2; do
    ./mira -O3 /tmp/mt/mira/$t.mira > /tmp/c.log 2>&1
    ec=$?
    if [ $ec -ne 0 ]; then
        echo "$t: COMPILE_FAIL: $(head -1 /tmp/c.log)"
        continue
    fi
    ./$t > /tmp/r.log 2>&1
    echo "$t: rc=$? out=[$(tr -d '\n' < /tmp/r.log)]"
done
# 再跑 3 遍 gojoin 确认稳定性
./mira -O3 /tmp/mt/mira/t_gojoin.mira > /dev/null 2>&1
for i in 1 2 3; do
    ./t_gojoin > /dev/null 2>&1
    echo "t_gojoin_again: rc=$?"
done
