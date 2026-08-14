#!/bin/sh
# Linux 侧 Mira 编译时间测量(避免 git bash 破坏 $() 的变通:脚本文件同步进 WSL 执行)
cd /root/mira
for i in 1 2 3; do
    s=$(date +%s%N)
    ./mira -O3 bench_fib.mira > /dev/null
    e=$(date +%s%N)
    echo "compile_$i: $(( (e - s) / 1000000 )) ms"
done
