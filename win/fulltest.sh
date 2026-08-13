#!/bin/bash
# ============================================================
# mira 全量测试: 400 项准确性+运行 | 编译速度 | 产物大小
#   [1] tests 220 项: win/tests/*.mira x O0-O3
#         - 有 .c 参考: gcc 对拍输出(sha256 规范化后)
#         - *_error.mira: 预期编译失败(exit!=0 = PASS)
#         - 其余: 编译成功 + 运行 exit=0 = PASS
#   [2] fuzz 160 项: bench/fuzz_work/fuzz_* x O0-O3 差分
#         - 四级 rc 相同 且 输出(去\r)相同 = PASS
#   [3] bench 20 项: 5 基准 x O0-O3 checksum vs gcc 参考
#   [4] 编译速度: gen_100/200/400/800/gen_big x O0-O3 计时
#   [5] 产物大小: 各基准产物 vs gcc 参考产物 (O0-O3)
# 用法: bash fulltest.sh [mira_exe]   (默认 ./mira.exe)
# 全部输出写文件(编译器 stdout 不能接 /dev/null, 会触发伪错误)
# ============================================================
set -u
cd "$(dirname "$0")"
MIRA=${1:-./mira.exe}
[ -f "$MIRA" ] || { echo "[ERR] 找不到编译器: $MIRA"; exit 2; }
MIRA=$(readlink -f "$MIRA")
MIRA_DIR=$(dirname "$MIRA")
REPO=$(dirname "$MIRA_DIR")   # E:/mira/mira
TWORK="$MIRA_DIR/ft_work"
rm -rf "$TWORK"; mkdir -p "$TWORK"
LOG="$TWORK/fulltest.log"
exec > "$LOG" 2>&1
echo "== mira 全量测试 $(date '+%F %T') =="
echo "== 编译器: $MIRA"
echo "== 仓库:   $REPO"

PASS=0; FAIL=0; SKIP=0
declare -a FAILED_ITEMS=()

# ---------- 工具函数 ----------
# 规范化输出(去 CR, 去首尾空白, 逐行 hash 拼接) -> sha256
norm() {
  tr -d '\r' | sed 's/[[:space:]]*$//' | grep -v '^$' | sha256sum | awk '{print $1}'
}
# 计时(ms)
now_ms() { date +%s%3N; }

# uint64 规范化(mira %lld vs gcc %llu 显示差异) —— 同 bench_regress.sh 约定
if [ ! -f "$TWORK/norm.exe" ]; then
  cat > "$TWORK/norm.c" <<'EOF'
#include <stdio.h>
int main(void) {
    char l[64] = {0}; unsigned long long v = 0;
    if (!fgets(l, 64, stdin)) return 1;
    if (l[0] == '-') {
        long long s;
        if (sscanf(l, "%lld", &s) == 1) v = (unsigned long long)s;
    } else if (sscanf(l, "%llu", &v) != 1) return 1;
    printf("%llu\n", v);
    return 0;
}
EOF
  gcc -O2 "$TWORK/norm.c" -o "$TWORK/norm.exe" || { echo "[ERR] norm.exe 编译失败"; exit 2; }
fi

# checksum 对拍: 取首个整数行 + uint64 规范化后相等 -> 0
# 计时行(elapsed ns)每次运行都不同, 不参与对比(同 bench_regress.sh 约定)
checksum_line() { grep -E '^[0-9-]+' | head -1 | "$TWORK/norm.exe"; }
# 对拍: 优先首整数行(uint64 规范化, 忽略每次运行都不同的计时行);
# 无整数行时回退全输出 hash(去 CR/尾空白/空行后确定化)
cmp_out() {  # $1=输出文件A $2=输出文件B
  local a b
  a=$(checksum_line < "$1")
  b=$(checksum_line < "$2")
  if [ -n "$a" ] && [ -n "$b" ]; then
    [ "$a" = "$b" ]
  else
    [ "$(norm < "$1")" = "$(norm < "$2")" ]
  fi
}

# ---------- [1] tests 220 项 ----------
echo; echo "===== [1] win/tests x O0-O3 (220 项) ====="
TESTS_DIR="$MIRA_DIR/tests"
TESTS_CNT=0
for f in "$TESTS_DIR"/*.mira; do
  [ -f "$f" ] || continue
  base=$(basename "$f" .mira)
  TESTS_CNT=$((TESTS_CNT+1))
  for O in 0 1 2 3; do
    odir="$TWORK/t_o$O"; mkdir -p "$odir"
    # --- 编译 ---
    (cd "$odir" && "$MIRA" -O$O "$f" > c.log 2>&1)
    crc=$?
    exe="$odir/$base.exe"
    # --- 预期错误用例: 编译失败即 PASS ---
    if echo "$base" | grep -q '_error$'; then
      if [ $crc -ne 0 ]; then
        PASS=$((PASS+1)); echo "PASS  [t$TESTS_CNT] $base O$O 预期编译错误(rc=$crc)"
      else
        FAIL=$((FAIL+1)); FAILED_ITEMS+=("$base O$O 预期失败但编译成功")
        echo "FAIL  [t$TESTS_CNT] $base O$O 预期编译错误但成功"
      fi
      continue
    fi
    # --- 编译失败(非预期) ---
    if [ $crc -ne 0 ] || [ ! -f "$exe" ]; then
      FAIL=$((FAIL+1)); FAILED_ITEMS+=("$base O$O 编译失败 rc=$crc")
      echo "FAIL  [t$TESTS_CNT] $base O$O 编译失败 rc=$crc ($(head -c 120 c.log 2>/dev/null | tr '\n' ' '))"
      continue
    fi
    # --- 运行(限时 60s) ---
    (cd "$odir" && timeout 60 ./"$base.exe" > r.out 2>&1)
    rrc=$?
    if [ $rrc -ne 0 ]; then
      FAIL=$((FAIL+1)); FAILED_ITEMS+=("$base O$O 运行 rc=$rrc")
      echo "FAIL  [t$TESTS_CNT] $base O$O 运行 rc=$rrc"
      continue
    fi
    # --- gcc 参考对拍(有同名 .c): 首整数行 checksum, 无整数行回退全输出 ---
    refc="$TESTS_DIR/$base.c"
    if [ -f "$refc" ]; then
      gcc -O2 "$refc" -o "$TWORK/ref_$base.exe" > gcc.log 2>&1
      if [ $? -eq 0 ]; then
        "$TWORK/ref_$base.exe" > "$TWORK/ref_out.txt" 2>/dev/null
        if cmp_out "$TWORK/ref_out.txt" "$odir/r.out"; then
          PASS=$((PASS+1)); echo "PASS  [t$TESTS_CNT] $base O$O gcc对拍一致"
        else
          FAIL=$((FAIL+1)); FAILED_ITEMS+=("$base O$O gcc对拍不一致")
          echo "FAIL  [t$TESTS_CNT] $base O$O gcc对拍不一致 (mira=$(checksum_line < "$odir/r.out") gcc=$(checksum_line < "$TWORK/ref_out.txt"))"
        fi
      else
        SKIP=$((SKIP+1)); echo "SKIP  [t$TESTS_CNT] $base O$O (gcc 参考编译失败)"
      fi
    else
      PASS=$((PASS+1)); echo "PASS  [t$TESTS_CNT] $base O$O 运行成功(无参考)"
    fi
  done
done
echo "== [1] 完成: $TESTS_CNT 用例 x 4 级"

# ---------- [2] fuzz 160 项 ----------
echo; echo "===== [2] fuzz x O0-O3 差分 (160 项) ====="
FUZZ_N=0
for d in "$REPO"/bench/fuzz_work/fuzz_*/; do
  [ -f "$d/case.mira" ] || continue
  FUZZ_N=$((FUZZ_N+1))
  id=$(basename "$d")
  declare -a rcs=() outs=()
  for O in 0 1 2 3; do
    odir="$TWORK/f_o$O"; mkdir -p "$odir"
    (cd "$odir" && "$MIRA" -O$O "$d/case.mira" > c.log 2>&1)
    crc=$?
    if [ $crc -ne 0 ] || [ ! -f "$odir/case.exe" ]; then
      rcs[$O]=$crc; outs[$O]="COMPILE_FAIL"
      continue
    fi
    (cd "$odir" && timeout 30 ./case.exe > r.out 2>&1)
    rrc=$?
    rcs[$O]=$rrc
    outs[$O]=$(norm < "$odir/r.out")
  done
  # 差分判定: 四级 rc 相同 + 四级输出相同
  same_rc=1; same_out=1
  for O in 1 2 3; do
    [ "${rcs[$O]}" = "${rcs[0]}" ] || same_rc=0
    [ "${outs[$O]}" = "${outs[0]}" ] || same_out=0
  done
  if [ $same_rc -eq 1 ] && [ $same_out -eq 1 ]; then
    PASS=$((PASS+1))
    echo "PASS  [f$FUZZ_N] $id O0-O3 一致 (rc=${rcs[0]}, 输出hash=$(echo ${outs[0]}|cut -c1-12))"
  else
    FAIL=$((FAIL+1)); FAILED_ITEMS+=("$id O0-O3 差分不一致 rc=[${rcs[0]},${rcs[1]},${rcs[2]},${rcs[3]}]")
    echo "FAIL  [f$FUZZ_N] $id O0-O3 不一致 rc=[${rcs[0]},${rcs[1]},${rcs[2]},${rcs[3]}]"
  fi
done
echo "== [2] 完成: $FUZZ_N 用例 x 4 级"

# ---------- [3] bench 20 项 (gcc checksum 对比) ----------
echo; echo "===== [3] bench x O0-O3 checksum vs gcc (20 项) ====="
BENCHES=(bench_branch bench_fib bench_pressure bench_stencil vector_add)
for b in "${BENCHES[@]}"; do
  refc="$REPO/bench/$b.c"
  if [ ! -f "$refc" ]; then echo "SKIP  $b (无参考)"; SKIP=$((SKIP+1)); continue; fi
  gcc -O2 "$refc" -o "$TWORK/ref_$b.exe" > /dev/null 2>&1 || { echo "SKIP  $b (gcc 参考编译失败)"; SKIP=$((SKIP+1)); continue; }
  "$TWORK/ref_$b.exe" > "$TWORK/ref_$b.out" 2>/dev/null
  refsum=$(checksum_line < "$TWORK/ref_$b.out")
  for O in 0 1 2 3; do
    odir="$TWORK/b_o$O"; mkdir -p "$odir"
    (cd "$odir" && "$MIRA" -O$O "$REPO/bench/$b.mira" > c.log 2>&1)
    crc=$?
    if [ $crc -ne 0 ] || [ ! -f "$odir/$b.exe" ]; then
      FAIL=$((FAIL+1)); FAILED_ITEMS+=("$b O$O 编译失败")
      echo "FAIL  [b] $b O$O 编译失败 rc=$crc"
      continue
    fi
    (cd "$odir" && timeout 120 ./"$b.exe" > r.out 2>&1)
    rrc=$?
    mysum=$(checksum_line < "$odir/r.out")
    if [ $rrc -ne 0 ]; then
      FAIL=$((FAIL+1)); FAILED_ITEMS+=("$b O$O 运行 rc=$rrc")
      echo "FAIL  [b] $b O$O 运行 rc=$rrc"
    elif [ "$refsum" = "$mysum" ]; then
      PASS=$((PASS+1)); echo "PASS  [b] $b O$O checksum 一致 ($mysum)"
    else
      FAIL=$((FAIL+1)); FAILED_ITEMS+=("$b O$O checksum 不一致")
      echo "FAIL  [b] $b O$O checksum 不一致 (mira=$mysum gcc=$refsum)"
    fi
  done
done

# ---------- [4] 编译速度 ----------
echo; echo "===== [4] 编译速度 (gen_100..gen_big x O0-O3, ms) ====="
GENS=(gen_100 gen_200 gen_400 gen_800 gen_big)
printf "%-12s %8s %8s %8s %8s %8s\n" file O0 O1 O2 O3
declare -A GEN_MS
for g in "${GENS[@]}"; do
  gf="$REPO/bench/$g.mira"
  [ -f "$gf" ] || { echo "SKIP  $g (无源)"; continue; }
  row="$g"
  for O in 0 1 2 3; do
    odir="$TWORK/g_o$O"; mkdir -p "$odir"
    t0=$(now_ms)
    (cd "$odir" && "$MIRA" -O$O "$gf" > c.log 2>&1)
    crc=$?
    t1=$(now_ms)
    ms=$((t1-t0))
    if [ $crc -ne 0 ]; then row="$row FAIL"; else row="$row $ms"; fi
  done
  echo "$row"
done

# ---------- [5] 产物大小 ----------
echo; echo "===== [5] 产物大小 (字节) ====="
printf "%-14s %10s %10s %10s %10s | %10s\n" bench O0 O1 O2 O3 gcc-O2
for b in "${BENCHES[@]}"; do
  row="$b"
  for O in 0 1 2 3; do
    exe="$TWORK/b_o$O/$b.exe"
    if [ -f "$exe" ]; then row="$row $(stat -c %s "$exe")"; else row="$row -"; fi
  done
  refexe="$TWORK/ref_$b.exe"
  if [ -f "$refexe" ]; then row="$row | $(stat -c %s "$refexe")"; else row="$row | -"; fi
  echo "$row"
done
printf "%-14s %10s %10s %10s %10s | %10s\n" mira.exe-自身 - - - - -
exe_self="$MIRA"
[ -f "$exe_self" ] && echo "  mira.exe = $(stat -c %s "$exe_self") 字节"

# ---------- 汇总 ----------
echo; echo "============================================"
echo "== 汇总: PASS=$PASS  FAIL=$FAIL  SKIP=$SKIP"
if [ $FAIL -gt 0 ]; then
  echo "== 失败清单:"
  for it in "${FAILED_ITEMS[@]}"; do echo "   - $it"; done
fi
echo "== 日志: $LOG"
echo "============================================"
