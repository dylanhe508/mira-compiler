# Pressure-Aware Machine Scheduling Implementation Plan

> Design: `docs/superpowers/specs/2026-08-16-pressure-aware-machine-scheduling-design.md`

**Goal:** Replace Mira's existing local instruction-move heuristic with a
deterministic, pressure-aware basic-block list scheduler while retaining its O3
gate and conservative barriers.

**Architecture:** Keep `ir_opt_ilp_schedule` as the public pass.  Extend its
instruction facts, build a bounded DAG for each schedulable region, calculate
critical paths, and choose ready nodes by critical path plus a live-register
cost.  Windows and Linux receive equivalent platform-neutral changes.

**Implementation language:** C11 compiler code, PowerShell regression runners.

---

## Task 1: Lock the scheduler contract with a failing unit test

**Files:**

- Create: `win/tests/ir_schedule_test.c`
- Create: `linux/tests/ir_schedule_test.c`

1. Construct direct `IrBuffer` regions for a long dependent chain plus an
   independent high-latency multiply/load, and assert the desired deterministic
   order.
2. Add negative cases for GPR RAW/WAR/WAW, flags, memory-write, call, and branch
   boundaries.
3. Compile the Windows test against `codegen/ir.c` and `codegen/ir_opt.c`.
4. Run it against the current implementation and record the expected RED from
   the scheduling-shape assertion.
5. Mirror the exact platform-neutral test to Linux and verify matching hashes.

## Task 2: Implement instruction facts and dependency DAG

**Files:**

- Modify: `win/codegen/ir_opt.c`
- Modify: `linux/codegen/ir_opt.c`

1. Add a compact instruction-facts structure containing GPR read/write masks,
   flag effects, latency, original index, and schedulability.
2. Preserve the existing barrier policy and conservatively reject unsupported
   or oversized regions.
3. Build forward RAW/WAR/WAW and flags edges within each region.
4. Compute each node's reverse critical-path length.
5. Run the unit test; dependency/barrier tests must pass even before ordering
   policy is enabled.

## Task 3: Add pressure-aware deterministic list scheduling

**Files:**

- Modify: `win/codegen/ir_opt.c`
- Modify: `linux/codegen/ir_opt.c`
- Modify: `win/tests/ir_schedule_test.c`
- Modify: `linux/tests/ir_schedule_test.c`

1. Maintain remaining-use counts and the currently live GPR mask while nodes
   are emitted.
2. Rank ready nodes by critical path, then live-range pressure delta, then
   original order.
3. Estimate original and proposed peak pressure; retain the original order if
   the proposed peak is greater or if no scheduling score improves.
4. Run the focused test to GREEN and repeat it to prove deterministic output.
5. Run `git diff --check` and verify Win/Linux test and scheduler hunks match.

## Task 4: Prove compiler correctness

**Files:**

- Modify: `win/tests/run_hot_loop_codegen.ps1`
- Modify: `linux/tests/run_hot_loop_codegen.ps1`

1. Add focused test build/run coverage to the hot-loop runner.
2. Clean-build the Windows compiler.
3. Run gradual types (`-Group all`), modules, float, infix, short-circuit,
   branch-return, stdlib core/data O0-O3, and existing C machine-IR tests.
4. Compile-check the touched Linux translation units; native Linux remains a
   separate claim unless the environment becomes available.
5. Record exact commands and results in
   `.superpowers/sdd/machine-scheduling-report.md`.

## Task 5: Measure profitability and accept or reject

**Files:**

- Modify: `win/tests/type_enhancement_report.md`
- Modify: `linux/tests/type_enhancement_report.md`
- Modify: `.superpowers/sdd/machine-scheduling-report.md`

1. Rebuild telemetry, matrix, deep-integer, register-pressure, and float
   benchmark binaries from the same compiler revision and record size/hash.
2. Run a short repeated timing gate with warmup; telemetry is the primary
   target and all other cases are guardrails.
3. Revert the production scheduling change if no repeatable primary gain is
   present or if a guardrail regresses by more than 3%.
4. If accepted, run the affected Mira-vs-GCC comparison for 1000 rounds and
   report medians and ratios.
5. Run the focused and full correctness suites once more, check for tracked
   build artifacts, and commit implementation and evidence separately.
