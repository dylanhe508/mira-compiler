# Gradual static types final fix report

Date: 2026-08-15

Fix baseline: `7671f86`

Status: all final-review Critical, Important, and requested Minor findings fixed

## Commits

- `487efb1 test(types): expose final review regressions`
  - Adds the persistent Win/Linux RED fixtures and C assertions before production fixes.
- `869610b fix(types): enforce frontend contracts and float comparisons`
  - Fixes C1-C3, I1-I2, typed-constant origins, void origins, and lexer-state restoration.
- `e5e8e74 fix(ssa): preserve typed control-flow ownership`
  - Fixes typed if/mem2reg PHIs, ownership-safe PHI destruction, and independent switch/try SSA stacks.

## Review finding disposition

| Finding | Result | Implementation |
|---|---|---|
| C1 legacy unannotated `mut` | Fixed | Unannotated flow types remain checker-local in `var_flow_types`; only `var_type_explicit` constrains SSA/global slots. The SSA builder infers each unannotated store from its current VReg. |
| C2 typed constants | Fixed | `CONST_INT`, `CONST_DOUBLE`, and `CONST_STR` strictly map to `i64`, `f64`, and `str`. Every constant stores its initializer origin, which is used for the mismatch. Nonmatching `bool`, `ptr`, and `void` declarations are rejected without entering unsafe codegen. |
| C3 six `f64` comparisons | Fixed | The SSA builder selects `FCMP_*` only when both operands are floating point. Lowering combines `UCOMISD` condition codes with parity so ordered `==`, `<`, `<=` reject NaN, `!=` accepts unordered, and `>`, `>=` remain false for NaN. Integer and pointer comparison selection is unchanged. |
| C4 typed if/alloca PHIs | Fixed | Direct if PHIs and mem2reg PHIs derive `FLOAT`, `PTR`, or `INT` from incoming/store definitions. Both PHI-destruction COPY layers retain the PHI type. |
| C4 owned string merge | Fixed | Ownership provenance is resolved before PHI destruction. All-owned merges transfer `needs_free` to the final merged definition. Mixed owned/borrowed merges use a conditional owner token: the owned edge carries the pointer and the borrowed edge carries null. COPY aliases share the token; nested PHI transfer clears the old owner. CALL/STORE/RET escape analysis follows the token. |
| C5 switch/try SSA | Fixed | Every switch arm/default and try body/catch starts from the same entry-vstack snapshot. Unreachable terminated exits are excluded. Live exits merge their common stack prefix using typed n-way PHIs. No-catch legacy try protocol remains unchanged. |
| I1 exact call argc/receiver | Fixed | Recursive and shunting-yard parser paths write `call_argc` plus `call_receiver_count` directly on IR. Identifiers ending in `?` are accepted by the fallback scanner, and method receivers are counted canonically rather than inferred from signature defaults. |
| I2 while complexity | Fixed | `IR_WHILE_INF` retains its parsed condition node. The checker consumes that metadata and the old per-loop source rescan was removed; the existing per-source call scan remains cached once per source. |
| Minor: per-type void origin | Fixed | Explicit-result mismatch uses the origin for the actual conflicting type, so the later branch value is reported. |
| Minor: lexer `line_start` | Fixed | Lexer push/peek/pop snapshots and restores `line_start`, preserving the importing file's exact column. |

## RED evidence

The persistent tests were committed in `487efb1` before production changes. The initial focused run against the review baseline/current pre-fix implementation showed:

- C1: `legacy_unannotated_mut_valid.mira` printed integer garbage (`4202518`) at O0/O3 instead of `typed`.
- C2: incompatible `i64`, `f64`, `str`, `bool`, and `ptr` constant initializers were accepted; the string-from-integer path could proceed into unsafe codegen. The `void` case was rejected by the annotation rule but lacked the unified strict-constant matrix.
- C3: negative, signed-zero, and NaN comparison results differed from the required IEEE truth table at O0/O3.
- C4: f64/string PHIs printed integer-bit/address garbage. The first owned `str-cat` branch was freed before the merge use and produced corrupted/blank output.
- C5: `switch_try_tail_values_valid.mira` produced `0,22,0` instead of `11,22,11`.
- I1: `ready?(1, 2)` and `point.add(1, 2)` compiled despite their one-explicit-argument contracts.
- I2: the structural condition-metadata assertion failed because `IR_WHILE_INF` did not retain its condition; the checker rescanned from source start for every such loop.
- Void-origin diagnostic pointed to Line 3, Column 16 instead of the conflicting Line 5, Column 9.
- Lexer-state C test failed because an import push/pop did not restore the parent `line_start`.

## Focused GREEN evidence

All focused cases were rerun after a forced compiler rebuild.

- C1 O0/O3: `typed`.
- C2: all six typed-constant negative fixtures are rejected with the expected type and source column; `const bad: str = 1` exits diagnostically and does not crash.
- C3 O0/O3 exact output:

  ```text
  0
  1
  1
  1
  1
  1
  1
  0
  0
  1
  0
  0
  0
  0
  ```

  This covers negative values, `+0.0/-0.0`, and all six operations with NaN.

- C4 typed if PHIs O0/O3 exact output:

  ```text
  1.5
  2.5
  left
  right
  3.5
  direct-right
  11
  22
  ```

- C4 owned/all-owned and mixed-owned/borrowed O0/O3 exact output:

  ```text
  mira
  typed
  mixed
  borrowed
  ```

- C5 i64/f64/str switch and normal try O0/O3 exact output:

  ```text
  11
  22
  11
  1.25
  2.5
  switch-left
  switch-right
  3.5
  try-normal
  ```

- Real error/catch regression `modern_error_catch.mira` prints `42` at O0 and O3.
- I1: both extra-argument fixtures reject; the valid implicit-receiver method prints `42`.
- I2: structural condition metadata, strict `while (true)`, strict rejection of `while (1)`, and legacy `while 1` all pass.
- Lexer-state C test prints `LEXER STATE PASS`; void mismatch reports Line 5, Column 9.

## Final verification

The final verification was run after commit `e5e8e74` from a clean compiler/runtime build.

| Command/suite | Result |
|---|---|
| `mingw32-make -C win clean all` | PASS |
| `run_gradual_types.ps1 -Group all` | PASS: declarations, calls, expressions, SSA |
| `run_modules.ps1` | PASS: namespace O0-O3, diagnostics, cycles |
| `run_float_var_arith.ps1` | PASS O0-O3 |
| `run_infix_line_continuation.ps1` | PASS O0-O3; exact C-oracle output |
| short-circuit golden | PASS O0-O3, 27 lines |
| branch-return golden | PASS O0-O3: `-1,15,100,4,7,9` |
| stdlib-core golden | PASS O0-O3, 14 lines |
| stdlib-data golden | PASS O0-O3, 30 lines including the intentional blank line |
| gradual C tests | PASS: type metadata, program-free metadata, lexer state |
| standalone C tests | PASS: builtin table, SSA ref suspend, SSA ref concurrency |
| `git diff --check` | PASS |

## Win/Linux synchronization

- Of 31 modified Win/Linux file pairs, 29 are byte-identical after newline normalization.
- The two whole-file differences are expected:
  - `codegen/ssa_lower.c` retains pre-existing target/ABI differences; the added FCMP lowering hunk is identical.
  - `tests/run_gradual_types.ps1` retains platform-specific binary-path handling; every added assertion and golden is equivalent.
- All modified Linux translation units (`lexer`, `memory`, `typecheck`, parser aggregate, SSA builder/lower/mem2reg/regalloc) passed `gcc -std=c11 -fsyntax-only -Ilinux` on the available host toolchain.

Native Linux execution was not available on this Windows host: `wsl.exe --status` failed with `Wsl/EnumerateDistros/Service/E_ACCESSDENIED`. This is the only verification limitation; there is no known remaining functional failure or downgraded review requirement.

## Residual concerns

No known Critical, Important, or requested Minor issue remains. The conditional owner-token path deliberately preserves existing ABI and builtin ownership contracts; the final SSA suite covers all-owned, mixed owned/borrowed, direct typed PHIs, and escape through user calls at O0/O3. A temporary structural COPY-alias probe also passed and was removed, leaving no generated probe or IR artifact in the worktree.
