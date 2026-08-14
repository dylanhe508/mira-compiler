# Mira SSA Induction Strength Reduction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace proven integer `i * K + C` expressions in natural loops with loop-carried additions while preserving Mira's 64-bit behavior.

**Architecture:** Add one SSA optimization pass between scalar SSA convergence and LICM. It consumes existing dominance and `SsaLoopInfo` facts, creates a derived variable slot initialized in the unique preheader, rewrites candidate expressions to load that slot, and updates the slot on the unique latch. Every transformation is gated by the loop decision plan and rejected unless all CFG, definition, use, and update proofs succeed.

**Tech Stack:** C11, Mira SSA IR, Windows x64 backend, PowerShell regression runners, TDM-GCC 10.3 oracle.

## Global Constraints

- Enable only at `-O3`.
- Require `loop->decision_plan.allow_affine_recurrence`.
- Do not match source names, benchmark names, or particular constants.
- Preserve 64-bit two's-complement wrapping behavior.
- Do not increase the warmed median compile time of a ten-line program beyond 100 ms.
- Leave existing machine-level `ir_opt_coalesce_affine_recurrences` unchanged.

---

### Task 1: Add a structural SSA regression harness

**Files:**
- Create: `tests/decision/test_ssa_induction_strength.c`
- Modify: `codegen/ir_ssa.h`

**Interfaces:**
- Consumes: `SsaFunction`, `SsaLoopInfo`, `ssa_new_vreg`, SSA instruction and block types.
- Produces: `bool ssa_opt_induction_strength_reduce(SsaFunction *func)`.

- [ ] **Step 1: Write the failing structural tests**

Construct a four-block function (`entry -> header -> body -> latch -> header`) with:

```c
/* entry */
slot_i = 3;
/* body */
v_i = load_var(slot_i);
v_mul = v_i * 7;
consume(v_mul);
/* latch */
v_next = load_var(slot_i) + 2;
store_var(v_next, slot_i);
```

Assert that the new pass:

```c
assert(ssa_opt_induction_strength_reduce(func));
assert(count_loop_opcode(func, SSA_OP_MUL) == 0);
assert(count_latch_update_by(func, 14) == 1);
```

Add rejection cases where the loop has two back edges, the step is nonconstant,
the induction slot is written twice, and `allow_affine_recurrence` is false.
Each rejection must assert the function's instruction digest is unchanged.

- [ ] **Step 2: Build the test and verify RED**

Run:

```powershell
gcc -std=c11 -O0 -g tests\decision\test_ssa_induction_strength.c codegen\ssa_builder.c codegen\ssa_opt.c codegen\ssa_dom.c codegen\ssa_ref.c codegen\decision.c hash.c memory.c error.c -o tests\decision\test_ssa_induction_strength.exe
```

Expected: link failure for undefined
`ssa_opt_induction_strength_reduce`, proving the test reaches the missing pass.

- [ ] **Step 3: Declare the pass**

Add to `codegen/ir_ssa.h`:

```c
bool ssa_opt_induction_strength_reduce(SsaFunction *func);
```

- [ ] **Step 4: Rebuild and verify the test still fails behaviorally**

Provide a temporary implementation returning `false`; rebuild and run.
Expected: assertion failure at the positive transformation case, while rejection
fixtures construct successfully.

### Task 2: Implement proof-only candidate discovery

**Files:**
- Modify: `codegen/ssa_opt.c`
- Test: `tests/decision/test_ssa_induction_strength.c`

**Interfaces:**
- Consumes: populated `func->vreg_defs`, dominance data, `SsaLoopInfo`.
- Produces: internal `InductionCandidate` records containing loop, preheader,
  induction slot, step, multiplication instruction, factor, offset, and latch.

- [ ] **Step 1: Add candidate-discovery assertions**

Extend the structural test with positive cases for:

```text
i += 1;  i * 7
i += 3;  i * -11
i -= 2;  i * 13 + 5
```

Assert discovered deltas are `7`, `-33`, and `-26`, respectively.

- [ ] **Step 2: Run and verify RED**

Run the structural test. Expected: the three positive assertions fail because no
candidate is transformed.

- [ ] **Step 3: Implement bounded discovery**

In `codegen/ssa_opt.c`, add:

```c
typedef struct {
    SsaLoopInfo *loop;
    SsaBasicBlock *preheader;
    SsaBasicBlock *latch;
    SsaInst *mul;
    SsaInst *optional_add;
    int induction_slot;
    int64_t step;
    int64_t factor;
    int64_t offset;
} InductionCandidate;
```

Discovery must:

- require one back edge and one nonmember predecessor of the header;
- require `allow_affine_recurrence`;
- require exactly one canonical induction-slot store in the loop;
- accept `MUL(load_var(slot), imm)` and the commuted form;
- accept one directly dependent `ADD(mul, imm)` or `ADD(imm, mul)`;
- reject calls, ownership transfer, extra slot writes, and candidates outside the
  loop;
- cap candidates at 64 per function and scan each instruction a bounded number
  of times.

Use unsigned arithmetic when computing `step * factor` so C signed-overflow
undefined behavior cannot affect compiler correctness:

```c
int64_t delta = (int64_t)((uint64_t)step * (uint64_t)factor);
```

- [ ] **Step 4: Run structural tests**

Expected: rejection tests remain unchanged; positive tests reach the transform
stage but still fail until Task 3.

### Task 3: Materialize the derived recurrence safely

**Files:**
- Modify: `codegen/ssa_opt.c`
- Test: `tests/decision/test_ssa_induction_strength.c`

**Interfaces:**
- Consumes: one fully proven `InductionCandidate`.
- Produces: a new internal variable slot and SSA instructions in preheader,
  candidate block, and latch.

- [ ] **Step 1: Add exact shape assertions**

For the `i=3`, `step=2`, `factor=7` fixture, require:

```text
preheader: derived = 21
body:      candidate result = load derived
latch:     derived = derived + 14
```

Require the original induction update to remain present and the original loop
`MUL` to be absent.

- [ ] **Step 2: Run and verify RED**

Expected: failure because the pass has only discovered the candidate.

- [ ] **Step 3: Implement the rewrite**

Reuse the function's variable-slot representation:

1. Allocate one new slot after `func->var_count` only if that count is owned by
   `SsaFunction`; otherwise extend the appropriate module/program slot allocator
   before changing SSA.
2. Insert preheader instructions before its terminator to compute
   `initial * factor + offset` and store the derived slot.
3. Replace the candidate result with `LOAD_VAR derived_slot`, retaining the
   original result VReg so uses need no rewrite.
4. Insert before the latch terminator:

```text
old = LOAD_VAR derived_slot
delta = IMM(step * factor)
next = ADD old, delta
STORE_VAR next, derived_slot
```

5. Update `vreg_defs`, instruction links, operand counts, and the variable map
   capacity transactionally. Allocate all memory before mutating instructions;
   on allocation failure, return without changing the function.

- [ ] **Step 4: Run structural tests and verify GREEN**

Expected: all positive and rejection fixtures pass.

### Task 4: Integrate with the optimizer and preserve decision control

**Files:**
- Modify: `codegen/ssa_opt.c`
- Modify: `codegen/decision.c` only if the existing loop plan fails to carry
  `allow_affine_recurrence` after loop reanalysis.
- Test: `tests/decision/test_decision_model.c`

**Interfaces:**
- Consumes: the completed pass.
- Produces: one `-O3` pipeline invocation before LICM.

- [ ] **Step 1: Add integration assertions**

Extend the decision test to prove:

```c
assert(plan.allow_affine_recurrence);
decision_pipeline_disable(&plan, "affine");
assert(!plan.allow_affine_recurrence);
```

Add a compile/run check showing `MIRA_DECISION_DISABLE=affine` preserves output
and leaves loop multiplication present.

- [ ] **Step 2: Run and verify RED**

Expected: program output is correct, but the enabled build still contains the
hot-loop multiply because the pass is not invoked.

- [ ] **Step 3: Insert the pass**

In `ssa_optimize_function` after the first scalar fixed point and before LICM:

```c
if (mira_opt_level >= 3) {
    ssa_analyze_loops(func);
    if (ssa_opt_induction_strength_reduce(func)) {
        ssa_opt_copy_propagate(func);
        ssa_opt_dce(func);
        ssa_analyze_loops(func);
    }
}
```

The pass itself still checks the per-loop decision flag.

- [ ] **Step 4: Rebuild Mira**

Run the command in `BUILD.txt`. Expected: exit code 0 with no new warnings.

### Task 5: Add source-level semantic regressions

**Files:**
- Create: `tests/regression_induction_strength.mira`
- Create: `tests/regression_induction_strength.c`
- Create: `tests/run_induction_strength.ps1`

**Interfaces:**
- Consumes: rebuilt `mira.exe`, GCC oracle, Mira runtime objects.
- Produces: repeatable O0–O3 differential test.

- [ ] **Step 1: Write the source and oracle**

Cover:

- positive, negative, and nonunit step;
- positive and negative multiplier;
- `i*K+C`;
- values near `INT64_MAX` and `INT64_MIN` using unsigned-equivalent wrap in C;
- nested loops with independent induction variables.

Print one aggregate checksum so dead-code elimination cannot erase the work.

- [ ] **Step 2: Verify the unoptimized baseline**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File tests\run_induction_strength.ps1
```

Before enabling the pass, expected: O0–O3 outputs match the GCC oracle, and O3
disassembly still contains the candidate multiply.

- [ ] **Step 3: Verify optimized semantics and shape**

After integration, rerun. Expected: O0–O3 outputs match; O3 disassembly lacks
the selected loop multiply and contains an add by the proven delta.

### Task 6: Full verification and performance gate

**Files:**
- Modify: `tests/extreme_apps/RESULTS.md`

**Interfaces:**
- Consumes: all compiler changes and tests.
- Produces: evidence-backed correctness, compile-time, and run-time results.

- [ ] **Step 1: Run all correctness suites**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File tests\extreme_apps\run.ps1
powershell -ExecutionPolicy Bypass -File tests\extreme_apps\run.ps1 -Only allocator_churn
powershell -ExecutionPolicy Bypass -File tests\extreme_apps\run.ps1 -Only deep_control_flow
powershell -ExecutionPolicy Bypass -File tests\run_induction_strength.ps1
```

Also compile and run `regression_branch_phi.mira`,
`regression_deep_small.mira`, and `regression_signed_pow2_div.mira` at O0–O3.
Expected: every output matches its recorded oracle.

- [ ] **Step 2: Benchmark the long matrix**

Compile `tests/matrix_stencil_long.mira` at O3. Run two warmups and seven timed
samples. Expected:

- result `-7778248811425506175`;
- median below `64.3 ms`;
- hot inner loop no longer multiplies `c` by `19349663` on each iteration.

- [ ] **Step 3: Measure compile time**

Compile `bench_fib.mira` five times after two warmups and report the median.
Expected: under 100 ms.

- [ ] **Step 4: Update results**

Record exact samples, median, best time, correctness matrix, remaining GCC
ratio, and whether the strategy stayed enabled in
`tests/extreme_apps/RESULTS.md`. Do not claim completion without fresh command
output.

