# Remove Interpreter, REPL, and Disassembler Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Completely remove Mira's built-in interpreter, REPL, and disassembler while preserving native compilation, linking, optimization, and `-S` output.

**Architecture:** Remove the public CLI routes first, then delete the now-unreachable subsystem sources and build inputs on both platforms. Verify absence structurally and preserve compiler behavior with command-line, regression, output-hash, size, and compile-time gates.

**Tech Stack:** C11, MinGW GCC, PowerShell regression scripts, GNU Make-compatible Linux build inputs, Git.

## Global Constraints

- Remove `mira -i`, file interpretation, interactive REPL, `--dump-asm`, `interpreter/`, and `mdisasm/`.
- Preserve normal compilation, linking, `-O0` through `-O3`, and `mira -S`.
- Removed options must fail explicitly as unsupported options.
- Update Windows and Linux source/build inputs together.
- Do not change generated code, program checksum, program size, or accepted compile-time gates.
- Refresh `E:\mira\mira-source` only after the isolated branch passes verification.

---

### Task 1: Lock the Removed and Preserved CLI Behavior

**Files:**
- Create: `win/tests/run_removed_cli_features.ps1`
- Test: `win/mira.exe`

**Interfaces:**
- Consumes: Mira command line and an existing minimal `.mira` fixture.
- Produces: a regression gate requiring `-i` and `--dump-asm` to fail and `-S` to succeed.

- [ ] **Step 1: Write the failing CLI regression**

Create a PowerShell test that invokes `mira.exe -i`, `mira.exe --dump-asm`, and
`mira.exe -S tests/regression_phi_inline.mira <temporary.asm>`. Assert nonzero
exit codes plus `unknown option` for the removed flags, and assert zero plus a
nonempty output file for `-S`.

- [ ] **Step 2: Run the test and verify RED**

Run: `powershell -ExecutionPolicy Bypass -File tests\run_removed_cli_features.ps1`

Expected: FAIL because the current compiler still enters the interpreter or
accepts `--dump-asm`.

- [ ] **Step 3: Commit the test-only RED state only after implementation is ready to follow immediately**

Keep the test and production changes in the same final commit so the branch is
never left intentionally broken.

### Task 2: Remove the Windows CLI Routes and Subsystems

**Files:**
- Modify: `win/main.c`
- Modify: `win/BUILD.txt`
- Modify: `win/Makefile`
- Delete: `win/interpreter/api.c`
- Delete: `win/interpreter/exec.c`
- Delete: `win/interpreter/infix.c`
- Delete: `win/interpreter/value.c`
- Delete: all tracked files under `win/mdisasm/`
- Test: `win/tests/run_removed_cli_features.ps1`

**Interfaces:**
- Consumes: the CLI contract from Task 1.
- Produces: a native-only Windows Mira compiler with no interpreter/disassembler linkage.

- [ ] **Step 1: Remove main-program dependencies**

Remove `#include "mdisasm/mdisasm.h"`, the `mira_dump_asm` state and object-file
disassembly branch, the `-i` dispatch, and the two removed help entries.
Add early option validation equivalent to:

```c
if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--dump-asm") == 0) {
    fprintf(stderr, "error: unsupported option '%s'\n", argv[i]);
    return 1;
}
```

- [ ] **Step 2: Remove Windows build inputs and subsystem files**

Delete `mdisasm/decode.c` and all `interpreter/*.c` tokens from `BUILD.txt` and
`Makefile`, then delete both subsystem directories' tracked sources and headers.

- [ ] **Step 3: Rebuild using the documented command**

Run the `gcc -O2 ... -o mira.exe` line from `BUILD.txt`.

Expected: exit 0 with no unresolved interpreter or `mdisasm` symbols.

- [ ] **Step 4: Run the CLI gate**

Run: `powershell -ExecutionPolicy Bypass -File tests\run_removed_cli_features.ps1`

Expected: `REMOVED CLI FEATURES PASS`.

- [ ] **Step 5: Commit Windows removal**

```text
git commit -m "refactor: remove interpreter and disassembler"
```

### Task 3: Synchronize Linux Sources and Build Inputs

**Files:**
- Modify: `linux/main.c`
- Modify: `linux/Makefile`
- Modify: `linux/BUILD.txt` if it references removed sources
- Delete: all tracked files under `linux/interpreter/`
- Delete: all tracked files under `linux/mdisasm/`

**Interfaces:**
- Consumes: the Windows native-only CLI contract.
- Produces: matching Linux source/build behavior.

- [ ] **Step 1: Apply the same CLI removal to Linux `main.c`**

Remove the include, flags, help entries, disassembly branch, and interpreter
dispatch. Use the same explicit unsupported-option message as Windows.

- [ ] **Step 2: Remove Linux build inputs and tracked subsystem sources**

Remove `mdisasm/decode.c` and `interpreter/api.c interpreter/exec.c
interpreter/infix.c interpreter/value.c` from `COMPILER_SRCS`, then delete the
subsystem source/header files.

- [ ] **Step 3: Perform structural Linux verification**

Run a production-source scan for `mdisasm`, `mira_interpret`, `mira_repl`,
`--dump-asm`, and `mira -i` outside design/history documents.

Expected: no matches in Linux production sources or build files.

- [ ] **Step 4: Commit Linux synchronization**

```text
git commit -m "refactor: sync native-only Linux compiler"
```

### Task 4: Regression, Performance, and Source-package Verification

**Files:**
- Modify: `E:\mira\mira-source\` by refreshing from the verified branch
- Test: current Windows regression scripts and `bench/gen_100.mira`, `bench/gen_800.mira`

**Interfaces:**
- Consumes: verified native-only source trees.
- Produces: final evidence and a source-only package without removed subsystems.

- [ ] **Step 1: Run representative functional regressions**

Run the current affine, induction, dynamic-slot, nonvolatile-call, phi-inline,
division, SIMD, and compile-profile PowerShell suites.

Expected: every suite exits 0.

- [ ] **Step 2: Compare generated outputs**

Compile representative runtime benchmarks with the pre-removal and current
compiler at `-O3`; compare SHA-256 and byte size.

Expected: identical generated executables and checksums.

- [ ] **Step 3: Run compile-time gates**

Run 5 warmups plus 101 measurements for `gen_100` and `gen_800`.

Expected: `gen_100` does not regress by more than 1%; `gen_800` median remains
at or below 250 ms.

- [ ] **Step 4: Refresh and audit the source-only package**

Recreate the curated contents of `E:\mira\mira-source` from the verified tree.
Assert zero `interpreter`, `mdisasm`, test, benchmark, Git, executable, object,
library, DLL, and PDB entries.

- [ ] **Step 5: Final diff and history check**

Run `git diff --check`, inspect `git status --short`, and record the final commit
range and verification results. Existing ignored/untracked regression artifacts
must not be added or deleted.
