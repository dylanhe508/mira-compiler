# Mira Nonvolatile Register Colors Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Keep values live across calls in Win64 callee-saved registers while preserving ABI, stack alignment, spills and all O0–O3 semantics.

**Architecture:** Extend the existing linear-scan allocator from five volatile scalar colors to twelve total colors. Add exact color-to-register lowering and save/restore only the nonvolatile colors actually used by SSA or fIR.

**Tech Stack:** C11, Mira SSA IR, Windows x64 ABI, PowerShell differential tests.

## Global Constraints

- Do not enable global graph coloring in this phase.
- Colors 5–11 map to R13, R14, R15, RBX, RDI, RSI, R12.
- R10 and R11 remain dedicated lowering/spill temporaries.
- Values crossing calls may use only colors 5–11.
- Preserve Windows x64 shadow space and 16-byte call alignment.
- Keep warmed ten-line compile median below 100 ms.
- Keep long-matrix runtime within 5% of an interleaved same-environment baseline.

---

### Task 1: Extend scalar colors and cross-call allocation

**Files:**
- Modify: `codegen/ssa_regalloc.c`
- Modify: `codegen/ssa_lower.c`
- Test: `tests/decision/test_nonvolatile_regalloc.c`

- [ ] Add structural red tests for color mapping, cross-call allocation and call-free leaf allocation.
- [ ] Confirm cross-call values currently spill or receive a volatile color.
- [ ] Add colors 5–11 and exact IrReg mapping while keeping R10/R11 reserved.
- [ ] Restrict cross-call intervals to colors 5–11 and preserve RAX/RDX division exclusions.
- [ ] Run structural tests and existing allocator tests.

### Task 2: Preserve used nonvolatile registers and stack alignment

**Files:**
- Modify: `codegen/ssa_lower.c`
- Test: `tests/decision/test_nonvolatile_lowering.c`

- [ ] Add red tests for one, odd and even counts of used nonvolatile registers.
- [ ] Compute the union of SSA-colored and fIR-used nonvolatile registers.
- [ ] Save in prologue and restore in reverse order before every RET.
- [ ] Include save count in spill offsets and call-stack alignment.
- [ ] Run lowering structural tests and inspect generated assembly.

### Task 3: Semantic and performance verification

**Files:**
- Create: `tests/regression_nonvolatile_calls.mira`
- Create: `tests/regression_nonvolatile_calls.c`
- Create: `tests/run_nonvolatile_calls.ps1`
- Modify: `tests/extreme_apps/RESULTS.md`

- [ ] Add C/Mira differential cases for nested calls, recursion, high pressure and 4,000,000 calls.
- [ ] Run O0–O3 differential tests and require exact equality.
- [ ] Rebuild and run existing focused/PHI/divrem regressions.
- [ ] Run long matrix with interleaved old/new binaries; require <=5% regression.
- [ ] Measure warmed `bench_fib.mira -O3` compile median; require <100 ms.
- [ ] Record exact outputs, samples, Device Guard blocks and review result.

