# Mira Compiler v5.14.0

Mira v5.14.0 is a correctness, type-system and compiler-interface release. It
keeps unannotated programs compatible while allowing modern code to opt into
static contracts, and it exposes the compiler's finalized IR, assembly and
object outputs through conventional command-line modes.

## Highlights

- Optional gradual types: `i64`, `f64`, `bool`, `str`, `ptr` and `void`.
- Type-aware calls, returns, assignments, conditions, constants and module
  diagnostics.
- Correct typed SSA values through calls and structured control flow.
- Fixed `f64` comparisons, including negative values, signed zero and NaN.
- Safer owned/borrowed string merging and checker-guided escape handling.
- Explicit `--emit=ir`, `-S`/`--emit=asm`, `-c`/`--emit=obj` and unified `-o`.
- GNU Intel assembly output without changing the normal self-contained
  encoder/linker pipeline.
- Pressure-aware scheduling and additional hot-loop/code-generation fixes.

## Compatibility

Existing unannotated Mira remains supported. Type annotations are opt-in and
become strict where present. Mira functions continue to return their final
expression; there is no required `return` keyword for ordinary tail results.

`-S` now means GNU Intel assembly output. Use `--emit=ir` when inspecting
Mira's internal finalized machine IR. The default optimization level remains
O2, and ordinary compilation still produces a native executable with Mira's
own encoder and linker.

## Command-line examples

```sh
mira hello.mira -o hello.exe
mira --emit=ir hello.mira -o hello.ir
mira -S hello.mira -o hello.s
mira -c hello.mira -o hello.obj
```

## Build

Windows x86-64 (MinGW-w64/TDM-GCC):

```sh
mingw32-make -C win
```

Linux x86-64 (GCC and GNU Make):

```sh
make -C linux -j
```

## Validation

The Windows release build is checked by the consolidated formal regression:

```sh
bash win/regress.sh
bash bench/bench_regress.sh win/mira.exe
```

The formal suite covers CLI parsing and artifact modes, IR/assembly writers,
O0-O3 execution goldens, gradual types, floating point, ownership/control-flow
merges, modules, standard-library modules and representative diagnostics.

The Windows and Linux platform-neutral source mirrors are checked for
synchronization, and Linux translation units receive host-side syntax/object
validation. Native Linux execution could not be performed in the release
environment because no usable WSL distribution was available. For that
reason v5.14.0 ships a Linux source tree, but no claimed native Linux binary.

## Packages

- `mira-compiler-v5.14.0-source.zip`
- `mira-compiler-v5.14.0-windows-x86_64.zip`
- `SHA256SUMS.txt`

This repository currently has no `LICENSE` file. v5.14.0 does not invent or
imply a license; the project owner can add the chosen license separately.
