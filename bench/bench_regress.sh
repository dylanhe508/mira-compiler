#!/usr/bin/env bash
# mira 编译器自动化回归:5 bench x O0-O3 checksum 对比 gcc 参考
# 用法:
#   ./bench_regress.sh [mira_exe] [--perf]
#     mira_exe  编译器路径,默认 ../win/mira_hoist.exe
#     --perf    追加 O3 性能采样(5 基准 x5 次, 第 2 个数字行=耗时 ns)
# 退出码: 0=全部 MATCH  1=有 MISMATCH  2=环境错误
set -u
CALLER_DIR=$PWD
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
MIRA_ARG=${1:-}
cd "$SCRIPT_DIR"

if [ -n "$MIRA_ARG" ]; then
    case "$MIRA_ARG" in
        /*) MIRA=$MIRA_ARG ;;
        *)  MIRA="$CALLER_DIR/$MIRA_ARG" ;;
    esac
else
    MIRA="$SCRIPT_DIR/../win/mira.exe"
fi
PERF=0
if [ "${2:-}" = "--perf" ]; then PERF=1; fi
[ -f "$MIRA" ] || { echo "[ERR] 找不到编译器: $MIRA"; exit 2; }
MIRA=$(readlink -f "$MIRA")

BENCHES=(bench_branch bench_fib bench_pressure bench_stencil vector_add)
BENCH_DIR=$(readlink -f .)
WORK="regress_tmp"
rm -rf "$WORK" && mkdir -p "$WORK"
WORK=$(readlink -f "$WORK")

# 无符号规范化 helper:mira 用 %lld、gcc 参考用 %llu 打印时,同一位模式
# 可能显示为负/正两个字符串,统一按 uint64 规范化后再比较。
cat > "$WORK/norm.c" <<'EOF'
#include <stdio.h>
int main(void) {
    char l[64] = {0}; unsigned long long v = 0;
    if (!fgets(l, 64, stdin)) return 1;
    /* 负值按 uint64 补码解读(mira 的 %lld 打印),正数按 %llu 直读
     * (可能 > INT64_MAX,故不能用 %lld) */
    if (l[0] == '-') {
        long long s;
        if (sscanf(l, "%lld", &s) == 1) v = (unsigned long long)s;
    } else if (sscanf(l, "%llu", &v) != 1) return 1;
    printf("%llu\n", v);
    return 0;
}
EOF
gcc -O2 "$WORK/norm.c" -o "$WORK/norm.exe" || { echo "[ERR] norm.exe 编译失败"; exit 2; }

echo "== [1/2] gcc 参考 checksum =="
# Linux(POSIX)/Windows(QueryPerformanceCounter) 计时参考分开:
# bench/*.c 是 Windows 版(MinGW 可编),*_linux.c 是 clock_gettime 版。
if [ "$(uname -s)" = "Linux" ]; then REFSUF=_linux; EXESUF=; else REFSUF=; EXESUF=.exe; fi
declare -A REF
for b in "${BENCHES[@]}"; do
    gcc -O2 "$b$REFSUF.c" -o "$WORK/ref_$b.exe" 2>/dev/null \
        || { echo "[ERR] gcc 编译 $b$REFSUF.c 失败"; exit 2; }
    REF[$b]=$("$WORK/ref_$b.exe" | grep -E '^[0-9-]+' | head -1 | "$WORK/norm.exe")
    echo "  $b: ${REF[$b]}"
done

echo "== [2/2] mira O0-O3 回归 =="
PASS=0; FAIL=0
for b in "${BENCHES[@]}"; do
    for opt in 0 1 2 3; do
        d="$WORK/${b}_O$opt"; mkdir -p "$d"
        if ! (cd "$d" && "$MIRA" -O$opt "$BENCH_DIR/$b.mira" >/dev/null 2>&1); then
            echo "  $b O$opt: 编译失败"; FAIL=$((FAIL + 1)); continue
        fi
        got=$(cd "$d" && "./$b$EXESUF" | grep -E '^[0-9-]+' | head -1 | "$WORK/norm.exe")
        if [ "$got" = "${REF[$b]}" ]; then
            echo "  $b O$opt: MATCH   ($got)"
            PASS=$((PASS + 1))
        else
            echo "  $b O$opt: MISMATCH (got=$got want=${REF[$b]})"
            FAIL=$((FAIL + 1))
        fi
    done
done
echo "---- $PASS PASS / $FAIL FAIL ----"

if [ "$PERF" = 1 ]; then
    echo "== [perf] all bench O3 x5 (末个数字行 = 耗时 ns) =="
    for b in "${BENCHES[@]}"; do
        d="$WORK/perf_$b"; mkdir -p "$d"
        (cd "$d" && "$MIRA" -O3 "$BENCH_DIR/$b.mira" >/dev/null 2>&1) \
            || { echo "  $b: 编译失败"; continue; }
        echo "  $b:"
        (cd "$d" && "./$b$EXESUF" >/dev/null 2>&1)  # 预热,丢弃冷 cache 首跑
        for i in 1 2 3 4 5; do
            (cd "$d" && "./$b$EXESUF" | grep -E '^[0-9-]+' | tail -1)
        done
    done
fi

rm -rf "$WORK"
[ "$FAIL" = 0 ] && exit 0 || exit 1
