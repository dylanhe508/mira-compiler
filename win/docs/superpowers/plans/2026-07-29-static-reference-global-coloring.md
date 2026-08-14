# Static-Reference Global Coloring Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a bounded deterministic global graph-coloring allocator for high-pressure O3 SSA functions.

**Architecture:** Static reference and decision facts authorize the pass; current live intervals build the interference relation; loop/use priority orders nodes. The allocator falls back unchanged to linear scan outside a 512-node budget.

**Tech Stack:** C11, Mira SSA IR, Windows x64 ABI, structural C tests, PowerShell differential benchmarks.

## Global Constraints

- Enable only at O3 through `prefer_global_graph_coloring`.
- Never color more than 512 active integer intervals.
- Do not add variable-slot auction, global variable base or loop-slot promotion.
- Preserve cross-call colors 5–11 and cross-div exclusions 0/2.
- Preserve fIR reserved colors and R10/R11 scratch reservation.
- Keep warmed compile median below 100 ms and runtime regression below 5%.

---

### Task 1: Add decision gate backed by static reference

**Files:**
- Modify: `codegen/decision.h`
- Modify: `codegen/ssa_opt.c`
- Modify: `tests/decision/test_decision_model.c`

- [ ] Add red decision tests for optimization level, pressure, value budget, known ratio and unknown effects.
- [ ] Add `prefer_global_graph_coloring` to `DecisionFunctionPlan`.
- [ ] Set it during `ssa_decision_refresh_plans()` only when all design gates pass.
- [ ] Add deterministic debug output and run decision tests.

### Task 2: Implement bounded deterministic coloring

**Files:**
- Modify: `codegen/ssa_regalloc.c`
- Create: `tests/decision/test_global_coloring.c`

- [ ] Add red structural tests for interference, non-interference, priority, call/div constraints, fIR reservations and 513-node fallback.
- [ ] Implement interval overlap and deterministic priority comparison.
- [ ] Build a bounded node list and greedily assign the first legal color.
- [ ] Return false without mutating final allocation when disabled, over budget or allocation fails.
- [ ] Call global coloring before linear scan; use linear scan only on false.
- [ ] Run allocator structural tests and existing nonvolatile tests.

### Task 3: Integrate correctness and performance gates

**Files:**
- Modify: `tests/extreme_apps/RESULTS.md`

- [ ] Rebuild the complete compiler.
- [ ] Run PHI, divrem, nonvolatile and global-color structural tests.
- [ ] Run focused O0–O3 and nonvolatile call differential tests.
- [ ] Compare graph coloring enabled/disabled spill counts on a dedicated high-pressure fixture.
- [ ] Interleave old/new long-matrix binaries for at least 11 samples each.
- [ ] Measure warmed `bench_fib.mira -O3` compilation five times.
- [ ] Record exact output, medians, known failures and final review.

