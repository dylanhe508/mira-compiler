# Checker-Guided Static Ownership Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move ownership and escape classification into Mira's type checker and make SSA consume those facts instead of rediscovering them from PHI/COPY producer graphs.

**Architecture:** Add a small `MiraOwnership` lattice to checked values, IR nodes, variable flow state, and function summaries. The checker annotates value-producing nodes and converges summaries; SSA copies those annotations into explicit ownership metadata. Register allocation still computes last use and inserts frees, but stops deciding provenance.

**Tech Stack:** C11, Mira postfix IR, Mira SSA, PowerShell regression runners, MinGW GCC.

## Global Constraints

- No new syntax, ownership keyword, runtime API, reference counting, GC, or borrow checker.
- Preserve ABI, legacy conservative behavior, and generated-program behavior.
- Existing builtin ownership/free contracts remain authoritative.
- Win/Linux platform-neutral changes match after newline normalization.
- Every production change follows observed RED → GREEN.
- Never claim native Linux execution while WSL is unavailable.

---

### Task 1: Ownership Lattice and Checker Facts

**Files:**
- Modify: `win/typecheck.h`, `linux/typecheck.h`
- Modify: `win/mira.h`, `linux/mira.h`
- Modify: `win/typecheck.c`, `linux/typecheck.c`
- Modify: `win/memory.c`, `linux/memory.c`
- Create: `win/tests/ownership_metadata_test.c`, Linux mirror
- Modify: `win/tests/run_gradual_types.ps1`, Linux mirror

**Interfaces:**
- Produces `MiraOwnership`: `UNKNOWN`, `BORROWED`, `OWNED`, `MAYBE_OWNED`, `ESCAPED`.
- Produces `IrNode.checked_ownership`, `checked_free_func_name`, and `ownership_checked`.
- Produces `Def.return_ownership`, `return_free_func_name`, `ownership_checked`, and `param_may_escape[param_count]`.
- Extends `MiraCheckedValue` with ownership, free function, and parameter-alias provenance.

- [ ] **Step 1: Write the failing metadata probe**

Parse real Mira source containing a string literal, allocating builtin, mixed `if`, owned return, and non-escaping parameter. Assert the wished-for API:

```c
static void expect_ownership(const IrNode *node, MiraOwnership expected) {
    if (!node || !node->ownership_checked ||
        node->checked_ownership != expected) exit(1);
}
expect_ownership(literal, MIRA_OWNERSHIP_BORROWED);
expect_ownership(allocation_call, MIRA_OWNERSHIP_OWNED);
expect_ownership(mixed_if, MIRA_OWNERSHIP_MAYBE_OWNED);
if (make_value->return_ownership != MIRA_OWNERSHIP_OWNED) exit(1);
if (identity->param_may_escape[0]) exit(1);
puts("OWNERSHIP METADATA PASS");
```

- [ ] **Step 2: Verify RED**

Run from `win/`:

```powershell
powershell -ExecutionPolicy Bypass -File .\tests\run_gradual_types.ps1 -Group ownership
```

Expected: C compilation fails because the ownership enum/fields do not exist.

- [ ] **Step 3: Add the exact ownership enum**

```c
typedef enum MiraOwnership {
    MIRA_OWNERSHIP_UNKNOWN = 0,
    MIRA_OWNERSHIP_BORROWED,
    MIRA_OWNERSHIP_OWNED,
    MIRA_OWNERSHIP_MAYBE_OWNED,
    MIRA_OWNERSHIP_ESCAPED
} MiraOwnership;
```

Add the interface fields above. Allocate `param_may_escape` with parameter metadata and release it in `program_free`.

- [ ] **Step 4: Propagate ownership through checked values**

Extend `MiraCheckedValue`:

```c
MiraOwnership ownership;
const char *free_func_name;
int owner_param; /* -1 unless this aliases a current parameter */
const IrNode *ownership_origin;
```

Use this pure merge rule:

```c
static MiraOwnership checker_merge_ownership(MiraOwnership a,
                                               MiraOwnership b) {
    if (a == b) return a;
    if (a == MIRA_OWNERSHIP_ESCAPED && b == MIRA_OWNERSHIP_ESCAPED)
        return MIRA_OWNERSHIP_ESCAPED;
    if (a == MIRA_OWNERSHIP_UNKNOWN || b == MIRA_OWNERSHIP_UNKNOWN)
        return MIRA_OWNERSHIP_UNKNOWN;
    return MIRA_OWNERSHIP_MAYBE_OWNED;
}
```

Classify scalars and string literals as borrowed; classify builtin calls from `StdlibBuiltin.owned_result`. Merge ownership alongside types in `if`, `switch`, and `try/catch`, then annotate the producing IR node.

- [ ] **Step 5: Compute conservative function summaries**

Track parameter aliases by slot. Mark a parameter escaping when returned, stored, placed in memory/container storage, or passed to an unknown/may-escape call. Merge explicit returns and tail-result ownership. Iterate summaries until no return/parameter fact changes; unknown and legacy calls remain conservative.

- [ ] **Step 6: Verify GREEN and compatibility**

Expected ownership-group tail:

```text
OWNERSHIP METADATA PASS
CHECKER OWNERSHIP PASS
```

Then run gradual groups `declarations`, `calls`, and `expressions`; all must pass with unchanged diagnostics.

- [ ] **Step 7: Mirror, check, commit**

Verify normalized Win/Linux hashes and `git diff --check`. Commit:

```text
feat(ownership): compute checker ownership facts
```

---

### Task 2: SSA Consumption and Explicit Owner Tokens

**Files:**
- Modify: `win/codegen/ir_ssa.h`, Linux mirror
- Modify: `win/codegen/ssa_builder.c`, Linux mirror
- Modify: `win/codegen/ssa_mem2reg.c`, Linux mirror
- Modify: `win/codegen/ssa_regalloc.c`, Linux mirror
- Create: `win/tests/ownership_ssa_metadata_test.c`, Linux mirror
- Create: `win/tests/types/checker_ownership_flow_valid.mira`, Linux mirror
- Modify: both gradual-type runners

**Interfaces:**
- Adds `SsaOwnership ownership` and `VReg owner_token` to `SsaInst`.
- Consumes checked IR ownership in the SSA builder.
- Removes recursive PHI/COPY ownership-provenance discovery.
- Retains live-interval last-use and free-call insertion.

- [ ] **Step 1: Write failing SSA metadata and execution tests**

The C probe asserts:

```c
if (owned_call->ownership != SSA_OWNERSHIP_OWNED) exit(1);
if (mixed_phi->ownership != SSA_OWNERSHIP_MAYBE_OWNED) exit(1);
if (!mixed_phi->owner_token) exit(1);
puts("OWNERSHIP SSA METADATA PASS");
```

The Mira fixture covers nested PHI→COPY→PHI, owned/borrowed returns, non-escaping and escaping calls, plus storage. Exact O0/O3 output:

```text
owned
borrowed
nested
returned
stored
```

- [ ] **Step 2: Verify RED**

Run the ownership group. Expected: the SSA C probe fails because explicit SSA ownership fields do not exist. Execution output alone is not acceptable RED evidence.

- [ ] **Step 3: Add explicit SSA ownership**

```c
typedef enum SsaOwnership {
    SSA_OWNERSHIP_UNKNOWN = 0,
    SSA_OWNERSHIP_BORROWED,
    SSA_OWNERSHIP_OWNED,
    SSA_OWNERSHIP_MAYBE_OWNED,
    SSA_OWNERSHIP_ESCAPED
} SsaOwnership;
```

Add `SsaOwnership ownership; VReg owner_token;` to `SsaInst`. Keep `needs_free` and `free_func_name` during migration for pass compatibility.

- [ ] **Step 4: Lower checker facts**

For value-producing instructions:

```c
inst->ownership = ssa_ownership_from_mira(node->checked_ownership);
inst->free_func_name = node->checked_free_func_name;
inst->needs_free = inst->ownership == SSA_OWNERSHIP_OWNED;
```

PHIs use the checker-merged fact. A `MAYBE_OWNED` PHI creates an owner token: owned incoming edges copy the pointer, borrowed edges copy zero.

- [ ] **Step 5: Simplify PHI destruction**

Delete recursive producer-graph ownership computation. Preserve `ownership`, free function, and owner token when PHIs become edge COPYs and across COPY aliases.

- [ ] **Step 6: Consume summaries in register allocation**

Populate intervals from explicit SSA ownership. For known user calls, escape only parameters marked by the checker summary. Preserve conservative escape for unknown/legacy calls. Keep last-use/free placement unchanged.

- [ ] **Step 7: Verify focused GREEN**

Expected ownership-group output includes:

```text
OWNERSHIP METADATA PASS
OWNERSHIP SSA METADATA PASS
CHECKER OWNERSHIP O0 PASS
CHECKER OWNERSHIP O3 PASS
CHECKER OWNERSHIP PASS
```

Run the existing SSA group and standalone SSA-reference suspend/concurrency tests; all existing owned/mixed PHI outputs must remain exact.

- [ ] **Step 8: Mirror, check, commit**

Verify normalized mirrors and `git diff --check`. Commit:

```text
refactor(ownership): consume checker facts in SSA
```

---

### Task 3: Compatibility and Final Evidence

**Files:**
- Modify: `win/tests/type_enhancement_report.md`, Linux mirror
- Create: `.superpowers/sdd/checker-ownership-report.md` (ignored local report)

- [ ] **Step 1: Fresh build**

From `win/` run `mingw32-make clean`, then `mingw32-make all`. Both must exit 0.

- [ ] **Step 2: Full regression**

Run gradual `all`, modules, float, and infix runners. Compile/run short-circuit, branch-return, stdlib-core, and stdlib-data at O0–O3 with exact golden output. Build/run builtin-table, SSA-reference, type metadata, program-free, lexer-state, and both ownership probes.

- [ ] **Step 3: Performance/output guard**

Rebuild recorded modern/int/float O3 fixtures and compare size/SHA with commit `3c523ff`. Run the typed 50-compile batch; require no material regression beyond noise. Any machine-code delta needs an ownership-specific explanation.

- [ ] **Step 4: Linux and hygiene checks**

Verify every changed platform-neutral pair after newline normalization. Compile changed Linux translation units with `-fsyntax-only`. If WSL still returns `E_ACCESSDENIED`, record native Linux as pending. Run `git diff --check`, require a clean worktree, and ensure no `.exe/.obj/.o/.ir` is tracked.

- [ ] **Step 5: Report and commit**

Record RED/GREEN evidence, ownership data flow, exact commands/results, output guard, mirror hashes, and limitations. Keep tracked Win/Linux reports byte-identical. Commit:

```text
docs(ownership): record checker-guided verification
```

