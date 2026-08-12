# Remove the Built-in Interpreter, REPL, and Disassembler

## Goal

Remove the unreliable, nonessential execution and inspection subsystems from
Mira while preserving the native single-file compiler, linker, optimizer, and
`-S` assembly/IR output workflow on Windows and Linux.

## Scope

- Delete the `interpreter/` implementation and all interpreter API usage.
- Remove `mira -i file.mira` and the interactive `mira -i` REPL.
- Delete the `mdisasm/` implementation and all disassembler API usage.
- Remove `--dump-asm` and its help text.
- Remove the deleted sources from Windows `BUILD.txt` and both platform
  Makefiles/build inputs.
- Remove the same directories from the source-only package.

## Preserved Behavior

- Normal `.mira` compilation and linking remain unchanged.
- `-O0` through `-O3` remain unchanged.
- `mira -S input.mira [output]` remains available.
- Existing compiler profiling and IR diagnostics unrelated to `--dump-asm`
  remain available.
- Unknown removed options fail explicitly instead of being ignored.

## Implementation Boundaries

The removal is structural rather than feature-flagged. No dormant interpreter
or disassembler code remains in the default source tree. Command-line parsing,
usage output, includes, source lists, and subsystem directories are removed in
one change so no unsupported half-state can build.

Windows and Linux copies are updated together. Platform-specific compiler and
runtime code outside these subsystems is not refactored.

## Verification

1. A source scan finds no interpreter, REPL, `--dump-asm`, or `mdisasm`
   references in production source/build files.
2. The Windows compiler rebuilds from the documented release command.
3. `-i` and `--dump-asm` return an unsupported-option error.
4. `-S`, normal compilation, linking, and representative `-O0` through `-O3`
   regressions pass.
5. `gen_100` and `gen_800` compile-time gates remain within their accepted
   limits, and generated program checksums and sizes do not regress.
6. Linux source/build inputs contain no deleted subsystem references and are
   ready for native Linux verification.

## Source-only Package

After repository verification, refresh `E:\mira\mira-source` from the verified
tree. The package continues to contain only buildable source and build files,
with no tests, benchmarks, Git metadata, or binary artifacts.
