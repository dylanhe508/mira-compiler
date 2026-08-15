# Checker-Guided Static Ownership Design

## Goal

Let Mira's existing type checker compute ownership and escape facts that the SSA pipeline can consume directly. Preserve the language syntax, ABI, generated-program behavior, and the existing last-use auto-free mechanism.

## Scope

This change adds no ownership keywords, reference counting, garbage collector, borrow checker, or new runtime API. It does not move register allocation or last-use calculation into the checker. It only replaces backend ownership inference that duplicates information already available while checking structured control flow.

## Ownership Model

Every checked value carries one of four facts:

- `borrowed`: the value does not own storage and must not be freed.
- `owned`: the value owns storage and must be released unless ownership escapes.
- `maybe-owned`: different live control-flow paths produce owned and borrowed values.
- `escaped`: ownership has transferred through a return, consuming call, variable/container store, or equivalent sink.

Builtin result ownership remains authoritative in the existing standard-library builtin table. Literal strings and externally borrowed pointers are borrowed. Values from allocating builtins are owned. Control-flow joins merge these facts without inspecting generated PHI/COPY shapes.

## Data Flow

The type checker records ownership beside existing checked type facts on IR nodes and function summaries. A function summary records whether its return can own storage and which parameters can escape. Recursive and forward calls begin conservatively and converge through the existing program-wide checking pass; unknown legacy calls retain the current conservative behavior.

`if`, `switch`, and `try/catch` merge ownership across live branches:

- borrowed + borrowed = borrowed
- owned + owned = owned
- owned + borrowed = maybe-owned
- escaped on every live path = escaped

The SSA builder copies checked ownership onto value-producing instructions and PHIs. A `maybe-owned` PHI deliberately creates the existing conditional owner token. `ssa_mem2reg` preserves the metadata through PHI destruction and COPY aliases. Register allocation consumes the metadata, extends the owner token to the final use, and inserts the existing free call. It no longer determines ownership by recursively rediscovering PHI provenance.

## Function Calls and Escape

Known builtin contracts keep their current allocation/free metadata. For typed user functions, the checker summarizes return ownership and parameter escape. Passing an owned value to a parameter that may escape transfers responsibility; passing it to a proven non-escaping parameter keeps ownership with the caller. Unknown or legacy calls remain conservative so old programs cannot acquire premature frees.

Returns always transfer an owned result to the caller. Stores that outlive the current expression transfer ownership. Plain local copies alias the same owner rather than duplicating ownership.

## Compatibility and Failure Handling

Ownership facts are internal metadata and cannot produce new source-level diagnostics in this phase. If ownership is unknown, the compiler uses the existing conservative escape behavior. The checker must never make an existing valid legacy program less safe merely to insert an earlier free.

Windows and Linux platform-neutral files must remain byte-equivalent after newline normalization. Platform ABI-specific lowering stays unchanged.

## Testing

Tests are written before production changes and must demonstrate the missing metadata path rather than merely repeat current output tests.

Focused coverage includes:

- owned, borrowed, and mixed joins through `if`, `switch`, and `try/catch`;
- nested PHI-to-COPY-to-PHI aliases;
- owned returns and borrowed returns;
- escaping and non-escaping typed parameters;
- variable and container stores;
- unknown legacy calls retaining conservative behavior;
- O0 and O3 execution with exact output;
- an internal metadata probe proving the checker produced the ownership fact and SSA consumed it.

The final gate includes a clean Windows build, gradual types, modules, float, infix, standard-library regressions, standalone ownership/reference tests, Linux translation-unit syntax checks, mirror checks, `git diff --check`, and a clean worktree. Native Linux execution remains explicitly pending if WSL is unavailable.

## Success Criteria

The backend no longer infers original ownership by recursively walking PHI/COPY producers. Existing owner-token behavior is driven by checker facts, all focused and compatibility tests pass, generated O3 output for unaffected fixtures remains unchanged, and no new runtime dependency or user-visible syntax is introduced.
