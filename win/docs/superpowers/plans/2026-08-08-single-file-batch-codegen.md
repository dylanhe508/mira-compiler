# Single-File Batch Codegen Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reduce Windows `bench/gen_800.mira -O3` complete compile/link median from about 694 ms to at most 250 ms without regressing small-file compile time, runtime, or binary size.

**Architecture:** First expose codegen substage timings under the existing profiling switch. Then add a module-owned open-addressed function index and use it to replace repeated whole-module name and identity scans in SSA construction and inlining. Rebuild index-derived call facts only at explicit call-graph mutation boundaries; fuse no facts with different invalidation lifetimes.

**Tech Stack:** C11, Mira SSA pipeline, MinGW-w64 GCC, PowerShell benchmark/regression scripts, Linux/SysV Makefile under WSL.

## Global Constraints

- The optimized command is one cold-start single-file invocation; no daemon, multi-file amortization, or cross-build cache.
- `gen_800.mira -O3`: 3 warmups + 31 samples, median at most 250 ms.
- `gen_100.mira -O3`: compile median may not regress by more than 1% after a 101-sample confirmation.
- Runtime benchmarks may not show a reproducible regression; binary size growth is at most 1%.
- O0--O3 semantics, Windows x86-64, and Linux/SysV must remain correct.
- No benchmark names, function names, or fixed constants in optimization policy.
- Every production change follows RED--GREEN and is committed independently.

---

### Task 1: Codegen Substage Profiling

**Files:**
- Modify: `win/codegen/program.c`
- Modify: `win/main.c`
- Create: `win/tests/run_compile_profile.ps1`

**Interfaces:**
- Consumes: existing `MIRA_COMPILE_PROFILE` environment switch.
- Produces: one stderr line beginning `codegen-profile` with numeric fields `build`, `closure`, `ref`, `opt`, `phi-map`, `regalloc`, `lower`, `machine`, and `total` in milliseconds.

- [ ] **Step 1: Write the failing profile-output test**

Create `run_compile_profile.ps1` so it invokes `mira.exe -O3 tests/regression_phi_inline.mira`, captures stderr with `MIRA_COMPILE_PROFILE=1`, and rejects absent or malformed fields:

```powershell
$required = 'build','closure','ref','opt','phi-map','regalloc','lower','machine','total'
$env:MIRA_COMPILE_PROFILE = '1'
$text = (& $mira -O3 $case 2>&1 | Out-String)
foreach ($name in $required) {
    if ($text -notmatch "(?:^| )$([regex]::Escape($name))=[0-9]+(?:\.[0-9]+)?") {
        throw "missing codegen profile field: $name"
    }
}
```

- [ ] **Step 2: Run RED**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File tests\run_compile_profile.ps1`

Expected: FAIL with `missing codegen profile field: build` because only the outer `compile-profile` line exists.

- [ ] **Step 3: Add profiling with zero hot-loop clock reads when disabled**

In `program.c`, sample `clock()` only at phase boundaries when the environment flag is present. Use this exact phase order around existing calls:

```c
clock_t p[10] = {0};
int prof = getenv("MIRA_COMPILE_PROFILE") != NULL;
#define CODEGEN_TICK(n) do { if (prof) p[(n)] = clock(); } while (0)
CODEGEN_TICK(0);
ssa_build_program(prog, &ssa_mod); CODEGEN_TICK(1);
ssa_build(&ssa_mod); CODEGEN_TICK(2);
ssa_ref_analyze_module(&ssa_mod); CODEGEN_TICK(3);
ssa_optimize_module(&ssa_mod); CODEGEN_TICK(4);
/* existing PHI destruction and var map calls */ CODEGEN_TICK(5);
ssa_allocate_registers(&ssa_mod); CODEGEN_TICK(6);
/* existing decision refresh */
ssa_lower_module(&ssa_mod, &cg->ir); CODEGEN_TICK(7);
/* existing machine IR passes */ CODEGEN_TICK(8);
```

Emit the single line after machine IR optimization, using the existing clock-to-ms convention from `main.c`; do not alter normal stdout.

- [ ] **Step 4: Run GREEN and existing profile smoke test**

Run the new script, then compile `bench/gen_100.mira` without the environment variable and verify no `codegen-profile` text appears. Expected: PASS.

- [ ] **Step 5: Commit**

```powershell
git add win/codegen/program.c win/tests/run_compile_profile.ps1
git commit -m "perf: expose codegen substage timings"
```

---

### Task 2: Module Function Name Index

**Files:**
- Create: `win/codegen/ssa_function_index.c`
- Modify: `win/codegen/ir_ssa.h`
- Modify: `win/codegen/ssa_builder.c`
- Modify: `win/codegen/ssa_builder.c` module init/free routines only as required
- Modify: `win/BUILD.txt`
- Modify: `win/Makefile`
- Create: `win/tests/ssa_function_index_test.c`

**Interfaces:**
- Produces:

```c
typedef struct SsaFunctionIndex SsaFunctionIndex;
bool ssa_function_index_rebuild(SsaModule *mod);
void ssa_function_index_invalidate(SsaModule *mod);
void ssa_function_index_free(SsaModule *mod);
SsaFunction *ssa_function_index_find(const SsaModule *mod, const char *name);
int ssa_function_index_ordinal(const SsaModule *mod, const SsaFunction *func);
uint64_t ssa_function_index_name_comparisons(const SsaModule *mod);
```

- `SsaModule` gains `SsaFunctionIndex *function_index` and `uint64_t function_epoch`.
- Every append/remove/reorder of `mod->functions` increments `function_epoch` and invalidates the index.

- [ ] **Step 1: Write index correctness and complexity tests**

The C test creates 800 functions named `f0` through `f799`, rebuilds the index, queries every name and pointer, checks a missing name, and asserts the comparison counter is bounded:

```c
assert(ssa_function_index_rebuild(&mod));
for (int i = 0; i < 800; ++i) {
    char name[32]; snprintf(name, sizeof(name), "f%d", i);
    assert(ssa_function_index_find(&mod, name) == mod.functions[i]);
    assert(ssa_function_index_ordinal(&mod, mod.functions[i]) == i);
}
assert(ssa_function_index_find(&mod, "missing") == NULL);
assert(ssa_function_index_name_comparisons(&mod) < 8000);
```

Also mutate the function array, increment the epoch through the public invalidation call, and assert lookup refuses stale data until rebuild.

- [ ] **Step 2: Run RED**

Run:

```powershell
gcc -std=c11 -O0 -I. tests\ssa_function_index_test.c codegen\ssa_builder.c codegen\ssa_function_index.c -o tests\ssa_function_index_test.exe
```

Expected: FAIL because the header types and implementation do not exist.

- [ ] **Step 3: Implement the minimal open-addressed index**

Use FNV-1a or the repository hash helper, power-of-two capacity at least twice `func_count`, linear probing, and full-string equality on hash matches. Store the module epoch captured by rebuild. Allocation failure returns `false` without replacing a valid old index. Empty modules rebuild successfully.

- [ ] **Step 4: Integrate builder lookups**

Refactor `ssa_build_program` into two explicit phases: create all `SsaFunction` shells and build the name index, then build bodies into the existing shells. Replace the loops around current call-type repair lookups at `ssa_builder.c:1812` and `ssa_builder.c:1864` with `ssa_function_index_find`. Preserve source/reverse-definition behavior and `mira_main` normalization exactly.

- [ ] **Step 5: Run GREEN and builder regressions**

Run the new unit test, `run_phi_inline.ps1`, `run_nonvolatile_calls.ps1`, and O0--O3 compilation for gen_100/200/400/800. Expected: all checksums unchanged.

- [ ] **Step 6: Measure before proceeding**

Run 3 warmups + 31 samples at O0 and O3. Record substage medians and comparison count. If O0 does not materially improve, stop and profile before Task 3 rather than layering another change.

- [ ] **Step 7: Commit**

```powershell
git add win/codegen/ssa_function_index.c win/codegen/ir_ssa.h win/codegen/ssa_builder.c win/BUILD.txt win/Makefile win/tests/ssa_function_index_test.c
git commit -m "perf: index SSA functions by name"
```

---

### Task 3: Batch Call Facts for Inlining

**Files:**
- Modify: `win/codegen/ssa_function_index.c`
- Modify: `win/codegen/ir_ssa.h`
- Modify: `win/codegen/ssa_opt.c`
- Modify: `win/tests/ssa_function_index_test.c`
- Create: `win/tests/ssa_call_facts_test.c`

**Interfaces:**
- Produces:

```c
bool ssa_function_index_rebuild_call_facts(SsaModule *mod);
uint32_t ssa_function_index_direct_calls(const SsaModule *mod,
                                         const SsaFunction *func);
bool ssa_function_index_is_referenced(const SsaModule *mod,
                                      const SsaFunction *func);
bool ssa_function_index_is_leaf(const SsaModule *mod,
                                const SsaFunction *func);
```

- Call facts carry both the function epoch and a call-graph epoch. Any CALL/ICALL insertion, deletion, or retargeting invalidates call facts.

- [ ] **Step 1: Write the failing call-facts test**

Construct callers with direct CALL operands, an address-taken `LEA_FUNC`, an unknown external call, and an ICALL. Assert exact direct-call counts, reference state, and leaf state. Retarget one call, invalidate, and assert stale queries return conservative values until rebuild.

- [ ] **Step 2: Run RED**

Compile and run the test. Expected: FAIL because call-fact APIs are missing.

- [ ] **Step 3: Implement one-pass fact collection**

Scan each instruction once. Resolve direct symbols through the name index, increment a saturating `uint32_t` call count, mark symbol operands as references, and set caller leaf=false for CALL or ICALL. Unknown symbols do not create module references. Extended operands, `op1`, and `op2` must all be covered without double-counting the CALL target.

- [ ] **Step 4: Replace inliner census and identity scans**

In `ssa_opt_inline`, replace the nested `original_func_count` name census, callee ordinal scan, `was_inlined` identity scan, and final whole-module per-candidate reference rescan with indexed ordinal/direct-call/reference queries. After any committed inline, invalidate call facts and rebuild once before removal of newly unreferenced functions; do not rebuild per callsite.

- [ ] **Step 5: Run GREEN and inliner shape regressions**

Run `ssa_call_facts_test`, `run_phi_inline.ps1`, `run_affine_collapse.ps1`, `run_affine_profitability.ps1`, `run_multi_factor_affine.ps1`, and `run_nonvolatile_calls.ps1`. Expected: exact existing checksums and shape counts.

- [ ] **Step 6: Rebuild and measure**

Run Windows 100/200/400/800 O0 and O3 benchmarks. The combined Tasks 2--3 must make growth near-linear. If gen_800 remains above 250 ms, use `codegen-profile` to select the next phase; do not assume scan fusion is next.

- [ ] **Step 7: Commit**

```powershell
git add win/codegen/ssa_function_index.c win/codegen/ir_ssa.h win/codegen/ssa_opt.c win/tests/ssa_function_index_test.c win/tests/ssa_call_facts_test.c
git commit -m "perf: batch SSA call-graph facts"
```

---

### Task 4: Evidence-Driven Safe Scan Fusion

**Files:**
- Modify: `win/codegen/ssa_opt.c` only if Task 3 profiling confirms module optimization remains the dominant phase
- Create: `win/tests/ssa_module_facts_test.c` only under the same measured condition
- Modify: `win/docs/superpowers/specs/2026-08-08-single-file-batch-codegen-design.md`

**Interfaces:**
- Consumes: Task 3 `codegen-profile` output.
- Produces: one fused fact collector whose outputs all share the same explicit invalidation epoch.

- [ ] **Step 1: State one measured hypothesis in the design evidence section**

Record the dominant remaining phase, its median cost, the exact repeated scans, and the expected saved work. If no phase can account for the gap to 250 ms, stop this task and report the evidence instead of changing code.

- [ ] **Step 2: Write a failing scan-count test**

Add a test-only counter at the selected public collector boundary, build an 800-function module, and assert one module instruction visit per fact epoch. Verify RED against the existing repeated collectors.

- [ ] **Step 3: Implement only the measured fusion**

Merge collectors only when they consume the same SSA/call-graph epoch. Keep legality checks and optimization decisions unchanged. Invalidation must be explicit at every mutator found by `rg`.

- [ ] **Step 4: Run GREEN and complete regression suite**

Run the focused test, all existing Windows O0--O3 regression scripts used by the affine work, then rebuild and run Linux/SysV versions.

- [ ] **Step 5: Run final acceptance benchmark**

Run 3 warmups + 31 samples for gen_200/400/800 and 3 + 101 for gen_100. Interleave baseline and candidate runs. Verify gen_800 <=250 ms, gen_100 ratio <=1.01, size ratio <=1.01, and stable checksums.

- [ ] **Step 6: Verify runtime non-regression**

Measure fib and stencil for 31 samples; branch, vector_add, and pressure for 101 samples when the initial ratio is outside noise. Reject any reproducible runtime regression.

- [ ] **Step 7: Commit final evidence**

```powershell
git add win/docs/superpowers/specs/2026-08-08-single-file-batch-codegen-design.md win/codegen/ssa_opt.c win/tests/ssa_module_facts_test.c
git commit -m "perf: fuse measured SSA module facts"
```

---

## Final Verification

- [ ] Build `win/mira.exe` using the exact command in `win/BUILD.txt`.
- [ ] Build `win/mira` and runtime with `make -j2 mira runtime` under WSL Ubuntu.
- [ ] Run all focused unit tests and Windows regression scripts.
- [ ] Run 200 seeded fuzz programs across O0--O3; require zero ICE/output/runtime differences, classifying generator-invalid O0 rejections against the formal baseline.
- [ ] Run `git diff --check` and request an independent code review.
- [ ] Do not replace `E:\mira\mira\win\mira.exe` until review and every acceptance gate pass.
