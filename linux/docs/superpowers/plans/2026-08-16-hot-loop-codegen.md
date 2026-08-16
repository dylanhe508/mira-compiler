# Hot-loop Code Generation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove three general hot-loop code-generation costs found by the 1000-round benchmark while preserving Mira semantics and compatibility.

**Architecture:** Add narrowly proved SSA/lowering improvements rather than benchmark-specific rewrites. Each optimization has an assembly-shape test, exact runtime golden, focused performance gate, full regression, and an independent commit so it can be reverted without affecting the others.

**Tech Stack:** C11 compiler implementation, Mira fixtures, PowerShell regression runner, x86-64 Win64/SysV lowering.

## Global Constraints

- Do not change Mira source syntax, runtime ABI, object format, or arithmetic semantics.
- Do not match function, file, or benchmark names in production code.
- Mirror platform-neutral production and fixture changes between `win/` and `linux/`.
- Preserve conservative behavior for unknown and legacy SSA values.
- Reject an optimization if an unaffected benchmark regresses repeatedly by more than 3%.

---

### Task 1: Constant Shift Immediate Recovery

**Files:**
- Modify: `win/codegen/ssa_lower.c`, `linux/codegen/ssa_lower.c`
- Create: `win/tests/types/shift_immediate_codegen.mira`, Linux mirror
- Create: `win/tests/run_hot_loop_codegen.ps1`, Linux mirror

**Interfaces:**
- Consumes: `SsaFunction.vreg_defs`, `SsaInst.op2`, `SSA_OP_IMM`.
- Produces: `static bool resolve_shift_immediate(const SsaFunction *, SsaOperand, int *)` and immediate x86 shift instructions when the count is provably constant.

- [ ] Add a fixture containing `value >> 7`, `value >> dynamic`, and `value << 3`; make the runner require exact output and require immediate shifts without `push rcx` for the constant sites while retaining the CL path for the dynamic site.
- [ ] Run the runner against current HEAD and record RED because all three counts arrive as VRegs and constant sites use the RCX save/restore path.
- [ ] Implement `resolve_shift_immediate`: accept `SSA_OPND_IMM`, or a VReg whose unique definition is `SSA_OP_IMM` with an integer operand; mask the result to 0..63. Do not chase COPY or PHI nodes.
- [ ] Use the helper in `SSA_OP_SHL`, `SSA_OP_ASHR`, and `SSA_OP_LSHR`; leave the existing CL lowering unchanged when it returns false.
- [ ] Rebuild, run the structural/runtime test, gradual expressions/SSA, telemetry golden, and a 31-round telemetry comparison.
- [ ] Commit as `perf(codegen): lower proven constant shifts directly` only if output is exact and telemetry improves.

---

### Task 2: Float Loop Register-Class Preservation

**Files:**
- Modify: `win/codegen/ssa_mem2reg.c`, Linux mirror
- Modify: `win/codegen/ssa_regalloc.c`, Linux mirror
- Modify only if required by the proved representation: `win/codegen/ssa_lower.c`, Linux mirror
- Create: `win/tests/types/float_loop_registers.mira`, Linux mirror
- Modify: both `run_hot_loop_codegen.ps1` runners

**Interfaces:**
- Consumes: `SsaInst.type`, PHI-to-COPY metadata, and `LiveInterval.is_float`.
- Produces: float loop-carried values and PHI copies whose intervals remain in the XMM register class.

- [ ] Add the variable-division recurrence fixture. Require exact numeric output and inspect `-S` output for the current per-iteration GPR↔XMM round trips.
- [ ] Run the focused runner and record RED on the assembly-shape assertion.
- [ ] Trace the first point at which a float PHI/COPY becomes an integer interval. Preserve `SSA_TYPE_FLOAT` and `is_float` at that point; do not infer floatness from opcode or source spelling.
- [ ] Ensure parallel-copy temporaries created during PHI destruction use the PHI type and that interval reconstruction observes the defining COPY type.
- [ ] Rebuild and require the recurrence to keep loop-carried values in XMM registers, while all typed SSA, float ABI, PHI ownership, and O0/O3 goldens pass.
- [ ] Run 31 rounds of variable division plus float multiply-add and constant division. Commit as `perf(codegen): keep loop-carried floats in xmm registers` only if the target improves and the other two do not regress above 3%.

---

### Task 3: Safe Nested-Loop Invariant Arithmetic

**Files:**
- Modify: `win/codegen/ssa_opt.c`, `linux/codegen/ssa_opt.c`
- Create: `win/tests/types/nested_licm_codegen.mira`, Linux mirror
- Modify: both `run_hot_loop_codegen.ps1` runners

**Interfaces:**
- Consumes: `SsaLoopInfo.members`, loop preheaders, dominator information, `ssa_licm_pure_integer_op`, and `SsaFunction.vreg_defs`.
- Produces: a must-execute nested-loop LICM candidate set containing only pure integer operations with operands available in the preheader.

- [ ] Add a nested-loop fixture with a row-invariant multiply in the unconditional inner-loop header plus a branch-only expression that must not be hoisted. Assert exact output and assembly placement.
- [ ] Run the test and record RED because the row-invariant multiply remains in the inner loop.
- [ ] Build a stable-value bitmap from IMM, immutable parameters, and definitions outside the current loop that dominate its preheader. Admit derived values only through `ssa_licm_pure_integer_op` with stable operands.
- [ ] Hoist candidates only from the loop header, only when the header dominates every backedge and exit-reaching iteration path, and only when all uses remain dominated. Never hoist calls, memory operations, division, ownership-producing instructions, or branch-only nodes.
- [ ] Rebuild and require fewer row-invariant multiplies inside the matrix inner loop, exact matrix output, and the branch-only negative assertion.
- [ ] Run 31 rounds of matrix, telemetry, deep-control-flow, and pressure-spill. Commit as `perf(ssa): hoist proved nested-loop invariants` only if matrix improves without a repeatable >3% unaffected regression.

---

### Task 4: Final Evidence

**Files:**
- Modify: `win/tests/type_enhancement_report.md`, Linux mirror
- Create: `.superpowers/sdd/hot-loop-codegen-report.md`

**Interfaces:**
- Consumes: the three independent optimization commits and the 1000-round baseline.
- Produces: exact correctness, binary-size, compilation, platform-mirror, and performance evidence.

- [ ] Run `mingw32-make -C win clean all`, gradual `all`, modules, float, infix, ownership, short-circuit, branch-return, stdlib core/data, builtin table, and SSA reference tests.
- [ ] Run changed Linux translation units with `-fsyntax-only`, verify normalized Win/Linux mirrors, `git diff --check`, no tracked generated artifacts, and a clean worktree.
- [ ] Rerun each affected benchmark for 1000 rounds with alternating current-Mira/GCC execution order and exact output comparison.
- [ ] Record before/after medians, P5/P95, sizes, hashes, rejected stages, and limitations in byte-identical tracked reports.
- [ ] Commit as `docs(perf): record hot-loop optimization evidence`.
