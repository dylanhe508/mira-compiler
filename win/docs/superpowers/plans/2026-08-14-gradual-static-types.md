# Mira Gradual Static Types Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Preserve Mira type annotations, reject contradictions in annotated code, and feed verified scalar types into SSA without breaking unannotated programs.

**Architecture:** Add a small semantic type layer shared by parser, checker, and SSA. The parser records annotations in existing `Def`, `Program`, and `IrNode` data; a new stack-aware checker runs after every module has parsed and before code generation; SSA uses checked signatures and variable metadata where they are known, while `unknown` follows the legacy path.

**Tech Stack:** C11, the existing arena-backed postfix IR, PowerShell integration tests, GCC/MinGW Makefiles, Mira's SSA backend.

## Global Constraints

- Semantic types are exactly `unknown`, `i64`, `f64`, `bool`, `str`, `ptr`, and `void`; `unknown` is internal and cannot be written in source. `ptr` preserves the existing runtime/stdlib ABI as a strict semantic type and does not enable pointer arithmetic.
- Explicit annotations are strict; fully unannotated legacy code keeps its current inference and truthiness behavior.
- There is no implicit `i64`/`f64` conversion.
- `void` is legal only as a function result and cannot be consumed as a value.
- The first milestone excludes `list<T>`, generics, union/null types, aliases, and user-defined conversions.
- Windows and Linux source/tests remain byte-for-byte mirrored except existing platform build files.
- Existing ownership metadata, ABI representation, standard-library names, and optimizer behavior remain unchanged.
- Every behavior change follows RED-GREEN-REFACTOR and receives a focused commit.

---

## File Map

- Create `typecheck.h`: public semantic-type and checker interface.
- Create `typecheck.c`: type names, compatibility, signature lookup, postfix-IR checking, and diagnostics.
- Modify `mira.h`: store semantic type metadata and source positions in existing IR/program structures.
- Modify `parser/index.c`: retain function/extern parameter and return annotations.
- Modify `parser/blocks.c`: retain `let`/`mut`/`const` annotations by variable or constant slot.
- Modify `parser/helpers.c`: allocate and initialize variable type metadata with variable tables.
- Modify `main.c`: invoke the checker after all imported modules parse and before `codegen`.
- Modify `codegen/ssa_builder.c`: translate verified semantic types to SSA parameter, variable, call, and return types.
- Modify `Makefile`: compile `typecheck.c` and rebuild parser/codegen when its header changes.
- Create `tests/types/*.mira`: focused positive and negative language cases.
- Create `tests/run_gradual_types.ps1`: builds cases, verifies exit status/diagnostics/output, and runs O0-O3 positive cases.

All listed source and tests are first changed under `win/`, then mirrored to `linux/` in Task 5.

### Task 1: Preserve and validate declared type names

**Files:**
- Create: `win/typecheck.h`
- Create: `win/typecheck.c`
- Modify: `win/mira.h`
- Modify: `win/parser/helpers.c`
- Modify: `win/parser/index.c`
- Modify: `win/parser/blocks.c`
- Modify: `win/main.c`
- Modify: `win/Makefile`
- Create: `win/tests/types/unknown_parameter_type.mira`
- Create: `win/tests/types/unknown_local_type.mira`
- Create: `win/tests/types/annotations_valid.mira`
- Create: `win/tests/run_gradual_types.ps1`

**Interfaces:**
- Produces: `MiraType`, `mira_type_from_name(const char *, size_t, MiraType *)`, `mira_type_name(MiraType)`, and `mira_typecheck_program(Compiler *, Program *)`.
- Produces: `Def.param_types`, `Def.param_type_explicit`, `Def.return_type`, `Def.return_type_explicit`, and matching `Program.var_types`/`var_type_explicit` arrays.
- Consumes: existing module-qualified `Def` list and program variable slots.

- [ ] **Step 1: Write the failing source cases and runner**

`unknown_parameter_type.mira`:

```mira
fn bad(value: number) -> i64 { value }
fn main() { print(0); }
```

`unknown_local_type.mira`:

```mira
fn main() { let value: number = 1; print(value); }
```

`annotations_valid.mira`:

```mira
fn choose(flag: bool, left: i64, right: i64) -> i64 {
    if (flag) { left } else { right }
}
fn main() -> void { print(choose(true, 7, 9)); }
```

The runner must build each negative case, require nonzero exit, and require `unknown type 'number'`; it must build and run the positive case at O0 and require output `7`.

- [ ] **Step 2: Run the focused runner and verify RED**

Run: `powershell -ExecutionPolicy Bypass -File tests/run_gradual_types.ps1 -Group declarations`

Expected: FAIL because both unknown type annotations are currently discarded and compile successfully.

- [ ] **Step 3: Add semantic metadata and preserve parser annotations**

Add this public shape in `typecheck.h` and store it from every modern function, typed `extern`, `let`, `mut`, and typed constant path:

```c
typedef enum MiraType {
    MIRA_TYPE_UNKNOWN = 0,
    MIRA_TYPE_I64,
    MIRA_TYPE_F64,
    MIRA_TYPE_BOOL,
    MIRA_TYPE_STR,
    MIRA_TYPE_PTR,
    MIRA_TYPE_VOID
} MiraType;

bool mira_type_from_name(const char *name, size_t len, MiraType *out);
const char *mira_type_name(MiraType type);
void mira_typecheck_program(Compiler *compiler, Program *program);
```

Grow `param_types` and `param_type_explicit` with the parameter arrays. Add `MiraType *var_types` and `unsigned char *var_type_explicit` in `Program`; reallocate and zero them in the same block as `var_names`. Reject a source-written `unknown` or an unrecognized name with `mira_error` at the annotation token. Reject `void` on parameters and variables.

Record `line` and `col` in `Def` and `IrNode`; initialize new IR nodes from `lexer_cur()` so later diagnostics retain a source position.

- [ ] **Step 4: Wire the checker and build dependency**

Call the checker in both object and IR-dump compilation paths immediately after `parser_parse` and before `codegen`:

```c
Program *prog = parser_parse(&c);
mira_typecheck_program(&c, prog);
codegen(&c, prog);
```

Add `typecheck.c` to `COMPILER_SRCS` and `typecheck.h` to parser/codegen dependencies.

- [ ] **Step 5: Run the focused runner and verify GREEN**

Run: `mingw32-make -C win mira.exe` then `powershell -ExecutionPolicy Bypass -File win/tests/run_gradual_types.ps1 -Group declarations`

Expected: PASS; both negative cases name `number`, and the positive program prints `7`.

- [ ] **Step 6: Commit**

```text
git add win/typecheck.c win/typecheck.h win/mira.h win/parser win/main.c win/Makefile win/tests/types win/tests/run_gradual_types.ps1
git commit -m "feat(types): preserve declared scalar types"
```

### Task 2: Check function calls and results

**Files:**
- Modify: `win/typecheck.c`
- Modify: `win/parser/index.c`
- Create: `win/tests/types/call_argument_type_error.mira`
- Create: `win/tests/types/call_arity_error.mira`
- Create: `win/tests/types/return_type_error.mira`
- Create: `win/tests/types/void_value_error.mira`
- Create: `win/tests/types/extern_signature_valid.mira`
- Modify: `win/tests/run_gradual_types.ps1`

**Interfaces:**
- Consumes: Task 1 `Def` signatures and `MiraType` helpers.
- Produces: module-wide `mira_find_signature` behavior used by the checker and SSA; call expressions have a known result whenever the callee has an explicit return annotation.

- [ ] **Step 1: Write one focused failing case per rule**

Use these exact contradictions:

```mira
fn square(x: f64) -> f64 { x * x }
fn main() { print(square(2)); }
```

```mira
fn sum(a: i64, b: i64) -> i64 { a + b }
fn main() { print(sum(1)); }
```

```mira
fn label() -> str { 42 }
fn main() { print(label()); }
```

```mira
fn log(value: i64) -> void { print(value); }
fn main() { let result: i64 = log(1); print(result); }
```

Require diagnostic fragments respectively: `argument 1 of 'square': expected f64, got i64`, `expects 2 arguments, got 1`, `function 'label': expected str, got i64`, and `returns void and cannot be used as a value`.

- [ ] **Step 2: Run and verify RED**

Run: `powershell -ExecutionPolicy Bypass -File win/tests/run_gradual_types.ps1 -Group calls`

Expected: FAIL because the current compiler accepts or mislowers every contradiction.

- [ ] **Step 3: Implement signature-aware postfix checking**

Build a checker value stack carrying `{ MiraType type; bool strict; int line; int col; }`. Literals push `i64`, `f64`, or `str`; parameter words push the annotated parameter type; calls pop exactly `param_count` arguments in source order and compare only positions whose parameter is explicit. A call pushes its result unless it is `void`. `return` and the function's final stack value are checked against an explicit return type.

The signature lookup compares full module-qualified names first and the existing resolved call symbol second, so imported calls use the same `Def` records already created by parsing imports. Typed `extern` follows the identical path.

- [ ] **Step 4: Run and verify GREEN**

Run: `powershell -ExecutionPolicy Bypass -File win/tests/run_gradual_types.ps1 -Group calls`

Expected: PASS with all four exact diagnostic fragments; `extern_signature_valid.mira` builds and prints `42`.

- [ ] **Step 5: Commit**

```text
git add win/typecheck.c win/parser/index.c win/tests/types win/tests/run_gradual_types.ps1
git commit -m "feat(types): check calls and function results"
```

### Task 3: Check locals, operators, conditions, and branches

**Files:**
- Modify: `win/typecheck.c`
- Modify: `win/parser/blocks.c`
- Create: `win/tests/types/assignment_type_error.mira`
- Create: `win/tests/types/mixed_numeric_error.mira`
- Create: `win/tests/types/condition_type_error.mira`
- Create: `win/tests/types/branch_type_error.mira`
- Create: `win/tests/types/scalars_valid.mira`
- Create: `win/tests/types/legacy_truthiness_valid.mira`
- Modify: `win/tests/run_gradual_types.ps1`

**Interfaces:**
- Consumes: checker value stack and Task 1 variable metadata.
- Produces: inferred `Program.var_types` for reliable initializers and strict checking for explicitly typed locals.

- [ ] **Step 1: Add failing operator and flow cases**

Cover these behaviors independently: assigning `str` to `mut count: i64`; adding `i64 + f64`; using annotated `i64` as an `if` condition; returning `i64` from one value-producing branch and `str` from another. Add a positive program using all scalar types, comparisons, logical operators, mutation, and matching branches. Add an unannotated legacy program containing `if (1)` and require it still compile and print the existing result.

- [ ] **Step 2: Run and verify RED**

Run: `powershell -ExecutionPolicy Bypass -File win/tests/run_gradual_types.ps1 -Group expressions`

Expected: FAIL because explicit local types and strict expression regions are currently discarded.

- [ ] **Step 3: Implement store, operator, and control-flow rules**

For `!`, pop destination and value, infer an `unknown` slot from the first reliable value, and reject mismatch when `var_type_explicit[slot]` is set. Arithmetic requires equal numeric types in strict regions; comparison returns `bool`; logical operators and strict conditions require `bool`. Preserve the old truthiness rule if the condition and surrounding function contain no explicit contract.

Check each reachable branch with a cloned stack state. At merge, equal types remain known, `unknown` merges with the known side for compatibility, and two unequal known value types produce the branch diagnostic when their result reaches a strict context.

- [ ] **Step 4: Run and verify GREEN plus legacy compatibility**

Run: `powershell -ExecutionPolicy Bypass -File win/tests/run_gradual_types.ps1 -Group expressions`

Expected: PASS; negative diagnostics identify expected/actual types, the scalar positive case prints its golden output, and legacy truthiness remains accepted.

- [ ] **Step 5: Commit**

```text
git add win/typecheck.c win/parser/blocks.c win/tests/types win/tests/run_gradual_types.ps1
git commit -m "feat(types): validate typed expressions and locals"
```

### Task 4: Feed verified types into SSA

**Files:**
- Modify: `win/typecheck.h`
- Modify: `win/codegen/ssa_builder.c`
- Create: `win/tests/types/ssa_typed_values.mira`
- Modify: `win/tests/run_gradual_types.ps1`

**Interfaces:**
- Consumes: checked `Def` and `Program` type metadata.
- Produces: `mira_type_to_ssa(MiraType)` returning `SSA_TYPE_INT`, `SSA_TYPE_FLOAT`, `SSA_TYPE_PTR`, or `SSA_TYPE_VOID`; typed load/call/return instructions use that result.

- [ ] **Step 1: Add an SSA-observable failing test**

Create a typed program whose user-defined functions separately return `f64`, `str`, `ptr`, `bool`, and `void`, then print/use them. Compile at O0-O3 and require identical golden output. Dump IR with `-S` and require float-return/call values to be float and string/pointer-return/call values to use pointer type rather than the legacy integer fallback.

- [ ] **Step 2: Run and verify RED**

Run: `powershell -ExecutionPolicy Bypass -File win/tests/run_gradual_types.ps1 -Group ssa`

Expected: FAIL on at least the IR type assertions because `ssa_build_function`, parameters, and unresolved calls default to `SSA_TYPE_INT`.

- [ ] **Step 3: Apply checked types in SSA without changing unknown paths**

Initialize each function with its explicit return type, create parameter loads with `param_types[index]`, seed variable loads/stores from `Program.var_types`, and set direct call result from the matched `Def`. Do not alter runtime builtin metadata or ownership tagging. If semantic type is `MIRA_TYPE_UNKNOWN`, execute the exact existing inference/default logic.

- [ ] **Step 4: Run and verify GREEN at all optimization levels**

Run: `powershell -ExecutionPolicy Bypass -File win/tests/run_gradual_types.ps1 -Group ssa`

Expected: PASS at O0, O1, O2, and O3 with identical output and the required IR type markers.

- [ ] **Step 5: Commit**

```text
git add win/typecheck.h win/codegen/ssa_builder.c win/tests/types/ssa_typed_values.mira win/tests/run_gradual_types.ps1
git commit -m "feat(types): lower verified types into SSA"
```

### Task 5: Mirror and build Linux

**Files:**
- Create: `linux/typecheck.h`
- Create: `linux/typecheck.c`
- Modify: `linux/mira.h`
- Modify: `linux/parser/helpers.c`
- Modify: `linux/parser/index.c`
- Modify: `linux/parser/blocks.c`
- Modify: `linux/main.c`
- Modify: `linux/codegen/ssa_builder.c`
- Modify: `linux/Makefile`
- Create: `linux/tests/types/*.mira`
- Create: `linux/tests/run_gradual_types.ps1`

**Interfaces:**
- Consumes: completed Windows implementation.
- Produces: byte-identical platform-neutral sources and tests, plus a Linux compiler containing `typecheck.c`.

- [ ] **Step 1: Mirror every platform-neutral changed file**

Copy only the files listed above from `win/` to the matching `linux/` path. Apply the `Makefile` source/dependency addition to Linux's existing platform-specific Makefile instead of replacing the whole file.

- [ ] **Step 2: Verify mirror hashes**

Run a PowerShell hash comparison over `typecheck.[ch]`, `mira.h`, the three parser files, `main.c`, `codegen/ssa_builder.c`, and every `tests/types/*.mira` file.

Expected: every platform-neutral pair has the same SHA-256 hash.

- [ ] **Step 3: Build both compiler targets**

Run: `mingw32-make -C win clean all` and `make -C linux clean all` in their native environments.

Expected: both builds exit 0 without new compiler warnings. If Linux execution is unavailable on the Windows host, build the ELF-target compiler here and record native runtime verification as pending rather than claiming it ran.

- [ ] **Step 4: Commit**

```text
git add linux win
git commit -m "feat(types): synchronize Windows and Linux frontends"
```

### Task 6: Full regression and performance guard

**Files:**
- Modify: `win/tests/run_gradual_types.ps1`
- Modify: `linux/tests/run_gradual_types.ps1`
- Create: `win/tests/type_enhancement_report.md`
- Create: `linux/tests/type_enhancement_report.md`

**Interfaces:**
- Consumes: complete type checker and SSA integration.
- Produces: reproducible correctness/performance evidence and final compatibility report.

- [ ] **Step 1: Run the full type suite from a clean build**

Run: `powershell -ExecutionPolicy Bypass -File win/tests/run_gradual_types.ps1 -Group all`

Expected: every declaration, call, expression, compatibility, and SSA test passes at its specified optimization levels.

- [ ] **Step 2: Run existing representative regressions**

Run the existing module suite, float-variable arithmetic suite, infix line-continuation suite, short-circuit regression, branch-return regression, core/data stdlib programs, and the existing C unit tests for builtin-table and SSA reference metadata.

Expected: all exit 0 and retain their golden output.

- [ ] **Step 3: Measure before/after compiler and program characteristics**

Using the baseline commit `9b7b628` and final tree, measure three repeated clean compiler builds, type-suite compile time, output size for `modern_typed_syntax.mira`, and runtime for the existing integer/float representative benchmarks. Record median values and percent differences. Treat any output mismatch as failure; investigate compile/size/runtime regression beyond measurement noise instead of accepting it automatically.

- [ ] **Step 4: Write and mirror the report**

The report must list exact commands, host/platform, commit IDs, pass counts, O0-O3 results, medians, sizes, and any native-Linux verification limitation. Mirror the report after results are final.

- [ ] **Step 5: Final verification and commit**

Run: `git diff --check`, verify Windows/Linux hashes again, and run `git status --short` to ensure no executable/object/test output is tracked.

```text
git add win/tests/run_gradual_types.ps1 linux/tests/run_gradual_types.ps1 win/tests/type_enhancement_report.md linux/tests/type_enhancement_report.md
git commit -m "test(types): verify compatibility and performance"
```
