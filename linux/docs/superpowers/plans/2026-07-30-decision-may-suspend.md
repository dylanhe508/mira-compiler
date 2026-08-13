# Decision may-suspend Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 由智策证明并选择不可挂起 go lambda 的无 Fiber 快路径。

**Architecture:** 静态引用生成 may_suspend 调用图摘要，独立改写阶段选择运行时入口，运行时用任务标志决定直接执行或 Fiber 执行。

**Tech Stack:** C11、Mira SSA、Static Reference、Decision 2.x、Windows Fiber。

## Global Constraints

- 未知调用必须视为可能挂起。
- 不根据函数名或基准内容特判用户代码。
- Channel、join、yield、sleep 任务必须保留 Fiber。

---

### Task 1: may_suspend 摘要

**Files:**
- Modify: `codegen/ir_ssa.h`
- Modify: `codegen/ssa_ref.c`
- Test: `tests/ssa_ref_suspend_test.c`

- [ ] 构造 pure、sleep、间接调用三类 SSA 函数并写失败断言。
- [ ] 增加 may_suspend 字段、运行时符号分类及调用图传播。
- [ ] 运行静态引用测试。

### Task 2: Fast runtime

**Files:**
- Modify: `runtime/rt_sched.c`
- Modify: `runtime/rt_sched.h`
- Test: `tests/runtime_sched_fast_test.c`

- [ ] 写 100000 fast task 正确性及 Fiber 创建数为 0 的失败测试。
- [ ] 增加 fast task 标志和直接 worker 执行路径。
- [ ] 验证普通 Channel/yield/join 回归。

### Task 3: 智策改写和链接

**Files:**
- Modify: `codegen/ssa_ref.c`
- Modify: `codegen/program.c`
- Modify: `main.c`
- Test: `tests/modern_go_fast.mira`
- Test: `tests/modern_go_blocking_bench.mira`

- [ ] 纯 lambda 的 IR 必须调用 `mira_go_start_fast0`。
- [ ] sleep lambda 的 IR 必须保留 `mira_go_start0`。
- [ ] 加入 extern 与 runtime 模块映射。
- [ ] 执行 O0-O3、并发全回归及吞吐复测。

