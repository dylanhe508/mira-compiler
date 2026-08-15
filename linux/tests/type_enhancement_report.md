# Mira gradual types: final regression and performance guard

Date: 2026-08-15 (Asia/Shanghai)
Result: **PASS on Windows; native Linux verification pending because WSL cannot start.**

## Scope and revisions

- Baseline: `9b7b628c69c8d765e8559c49ea2f0b40e17e9a5b`
- Measured performance implementation: `fa0997b77b86b49aa8c4a91169c1b73f0ee76633`
- Post-measure correctness review: `ac2b92d8d173373ed8ced58e814e12ea8817989c`
- Starting Task 6 HEAD: `646064b8e11adeb1e5ffa7a03f29ad08271719c5`
- Host: `QAQAHZH`, Microsoft Windows `10.0.26200.9168`, x64, 16 logical processors
- Toolchain: PowerShell `7.6.4`, TDM-GCC `10.3.0`, GNU Make `3.82.90`

Task 6 found and fixed four test/compatibility gaps. The first three were fixed before measurement; the comparison fix followed review:

1. Both gradual-type runners rejected the specified `-Group all` argument before executing tests. The runner now accepts `all` and executes the four existing groups.
2. The new checker rejected the existing stdlib `ptr` contracts. `ptr` is now a strict semantic type that lowers to `SSA_TYPE_PTR`; it remains distinct from `str`. Opaque list/memory handles remain `ptr`, while text paths, contents, messages, input, and string results use `str`. Runtime ABI and ownership metadata are unchanged. Legacy `mem_alloc` retains its existing integer compatibility type.
3. The baseline builtin-table test expected `__mira_time_sleep` to be Windows-only even though the baseline table already marked it `STDLIB_PLATFORM_ALL`. The stale test assertion now matches the table.
4. Equality previously skipped operand compatibility and ordered comparisons did not enforce a common numeric type. User-written `==`/`!=` now require compatible candidate types, and `<`/`<=`/`>`/`>=` require the same numeric type (`i64` or `f64`). Completely unknown legacy expressions remain compatible. The parser's internal logical-booleanization comparison is explicitly marked so this stricter user rule does not reject legacy `&&`/`||` lowering.

## Reproducible correctness commands

```powershell
mingw32-make -C win clean all
powershell -NoProfile -ExecutionPolicy Bypass -File win/tests/run_gradual_types.ps1 -Group all
powershell -NoProfile -ExecutionPolicy Bypass -File win/tests/run_modules.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File win/tests/run_float_var_arith.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File win/tests/run_infix_line_continuation.ps1
```

### Exact representative regression commands and goldens

Each of the four programs used the same explicit working directory, `C:\Users\qa345\Documents\Codex\2026-08-12\new-chat\work\mira-type-enhancement\win`, but distinct source and output paths:

| Fixture | Source from that cwd | Executable from that cwd |
|---|---|---|
| short circuit | `tests\regression_short_circuit.mira` | `.\regression_short_circuit.exe` |
| branch return | `tests\regression_branch_return.mira` | `.\regression_branch_return.exe` |
| stdlib core | `tests\stdlib_core_modules.mira` | `.\stdlib_core_modules.exe` |
| stdlib data | `tests\stdlib_data_modules.mira` | `.\stdlib_data_modules.exe` |

This is the exact reproducible O0-O3 compile, direct-run, exit-code, line-count, and per-line golden comparison. The empty string in the stdlib-data golden is intentional: it is the blank line immediately after `77`.

```powershell
Set-Location 'C:\Users\qa345\Documents\Codex\2026-08-12\new-chat\work\mira-type-enhancement\win'
$cases = @(
    @{
        Name = 'short circuit'
        Source = 'tests\regression_short_circuit.mira'
        Exe = '.\regression_short_circuit.exe'
        Golden = @(
            '0','0','1','0','1','1','1','2','1','2','1','3','0','1','3',
            '0','3','1','3','1','4','1','4','1','5','1','6'
        )
    },
    @{
        Name = 'branch return'
        Source = 'tests\regression_branch_return.mira'
        Exe = '.\regression_branch_return.exe'
        Golden = @('-1','15','100','4','7','9')
    },
    @{
        Name = 'stdlib core'
        Source = 'tests\stdlib_core_modules.mira'
        Exe = '.\stdlib_core_modules.exe'
        Golden = @('1','1','1','1','1','1','4','1','1','2','66','-42','1','1')
    },
    @{
        Name = 'stdlib data'
        Source = 'tests\stdlib_data_modules.mira'
        Exe = '.\stdlib_data_modules.exe'
        Golden = @(
            '3','10','25','30','1','1','0','77','','-7','12','22','1','0','2',
            '-7','-2','0','5','5','9','12','12','-7','12','-7','99','-7','6','0'
        )
    }
)
foreach ($case in $cases) {
    foreach ($opt in 0..3) {
        & .\mira.exe "-O$opt" $case.Source
        if ($LASTEXITCODE -ne 0) {
            throw "$($case.Name) O$opt compile failed"
        }
        $actual = @(& $case.Exe)
        if ($LASTEXITCODE -ne 0) {
            throw "$($case.Name) O$opt run failed"
        }
        if ($actual.Count -ne $case.Golden.Count) {
            throw "$($case.Name) O$opt line count: expected $($case.Golden.Count), got $($actual.Count)"
        }
        for ($line = 0; $line -lt $case.Golden.Count; $line++) {
            if ($actual[$line] -cne $case.Golden[$line]) {
                throw "$($case.Name) O$opt line $($line + 1): expected '$($case.Golden[$line])', got '$($actual[$line])'"
            }
        }
    }
}
```

### Exact C metadata commands

The C tests used repository-root cwd `C:\Users\qa345\Documents\Codex\2026-08-12\new-chat\work\mira-type-enhancement`. The following are the Task 6 report's recorded, reproducible build commands; the separate raw compiler transcript was not retained. The verification block directly runs the actual output paths, requires exit zero, and checks exact stdout (the builtin-table test is intentionally silent).

```powershell
Set-Location 'C:\Users\qa345\Documents\Codex\2026-08-12\new-chat\work\mira-type-enhancement'
gcc -O0 -g -Iwin win/tests/stdlib_builtin_table_test.c win/codegen/stdlib_builtins.c -o win/tests/stdlib_builtin_table_test.exe
if ($LASTEXITCODE -ne 0) { throw 'stdlib_builtin_table_test build failed' }
gcc -O0 -g -Iwin -Iwin/codegen win/tests/ssa_ref_suspend_test.c win/codegen/ssa_ref.c win/codegen/stdlib_builtins.c -o win/tests/ssa_ref_suspend_test.exe
if ($LASTEXITCODE -ne 0) { throw 'ssa_ref_suspend_test build failed' }
gcc -O0 -g -Iwin -Iwin/codegen win/tests/ssa_ref_concurrency_test.c win/codegen/ssa_ref.c win/codegen/stdlib_builtins.c -o win/tests/ssa_ref_concurrency_test.exe
if ($LASTEXITCODE -ne 0) { throw 'ssa_ref_concurrency_test build failed' }
```

```powershell
$repoRoot = 'C:\Users\qa345\Documents\Codex\2026-08-12\new-chat\work\mira-type-enhancement'
Set-Location $repoRoot
$cCases = @(
    @{ Name = 'builtin table'; Exe = '.\win\tests\stdlib_builtin_table_test.exe'; Golden = @() },
    @{ Name = 'SSA ref suspend'; Exe = '.\win\tests\ssa_ref_suspend_test.exe'; Golden = @('ssa_ref_suspend_test: PASS') },
    @{ Name = 'SSA ref concurrency'; Exe = '.\win\tests\ssa_ref_concurrency_test.exe'; Golden = @('ssa_ref_concurrency_test: PASS') }
)
foreach ($case in $cCases) {
    $actual = @(& $case.Exe)
    if ($LASTEXITCODE -ne 0) { throw "$($case.Name) exited $LASTEXITCODE" }
    if (($actual -join "`n") -cne ($case.Golden -join "`n")) {
        throw "$($case.Name) stdout mismatch"
    }
}
```

### Pass counts

- Gradual types: 76 Mira source fixtures: 58 expected diagnostics and 18 accepted fixtures. The accepted SSA fixture compiled and ran at O0-O3; two C metadata checks also passed.
  - declarations: 5 expected diagnostics, 2 accepted/run fixtures, 2 C metadata checks
  - calls: 30 expected diagnostics, 7 accepted fixtures
  - expressions: 23 expected diagnostics, 8 accepted/run fixtures
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

## Exact measurement reproduction commands

The current-tree working directory was:

`C:\Users\qa345\Documents\Codex\2026-08-12\new-chat\work\mira-type-enhancement`

The independent baseline root resolved to:

`C:\Users\qa345\AppData\Local\Temp\mira-task6-baseline-8869d56c60fe42fabc696048cc0b5b13`

In both trees the relative Windows build directory was `.\win`. The numeric trial values are preserved in this report; the ephemeral PowerShell result objects and console transcript were not retained. Therefore, the blocks below are the exact reproducible forms of the recorded procedure, not a claim that raw timing files still exist.

Baseline extraction (the generated GUID resolved to the exact path above):

```powershell
$baselineRoot = Join-Path $env:TEMP ('mira-task6-baseline-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $baselineRoot | Out-Null
$archive = Join-Path $baselineRoot 'baseline.tar'
git archive --format=tar --output=$archive 9b7b628
tar -xf $archive -C $baselineRoot
```

Three interleaved clean-build trials:

```powershell
$baselineRoot = 'C:\Users\qa345\AppData\Local\Temp\mira-task6-baseline-8869d56c60fe42fabc696048cc0b5b13'
$currentRoot = 'C:\Users\qa345\Documents\Codex\2026-08-12\new-chat\work\mira-type-enhancement'
$buildResults = @()
foreach ($trial in 1..3) {
    foreach ($tree in @(
        @{ Name = 'baseline'; Root = $baselineRoot },
        @{ Name = 'current'; Root = $currentRoot }
    )) {
        $elapsed = (Measure-Command {
            & mingw32-make -C (Join-Path $tree.Root 'win') clean all *> $null
            if ($LASTEXITCODE -ne 0) { throw "$($tree.Name) build failed" }
        }).TotalMilliseconds
        $buildResults += [pscustomobject]@{
            Trial = $trial; Tree = $tree.Name; Milliseconds = $elapsed
        }
    }
}
$buildResults
```

Three full current-suite trials:

```powershell
Set-Location 'C:\Users\qa345\Documents\Codex\2026-08-12\new-chat\work\mira-type-enhancement'
$gradualResults = foreach ($trial in 1..3) {
    $elapsed = (Measure-Command {
        & powershell -NoProfile -ExecutionPolicy Bypass -File win/tests/run_gradual_types.ps1 -Group all *> $null
        if ($LASTEXITCODE -ne 0) { throw 'gradual all failed' }
    }).TotalMilliseconds
    [pscustomobject]@{ Trial = $trial; Milliseconds = $elapsed }
}
$gradualResults
```

Three interleaved 50-compile batches for the exact fixture `win/tests/modern_typed_syntax.mira`:

```powershell
$trees = @(
    @{ Name = 'baseline'; Root = 'C:\Users\qa345\AppData\Local\Temp\mira-task6-baseline-8869d56c60fe42fabc696048cc0b5b13' },
    @{ Name = 'current'; Root = 'C:\Users\qa345\Documents\Codex\2026-08-12\new-chat\work\mira-type-enhancement' }
)
$typedResults = @()
foreach ($trial in 1..3) {
    foreach ($tree in $trees) {
        $win = Join-Path $tree.Root 'win'
        $mira = Join-Path $win 'mira.exe'
        $command = "for /L %i in (1,1,50) do @`"$mira`" -O3 tests\modern_typed_syntax.mira >nul"
        Push-Location $win
        try {
            $elapsed = (Measure-Command {
                & cmd.exe /d /c $command
                if ($LASTEXITCODE -ne 0) { throw "$($tree.Name) typed batch failed" }
            }).TotalMilliseconds
        } finally {
            Pop-Location
        }
        $typedResults += [pscustomobject]@{
            Trial = $trial; Tree = $tree.Name; Milliseconds = $elapsed
        }
    }
}
$typedResults
```

The three representative O3 sources were `win/tests/modern_typed_syntax.mira`, `win/tests/regression_int_o2_branch_loop.mira`, and `win/tests/regression_float_var_arith.mira`. Each tree was compiled, outputs were compared before timing, and sizes/hashes were collected as follows:

```powershell
foreach ($tree in $trees) {
    $win = Join-Path $tree.Root 'win'
    Push-Location $win
    try {
        & .\mira.exe -O3 tests\modern_typed_syntax.mira
        & .\mira.exe -O3 tests\regression_int_o2_branch_loop.mira
        & .\mira.exe -O3 tests\regression_float_var_arith.mira
        if ($LASTEXITCODE -ne 0) { throw "$($tree.Name) O3 compile failed" }

        $modernOut = ((& .\modern_typed_syntax.exe) -join "`n").TrimEnd("`r", "`n")
        $intOut = ((& .\regression_int_o2_branch_loop.exe) -join "`n").TrimEnd("`r", "`n")
        $floatOut = ((& .\regression_float_var_arith.exe) -join "`n").TrimEnd("`r", "`n")
        if ($modernOut -ne '42') { throw "$($tree.Name) modern output mismatch" }
        if ($intOut -ne '1236698126') { throw "$($tree.Name) integer output mismatch" }
        if ($floatOut -ne "1`n1.12751") { throw "$($tree.Name) float output mismatch" }

        Get-Item .\modern_typed_syntax.exe,
            .\regression_int_o2_branch_loop.exe,
            .\regression_float_var_arith.exe |
            Select-Object Name, Length
        Get-FileHash .\modern_typed_syntax.exe,
            .\regression_int_o2_branch_loop.exe,
            .\regression_float_var_arith.exe -Algorithm SHA256
        [Convert]::ToHexString(
            [Security.Cryptography.SHA256]::HashData(
                [Text.Encoding]::UTF8.GetBytes($modernOut)
            )
        )
    } finally {
        Pop-Location
    }
}
```

Three interleaved runtime batches used one `cmd.exe` loop per trial: 1,000 launches for the integer fixture and five launches for the 20,000,000-iteration float fixture.

```powershell
$runtimeResults = @()
foreach ($trial in 1..3) {
    foreach ($tree in $trees) {
        $win = Join-Path $tree.Root 'win'
        $intExe = Join-Path $win 'regression_int_o2_branch_loop.exe'
        $floatExe = Join-Path $win 'regression_float_var_arith.exe'
        $intCommand = "for /L %i in (1,1,1000) do @`"$intExe`" >nul"
        $floatCommand = "for /L %i in (1,1,5) do @`"$floatExe`" >nul"
        $integerMs = (Measure-Command {
            & cmd.exe /d /c $intCommand
            if ($LASTEXITCODE -ne 0) { throw "$($tree.Name) integer batch failed" }
        }).TotalMilliseconds
        $floatMs = (Measure-Command {
            & cmd.exe /d /c $floatCommand
            if ($LASTEXITCODE -ne 0) { throw "$($tree.Name) float batch failed" }
        }).TotalMilliseconds
        $runtimeResults += [pscustomobject]@{
            Trial = $trial; Tree = $tree.Name
            Integer1000Ms = $integerMs; Float5Ms = $floatMs
        }
    }
}
$runtimeResults
```

After the comparison-only review fix, the three current O3 executables were rebuilt and their sizes, hashes, and exact outputs were checked again with the preceding artifact block. They remained byte-identical to the measured `fa0997b` artifacts, so the process-dominated timing batches were not rerun and the performance table is not relabeled as an `ac2b92d` measurement.


## Windows/Linux mirror and native-Linux status

A SHA-256 manifest at the measured `fa0997b` commit covered 86 platform-neutral pairs: `typecheck.[ch]`, `mira.h`, `main.c`, the parser files, `codegen/ssa_builder.c`, the design/plan, changed stdlib files, metadata C tests, and every then-present `tests/types/*.mira` except the target-specific SSA ABI fixture. All 86 pairs matched. Its recorded manifest SHA-256 was:

`875F8DF25B6F267074A001BB57C3E9AEA780E29065970BBC28033BA44BDB3A0C`

At the final correctness commit `ac2b92d`, the same source rule includes the six added mirrored comparison fixtures: 92 pairs matched with zero mismatch, and the manifest SHA-256 was:

`AD22765B24E4BF6FF80C48E0CCEBFD11CA7CC2DD939ED4B3D53EB3E8DEA03782`

The complete manifest source list and exact deterministic generation command were:

```powershell
$rels = @(
    'typecheck.c', 'typecheck.h', 'mira.h', 'main.c',
    'parser/index.c', 'parser/helpers.c', 'parser/blocks.c',
    'codegen/ssa_builder.c',
    'docs/superpowers/plans/2026-08-14-gradual-static-types.md',
    'docs/superpowers/specs/2026-08-14-gradual-static-types-design.md',
    'stdlib/std/fs.mira', 'stdlib/std/io.mira', 'stdlib/std/list.mira',
    'stdlib/std/random.mira', 'stdlib/std/string.mira',
    'tests/stdlib_builtin_table_test.c', 'tests/type_metadata_test.c'
)
$rels += Get-ChildItem win/tests/types/*.mira |
    Where-Object { $_.Name -ne 'ssa_typed_values.mira' } |
    ForEach-Object { 'tests/types/' + $_.Name }
$rels = $rels | Sort-Object -Unique
$lines = foreach ($rel in $rels) {
    $winHash = (Get-FileHash (Join-Path win $rel) -Algorithm SHA256).Hash
    $linuxHash = (Get-FileHash (Join-Path linux $rel) -Algorithm SHA256).Hash
    if ($winHash -ne $linuxHash) { throw "mirror mismatch: $rel" }
    "$rel $winHash"
}
$manifestPath = Join-Path $env:TEMP 'mira-type-enhancement-manifest.txt'
[IO.File]::WriteAllText(
    $manifestPath, (($lines -join "`n") + "`n"),
    [Text.UTF8Encoding]::new($false)
)
[pscustomobject]@{
    PairCount = $rels.Count
    ManifestSHA256 = (Get-FileHash $manifestPath -Algorithm SHA256).Hash
}
```

Current platform-specific hashes:

- Windows runner: `5A83CDDFBCAF7CD82DE2065130D99DDA0113AA4C4471FDCCF6DFE24442B582E2`
- Linux runner: `8A00F8271EF791944AB7887E2BC3620CB2C99D284456441BC918B50DB145A20B`
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
