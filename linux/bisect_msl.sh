#!/bin/bash
# 二分定位 -O2 回归:4 个源码版本,各重编 + 跑 matrix_stencil_long
cd /tmp/mt/mira || exit 1
run_ver() {
  name=$1; ic=$2; pc=$3
  cp "parser/$ic" parser/index.c
  cp "parser/$pc" parser/parse_one.c
  make clean >/dev/null 2>&1
  make CFLAGS="-O2 -g -Wall -fno-pie -D_POSIX_C_SOURCE=200809L" -j4 >/dev/null 2>&1
  m=$(./mira /tmp/msl.mira 2>&1 | tail -1)
  out=$(./msl 2>&1 | tail -1)
  echo "$name 编译:$m 运行:$out"
}
echo "== -O2 四版本矩阵测试 =="
run_ver "A_orig_orig" index.c.orig  parse_one.c.orig
run_ver "B_fnmain   " index.c.bak2  parse_one.c.orig
run_ver "C_fn_semi  " index.c.full  parse_one.c.orig
run_ver "D_all      " index.c.full  parse_one.c.full
