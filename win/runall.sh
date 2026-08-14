#!/bin/sh
cd /root/mira
for t in t1 t2 t3 t4 t5 ct ct2 d1 t42 loop bf bf2 xf params c3 hello; do
  ./mira -O2 /tmp/$t.mira > /tmp/c.log 2>&1
  ec=$?
  if [ $ec -ne 0 ]; then echo "$t: COMPILE_FAIL"; continue; fi
  timeout 5 ./$t > /tmp/r.log 2>&1
  rc=$?
  out=$(tr -d '\n' < /tmp/r.log)
  echo "$t: exit=$rc out=[$out]"
done
