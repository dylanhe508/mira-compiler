# Changelog

## v5.14.0 — 2026-08-17

### Language and diagnostics

- Added optional gradual type annotations for functions, parameters, returns,
  constants, locals, struct fields and extern declarations.
- Added the public scalar types `i64`, `f64`, `bool`, `str`, `ptr` and `void`.
- Added signature-aware argument count/type checking, return checking, strict
  typed assignment and condition validation, and source-accurate diagnostics
  across imported modules.
- Preserved legacy unannotated Mira behavior; annotations opt into stricter
  checking rather than requiring a whole-program migration.

### Correctness and ownership

- Propagated declared types through SSA parameters, calls, locals, returns,
  PHI nodes, switch expressions and try expressions.
- Fixed floating-point comparison lowering for negative values, signed zero
  and NaN behavior.
- Fixed typed control-flow tail values and mixed owned/borrowed string PHIs.
- Added checker-guided escape/ownership metadata and corrected cleanup across
  calls, returns, stores and control-flow merges.
- Fixed typed constants, call arity for modern names/methods, lexer state
  restoration and several diagnostic provenance cases.

### Compiler interface

- Added explicit output modes: `--emit=ir`, `-S`/`--emit=asm`, and
  `-c`/`--emit=obj`.
- Unified output naming through `-o <path>`.
- Added GNU Intel assembly emission while retaining Mira's direct x86-64
  encoder and self-written linker for normal executable builds.
- Kept O2 as the default optimization level.

### Code generation

- Improved pressure-aware machine scheduling and hot-loop code generation.
- Fixed floating-point ABI parameter loads and typed value handling on Win64
  and SysV paths.
- Preserved representative optimized executable size and output checks in the
  release regression suite.

### Repository

- Consolidated long-lived validation into `win/regress.sh`,
  `linux/regress.sh` and `bench/bench_regress.sh`.
- Removed one-off feature runners, probes and temporary measurement scripts
  while retaining all tracked Mira and C regression cases.

## v5.13.4

- Previous public release baseline.
