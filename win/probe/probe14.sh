#!/bin/bash
cd /tmp/mt/mira
mk() { # mk <文件> <调用次数> <除法 0/1> <打印模式>
  local f=$1 n=$2 div=$3 pr=$4
  cat > /tmp/$f.mira <<EOF
fn long_cell(r, c, seed) {
    (r * 73856093) ^ (c * 19349663) ^ seed
}

fn f(rows, cols, rounds) {
    mut total = 0;
    mut round = 0;
    while (round < rounds) {
        $pr
        mut r = 1;
        while (r < rows) {
            mut row = 0;
            mut c = 1;
            while (c < cols) {
                mut v = long_cell(r, c, round + 17);
$( [ "$n" -ge 2 ] && echo "                v = v + long_cell(r - 1, c, round + 17);" )
$( [ "$n" -ge 3 ] && echo "                v = v + long_cell(r + 1, c, round + 17);" )
$( [ "$n" -ge 4 ] && echo "                v = v + long_cell(r, c - 1, round + 17);" )
$( [ "$n" -ge 5 ] && echo "                v = v + long_cell(r, c + 1, round + 17);" )
$( [ "$div" = "1" ] && echo "                v = v / 8;" )
                row = row + v;
                c = c + 1;
            }
            total = total * 17 + row;
            r = r + 1;
        }
        round = round + 1;
    }
    total
}

fn main() { print(f(19, 25, 30)); }
EOF
  ./mira /tmp/$f.mira >/dev/null 2>&1
  t0=$(date +%s%N)
  timeout 6 ./$f > /tmp/${f}_out.txt 2>&1
  rc=$?
  t1=$(date +%s%N)
  echo "$f(n=$n div=$div $pr): $(( (t1-t0)/1000000 ))ms rc=$rc 输出=$(cat /tmp/${f}_out.txt | head -c 60)"
}
echo "===== 二分定位 ====="
mk a2 5 0 ""          # 5 次调用,无除法
mk a3 1 1 ""          # 1 次调用 + 除法
mk a4 5 1 ""          # 5 次调用 + 除法(= st_noif)
echo "===== wh5 重新验证(直接看输出与 rc)====="
timeout 6 ./wh5 > /tmp/wh5_out.txt 2>&1
echo "wh5 rc=$? 行数=$(wc -l < /tmp/wh5_out.txt) 最后3行:"
tail -3 /tmp/wh5_out.txt
