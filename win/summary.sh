# 打印一个 exe 的 min/median (单输出基准)
s1() {
  exe="$1"; lbl="$2"
  [ -f "${exe}.exe" ] || { echo "  $lbl: (missing)"; return; }
  for w in 1 2 3; do ./${exe}.exe >/dev/null 2>&1; done
  vals=""
  for i in $(seq 1 31); do
    out=$(./${exe}.exe 2>&1)
    ns=$(echo "$out" | grep -A1 elapsed_ns | tail -1 | tr -d '[:space:]')
    [ -z "$ns" ] && ns=$(echo "$out" | grep -v '^$' | tail -1 | tr -d '[:space:]')
    vals="$vals $ns"
  done
  echo "$vals" | tr ' ' '\n' | grep -v '^$' | awk -v L="$lbl" '
  { a[NR]=$1; if($1<min||NR==1) min=$1 }
  END { n=NR; for(i=1;i<=n;i++) for(j=i+1;j<=n;j++) if(a[i]>a[j]){t=a[i];a[i]=a[j];a[j]=t}
        med=(n%2)?a[(n+1)/2]:(a[n/2]+a[n/2+1])/2
        printf "  %-22s min=%7.1f  median=%7.1f (ms)\n", L, min/1e6, med/1e6 }'
}
echo "[fib 1e8 迭代加法 — 纯算术依赖链]"
s1 fib_gcc_O0 "gcc -O0"
s1 fib_gcc_O2 "gcc -O2"
s1 fib_gcc_O3 "gcc -O3"
s1 fib_mira_O0 "mira -O0"
s1 fib_mira_O1 "mira -O1"
s1 fib_mira_O2 "mira -O2"
s1 fib_mira_O3 "mira -O3"
echo ""
echo "[LCG 5e7 乘法累加 — 纯算术无依赖]"
s1 lcg_gcc_O2 "gcc -O2"
s1 lcg_gcc_O3 "gcc -O3"
s1 lcg_mira_O2 "mira -O2"
s1 lcg_mira_O3 "mira -O3"
echo ""
echo "[vector_add triad 1M x64轮 — 向量化]"
s1 vadd_gcc_O3_native "gcc -O3 -march=native"
s1 vadd_mira_O3 "mira -O3"
