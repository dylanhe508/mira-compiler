#!/usr/bin/env bash
# 用法: bench31.sh <exe> <label> <grep_pattern_for_ns>
# 自动 3 预热 + 31 次实测,输出 min/median/mean/p90 (ms)
exe="$1"; label="$2"
# 3 次预热,丢弃
for w in 1 2 3; do ./${exe}.exe >/dev/null 2>&1; done
# 31 次实测,收集 ns
vals=""
for i in $(seq 1 31); do
  out=$(./${exe}.exe 2>&1)
  ns=$(echo "$out" | grep -A1 'elapsed_ns' | tail -1 | tr -d '[:space:]')
  vals="$vals $ns"
done
# 一次性算统计量 (awk)
echo "$vals" | tr ' ' '\n' | grep -v '^$' | awk '
{ a[NR]=$1; sum+=$1; if($1<min||NR==1) min=$1 }
END {
  n=NR;
  # sort
  for(i=1;i<=n;i++) for(j=i+1;j<=n;j++) if(a[i]>a[j]){t=a[i];a[i]=a[j];a[j]=t}
  med = (n%2)? a[(n+1)/2] : (a[n/2]+a[n/2+1])/2
  p90 = a[int(n*0.9)+1]
  mean = sum/n
  printf "  %-15s min=%7.1f  median=%7.1f  mean=%7.1f  p90=%7.1f  (ms)\n", LBL, min/1e6, med/1e6, mean/1e6, p90/1e6
}' LBL="$label"
