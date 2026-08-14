# Mira Div/Rem Reuse Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reuse a preceding same-block signed quotient to derive a matching signed remainder without a second hardware division.

**Architecture:** Add one bounded SSA optimization pass in `ssa_opt.c`. It performs exact operand matching, inserts `MUL`, rewrites `SREM` to `SUB`, and runs before the existing scalar fixed point.

**Tech Stack:** C11, Mira SSA IR, Windows x64 backend, PowerShell differential tests.

## Global Constraints

- Only enable at O2/O3 when `decision_plan.pipeline.allow_magic_division` is true.
- Search only the same basic block and at most 128 preceding instructions.
- Do not add fib-, matrix-, or constant-sequence-specific recognition.
- Preserve O0–O3 behavior and signed division semantics.
- Keep warmed ten-line compile median below 100 ms.
- Keep long-matrix median within 5% of 64.725 ms.

---

### Task 1: Add structural red tests and the bounded SSA pass

**Files:**
- Create: `tests/decision/test_divrem_reuse.c`
- Modify: `codegen/ssa_opt.c`

**Interfaces:**
- Consumes: `SsaFunction`, `SsaBasicBlock`, `SsaInst`, `ssa_new_vreg()`
- Produces: `bool ssa_opt_reuse_divrem(SsaFunction *func)`

- [ ] Create a structural test function containing same-block `SDIV` then matching `SREM`; assert the pass changes `SREM` into `SUB` and inserts a `MUL` using the quotient VReg.
- [ ] Add negative structural cases for different operands and different blocks.
- [ ] Compile and run the structural test before implementation; expect the matching-case assertion to fail because the pass is absent.
- [ ] Implement exact IMM/VReg operand equality and a backward search capped at 128 instructions.
- [ ] Insert `MUL quotient, divisor` before the remainder and rewrite the remainder as `SUB dividend, product`.
- [ ] Run the structural test; expect all cases to pass.

### Task 2: Add semantic differential coverage

**Files:**
- Create: `tests/regression_divrem_reuse.mira`
- Create: `tests/regression_divrem_reuse.c`
- Create: `tests/run_divrem_reuse.ps1`

**Interfaces:**
- Consumes: `mira.exe`, GCC, Mira runtime objects
- Produces: O0–O3 result comparison and an O2/O3 IR/SSA structural count

- [ ] Write a deterministic accumulator over positive, negative and zero dividends with positive and negative nonzero divisors.
- [ ] Build and run the C oracle; record its exact output.
- [ ] Compile and run Mira at O0–O3 and require exact equality with the oracle.
- [ ] Verify O2/O3 optimized IR contains only one signed division for each paired quotient/remainder computation.

### Task 3: Full verification and performance gate

**Files:**
- Modify: `tests/extreme_apps/RESULTS.md`

**Interfaces:**
- Consumes: the rebuilt compiler and existing regression/benchmark sources
- Produces: exact correctness and timing evidence

- [ ] Rebuild the complete compiler.
- [ ] Run PHI/CFG structural tests and focused O0–O3 regressions.
- [ ] Run `regression_divrem_reuse` O0–O3.
- [ ] Run long matrix after two warmups for seven samples; require median <= 67.961 ms.
- [ ] Compile `bench_fib.mira -O3` after two warmups for five samples; require median < 100 ms.
- [ ] Record exact samples, medians, Device Guard blocks and review result in `RESULTS.md`.

