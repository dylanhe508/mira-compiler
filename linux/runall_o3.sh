#!/bin/sh
# -O3 全套回归:runall 17 项 + tests/ 并发与现代语法测试
cd /tmp/mt/mira
run() {
    ./mira -O3 "$1" > /tmp/c.log 2>&1
    ec=$?
    b=$(basename "$1" .mira)
    if [ $ec -ne 0 ]; then echo "$b: COMPILE_FAIL"; return; fi
    timeout 5 ./$b > /tmp/r.log 2>&1
    rc=$?
    out=$(tr -d '\n' < /tmp/r.log)
    echo "$b: exit=$rc out=[$out]"
}
for t in t1 t2 t3 t4 t5 ct ct2 d1 t42 loop bf bf2 xf params c3 hello tadd tadd2; do
    run /tmp/$t.mira
done
for t in tests/modern_go_channel tests/modern_go_fast tests/modern_parallel tests/modern_channel_main tests/modern_go_no_capture tests/_xplat_abi tests/_xplat_fib tests/modern_go_blocking_bench tests/modern_compound_assignment tests/modern_dynamic_range tests/modern_enum_match tests/modern_range_for tests/modern_struct_syntax tests/modern_typed_syntax tests/branch_ifconvert_safe tests/regression_branch_phi tests/regression_dynamic_internal_slots tests/regression_global_coloring tests/regression_induction_strength tests/regression_divrem_shape; do
    run "$t.mira"
done
echo DONE
