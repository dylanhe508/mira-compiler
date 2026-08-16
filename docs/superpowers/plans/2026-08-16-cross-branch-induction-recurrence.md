# Cross-Branch Induction Recurrence Implementation Plan

**Goal:** Reuse Mira's proved induction recurrence in branch-heavy natural
loops and remove repeated integer constant multiplies without changing program
semantics or adding a new optimization subsystem.

## Task 1: Add the failing cross-branch regression

- Extend `win/tests/regression_induction_strength.mira` and its C oracle with a
  branch-heavy loop containing two distinct `induction * constant` groups.
- Mirror the fixture changes under `linux/tests/`.
- Extend both induction runners to capture O3 decision diagnostics and assert
  that the selected hot-loop factor is admitted.
- Run the suite against the current compiler and record the expected RED: the
  branch-loop affine pass is disabled before candidate discovery.

## Task 2: Remove only the obsolete blanket vetoes

- In both `win/codegen/ssa_opt.c` and `linux/codegen/ssa_opt.c`, keep vector,
  unroll, scalar-loop, magic-division, and rotation restrictions for branchy
  loops, but do not disable the independently proved affine recurrence pass.
- Remove the redundant `member_count > 4` early return from the affine pass.
- Preserve the existing pressure threshold and the one-group budget for loops
  with more than four blocks.
- Rebuild and require the focused test to turn GREEN with exact oracle output.

## Task 3: Inspect generated code and compatibility

- Add a focused RED assertion showing that the new recurrence makes the shared
  masked branch selector spill in a telemetry-shaped loop.
- Add a bounded SSA rematerialization helper for immediate-mask `AND` values
  used by comparisons in remote blocks; clone immediately before the consumer
  and let the existing dead-code pass remove the long live range.
- Require the high-pressure fixture to keep the multiply reduction while
  eliminating local stack accesses.
- Compile telemetry with decision diagnostics and verify candidate discovery.
- Compare pre-change and post-change disassembly, confirming that the selected
  loop multiply becomes a recurrence update and that division/branch layout is
  unchanged.
- Run the existing induction suite plus gradual types, modules, float, infix,
  short-circuit, branch-return, standard-library, and C metadata/reference
  tests used by the current branch.
- Check Win/Linux mirror equality, whitespace, tracked artifacts, and status.

## Task 4: Performance acceptance

- Build an exact compiler baseline from the commit immediately before the
  production change.
- Run alternating correctness-checked short A/B gates on telemetry, matrix,
  and at least one unaffected benchmark.
- If the short gate is positive and no repeatable regression exceeds 3%, run
  1000 alternating pairs for telemetry and matrix and compare Mira with GCC
  `-O3` using the same inputs.
- Commit the production change and report only results supported by the fresh
  measurements; otherwise revert it and retain the diagnostic report.
