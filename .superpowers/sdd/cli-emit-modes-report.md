# CLI Explicit Emit Modes Report

Date: 2026-08-17
Branch: master
Implementation range: 79065b7..6382d34

## Delivered

- Unified CLI parsing for executable, GNU Intel assembly, internal IR, and object-only output.
- `-S` / `--emit=asm` writes assembler-consumable Intel-syntax `.s`.
- `--emit=ir` writes the finalized Mira machine IR.
- `-c` / `--emit=obj` writes an object without linking.
- `-o <path>` works consistently for all four output modes.
- Normal executable builds retain Mira's direct encoder and self-written linker.
- Windows and Linux source mirrors contain equivalent platform-neutral changes.
- README and `--help` describe the real O2 default and the new output modes.

## TDD Evidence

- CLI parser RED: unsupported/new options were not parsed consistently.
- Final IR pipeline RED: `-o` was treated as an input path.
- IR dump RED: an unknown opcode was silently emitted as a comment.
- Assembly RED: no writer existed; first implementation exposed duplicate file-level local labels.
- Artifact RED: executable `-o` was treated as the source file.

Each failure was retained as a focused regression test and then made GREEN.

## Fresh Verification

From repository root:

- `mingw32-make -C win clean` — exit 0.
- `mingw32-make -C win mira.exe` — exit 0.
- `mingw32-make -C win runtime` — exit 0.
- `win/tests/run_cli_parse.ps1` — `CLI PARSE PASS`.
- `win/tests/run_cli_emit_modes.ps1 -Group all` — `CLI EMIT MODES PASS`.
- `linux/tests/run_cli_emit_modes.ps1 -Group all -Mira win/mira.exe` — pass using the Windows host compiler against the Linux mirror fixtures/runner.
- `win/tests/run_gradual_types.ps1 -Group all` — all declarations, calls, expressions, and SSA groups pass.
- `win/tests/run_modules.ps1` — module namespace O0-O3, diagnostics, and cycle tests pass.
- `win/tests/run_float_var_arith.ps1` — pass.
- `win/tests/run_infix_line_continuation.ps1` — O0-O3 pass.
- `win/tests/run_hot_loop_codegen.ps1` — shift and nested affine checks pass.
- `win/tests/run_divrem_reuse.ps1` — O0-O3 and shape checks pass.
- Short-circuit, branch-return, stdlib-core, and stdlib-data fixtures match their recorded per-line goldens at O0-O3.
- `gcc -std=gnu11 -Ilinux -fsyntax-only linux/main.c linux/cli.c linux/codegen/asm_writer.c linux/codegen/ir_dump.c` — exit 0.
- `git diff --check` — clean.

## Artifact Stability

For `win/tests/regression_phi_inline.mira -O3`:

- executable size: 3584 bytes
- SHA-256: `950A8EF9D3739E2BAE9C5BDE299665B020590B0997F805632FC3100FAAC6DF30`

This exactly matches the pre-change baseline, showing that the default executable code-generation path did not change.

## Known Environment Limitation

Native Linux execution was not available: `wsl.exe -l -q` failed with
`Wsl/EnumerateDistros/Service/E_ACCESSDENIED`. Linux changes were therefore
checked by source mirroring, host-side syntax compilation, and the mirrored CLI
runner using the Windows compiler; this report does not claim a native Linux run.

## Mirror Manifest

Normalized LF/CRLF comparison over the changed `win/` files found 24 existing
counterpart paths. Eighteen are byte-equivalent after newline normalization.
Six retain pre-existing platform/test-runner differences: `Makefile`,
`run_affine_profitability.ps1`, `run_gradual_types.ps1`,
`run_hot_loop_codegen.ps1`, `run_mul_imm_strength.ps1`, and
`run_multi_factor_affine.ps1`. The CLI-specific hunks in those runners are
equivalent. Two Windows-only historical runners have no Linux counterpart:
`run_affine_collapse.ps1` and `run_removed_cli_features.ps1`.

Normalized manifest SHA-256:
`02347EB477242BD93912F61A618054F80D6EFAB85B1E82CF1813D89A4E595D68`.

Tracked generated artifacts matching `.exe/.obj/.o/.s/.ir`: 0.

## Existing Stale Shape Assertions

Two older optimization scripts have correct O0-O3 runtime output but stale
instruction-count thresholds unrelated to this CLI work:

- affine profitability expects disabled `imul=8`, current output is 6;
- multiply-immediate strength expects `imul=1`, current optimized output is 0;
- multi-factor affine expects disabled `imul=3`, current output is 1.

The CLI change did not alter the direct encoded executable output, and these
thresholds were not rewritten to manufacture a pass.
