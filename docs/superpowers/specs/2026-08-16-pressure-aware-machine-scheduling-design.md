# Pressure-Aware Machine Scheduling Design

## Goal

Improve Mira's O3 hot-loop instruction ordering without adding a large global
scheduling subsystem or changing language semantics.  The first target is the
remaining integer gap in the telemetry benchmark; matrix and the established
correctness suite are guardrails.

## Existing State

`ir_opt_ilp_schedule` already runs at O3 when the decision plan enables
scheduling.  It scans a 32-instruction window, notices an adjacent register
dependency, and moves the first earlier independent instruction into the gap.
It models GPR RAW/WAR/WAW and flags, but it has no latency model, no critical
path, no ready queue, and no register-pressure cost.  Scalar floating-point and
vector instructions are barriers because their register namespace is not yet
modeled safely.

## Chosen Approach

Replace the local move heuristic in place with a conservative basic-block list
scheduler.  Do not add cross-block motion, software pipelining, speculative
loads, or a target-specific CPU database in this stage.

For each straight-line schedulable region:

1. Describe every instruction's GPR reads, GPR writes, flag reads, flag writes,
   and conservative latency.
2. Build forward dependency edges for register RAW/WAR/WAW and flag
   RAW/WAR/WAW hazards.
3. Compute the remaining critical-path length from every node.
4. Select from ready instructions deterministically.  Prefer greater critical
   path, then prefer instructions that consume an already-live value or avoid
   creating a new live value, then retain source order as the final tie-break.
5. Reject a candidate ordering if its estimated peak GPR pressure exceeds the
   original region's peak or the configured safe cap.

Calls, branches, labels, stack operations, memory writes, relocation-bearing
address loads, division/wide multiply, and unmodeled scalar-FP/vector operations
remain region boundaries.  Ordinary register-only operations, address
calculation, and read-only loads may be reordered when dependencies allow it.

## Latency Model

Use a deliberately small generic x86-64 model rather than tuning for one CPU:

- register/immediate move and simple ALU: 1
- address calculation and shifts: 1
- read-only load: 4
- integer multiply: 3

The model is only a priority signal.  Correctness comes from dependencies and
barriers, and pressure protection can veto the resulting order.

## Integration

Keep the existing `ir_opt_ilp_schedule` entry point and O3 decision gate, so the
change does not add a new pipeline stage.  Implement the same platform-neutral
logic in `win/codegen/ir_opt.c` and `linux/codegen/ir_opt.c`; target ABI-specific
lowering remains untouched.

## Tests and Acceptance

Add a focused C-level scheduler test that constructs machine IR directly:

- an independent multiply/load can be advanced to cover a dependency chain;
- RAW, WAR, WAW, flags, memory-write, call, and branch barriers are preserved;
- output is deterministic;
- estimated peak pressure does not increase.

TDD requires the scheduling-shape case to fail against the existing heuristic
before production code changes.  Then run clean builds and the full gradual,
module, float, infix, short-circuit, branch-return, standard-library, and
machine-IR regression suites.

Performance acceptance uses repeated batches rather than one timing:

- telemetry is the primary expected win;
- matrix, deep integer, register-pressure, and float cases are guardrails;
- reject the patch if any unaffected guardrail regresses by more than 3%;
- only after passing the short gate, run the affected comparison for 1000
  rounds and record binary size/hash and medians.

If the generic scheduler produces no repeatable gain, keep the tests and revert
the production change rather than expanding immediately into cross-block or
microarchitecture-specific scheduling.
