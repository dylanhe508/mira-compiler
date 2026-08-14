#!/usr/bin/env bash
exe="$1"; label="$2"
for w in 1 2 3; do ./${exe}.exe >/dev/null 2>&1; done
vals=""
for i in $(seq 1 31); do
  out=$(./${exe}.exe 2>&1)
  ns=$(echo "$out" | grep -v '^$' | tail -1 | tr -d '[:space:]')
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
  printf "  %-26s min=%6.1f  median=%6.1f  mean=%6.1f  p90=%6.1f (ms)\n", LBL, min/1e6, med/1e6, mean/1e6, p90/1e6
}' LBL="$label"
