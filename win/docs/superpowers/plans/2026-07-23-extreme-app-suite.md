# Mira Extreme Application Suite Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add six deterministic, C-oracled extreme applications and a bounded O0–O3/performance runner.

**Architecture:** Independent Mira sources feed one runner; one C oracle selects the equivalent workload by name. Correctness is a hard gate before O3 timing.

**Tech Stack:** Mira, C11, MinGW GCC, PowerShell.

## Global Constraints

- Use the copied E-drive latest compiler source.
- Never modify optimizer code while discovering failures.
- Use 10-second child-process timeouts.
- Require exact output equality at O0–O3.
- Use 3 warmups and 11 measured samples for performance.

---

### Task 1: Runner contract

**Files:**
- Create: `tests/extreme_apps/run.ps1`
- Create: `tests/extreme_apps/oracle.c`

- [ ] Make the runner fail when the six required Mira sources are absent.
- [ ] Verify the missing-source failure.
- [ ] Add the oracle selector, bounded process helper, build/link loop and output comparison.

### Task 2: Integer pipeline applications

**Files:**
- Create: `tests/extreme_apps/telemetry_pipeline.mira`
- Create: `tests/extreme_apps/particle_simulation.mira`
- Create: `tests/extreme_apps/hash_aggregation.mira`
- Modify: `tests/extreme_apps/oracle.c`

- [ ] Add equivalent C and Mira workloads.
- [ ] Run O0–O3 and preserve the first mismatch as evidence.

### Task 3: Nested, memory and call applications

**Files:**
- Create: `tests/extreme_apps/matrix_stencil.mira`
- Create: `tests/extreme_apps/allocator_churn.mira`
- Create: `tests/extreme_apps/deep_control_flow.mira`
- Modify: `tests/extreme_apps/oracle.c`

- [ ] Add equivalent C and Mira workloads.
- [ ] Run O0–O3 and preserve the first mismatch as evidence.

### Task 4: Performance report

**Files:**
- Modify: `tests/extreme_apps/run.ps1`

- [ ] After correctness passes, build O3 GCC cases.
- [ ] Run 3 warmups and 11 alternating measurements.
- [ ] Print checksum, medians, ratio, compile times and executable sizes.

