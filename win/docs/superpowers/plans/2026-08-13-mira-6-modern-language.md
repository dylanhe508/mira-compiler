# Mira 6.0 Modern Language and Standard Library Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship Mira 6.0 with one modern infix source language, namespaced modules, modern FFI, and a rewritten cross-platform standard library while preserving the existing compiler backend, runtime performance, and tiny native outputs.

**Architecture:** Keep lexer -> IR -> SSA -> x86-64 -> self-written linker unchanged below the parser boundary. Add a focused module/name-resolution layer before normal definition lowering, expose runtime primitives through reserved `__mira_*` symbols, implement public APIs in `stdlib/std/*.mira`, migrate all executable repository sources, and only then delete postfix parsing and legacy public builtins.

**Tech Stack:** C11 compiler/runtime, Mira source, x86-64 Win64 and SysV ABIs, COFF/PE and ELF, MinGW GCC, Linux GCC, PowerShell and Bash regression scripts.

## Global Constraints

- Work from `E:/mira/mira`; `win/` is authoritative and every platform-neutral source change must be mirrored to `linux/` with only line-ending differences.
- Preserve the uncommitted parser/codegen correctness fixes already present in both trees; establish a clean checkpoint commit before Task 1.
- Use TDD: every task begins with a focused failing test and ends with focused plus cumulative verification.
- Do not rewrite IR, SSA, register allocation, machine encoding, PE/ELF writers, or the linker architecture.
- Do not add generics, Unicode redesign, package management, a new ownership model, directory/network APIs, or source-level `select`.
- Mira 6.0 intentionally rejects all postfix source syntax and all old public standard-library names.
- Public APIs use namespaced modern calls; compiler/runtime-only names begin with `__mira_` and direct user calls are rejected.
- Default optimization remains unchanged during the refactor; reconcile the README/default mismatch in the release task after measuring all levels.
- Every supported modern program must produce identical observable results at `-O0`, `-O1`, `-O2`, and `-O3`.
- No stable runtime performance or executable-size regression greater than 3% on the existing benchmark median unless separately approved.

---

## Delivery sequence

This plan is intentionally staged. Phases A-D leave a buildable compiler at every commit; Phase E performs the irreversible legacy deletion only after every retained source has migrated.

### Phase A: Protect the current compiler and introduce module infrastructure

### Task 0: Preserve the existing correctness fixes

**Files:**
- Existing changes: `win/parser/infix.c`, `win/parser/parse_one.c`, `win/codegen/{ir_cfg_cleanup,program,ssa_builder,ssa_opt}.c`
- Existing changes: matching `linux/` files
- Existing tests: `win/tests/regression_*.mira`, `linux/tests/regression_*.mira`, focused PowerShell runners

**Interfaces:**
- Consumes: current dirty working tree
- Produces: a named Git checkpoint containing only the already-verified unary-negative, `else if`, floating-variable, and optimization-correctness fixes

- [ ] **Step 1: Record exact dirty scope**

Run:

```powershell
git -c safe.directory=E:/mira/mira -C E:/mira/mira status --short
git -c safe.directory=E:/mira/mira -C E:/mira/mira diff --check
```

Expected: only the known parser/codegen fixes and their regression tests; `diff --check` reports no whitespace errors.

- [ ] **Step 2: Rebuild and rerun focused regressions**

Run from `E:/mira/mira/win`:

```powershell
mingw32-make
powershell -ExecutionPolicy Bypass -File tests/run_float_var_arith.ps1
powershell -ExecutionPolicy Bypass -File tests/run_int_o2_branch_loop.ps1
.\mira.exe -O3 tests/regression_unary_negative_variable.mira
.\tests\regression_unary_negative_variable.exe
.\mira.exe -O3 tests/regression_else_if_chain.mira
.\tests\regression_else_if_chain.exe
```

Expected: build succeeds; scripts print their `PASS` messages; direct programs print their checked regression values and exit `0`.

- [ ] **Step 3: Commit the checkpoint without the Mira 6 plan**

```powershell
git -c safe.directory=E:/mira/mira -C E:/mira/mira add win/parser win/codegen win/tests linux/parser linux/codegen linux/tests
git -c safe.directory=E:/mira/mira -C E:/mira/mira commit -m "fix: preserve modern syntax and optimizer correctness"
```

Expected: one checkpoint commit; subsequent Mira 6 commits have a clean, reviewable base.

### Task 1: Add parser-level module data structures

**Files:**
- Modify: `win/mira.h`
- Modify: `win/parser/parser.h`
- Create: `win/parser/modules.c`
- Modify: `win/parser.c`
- Modify: `win/Makefile`
- Create: `win/tests/module_table_test.c`
- Mirror: corresponding `linux/` files

**Interfaces:**
- Produces: `ModuleId`, `ModuleImport`, `ModuleSymbol`, `module_table_init`, `module_intern_path`, `module_add_import`, `module_qualify_symbol`, `module_finish_loading`
- Consumes later: Tasks 2-8 use these names exactly

- [ ] **Step 1: Write a failing C unit test**

Create `tests/module_table_test.c` that initializes an arena and asserts:

```c
ModuleTable table;
module_table_init(&table, &arena);
ModuleId math = module_intern_path(&table, "std.math", 8);
ModuleId same = module_intern_path(&table, "std.math", 8);
assert(math == same);
assert(module_add_import(&table, 0, math, "math", 4, NULL));
assert(!module_add_import(&table, 0, math, "math", 4, NULL));
assert(strcmp(module_qualify_symbol(&table, math, "max", 3), "std.math.max") == 0);
```

- [ ] **Step 2: Compile the test and verify RED**

```powershell
gcc -O0 -g -I. tests/module_table_test.c memory.c -o tests/module_table_test.exe
```

Expected: compilation fails because `ModuleTable` and module functions are undefined.

- [ ] **Step 3: Implement the module table**

Add focused structures to `mira.h`:

```c
typedef uint32_t ModuleId;
typedef enum { MODULE_UNSEEN, MODULE_LOADING, MODULE_LOADED } ModuleState;

typedef struct {
    char *path;
    size_t path_len;
    ModuleState state;
} ModuleRecord;

typedef struct {
    ModuleId owner;
    ModuleId target;
    char *alias;
    size_t alias_len;
} ModuleImport;

typedef struct {
    Arena *arena;
    ModuleRecord *modules;
    size_t module_count, module_cap;
    ModuleImport *imports;
    size_t import_count, import_cap;
} ModuleTable;
```

Implement allocation, path interning, duplicate-alias rejection, loading states, and qualified symbol creation in `parser/modules.c`. Include it from `parser.c` beside the existing parser components and add it to `PARSER_DEPS` in both Makefiles.

- [ ] **Step 4: Run unit and compiler builds**

```powershell
gcc -O0 -g -I. tests/module_table_test.c memory.c -o tests/module_table_test.exe
.\tests\module_table_test.exe
mingw32-make
```

Expected: test exits `0`; compiler and runtime build successfully.

- [ ] **Step 5: Mirror and commit**

Synchronize platform-neutral files to `linux/`, normalize only line endings, then:

```powershell
git -c safe.directory=E:/mira/mira -C E:/mira/mira add win/mira.h win/parser win/parser.c win/Makefile win/tests/module_table_test.c linux/mira.h linux/parser linux/parser.c linux/Makefile linux/tests/module_table_test.c
git -c safe.directory=E:/mira/mira -C E:/mira/mira commit -m "feat: add module identity and import tables"
```

### Task 2: Parse modern imports and resolve module files

**Files:**
- Modify: `win/lexer.c`
- Modify: `win/parser/modules.c`
- Modify: `win/parser/index.c`
- Modify: `win/main.c`
- Create: `win/tests/modules/std/math.mira`
- Create: `win/tests/modules/import_math.mira`
- Create: `win/tests/modules/import_alias.mira`
- Create: `win/tests/modules/import_cycle_{a,b}.mira`
- Create: `win/tests/run_modules.ps1`
- Mirror: corresponding `linux/` files

**Interfaces:**
- Produces: `parser_resolve_module(Compiler *, const char *, size_t, char *, size_t)`, `parser_load_module(Program *, ModuleId, const char *)`
- Syntax: `import std.math;` and `import std.math as m;`

- [ ] **Step 1: Add failing fixture programs**

`import_math.mira`:

```mira
import std.math;

fn main() {
    print(math.answer());
}
```

`std/math.mira`:

```mira
fn answer() {
    42
}
```

`import_alias.mira` calls `m.answer()` after `import std.math as m;`. Cycle fixtures import each other and must fail with `module import cycle: modules.import_cycle_a -> modules.import_cycle_b -> modules.import_cycle_a`.

- [ ] **Step 2: Verify modern imports fail**

```powershell
.\mira.exe tests/modules/import_math.mira
```

Expected: nonzero exit because dotted modern import syntax is not implemented.

- [ ] **Step 3: Implement token and path parsing**

Parse an import path as dot-separated identifiers terminated by `;`, optionally followed by `as <identifier>`. Convert `std.math` to `stdlib/std/math.mira`; convert relative module names to a path below the importing file's directory. Reject `..`, absolute paths, duplicate aliases, and missing terminators.

Use an explicit resolver signature:

```c
bool parser_resolve_module(Compiler *c, const char *logical, size_t logical_len,
                           char *resolved, size_t resolved_cap);
```

The resolver must use the compiler executable's standard-library root for `std.*` and the current source directory otherwise.

- [ ] **Step 4: Load each module once and detect cycles**

Before pushing a lexer file, set its `ModuleRecord.state` to `MODULE_LOADING`; after parsing its top-level definitions, set `MODULE_LOADED`. Encountering `MODULE_LOADING` constructs the diagnostic from the maintained import stack and exits nonzero.

- [ ] **Step 5: Run focused module tests**

```powershell
powershell -ExecutionPolicy Bypass -File tests/run_modules.ps1
```

Expected:

```text
MODULE BASIC PASS
MODULE ALIAS PASS
MODULE CYCLE PASS
```

- [ ] **Step 6: Commit**

```powershell
git -c safe.directory=E:/mira/mira -C E:/mira/mira add win/lexer.c win/parser win/main.c win/tests/modules win/tests/run_modules.ps1 linux/lexer.c linux/parser linux/main.c linux/tests/modules linux/tests/run_modules.ps1
git -c safe.directory=E:/mira/mira -C E:/mira/mira commit -m "feat: parse and load modern modules"
```

### Task 3: Qualify definitions and calls by module identity

**Files:**
- Modify: `win/parser/modules.c`
- Modify: `win/parser/index.c`
- Modify: `win/parser/infix.c`
- Modify: `win/parser/parse_one.c`
- Modify: `win/codegen/ssa_function_index.c`
- Modify: `win/codegen/ssa_module_facts.c`
- Extend: `win/tests/modules/*.mira`, `win/tests/run_modules.ps1`
- Mirror: corresponding `linux/` files

**Interfaces:**
- Consumes: `ModuleId` and imports from Tasks 1-2
- Produces: source `alias.member(...)` resolved to canonical symbol `logical.path.member`

- [ ] **Step 1: Add failing isolation and unknown-member tests**

Create two modules that both define `fn value()` and import both under different aliases. Main must print `42` from `left.value() + right.value()`. Add `_error.mira` fixtures for an unknown alias and unknown member with exact diagnostic fragments.

- [ ] **Step 2: Verify RED**

Run `tests/run_modules.ps1`.

Expected: collision or unresolved dotted call.

- [ ] **Step 3: Canonicalize top-level definitions**

Store every non-entry definition under `<module-path>.<source-name>`. Preserve `main` only in the root module; imported modules defining `main` produce an error. Add the current `ModuleId` to parser state so struct constructors, methods, enums, functions, and extern declarations share the same qualification rule.

- [ ] **Step 4: Resolve dotted calls without textual substitution**

When parsing `alias.member(...)`, look up `alias` in the current module's imports, build the canonical name, and emit the normal call IR with that name. Structure field access remains governed by the receiver's known `StructDef` and must be tested against the module-call ambiguity.

- [ ] **Step 5: Verify function facts and optimization indexes**

Run module tests at all optimization levels:

```powershell
0..3 | ForEach-Object { .\mira.exe "-O$_" tests/modules/import_math.mira; .\tests\modules\import_math.exe }
```

Expected: `42` four times, no duplicate function-index entries.

- [ ] **Step 6: Commit**

```powershell
git -c safe.directory=E:/mira/mira -C E:/mira/mira add win/parser win/codegen/ssa_function_index.c win/codegen/ssa_module_facts.c win/tests/modules linux/parser linux/codegen/ssa_function_index.c linux/codegen/ssa_module_facts.c linux/tests/modules
git -c safe.directory=E:/mira/mira -C E:/mira/mira commit -m "feat: resolve namespaced module symbols"
```

### Phase B: Modern FFI and stable runtime primitive boundary

### Task 4: Implement `extern fn` and reserve `__mira_*`

**Files:**
- Modify: `win/parser/index.c`
- Modify: `win/parser/helpers.c`
- Modify: `win/codegen/ssa_builder.c`
- Create: `win/tests/modern_extern_fn.mira`
- Create: `win/tests/reserved_runtime_name_error.mira`
- Mirror: corresponding `linux/` files

**Interfaces:**
- Syntax: `extern fn symbol(a: i64) -> i64;`
- Produces: ordinary `Def` with `is_extern=true`, canonical external symbol, arity, and scalar return metadata

- [ ] **Step 1: Write failing modern FFI tests**

```mira
extern fn mira_abs(value: i64) -> i64;

fn main() {
    print(mira_abs(-42));
}
```

Add an expected-error fixture that declares or directly calls `__mira_string_length` from user source.

- [ ] **Step 2: Verify RED**

Compile both fixtures. Expected: modern extern fails to parse; reserved-name fixture is not yet rejected correctly.

- [ ] **Step 3: Parse modern extern declarations**

Reuse modern function parameter parsing but require `;` instead of a body. Record parameter count and recognized scalar type tags (`i64`, `f64`, pointer-compatible string/list handles); unknown types produce a precise error. Do not keep either legacy `extern name: { ... }` branch after Phase E.

- [ ] **Step 4: Enforce reserved names by source origin**

Add a parser flag identifying standard-library source. User files cannot declare or call names beginning `__mira_`; files loaded from the compiler-owned `stdlib/std/` root can. Do not infer privilege from a user-controlled textual path.

- [ ] **Step 5: Verify and commit**

Run focused tests and O0-O3 modern regressions, then commit:

```powershell
git -c safe.directory=E:/mira/mira -C E:/mira/mira add win/parser win/codegen/ssa_builder.c win/tests/modern_extern_fn.mira win/tests/reserved_runtime_name_error.mira linux/parser linux/codegen/ssa_builder.c linux/tests
git -c safe.directory=E:/mira/mira -C E:/mira/mira commit -m "feat: add modern extern functions and runtime namespace"
```

### Task 5: Centralize standard-library runtime primitives

**Files:**
- Create: `win/codegen/stdlib_builtins.c`
- Create: `win/codegen/stdlib_builtins.h`
- Modify: `win/codegen.c`
- Modify: `win/codegen/ssa_builder.c`
- Modify: `win/codegen/words.c`
- Modify: `win/Makefile`
- Create: `win/tests/stdlib_builtin_table_test.c`
- Mirror: corresponding `linux/` files

**Interfaces:**
- Produces: `const StdlibBuiltin *stdlib_builtin_lookup(const char *, size_t)`
- Record fields: source name, runtime symbol, arity, result count, SSA result type, platform mask, may-suspend flag

- [ ] **Step 1: Write a failing table test**

Assert exact mappings:

```c
const StdlibBuiltin *b = stdlib_builtin_lookup("__mira_math_sqrt", 16);
assert(b && strcmp(b->runtime_symbol, "mira_f_sqrt") == 0);
assert(b->arity == 1 && b->result_type == SSA_TYPE_FLOAT);
assert(stdlib_builtin_lookup("str-cat", 7) == NULL);
```

- [ ] **Step 2: Verify RED**

Compile the unit test; expect missing header/function errors.

- [ ] **Step 3: Implement the authoritative table**

Move SSA runtime metadata for math, random, string, time, IO, file, list, task, channel, memory, process, and Windows primitives into `stdlib_builtins.c`. Use `__mira_*` source names and retain existing C runtime symbol names to avoid a runtime ABI rewrite.

- [ ] **Step 4: Route SSA and legacy lowering through one lookup**

Replace duplicate tables in `ssa_builder.c` with `stdlib_builtin_lookup`. During migration, existing public names may remain in `words_*.c`, but no new standard API may be added there. Phase E deletes those aliases.

- [ ] **Step 5: Build, test, and commit**

```powershell
gcc -O0 -g -I. tests/stdlib_builtin_table_test.c codegen/stdlib_builtins.c -o tests/stdlib_builtin_table_test.exe
.\tests\stdlib_builtin_table_test.exe
mingw32-make
git -c safe.directory=E:/mira/mira -C E:/mira/mira add win/codegen win/Makefile win/tests/stdlib_builtin_table_test.c linux/codegen linux/Makefile linux/tests/stdlib_builtin_table_test.c
git -c safe.directory=E:/mira/mira -C E:/mira/mira commit -m "refactor: centralize standard library runtime primitives"
```

### Phase C: Build the new standard library

### Task 6: Add Prelude and core modules

**Files:**
- Create: `win/stdlib/std/prelude.mira`
- Create: `win/stdlib/std/{math,random,string,time}.mira`
- Modify: `win/main.c`
- Create: `win/tests/stdlib/{prelude,math,random,string,time}.mira`
- Create: `win/tests/run_stdlib_core.ps1`
- Mirror: corresponding `linux/` files

**Interfaces:**
- Public: APIs listed in the approved design document
- Internal: wrappers call reserved primitives from Task 5

- [ ] **Step 1: Write failing API tests before module files**

Examples must test observable behavior, not only compilation:

```mira
import std.math;

fn main() {
    print(math.abs(-42));
    print(math.clamp(120, 0, 100));
}
```

Random test uses seed `12345`, verifies two fresh seeds reproduce the same first two values, and asserts every `random.range(10, 20)` result is `>= 10 && < 20`. String tests cover empty string, concat/equality, contains, byte indexing, trimming, and integer conversion. Time tests assert monotonic time does not decrease across `time.sleep(1)`.

- [ ] **Step 2: Verify RED**

Run `tests/run_stdlib_core.ps1`; expect missing `stdlib/std` modules.

- [ ] **Step 3: Load Prelude automatically**

Resolve `stdlib/std/prelude.mira` from the trusted compiler-owned root before the root module and make only its exported names visible unqualified. Prevent prelude from importing platform-specific modules.

- [ ] **Step 4: Implement wrappers and pure Mira helpers**

Use direct primitive wrappers for existing runtime operations. Implement `math.clamp`, `random.range`, and `random.bool` in modern Mira. Add runtime support only for `random.float` if it can be represented with current `f64` semantics; otherwise omit it from implementation and remove it from public 6.0 docs before release rather than shipping an incorrect function.

- [ ] **Step 5: Test O0-O3 and invalid arguments**

`random.range(5, 5)` and string invalid conversions must take the standard panic path and exit nonzero with stable message prefixes.

- [ ] **Step 6: Commit**

```powershell
git -c safe.directory=E:/mira/mira -C E:/mira/mira add win/stdlib win/main.c win/tests/stdlib win/tests/run_stdlib_core.ps1 linux/stdlib linux/main.c linux/tests/stdlib linux/tests/run_stdlib_core.ps1
git -c safe.directory=E:/mira/mira -C E:/mira/mira commit -m "feat: add Mira 6 core standard library"
```

### Task 7: Add IO, filesystem, list, and memory modules

**Files:**
- Create: `win/stdlib/std/{io,fs,list,memory}.mira`
- Modify if required: `win/runtime/rt_{io,file,collection,mem}.c`
- Extend: `win/codegen/stdlib_builtins.c`
- Create: `win/tests/stdlib/{io,fs,list,memory}.mira`
- Create: `win/tests/run_stdlib_data.ps1`
- Mirror: corresponding `linux/` files

**Interfaces:**
- Public: exact APIs in the design spec
- Memory call order: destination, source, size for copy; pointer, value for stores

- [ ] **Step 1: Add failing end-to-end tests**

Use a temporary file beneath the test working directory; write, append, read, compare, and remove it. Build a list, push values, set/get a boundary element, check length, and free it. Allocate 16 bytes, store/load one i64 and one u8, zero it, verify zero, and free it.

- [ ] **Step 2: Verify RED**

Run `tests/run_stdlib_data.ps1`; expect missing modules.

- [ ] **Step 3: Implement modern wrappers**

Map public full-word names to reserved primitives. Add explicit load/store primitives rather than exposing `@`, `!`, `c@`, `c!`. Preserve current runtime allocation and collection representation.

- [ ] **Step 4: Validate failures**

List out-of-range access and missing file reads must report stable errors. Unsafe invalid raw addresses are not deliberately dereferenced in automated tests.

- [ ] **Step 5: Run tests and commit**

Run data tests at O0-O3 and commit both platform trees with message:

```text
feat: add IO data and memory standard modules
```

### Task 8: Add task, channel, process, and Windows modules

**Files:**
- Create: `win/stdlib/std/{task,channel,process,windows}.mira`
- Create: `linux/stdlib/std/{task,channel,process}.mira`
- Extend: `win/codegen/stdlib_builtins.c`, `linux/codegen/stdlib_builtins.c`
- Modify only if required: scheduler/channel/platform runtime files
- Create: `win/tests/stdlib/{task,channel,process,windows}.mira`
- Create: `win/tests/run_stdlib_system.ps1`
- Mirror cross-platform tests to `linux/`

**Interfaces:**
- Public task model: `task.spawn`, `task.join`, `task.yield`, `task.wait_all`
- Public channel model: `channel.new`, `send`, `receive`, `close`, `free`
- Windows module is rejected for non-Win64 targets

- [ ] **Step 1: Write failing task/channel tests**

Spawn two tasks, communicate a deterministic checksum through buffered and unbuffered channels, join both handles, and verify exactly-once execution. Include 10,000 short tasks as a non-timing correctness stress case.

- [ ] **Step 2: Write platform tests**

`process.id()` must be positive. `process.exit` is tested in a child executable for exact exit code. Compile a Linux-target fixture importing `std.windows` and assert `module std.windows is only available for target windows`.

- [ ] **Step 3: Implement wrappers over existing scheduler/runtime**

Map both old task families to the single public task API. Keep static may-suspend analysis and fast/Fiber selection internal. Do not expose source-level `select`.

- [ ] **Step 4: Run correctness and throughput baselines**

Run existing scheduler/channel C tests plus new Mira tests. Record, but do not gate on, noisy single-run timings; use median of at least 9 runs for the 3% performance criterion.

- [ ] **Step 5: Commit**

```text
feat: add task channel and platform standard modules
```

### Phase D: Migrate the repository before removing compatibility

### Task 9: Convert retained standard programs, tests, fuzz assets, and templates

**Files:**
- Rewrite: all retained `win/libs-mira/*.mira` functionality into `win/stdlib/std/*.mira`; then remove old library files in Task 10
- Modify: all retained `win/*.mira`, `win/apps/*.mira`, `win/tests/*.mira`, `bench/*.mira`, `bench/fuzz_work/**/case.mira`
- Modify: `win/main.c` project template
- Modify: fuzz generators/translators under `bench/`
- Mirror: corresponding `linux/` files

**Interfaces:**
- Consumes: complete modern module/stdlib surface from Tasks 2-8
- Produces: repository with no retained executable source depending on postfix syntax or legacy public builtins

- [ ] **Step 1: Generate an exact migration inventory**

```powershell
rg -l '^\s*[A-Za-z_][A-Za-z0-9_-]*:\s*\{' win linux bench -g '*.mira'
rg -l '(^|\s)(dup|drop|swap|over|nip|rot|depth|@|!|c@|c!)(\s|$)' win linux bench -g '*.mira'
```

Classify each result as retained modern test/example, superseded legacy-only test, benchmark/fuzz input, or historical documentation. Do not delete a correctness scenario merely because its syntax is old; rewrite its behavior first.

- [ ] **Step 2: Convert the project generator first**

Generated `main.mira` must be:

```mira
fn main() {
    println("Hello from Mira!");
}
```

Run `mira -n smoke-project`, compile its `main.mira`, execute it, and remove the temporary generated project after verification.

- [ ] **Step 3: Convert tests by subsystem**

Convert arithmetic/control flow, then structures/memory, then concurrency, then applications. After each group, run it at O0-O3 before proceeding. Preserve expected outputs and paired GCC references.

- [ ] **Step 4: Convert benchmarks and fuzz generation**

Update generation templates to emit only `fn`, modern declarations, infix expressions, and namespaced imports. Run at least 2,000 differential cases before legacy deletion.

- [ ] **Step 5: Verify the inventory is empty outside history**

Repeat the searches from Step 1. Expected: no executable `.mira` matches outside files explicitly scheduled for deletion in Task 10.

- [ ] **Step 6: Commit the migration**

```text
refactor: migrate repository programs to Mira 6 syntax
```

### Phase E: Delete legacy language and publish Mira 6 behavior

### Task 10: Delete postfix parsing and legacy public names

**Files:**
- Modify: `win/parser/index.c`
- Modify: `win/parser/blocks.c`
- Modify: `win/parser/parse_one.c`
- Modify: `win/parser/helpers.c`
- Modify: `win/parser/parser.h`
- Modify: `win/codegen/words*.c`
- Remove: `win/libs-mira/`
- Create: `win/tests/legacy/{postfix_function,colon_function,stack_word,var_fetch,syntax_pragma}_error.mira`
- Create: `win/tests/run_legacy_rejection.ps1`
- Mirror: corresponding `linux/` files

**Interfaces:**
- Produces: a single infix parser with targeted errors for recognized 5.x forms

- [ ] **Step 1: Write failing rejection tests**

Each fixture must assert a diagnostic containing both `Mira 6 removed postfix syntax` and a modern replacement example. Before deletion, these tests fail because old syntax still compiles.

- [ ] **Step 2: Remove syntax-mode switching**

Delete `current_syntax_mode`; make modern parsing unconditional. Remove `!syntax`, classic colon definitions, postfix function definitions, legacy `var`/colon assignment, postfix block rewrites, and postfix list/lambda paths only after their modern equivalents are covered.

- [ ] **Step 3: Remove stack words and legacy aliases from source lookup**

Delete public source recognition for stack manipulation, postfix loads/stores, short string/list names, duplicate task APIs, and legacy WinAPI words. Keep internal code-generation helpers only if reachable from modern IR lowering; otherwise delete them and their Makefile dependencies.

- [ ] **Step 4: Add targeted migration diagnostics**

Recognize the leading shapes of removed syntax before generic parse failure. Do not implement a compatibility parser: consume only enough tokens to report the replacement and terminate compilation.

- [ ] **Step 5: Run rejection and modern suites**

Expected: every legacy fixture fails with the specified diagnostic; every modern fixture and standard module still passes O0-O3.

- [ ] **Step 6: Commit irreversible deletion**

```text
feat!: remove postfix language and legacy standard library
```

### Task 11: Cross-platform full verification and performance gate

**Files:**
- Modify as needed: `win/fulltest.sh`, `linux/regress.sh`, benchmark scripts
- Create: `bench/mira6_baseline.md`

**Interfaces:**
- Produces: reproducible evidence for semantic consistency, performance, compile time, and output size

- [ ] **Step 1: Clean-build Windows and Linux**

Windows:

```powershell
mingw32-make clean
mingw32-make
```

Linux via native host or WSL:

```sh
make clean
make
```

Expected: warning review contains no new parser/module warnings; both runtime sets build.

- [ ] **Step 2: Run complete correctness suites**

Run modern tests, standard-library tests, scheduler/channel C tests, full O0-O3 regressions, and at least 2,000 differential generated programs on Windows. Run the mirrored regression set natively on Linux.

- [ ] **Step 3: Measure medians**

For existing fib, branch, pressure, stencil, vector, and 2,000-case expanded workloads, run at least 9 measured rounds after 3 warmups. Record compiler time, program runtime, and executable bytes for the pre-Mira-6 checkpoint and final build.

- [ ] **Step 4: Apply the 3% gate**

If a stable median regression exceeds 3%, stop and identify whether it comes from wrapper non-inlining, extra runtime linkage, module lookup, or unrelated noise. Do not waive it in the same commit.

- [ ] **Step 5: Record evidence and commit**

Write commands, environment, medians, sizes, and pass counts to `bench/mira6_baseline.md`, then commit:

```text
test: verify Mira 6 correctness and performance
```

### Task 12: Documentation, version, and release readiness

**Files:**
- Modify: `README.md`
- Create: `LANGUAGE.md`
- Create: `STDLIB.md`
- Create: `MIGRATION-6.0.md`
- Modify: `win/main.c`, `linux/main.c`
- Mirror design/plan docs into the chosen shared documentation layout or retain `win/docs` as authoritative with README links

**Interfaces:**
- Produces: accurate public contract for Mira 6.0

- [ ] **Step 1: Write documentation verification examples**

Extract every fenced Mira example into a temporary file, compile it, and execute examples with deterministic output. The script fails if documentation references any removed name.

- [ ] **Step 2: Rewrite the public language guide**

Document modern functions and both return forms, declarations, operators, control flow, structures/methods, enums/match, lambdas, exceptions, modules/imports, FFI, unsafe memory, task/channel semantics, and exact unsupported areas.

- [ ] **Step 3: Document every public standard function**

For each function list signature, ownership/allocation behavior, platform availability, boundary behavior, and one compiling example. Do not document implementation-only `__mira_*` names as usable APIs.

- [ ] **Step 4: Update version and CLI truthfully**

Set `MIRA_VERSION` to `6.0.0`. Make help accurately describe `-S` as IR output unless actual assembly output is implemented separately. Decide the documented default optimization from the actual driver value; do not claim O3 while code defaults to O2.

- [ ] **Step 5: Final clean verification**

Run documentation examples, clean builds, focused standard-library suites, legacy rejection, full O0-O3 suite, and cross-platform smoke programs from outside the source directory.

- [ ] **Step 6: Commit release-ready docs**

```text
docs: publish Mira 6 language and standard library guides
```

## Final completion checklist

- [ ] `git status --short` contains no build outputs or accidental files.
- [ ] Windows and Linux platform-neutral file hashes match after line-ending normalization.
- [ ] No executable `.mira` source uses postfix definitions, stack words, postfix memory operations, or old standard-library names.
- [ ] Every expected-error fixture fails for the intended reason.
- [ ] All retained modern programs agree at O0-O3.
- [ ] The 2,000-case differential suite passes.
- [ ] Performance, compile-time, and output-size medians satisfy the 3% gate.
- [ ] README, LANGUAGE, STDLIB, migration guide, CLI help, and version agree.
