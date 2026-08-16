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

No runtime component, VM profiling format, static-reference analysis, machine
scheduler, or language feature is added.  Static Reference continues to supply
the existing safety facts; it is not placed in the runtime path.

## Register-pressure companion

The first measured prototype removed the multiply but made telemetry slower:
the new loop-carried value exhausted the nonvolatile pool, so the shared
`adjusted & 7` branch selector was written once and reloaded twice from the
stack.  Address this locally rather than weakening allocator safety.  When a
pure integer `AND` with an immediate mask is consumed only by comparisons and
a consumer is in a different block, clone that cheap expression immediately
before the remote comparison and retarget that operand.  Normal dead-code
elimination can then shorten or remove the original live range.

This is bounded rematerialization, not general code duplication: only one
side-effect-free instruction is cloned, only comparison operands qualify, and
no load, call, ownership value, division, or user-visible operation can move.

## Verification

- Add a general cross-branch induction fixture with multiple branch arms and a
  C oracle.  O0 through O3 must agree with the oracle.
- Before the production change, O3 debug output must show the branch-loop veto.
  Afterwards it must show a discovered affine candidate, and the hot loop must
  no longer contain the selected constant multiply.
- The high-pressure branch fixture must contain no local spill access after
  recurrence plus selector rematerialization.
- Run the existing induction-strength suite and the full correctness matrix.
- Compare an exact pre-change compiler and the candidate on telemetry and the
  matrix benchmark.  Use short alternating gates first, then 1000 alternating
  pairs for any claimed improvement.  Revert the production change if it is
  not correct or causes a repeatable regression outside the target workload.
- Mirror platform-neutral source and tests to Windows and Linux; native Linux
  execution is reported separately if the environment remains unavailable.
