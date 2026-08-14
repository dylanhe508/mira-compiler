#!/usr/bin/env bash
# 通用计时:3 预热 + 31 次实测,取输出最后一行数字为 elapsed_ns
# 用法: bench_run.sh <exe> <label>
exe="$1"; label="$2"
for w in 1 2 3; do ./"$exe" >/dev/null 2>&1; done
vals=""
for i in $(seq 1 31); do
  ns=$(./"$exe" 2>&1 | grep -E '^[0-9]+$' | tail -1)
  vals="$vals $ns"
done
echo "$vals" | tr ' ' '\n' | grep -v '^$' | awk '
{ a[NR]=$1; sum+=$1; if($1<min||NR==1) min=$1 }
END {
  n=NR;
  for(i=1;i<=n;i++) for(j=i+1;j<=n;j++) if(a[i]>a[j]){t=a[i];a[i]=a[j];a[j]=t}
  med = (n%2)? a[(n+1)/2] : (a[n/2]+a[n/2+1])/2
  p90 = a[int(n*0.9)+1]
  mean = sum/n
  printf "  %-12s min=%8.2f  median=%8.2f  mean=%8.2f  p90=%8.2f  (ms)\n", LBL, min/1e6, med/1e6, mean/1e6, p90/1e6
}' LBL="$label"
