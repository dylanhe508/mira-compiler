#!/bin/bash
cd /tmp/mt/mira
for m in 15 8 1 7 3; do
  sed "s/if ((value & 7) == 0) { row = row - value; }/if ((value \& ${m}) == 0) { row = row - value; }/" /tmp/st_small.mira > /tmp/st_m${m}.mira
  ./mira /tmp/st_m${m}.mira >/dev/null 2>&1
  timeout 8 ./st_m${m} > /dev/null 2>&1
  echo "mask=$m RC=$?"
done
