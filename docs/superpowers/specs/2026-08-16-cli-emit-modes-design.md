# Mira CLI Emit Modes Design

## Goal

Make Mira's output commands match established compiler conventions and ensure
that every emitted artifact comes from the same optimized compilation pipeline.
The change must not alter normal executable output or add an external assembler
dependency to ordinary builds.

## User interface

The supported forms are:

```text
mira input.mira
mira input.mira -o program.exe
mira -S input.mira
mira --emit=asm input.mira
mira --emit=ir input.mira
mira -c input.mira
mira --emit=obj input.mira
```

All emit modes accept `-o <path>`. Without `-o`, output names use the input
stem in the current directory:

| Mode | Windows default | Linux default |
| --- | --- | --- |
| executable | `input.exe` | `input` |
| assembly | `input.s` | `input.s` |
| internal IR | `input.ir` | `input.ir` |
| object | `input.obj` | `input.o` |

`-S` is an alias for `--emit=asm`. `-c` is an alias for `--emit=obj`.
The former `-S input [output]` IR behavior is intentionally replaced; repository
tests that inspect internal IR will use `--emit=ir input -o output.ir`.

Optimization and target options may appear before or after the input, but every
option that takes a value consumes it explicitly. Unknown options, missing values,
multiple input files, and conflicting emit modes fail with a concise diagnostic
and nonzero exit status.

## Compilation architecture

Parsing, type checking, SSA construction, optimization, register allocation,
machine-IR lowering, and late machine-IR optimization form one shared pipeline.
The pipeline produces a finalized `IrBuffer`; artifact writers consume that
buffer without rerunning or skipping optimization stages.

The final branch is:

```text
final IrBuffer
  +-- internal IR writer -> .ir
  +-- assembly writer    -> .s
  +-- machine encoder    -> .obj/.o
                            +-- self-written linker -> executable
```

Normal executable builds retain the existing direct machine-code encoder and
self-written linker. They do not invoke the emitted assembly or an external
assembler.

## Assembly contract

Assembly output uses GNU assembler Intel syntax so the same textual convention
works with MinGW `as` on Windows and GNU `as` on Linux. It includes the sections,
symbol visibility, external declarations, labels, data, BSS, scalar instructions,
floating-point instructions, and AVX instructions represented by the finalized
machine IR.

The output is assembler-consumable for the selected host target, not merely a
debug pretty-print. Symbolic calls and addresses remain relocatable. Local labels
are deterministic so repeated compilations produce stable text.

The writer rejects an unsupported machine-IR opcode instead of silently printing
an approximation. The diagnostic identifies the opcode and output path.

## Internal IR contract

`--emit=ir` preserves the existing textual IR vocabulary used by compiler tests,
but it observes the same late optimization passes as object and executable output.
It is a debugging format, not a stable source language or assembler input.

## Object and executable modes

`-c`/`--emit=obj` writes only the program object and does not invoke the linker.
Unresolved runtime symbols are valid in this artifact.

The default executable mode retains selective runtime linking. `-o` changes only
the final executable path; the temporary object remains an implementation detail
under `out/`.

## Cross-platform behavior

Platform-neutral CLI parsing, pipeline sharing, IR emission, and diagnostics are
mirrored between `win/` and `linux/`. Assembly formatting uses the selected host
object convention where symbol decoration or section spelling differs. This work
does not turn `--target=linux` on a Windows compiler into a complete cross linker;
existing target limitations remain explicit.

## Help and documentation

`mira --help` groups commands, output modes, optimization, target selection, and
information flags. It reports the actual default optimization level from
`mira_opt_level`, avoiding a duplicated hard-coded value. README examples use the
new spellings and clearly distinguish `.ir`, `.s`, object, and executable output.

## Testing

Tests must establish:

1. `-S` and `--emit=asm` produce equivalent Intel-syntax assembly.
2. Representative integer, floating-point, branch, call, data, BSS, and AVX
   programs emit assembly accepted by the host GNU assembler.
3. `--emit=ir` retains the IR patterns required by existing optimization tests.
4. `-c` and `--emit=obj` produce valid host object files without linking.
5. `-o`, default names, option ordering, aliases, conflicts, missing operands,
   and unknown options have deterministic behavior.
6. O0 through O3 affect IR, assembly, object, and executable modes consistently.
7. Before and after the refactor, normal regression executables have identical
   output; representative unchanged programs retain identical object/executable
   hashes when no intended code-generation change is involved.
8. Windows and Linux platform-neutral changes remain mirrored; native Linux
   execution is verified when the environment is available and otherwise reported
   separately from source/build checks.

## Non-goals

This change does not add `run`, `check`, `test`, formatting, package management,
a REPL, a new language syntax, or a new linker. Those belong to later UX work.
