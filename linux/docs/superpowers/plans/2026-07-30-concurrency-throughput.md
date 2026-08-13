# Mira Concurrency Throughput Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 降低 Mira 大量极小 detached task 的创建、提交和回收成本。

**Architecture:** 将 detached 与 joinable task 的创建路径分离，detached 路径不创建 JoinHandle/Event；随后为 detached task 增加受调度器锁保护的对象池。保留 Fiber 以维持 Channel/park 语义。

**Tech Stack:** C11、Windows Fiber、SRWLOCK、Condition Variable、Mira runtime。

## Global Constraints

- 不改变 join、Channel、yield、park 和 wait-all 语义。
- 不为跑分加入任务内容特判。
- 每次改动先失败测试，后实现，再运行完整并发回归。

---

### Task 1: Detached task 不创建 JoinHandle

**Files:**
- Modify: `runtime/rt_sched.c`
- Modify: `runtime/rt_sched.h`
- Test: `tests/runtime_sched_detached_test.c`

**Interfaces:**
- Consumes: `mira_go_start(MiraTaskFn, void *)`
- Produces: detached task 的零 Event 提交路径；测试查询接口 `mira_sched_join_handle_creations()`

- [ ] 添加测试：提交 10000 个 `mira_go_start`，断言执行数为 10000 且 JoinHandle 创建数为 0。
- [ ] 运行测试，确认当前实现返回 10000 个 JoinHandle 创建。
- [ ] 将公共入队逻辑抽为接收可空 handle 的内部函数；`mira_go_start` 直接创建 task，`mira_go_start_handle` 才创建 handle。
- [ ] 运行 detached、join、yield、Channel、select 测试。
- [ ] 重跑 100000 任务吞吐基准并记录中位数。

### Task 2: Detached task 对象池

**Files:**
- Modify: `runtime/rt_sched.c`
- Test: `tests/runtime_sched_pool_test.c`

**Interfaces:**
- Consumes: detached task 完成路径和调度器锁
- Produces: bounded free-task 链表，关闭时释放

- [ ] 添加测试：分十批提交 1000 个任务，断言任务分配次数显著低于 10000。
- [ ] 运行测试确认当前每任务一次分配。
- [ ] 在 scheduler 中增加 free task 链表和计数，提交时复用，完成时回收，shutdown 时释放。
- [ ] 运行完整并发回归和吞吐基准。

### Task 3: 性能与功能验收

**Files:**
- Test: `tests/concurrency_throughput_compare.c`
- Test: `tests/modern_go_channel.mira`
- Test: `tests/modern_go_blocking_bench.mira`

**Interfaces:**
- Consumes: Tasks 1-2 的调度器
- Produces: 可复现的吞吐、阻塞延迟和正确性数据

- [ ] 连续运行五次 100000 任务基准，报告 Mira 与 Windows Thread Pool 中位数。
- [ ] 运行 O0-O3 `modern_go_channel.mira`，每次必须输出 42。
- [ ] 运行全部 scheduler/channel/select/静态引用测试。
- [ ] 确认两个 120 ms 任务仍在合理调度抖动范围内完成。

