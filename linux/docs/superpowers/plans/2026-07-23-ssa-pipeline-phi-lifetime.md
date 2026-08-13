# Mira SSA PHI Lifetime Pipeline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Preserve legal SSA through module inlining and optimization, then destroy PHI exactly once before register allocation.

**Architecture:** Separate mem2reg construction from optimization and PHI destruction. The module optimizer becomes the owner of inlining, analysis refresh, and SSA fixed points; `program.c` becomes the explicit phase boundary that destroys PHI after optimization.

**Tech Stack:** C11, Mira SSA IR, dominance analysis, Windows x64 backend, PowerShell differential tests.

## Global Constraints

- Do not add induction strength reduction in this phase.
- Do not add hidden `mira_vars` slots or change BSS size.
- Preserve O0–O3 behavior.
- Validate current instruction chains instead of trusting stale `vreg_defs`.
- Keep warmed ten-line compile median below 100 ms.

---

### Task 1: Split PHI destruction from SSA construction

**Files:**
- Modify: `codegen/ssa_mem2reg.c`
- Modify: `codegen/ir_ssa.h`
- Modify: `codegen/program.c`

- [ ] Remove the per-function `ssa_optimize_function()` and `destroy_phis()` calls from `ssa_build()`.
- [ ] Export `ssa_destroy_phis_module(SsaModule *)`, using the existing per-function destroyer.
- [ ] Call it in `program.c` immediately after `ssa_optimize_module()` and before `ssa_compute_var_reg_maps()`/register allocation.
- [ ] Rebuild and run the four focused O0–O3 regressions.

### Task 2: Make module optimization operate on retained SSA

**Files:**
- Modify: `codegen/ssa_opt.c`
- Modify: `codegen/ssa_dom.c` if a public recomputation helper is required.
- Test: existing focused and extreme regressions.

- [ ] Add a current-chain VReg single-definition validator and def-table rebuilder.
- [ ] After inlining, renumber blocks and recompute CFG predecessor/successor and dominance facts.
- [ ] Refresh static-reference facts, pressure, decisions, and loop analysis.
- [ ] Remove the block-name-based `has_inlined_blocks` skip.
- [ ] Run `ssa_optimize_function()` only when current-chain SSA validation succeeds.
- [ ] Revalidate before PHI destruction and report a deterministic diagnostic under `MIRA_DECISION_DEBUG` when a pass is skipped.
- [ ] Rebuild and run the full extreme suite.

### Task 3: Verify phase-one performance and stability

**Files:**
- Modify: `tests/extreme_apps/RESULTS.md`

- [ ] Run all focused O0–O3 regressions and the six extreme applications.
- [ ] Run the long matrix after two warmups for seven samples.
- [ ] Measure warmed `bench_fib.mira` compilation five times.
- [ ] Record exact output, samples, medians, and any skipped SSA passes.
- [ ] Proceed to induction-strength implementation only when correctness is complete and long-matrix regression is within 5%.

