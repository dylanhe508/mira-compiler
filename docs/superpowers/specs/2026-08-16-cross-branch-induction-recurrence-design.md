# Cross-Branch Induction Recurrence Design

## Problem

Mira already turns a canonical loop product such as `i * 17` into a derived
addition recurrence.  The legality proof and rewrite operate over every block
in a natural loop, but the decision layer disables the pass for every branchy
loop and the pass repeats an older `member_count > 4` rejection.  Telemetry's
eight-block loop is therefore rejected before candidate discovery, leaving two
integer multiplies in paths where GCC uses additions.

## Design

Keep the existing SSA transform and remove only the two blanket branch-shape
vetoes.  A candidate remains legal only when the current proof finds one
natural-loop header, one backedge, one outside preheader, one initial store,
one in-loop induction update, a non-zero constant step, and an integer multiply
of that induction value by a constant.  Calls, ownership transfers, unsafe
memory reordering, nested loops, ambiguous stores, and disabled affine policy
remain rejected.

The existing pressure budget remains authoritative.  A loop with more than
four blocks may receive at most one derived recurrence, selected by the number
of eliminated multiplications.  The recurrence is initialized in the proven
preheader and updated next to the proven induction store, so branches neither
speculate user work nor change side-effect order.  Integer arithmetic retains
Mira's modulo-2^64 behavior.

No runtime component, machine scheduler, or language feature is added.  Static
Reference continues to run only at compile time; its branch evidence receives
the bounded history described below.

## Profitability and branch policy

Measurement showed that replacing one multiply per iteration with one derived
addition creates a loop-carried dependency and is slower on the target x86-64
CPU even without spills.  Branch-heavy loops therefore require at least two
static uses of the same induction product and one use that dominates the
proved latch update.  This rejects two mutually exclusive arm-local products,
which still execute only once per iteration.  Branch-heavy loops nested inside
another loop retain the established nested-loop optimization path.  The
one-group pressure budget still applies.  Telemetry's two different one-use
factors are rejected and its optimized binary remains unchanged.

A profitable repeated-product case exposed an independent interaction: the
machine if-converter evaluated both arithmetic arms and selected with `cmov`,
doubling the accumulator dependency chain for a perfectly periodic branch.
The existing bounded Static Reference VM now retains the last 64 outcomes and
recognizes exact periods from one through eight after at least 32 samples.
Periodic evidence requests a real branch even when the aggregate taken ratio
is 50/50.  This metadata is compile-time only and adds no runtime state.

## Verification

- Add a general cross-branch induction fixture with multiple branch arms and a
  C oracle.  O0 through O3 must agree with the oracle.
- Before the production change, O3 debug output must show the branch-loop veto.
  Afterwards it must show a discovered affine candidate, and the hot loop must
  no longer contain the selected constant multiply.
- A one-use telemetry-shaped branch loop must remain identical with affine
  optimization enabled or disabled and contain no local spill access.
- A repeated-product branch loop must remove its multiplies and preserve a
  Static-Reference-proven short-period branch instead of emitting `cmov`.
- Run the existing induction-strength suite and the full correctness matrix.
- Compare an exact pre-change compiler and the candidate on telemetry and the
  matrix benchmark.  Use short alternating gates first, then 1000 alternating
  pairs for any claimed improvement.  Revert the production change if it is
  not correct or causes a repeatable regression outside the target workload.
- Mirror platform-neutral source and tests to Windows and Linux; native Linux
  execution is reported separately if the environment remains unavailable.
