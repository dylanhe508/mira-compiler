# Mira Pressure Arithmetic Optimization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Collapse proven non-loop integer affine SSA regions so `bench_pressure` reaches or exceeds GCC `-O3` without regressions or more than 1% compile-time cost.

**Architecture:** Add a focused SSA affine-fact module that computes one-base wrapping forms in linear time, prove only identical terminal-mask coverage, and ask the existing Decision model whether to commit each rewrite. Keep register allocation and machine scheduling unchanged; existing lowering selects the final multiply instruction.

**Tech Stack:** C11, Mira SSA IR, Decision 2.1, TDM-GCC 10.3, PowerShell regression runners, Windows x86-64 and Linux SysV backends.

## Global Constraints

- Use rebuilt `win/mira.exe` as the baseline; never overwrite it with a candidate before final acceptance.
- Do not match benchmark names, function names, or particular integer constants.
- Preserve Mira's 64-bit two's-complement wrapping behavior at O0-O3.
- Reject every transform whose legality or profitability proof is incomplete.
- `pressure` must reach or beat GCC's 101-sample median.
- No reproducible runtime regression is allowed in other benchmarks.
- Warm compile-time median regression must not exceed 1%; values within noise require 101 samples.
- Every optimization begins with a failing test and ends with the full regression matrix.

---

### Task 1: Add the affine Decision candidate

**Files:**
- Modify: `codegen/decision.h`
- Modify: `codegen/decision.c`
- Create: `tests/affine_decision_test.c`

**Interfaces:**
- Consumes: `DecisionContext`, `DecisionCandidate`, `decision_choose`.
- Produces: `DECISION_SITE_ARITHMETIC`, `DECISION_AFFINE_COLLAPSE`, and `DecisionKind decision_choose_affine(uint32_t old_insts, uint32_t new_insts, uint32_t old_bytes, uint32_t new_bytes, uint32_t old_pressure, uint32_t new_pressure, uint32_t confidence, DecisionResult *detail)`.

- [ ] **Step 1: Write the failing Decision test**

Create `tests/affine_decision_test.c` with assertions that a `24 -> 3` instruction candidate is accepted, equal instruction counts are rejected, code growth is rejected, pressure growth is rejected, and zero confidence is rejected:

```c
#include "../codegen/decision.h"
#include <assert.h>
int main(void) {
    DecisionResult d;
    assert(decision_choose_affine(24, 3, 96, 12, 12, 2,
                                  DECISION_SCALE, &d) == DECISION_AFFINE_COLLAPSE);
    assert(decision_choose_affine(3, 3, 12, 12, 2, 2,
                                  DECISION_SCALE, &d) == DECISION_KEEP);
    assert(decision_choose_affine(24, 3, 12, 16, 12, 2,
                                  DECISION_SCALE, &d) == DECISION_KEEP);
    assert(decision_choose_affine(24, 3, 96, 12, 2, 3,
                                  DECISION_SCALE, &d) == DECISION_KEEP);
    assert(decision_choose_affine(24, 3, 96, 12, 12, 2, 0, &d) == DECISION_KEEP);
    return 0;
}
```

- [ ] **Step 2: Build and verify RED**

Run:

```powershell
gcc -std=c11 -O0 tests\affine_decision_test.c codegen\decision.c -o tests\affine_decision_test.exe
```

Expected: compile failure because the new enum values and function are undefined.

- [ ] **Step 3: Implement the minimal Decision wrapper**

Add the two enum values and implement `decision_choose_affine` with KEEP plus one collapse candidate. Set `legal` only when `new_insts < old_insts`, `new_bytes <= old_bytes`, and `new_pressure <= old_pressure`; set benefit to `(old_insts-new_insts)*16`, cost to `new_insts*2`, code-size cost to `new_bytes`, register cost to `new_pressure`, and use the supplied confidence.

- [ ] **Step 4: Run the Decision test and verify GREEN**

Run the command from Step 2 and then `tests\affine_decision_test.exe`.
Expected: exit code 0.

- [ ] **Step 5: Commit**

```powershell
git add codegen\decision.h codegen\decision.c tests\affine_decision_test.c
git commit -m "feat: add affine decision candidate"
```

### Task 2: Compute bounded non-loop affine facts

**Files:**
- Create: `codegen/ssa_affine.c`
- Modify: `codegen/ir_ssa.h`
- Create: `tests/ssa_affine_test.c`

**Interfaces:**
- Consumes: `SsaFunction`, `SsaInst`, `func->vreg_defs`, SSA opcodes.
- Produces: `SsaAffineFact` and `bool ssa_affine_analyze(const SsaFunction *func, SsaAffineFact *facts, size_t fact_count)`.

- [ ] **Step 1: Write structural RED tests**

Build a one-block function containing IMM, LOAD_PARAM, COPY, ADD, SUB, and constant MUL nodes. Assert that `((x*3+2)*5)+(x*7+4)` becomes `{base=x, coefficient=22, constant=14, proven=true}`. Add rejection fixtures for `x+y`, `x*y`, CALL, and a chain whose VReg index exceeds the fact array.

- [ ] **Step 2: Build and verify RED**

Run:

```powershell
gcc -std=c11 -O0 -I. tests\ssa_affine_test.c codegen\ssa_affine.c -o tests\ssa_affine_test.exe
```

Expected: compile failure because `ssa_affine.c`, `SsaAffineFact`, and the API do not yet exist.

- [ ] **Step 3: Define the fact type and linear analyzer**

Add to `codegen/ir_ssa.h`:

```c
typedef struct {
    VReg base;
    uint64_t coefficient;
    uint64_t constant;
    uint32_t instruction_count;
    bool proven;
} SsaAffineFact;
bool ssa_affine_analyze(const SsaFunction *func, SsaAffineFact *facts,
                        size_t fact_count);
```

Implement one forward pass by VReg definition order. Use `uint64_t` arithmetic for coefficient and constant. Merge only equal bases or a proven constant side. Mark every unsupported operation unproven without recursion or allocation.

- [ ] **Step 4: Run structural tests and verify GREEN**

Run the command from Step 2 and execute the test. Expected: exit code 0 and no output.

- [ ] **Step 5: Commit**

```powershell
git add codegen\ssa_affine.c codegen\ir_ssa.h tests\ssa_affine_test.c
git commit -m "feat: analyze bounded SSA affine facts"
```

### Task 3: Prove terminal-mask coverage and rewrite transactionally

**Files:**
- Modify: `codegen/ssa_affine.c`
- Modify: `codegen/ir_ssa.h`
- Modify: `tests/ssa_affine_test.c`

**Interfaces:**
- Consumes: facts from `ssa_affine_analyze`, use counts, `decision_choose_affine`.
- Produces: `bool ssa_opt_affine_collapse(SsaFunction *func)`.

- [ ] **Step 1: Add RED fixtures for rewrite and mask legality**

Add a pressure-shaped single block with twelve affine temporaries, a weighted sum, an inner `AND 0x7fffffffffffffff`, and the same terminal AND. Require one MUL, at most two ADDs, and one final AND after the pass. Add digest-based rejection tests for a different mask, a shift between masks, two bases, a CALL boundary, and a Decision KEEP result.

- [ ] **Step 2: Run and verify RED**

Expected: positive shape assertions fail while rejection fixtures remain unchanged.

- [ ] **Step 3: Implement use counts and mask proof**

Count VReg uses in one linear scan. Accept mask removal only when both masks are identical `2^n-1`, every path from the inner value to the final AND is ADD/SUB/constant-MUL, and all intermediate results are exclusively consumed by the proven region.

- [ ] **Step 4: Implement a transactional rewrite**

Before mutation, allocate all replacement instructions. Emit `MUL base, coefficient`, optional `ADD constant`, and the retained terminal AND using the original root destination VReg. Ask `decision_choose_affine` using old/new instruction counts, byte estimates, current `estimated_scalar_pressure`, and full confidence. On KEEP or allocation failure, free candidates and leave the digest unchanged. On COLLAPSE, splice the replacement and rebuild `vreg_defs`.

- [ ] **Step 5: Run structural tests and verify GREEN**

Expected: all positive and rejection fixtures pass.

- [ ] **Step 6: Commit**

```powershell
git add codegen\ssa_affine.c codegen\ir_ssa.h tests\ssa_affine_test.c
git commit -m "feat: collapse proven affine SSA regions"
```

### Task 4: Integrate at O3 and build the candidate compiler

**Files:**
- Modify: `codegen/ssa_opt.c`
- Modify: `BUILD.txt`
- Modify: `Makefile`
- Create: `tests/run_affine_collapse.ps1`

**Interfaces:**
- Consumes: `ssa_opt_affine_collapse`.
- Produces: one O3 pipeline invocation after inlining/final scalar cleanup and before PHI destruction/register allocation.

- [ ] **Step 1: Write the source-level RED runner**

Create a generic `.mira`/C oracle pair inside the runner's temporary directory. Compile O0-O3, compare checksums, and assert that O3 `-S` still contains more than one `imul` before integration. Also set `MIRA_DECISION_DISABLE=affine-collapse` and require the original IR shape.

- [ ] **Step 2: Run and verify RED**

Run `powershell -ExecutionPolicy Bypass -File tests\run_affine_collapse.ps1`.
Expected: semantic checks pass; optimized shape assertion fails.

- [ ] **Step 3: Integrate the pass and build files**

Call `ssa_opt_affine_collapse` only when `mira_opt_level >= 3`, after module inlining and the subsequent scalar fixed point. If it changes a function, run copy propagation and DCE once. Add `codegen\ssa_affine.c` to Windows and Linux build source lists. Add `affine-collapse` to `decision_pipeline_disable` without changing the existing `affine` loop switch.

- [ ] **Step 4: Build a candidate without replacing formal Mira**

Run the `BUILD.txt` command with output changed to `out\mira-pressure-candidate.exe`.
Expected: exit code 0 and no new warnings.

- [ ] **Step 5: Run the runner and verify GREEN**

Run the runner with the candidate path. Expected: O0-O3 checksums match, enabled O3 has the collapsed shape, disabled O3 retains the original shape.

- [ ] **Step 6: Commit**

```powershell
git add codegen\ssa_opt.c codegen\decision.c BUILD.txt Makefile tests\run_affine_collapse.ps1
git commit -m "feat: integrate affine collapse at O3"
```

### Task 5: Full correctness and performance gate

**Files:**
- Modify: `docs/superpowers/specs/2026-08-08-pressure-arithmetic-design.md` only to append measured acceptance evidence.

**Interfaces:**
- Consumes: candidate compiler and all existing regression runners.
- Produces: an evidence-backed accept/reject decision; no formal binary replacement.

- [ ] **Step 1: Run focused regressions**

Run `run_affine_collapse.ps1`, `run_affine_profitability.ps1`, `run_multi_factor_affine.ps1`, `run_induction_strength.ps1`, `run_dynamic_internal_slots.ps1`, `run_mul_imm_strength.ps1`, `run_divrem_reuse.ps1`, `run_phi_inline.ps1`, and `run_nonvolatile_calls.ps1` against the candidate. Expected: every O0-O3 checksum matches its oracle.

- [ ] **Step 2: Run broad differential coverage**

Run the five-benchmark O0-O3 regression, six extreme applications, existing fuzz differential suite, and mira2c 40-program measure set. Expected: zero mismatches, crashes, or new diagnostics on Windows; then build and run the portable subset under Linux/SysV.

- [ ] **Step 3: Measure pressure acceptance**

Compile candidate and GCC outputs once, warm each three times, then collect 101 internal elapsed samples. Require equal checksums and candidate median `<=` GCC median.

- [ ] **Step 4: Measure zero-regression runtime gate**

For branch, fib, stencil, vector_add, and the existing extreme-app performance cases, collect 101 samples when the initial 31-sample comparison differs by up to 1%. Reject the change for any reproducible slowdown.

- [ ] **Step 5: Measure compile-time gate**

Use unchanged ten-line, fib, and pressure sources. After three warmups, collect 101 complete compile/link samples from formal and candidate compilers. Require candidate median/formal median `<= 1.01` for every source.

- [ ] **Step 6: Record evidence and commit**

Append exact versions, sample counts, medians, ratios, checksums, regression totals, and acceptance decision to the design document. Run `git diff --check`, then commit:

```powershell
git add win\docs\superpowers\specs\2026-08-08-pressure-arithmetic-design.md
git commit -m "docs: record pressure optimization evidence"
```

- [ ] **Step 7: Stop before formal installation**

If every gate passes, report the candidate path and commit. Do not overwrite `win/mira.exe` until the user explicitly approves installation.
