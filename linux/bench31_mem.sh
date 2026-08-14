#!/usr/bin/env bash
# 内存基准: 输出 sum, write_ns, read_ns 三行。分别对 write/read 各统计。
exe="$1"; label="$2"
for w in 1 2 3; do ./${exe}.exe >/dev/null 2>&1; done
wvals=""; rvals=""
for i in $(seq 1 31); do
  out=$(./${exe}.exe 2>&1)
  wns=$(echo "$out" | grep -v '^$' | sed -n '2p' | tr -d '[:space:]')
  rns=$(echo "$out" | grep -v '^$' | sed -n '3p' | tr -d '[:space:]')
  wvals="$wvals $wns"; rvals="$rvals $rns"
done
stat() {
  echo "$1" | tr ' ' '\n' | grep -v '^$' | awk '
  { a[NR]=$1; sum+=$1; if($1<min||NR==1) min=$1 }
  END {
    n=NR;
    for(i=1;i<=n;i++) for(j=i+1;j<=n;j++) if(a[i]>a[j]){t=a[i];a[i]=a[j];a[j]=t}
    med = (n%2)? a[(n+1)/2] : (a[n/2]+a[n/2+1])/2
    p90 = a[int(n*0.9)+1]
    printf "    %-12s min=%6.1f  median=%6.1f  p90=%6.1f (ms)\n", WHAT, min/1e6, med/1e6, p90/1e6
  }' WHAT="$2"
}
echo "  $label:"
stat "$wvals" "write"
stat "$rvals" "read"
