# Mira gradual types: final regression and performance guard

Date: 2026-08-15 (Asia/Shanghai)
Result: **PASS on Windows; native Linux verification pending because WSL cannot start.**

## Scope and revisions

- Baseline: `9b7b628c69c8d765e8559c49ea2f0b40e17e9a5b`
- Measured final implementation: `fa0997b77b86b49aa8c4a91169c1b73f0ee76633`
- Starting Task 6 HEAD: `646064b8e11adeb1e5ffa7a03f29ad08271719c5`
- Host: `QAQAHZH`, Microsoft Windows `10.0.26200.9168`, x64, 16 logical processors
- Toolchain: PowerShell `7.6.4`, TDM-GCC `10.3.0`, GNU Make `3.82.90`

Task 6 found and fixed three test/compatibility gaps before measurement:

1. Both gradual-type runners rejected the specified `-Group all` argument before executing tests. The runner now accepts `all` and executes the four existing groups.
2. The new checker rejected the existing stdlib `ptr` contracts. `ptr` is now a strict semantic type that lowers to `SSA_TYPE_PTR`; it remains distinct from `str`. Opaque list/memory handles remain `ptr`, while text paths, contents, messages, input, and string results use `str`. Runtime ABI and ownership metadata are unchanged. Legacy `mem_alloc` retains its existing integer compatibility type.
3. The baseline builtin-table test expected `__mira_time_sleep` to be Windows-only even though the baseline table already marked it `STDLIB_PLATFORM_ALL`. The stale test assertion now matches the table.

## Reproducible correctness commands

```powershell
mingw32-make -C win clean all
powershell -NoProfile -ExecutionPolicy Bypass -File win/tests/run_gradual_types.ps1 -Group all
powershell -NoProfile -ExecutionPolicy Bypass -File win/tests/run_modules.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File win/tests/run_float_var_arith.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File win/tests/run_infix_line_continuation.ps1
```

The short-circuit, branch-return, stdlib core, and stdlib data fixtures were compiled and run at each of `-O0`, `-O1`, `-O2`, and `-O3`; each output was compared to an explicit line-for-line golden value.

```powershell
gcc -O0 -g -Iwin win/tests/stdlib_builtin_table_test.c win/codegen/stdlib_builtins.c -o win/tests/stdlib_builtin_table_test.exe
gcc -O0 -g -Iwin -Iwin/codegen win/tests/ssa_ref_suspend_test.c win/codegen/ssa_ref.c win/codegen/stdlib_builtins.c -o win/tests/ssa_ref_suspend_test.exe
gcc -O0 -g -Iwin -Iwin/codegen win/tests/ssa_ref_concurrency_test.c win/codegen/ssa_ref.c win/codegen/stdlib_builtins.c -o win/tests/ssa_ref_concurrency_test.exe
```

Each C executable was then run directly and required exit code zero.

### Pass counts

- Gradual types: 70 Mira source fixtures: 53 expected diagnostics and 17 accepted fixtures. The accepted SSA fixture compiled and ran at O0-O3; two C metadata checks also passed.
  - declarations: 5 expected diagnostics, 2 accepted/run fixtures, 2 C metadata checks
  - calls: 30 expected diagnostics, 7 accepted fixtures
  - expressions: 18 expected diagnostics, 7 accepted/run fixtures
  - SSA: 1 fixture at O0-O3 plus O0 IR assertions for float, string, pointer, bool, and void lowering
- Modules: 21 positive executions (five programs at O0-O3 plus one O0 program) and 8 unique diagnostic/cycle fixtures passed. Provenance/location assertions also passed.
- Float-variable arithmetic: 1 fixture at O0-O3 passed with `1` and `1.12751`.
- Infix line continuation: 1 fixture at O0-O3 matched the C oracle:
  `4611686018427387925,-4611686018427387937,228,21`.
- Short circuit: 1 fixture at O0-O3 matched its 27-line golden output.
- Branch return: 1 fixture at O0-O3 matched `-1,15,100,4,7,9`.
- Stdlib core: 1 fixture at O0-O3 matched its 14-line golden output.
- Stdlib data: 1 fixture at O0-O3 matched its 30-line golden output, including the intentional blank line after `77`.
- C metadata: builtin table, `ssa_ref_suspend_test`, and `ssa_ref_concurrency_test` all passed.

### Optimization-level matrix

| Suite | O0 | O1 | O2 | O3 |
|---|---:|---:|---:|---:|
| typed SSA values and IR metadata | PASS | PASS | PASS | PASS |
| module namespace programs | PASS | PASS | PASS | PASS |
| float-variable arithmetic | PASS | PASS | PASS | PASS |
| infix line continuation | PASS | PASS | PASS | PASS |
| short circuit | PASS | PASS | PASS | PASS |
| branch return | PASS | PASS | PASS | PASS |
| stdlib core | PASS | PASS | PASS | PASS |
| stdlib data | PASS | PASS | PASS | PASS |

## Performance method and results

The baseline was extracted with `git archive 9b7b628` into an independent temporary directory. The current branch was never reset. Baseline and current trials were interleaved. All medians are the middle of three trials.

### Clean compiler/runtime build

Command under each tree:

```powershell
mingw32-make -C win clean all
```

| Tree | Trial 1 (ms) | Trial 2 (ms) | Trial 3 (ms) | Median (ms) | Difference |
|---|---:|---:|---:|---:|---:|
| baseline | 16,843.439 | 16,700.569 | 16,782.404 | 16,782.404 | — |
| current | 17,263.424 | 17,269.976 | 17,556.170 | 17,269.976 | +2.905% |

The roughly 0.49 s increase is consistent across trials and includes compiling/linking the added checker implementation in a full clean build. No new compiler warnings were emitted by the Windows build.

### Full gradual suite wall time

The full final runner was measured end-to-end three times:

| Trial 1 (ms) | Trial 2 (ms) | Trial 3 (ms) | Median (ms) |
|---:|---:|---:|---:|
| 4,859.287 | 4,762.980 | 4,753.532 | 4,762.980 |

There is no honest baseline percentage: `run_gradual_types.ps1` and its fixtures do not exist at `9b7b628`. The comparable baseline/current typed workload below is used for the compiler guard.

### Comparable typed-syntax compile and output size

To reduce cold-cache and process-start noise, each trial ran 50 consecutive commands:

```powershell
cmd /d /c "for /L %i in (1,1,50) do @mira.exe -O3 tests\modern_typed_syntax.mira >nul"
```

| Tree | Batch trials, 50 compiles (ms) | Median batch (ms) | Median per compile (ms) | Difference |
|---|---|---:|---:|---:|
| baseline | 606.522, 586.620, 596.938 | 596.938 | 11.939 | — |
| current | 598.330, 588.421, 605.153 | 598.330 | 11.967 | +0.233% |

The +0.233% difference is below run-to-run scatter and is treated as measurement noise.

| Artifact | Baseline | Current | Difference |
|---|---:|---:|---:|
| `modern_typed_syntax.exe` O3 size | 3,584 B | 3,584 B | 0.000% |

Both executables had SHA-256 `4FDFA61F984B471608361CB6F57CA77D123FD14A6BD1F783BCBFFF247710C133`. Their output SHA-256 was also identical: `73475CB40A568E8DA8A045CED110137E159F890AC4DA883B6B17DC651B3A8049`.

### Representative runtimes

The integer fixture is shorter than process startup. Each trial therefore launched it 1,000 times through one `cmd.exe` loop. The result is explicitly process-launch dominated and is not used to claim a code-generation speedup.

| Integer O3 | Batch trials, 1,000 launches (ms) | Median batch (ms) | Median/launch (ms) | Difference |
|---|---|---:|---:|---:|
| baseline | 7,228.477, 5,530.390, 7,230.417 | 7,228.477 | 7.228 | — |
| current | 6,283.356, 5,648.659, 7,313.700 | 6,283.356 | 6.283 | -13.075% |

The large trial scatter confirms startup/scheduling noise. Both integer executables were 3,584 B and had identical SHA-256 `6E257C4B6FB0694B8428668495C8281B09D22F554C85D4A6D5EC31045B13D182`.

The float fixture performs 20,000,000 loop iterations. Each trial ran five complete processes:

| Float O3 | Batch trials, 5 runs (ms) | Median batch (ms) | Median/run (ms) | Difference |
|---|---|---:|---:|---:|
| baseline | 546.838, 580.070, 541.153 | 546.838 | 109.368 | — |
| current | 551.501, 535.806, 539.716 | 539.716 | 107.943 | -1.302% |

Both float executables were 3,584 B and had identical SHA-256 `93B9AA7C88843DA9C5E78F9A02381CA4AE14EE6C972DC5BC827589EA9CE8C5F6`. Because the machine-code artifacts are byte-identical, the measured runtime difference is scheduling noise, not a generated-code change.

## Windows/Linux mirror and native-Linux status

A SHA-256 manifest covered 86 platform-neutral pairs: `typecheck.[ch]`, `mira.h`, `main.c`, the parser files, `codegen/ssa_builder.c`, the design/plan, changed stdlib files, metadata C tests, and every `tests/types/*.mira` except the target-specific SSA ABI fixture. All 86 pairs matched. Manifest SHA-256:

`875F8DF25B6F267074A001BB57C3E9AEA780E29065970BBC28033BA44BDB3A0C`

Expected platform-specific hashes:

- Windows runner: `4DA40A446FCCF85DEA753F6755B001D4552E8AA2BE9F7DD68F5BBB3CCC4B8168`
- Linux runner: `F18D5D7C862946457D3649C0AEC75D0BA8E707B6A0A62AB6460394336F8C3721`
- Windows SSA fixture: `52710345931E7896271D11EF88863D7D58B34320841376BFB800C4F5566BB674`
- Linux SSA fixture: `BFC5EC4DE5F7ABE351D4A4D12677A63B6548AEDAE155983168E5084238DE9818`

The runner difference is binary suffix/Win64-vs-SysV IR checking. The Linux SSA fixture additionally exercises mixed and overflow SysV parameters.

Native Linux remains **pending**, not passed. Exact probe:

```powershell
wsl.exe -e sh -lc "uname -a"
```

Result: exit `-1`, `Wsl/Service/CreateInstance/E_ACCESSDENIED`.

A Windows-hosted `mingw32-make -C linux clean all` is not a substitute for native Linux: `clean` cannot invoke POSIX `rm` on this host, and an `all` attempt selects Windows/COFF code through the host compiler while the Linux Makefile omits those objects, producing unresolved COFF/PE symbols. No native Linux test or runtime claim is made.

## Final guard

Before the report commit, `git diff --check` passed, the mirror manifest had zero mismatches, and `git status --short` contained no executable, object, IR, or test-output paths. Generated files are ignored and no generated artifact is tracked.
