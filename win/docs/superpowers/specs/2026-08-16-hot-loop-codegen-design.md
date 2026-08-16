# Hot-loop code generation design

## Goal

Improve the three general code-generation weaknesses exposed by the 1000-round
Mira/GCC suite without changing language semantics, runtime behavior, or binary
format: constant shift lowering, floating loop register class preservation, and
nested-loop invariant arithmetic.

## Evidence

- `telemetry_pipeline` emits register-count shifts with `push rcx`/`pop rcx`
  even for source constants `7` and `3`.
- `float_division_variable` moves both loop-carried doubles from GPRs into XMM
  registers and back on every iteration.
- `matrix_stencil_long` repeats row-invariant multiplications inside its inner
  column loop and recomputes adjacent affine products.

All three programs match the GCC oracle. Therefore output equality is a hard
gate for every optimization.

## Approach

Implement three independent, general transformations in increasing risk order.

1. Lower integer shifts with a compile-time constant count directly to the
   immediate-count machine form. Variable shifts retain the existing RCX path.
2. Preserve `SSA_TYPE_FLOAT` for loop-carried PHI/COPY values through register
   allocation so their physical class remains XMM. Integer and pointer values
   retain existing allocation behavior.
3. Extend existing SSA loop optimization to hoist side-effect-free arithmetic
   whose operands are defined outside the current loop. Reuse existing value
   numbering for identical expressions; do not add benchmark-name patterns or
   floating reassociation.

Each stage gets a structural IR/assembly regression and a runtime golden before
production changes. Stages are committed independently and retained only when
the target benchmark improves without meaningful regression elsewhere.

## Safety constraints

- Preserve signed overflow and signed division semantics already implemented by
  Mira; do not introduce algebraic reassociation that changes results.
- Never hoist loads, stores, calls, division, or trapping/side-effecting nodes.
- Keep unknown/legacy SSA types on their existing conservative path.
- Mirror platform-neutral changes between Windows and Linux.
- Require the complete gradual-type, module, float, infix, short-circuit,
  branch-return, and ownership regressions before final acceptance.

## Performance acceptance

Use the current 1000-round report as baseline. For each stage, run a shorter
31-round median gate during development and rerun the affected case for 1000
rounds before final reporting. Reject any stage that produces incorrect output,
increases representative O3 binary size materially without justification, or
causes a repeatable regression above 3% in an unaffected benchmark.
