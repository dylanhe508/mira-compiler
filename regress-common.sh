#!/usr/bin/env bash
# Shared implementation for win/regress.sh and linux/regress.sh.
set -euo pipefail

PLATFORM_DIR=$1
MIRA=${2:-}
TEST_DIR="$PLATFORM_DIR/tests"
if [ -z "$MIRA" ]; then
    if [ -f "$PLATFORM_DIR/mira.exe" ]; then MIRA="$PLATFORM_DIR/mira.exe";
    else MIRA="$PLATFORM_DIR/mira"; fi
fi
GCC=${GCC:-gcc}
EXPECTED_VERSION=${MIRA_EXPECTED_VERSION:-5.14.0}

[ -f "$MIRA" ] || { echo "[FAIL] compiler not found: $MIRA" >&2; exit 2; }
[ -d "$TEST_DIR" ] || { echo "[FAIL] tests not found: $TEST_DIR" >&2; exit 2; }

case "$MIRA" in
    *.exe) EXE_SUFFIX=.exe; OBJ_SUFFIX=.obj ;;
    *)     EXE_SUFFIX=; OBJ_SUFFIX=.o ;;
esac

WORK="$TEST_DIR/regression-out/formal-release"
rm -rf "$WORK"
mkdir -p "$WORK"
trap 'rm -rf "$WORK"' EXIT

PASS=0
fail() { echo "[FAIL] $*" >&2; exit 1; }
pass() { PASS=$((PASS + 1)); echo "[PASS] $*"; }

VERSION=$($MIRA -v | tr -d '\r')
case "$VERSION" in mira\ *) ;; *) fail "unexpected version output: $VERSION" ;; esac
if [ -n "$EXPECTED_VERSION" ] && [ "$VERSION" != "mira $EXPECTED_VERSION" ]; then
    fail "version is '$VERSION', expected 'mira $EXPECTED_VERSION'"
fi
HELP=$($MIRA --help | tr -d '\r')
for text in '--emit=asm' '--emit=ir' '--emit=obj' '-o <path>' 'default: -O2'; do
    grep -Fq -- "$text" <<<"$HELP" || fail "help is missing: $text"
done
pass "version and help"

compile_run_all_opts() {
    local rel=$1 expected=$2 name base opt case_dir actual
    name=${rel%.mira}
    base=${name##*/}
    for opt in 0 1 2 3; do
        case_dir="$WORK/run/${base}-O${opt}"
        mkdir -p "$case_dir"
        (cd "$case_dir" && "$MIRA" "-O$opt" "$TEST_DIR/$rel" >/dev/null) \
            || fail "$rel O$opt compile"
        actual=$(cd "$case_dir" && timeout 30 "./$base$EXE_SUFFIX" | tr -d '\r') \
            || fail "$rel O$opt run"
        [ "$actual" = "$expected" ] \
            || fail "$rel O$opt output mismatch; got '$actual'"
    done
    pass "$rel O0-O3"
}

expect_compile_error() {
    local rel=$1 fragment=$2 message rc
    set +e
    message=$(cd "$WORK" && "$MIRA" "$TEST_DIR/$rel" 2>&1)
    rc=$?
    set -e
    [ "$rc" -ne 0 ] || fail "$rel unexpectedly compiled"
    grep -Fq -- "$fragment" <<<"$message" \
        || fail "$rel diagnostic missing '$fragment'"
    pass "$rel diagnostic"
}

# CLI parser and output modes.
"$GCC" -std=c11 -Wall -Wextra -Werror -I "$PLATFORM_DIR" \
    "$TEST_DIR/cli_parse_test.c" "$PLATFORM_DIR/cli.c" \
    -o "$WORK/cli_parse_test$EXE_SUFFIX"
"$WORK/cli_parse_test$EXE_SUFFIX" >/dev/null

"$GCC" -std=c11 -Wall -Wextra -Werror -I "$PLATFORM_DIR" \
    "$TEST_DIR/ir_dump_test.c" "$PLATFORM_DIR/codegen/ir_dump.c" \
    -o "$WORK/ir_dump_test$EXE_SUFFIX"
"$WORK/ir_dump_test$EXE_SUFFIX" "$WORK/invalid.ir" >/dev/null

"$GCC" -std=c11 -Wall -Wextra -Werror -I "$PLATFORM_DIR" \
    "$TEST_DIR/asm_writer_test.c" "$PLATFORM_DIR/codegen/asm_writer.c" \
    -o "$WORK/asm_writer_test$EXE_SUFFIX"
"$WORK/asm_writer_test$EXE_SUFFIX" >/dev/null

mkdir -p "$WORK/cli"
(
    cd "$WORK/cli"
    "$MIRA" -O3 "$TEST_DIR/final_ir_pipeline.mira" -o "custom$EXE_SUFFIX" >/dev/null
    [ "$(./custom$EXE_SUFFIX | tr -d '\r')" = '42' ]
    "$MIRA" -O3 --emit=ir "$TEST_DIR/final_ir_pipeline.mira" -o final.ir >/dev/null
    grep -q '^;; Mira IR dump' final.ir
    "$MIRA" -O3 -S "$TEST_DIR/asm_emit_full.mira" -o full.s >/dev/null
    "$GCC" -c full.s -o "full$OBJ_SUFFIX"
    "$MIRA" -O3 -c "$TEST_DIR/final_ir_pipeline.mira" -o "standalone$OBJ_SUFFIX" >/dev/null
    [ -s "standalone$OBJ_SUFFIX" ]
    [ ! -e "standalone$EXE_SUFFIX" ]
) || fail "CLI artifact modes"
pass "CLI parser, IR, assembly and object modes"

# Representative exact-output execution coverage.
compile_run_all_opts 'modern_typed_syntax.mira' '42'
compile_run_all_opts 'regression_float_var_arith.mira' $'1\n1.12751'
compile_run_all_opts 'regression_infix_line_continuation.mira' \
    $'4611686018427387925\n-4611686018427387937\n228\n21'
compile_run_all_opts 'regression_short_circuit.mira' \
    $'0\n0\n1\n0\n1\n1\n1\n2\n1\n2\n1\n3\n0\n1\n3\n0\n3\n1\n3\n1\n4\n1\n4\n1\n5\n1\n6'
compile_run_all_opts 'regression_branch_return.mira' $'-1\n15\n100\n4\n7\n9'
compile_run_all_opts 'stdlib_core_modules.mira' \
    $'1\n1\n1\n1\n1\n1\n4\n1\n1\n2\n66\n-42\n1\n1'
compile_run_all_opts 'stdlib_data_modules.mira' \
    $'3\n10\n25\n30\n1\n1\n0\n77\n\n-7\n12\n22\n1\n0\n2\n-7\n-2\n0\n5\n5\n9\n12\n12\n-7\n12\n-7\n99\n-7\n6\n0'

# Typed SSA and ownership/control-flow regressions.
compile_run_all_opts 'types/ssa_typed_values.mira' $'2.5\n2.5\ntyped\n1\nvoid'
compile_run_all_opts 'types/f64_comparisons_valid.mira' \
    $'0\n1\n1\n1\n1\n1\n1\n0\n0\n1\n0\n0\n0\n0'
compile_run_all_opts 'types/if_owned_string_phi_valid.mira' \
    $'mira\ntyped\nmixed\nborrowed'
compile_run_all_opts 'types/switch_try_tail_values_valid.mira' \
    $'11\n22\n11\n1.25\n2.5\nswitch-left\nswitch-right\n3.5\ntry-normal'

# Module namespace and imported ABI coverage.
compile_run_all_opts 'modules/import_math.mira' $'42\n100'
compile_run_all_opts 'modules/import_alias.mira' '23'
compile_run_all_opts 'modules/stdlib_abi_types.mira' $'9\n12345\n77'

# Representative strict diagnostics; all remaining fixtures stay in the tree.
expect_compile_error 'types/typed_const_i64_error.mira' \
    "constant 'bad': expected i64, got f64"
expect_compile_error 'types/call_extra_argument_error.mira' \
    'expects 1 arguments, got 2'
expect_compile_error 'types/assignment_type_error.mira' \
    'assignment to count: expected i64, got str'
expect_compile_error 'types/ptr_string_equality_type_error.mira' \
    'expected matching types, got ptr and str'
expect_compile_error 'types/condition_void_value_error.mira' \
    'returns void and cannot be used as a value'
expect_compile_error 'modules/arity_error_import.mira' \
    "function 'arity_error_dep.pair' expects 2 arguments, got 3"
expect_compile_error 'modules/import_cycle_main.mira' 'module import cycle'

echo "FORMAL REGRESSION PASS ($PASS suites)"
