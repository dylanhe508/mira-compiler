#!/usr/bin/env bash
# 用法: bench31.sh <exe> <label>
#   exe   产物路径(不含后缀, linux/win 自动适配; 传了 .exe 也不重复追加)
#   label 输出标签,如 O3
# 自动 3 预热 + 31 次实测,输出 min/median/mean/p90 (ms)
exe="$1"; label="$2"
[ -n "$exe" ] || { echo "用法: bench31.sh <exe> <label>"; exit 2; }

# 平台后缀: linux 产物无 .exe, win 有; 传参已带 .exe 则不追加
EXESUF=
if [ "$(uname -s)" != "Linux" ]; then EXESUF=.exe; fi
case "$exe" in *.exe) EXESUF=;; esac

# 3 次预热,丢弃
for w in 1 2 3; do ./${exe}${EXESUF} >/dev/null 2>&1; done
# 31 次实测,收集 ns(末个数字行=耗时, 兼容无 elapsed_ns 标签的基准)
vals=""
for i in $(seq 1 31); do
  out=$(./${exe}${EXESUF} 2>&1)
  ns=$(echo "$out" | grep -E '^[0-9-]+' | tail -1 | tr -d '[:space:]')
  [ -n "$ns" ] || { echo "  $label: 采样失败(运行无输出)"; exit 1; }
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
