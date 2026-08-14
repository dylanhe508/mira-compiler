# Mira Go 式并发实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现多核轻量任务、Channel、select，并用静态引用驱动并发合法性与零拷贝优化。

**Architecture:** Windows 固定工作线程运行线程亲和 Fiber，本地队列加工作窃取；Channel 挂起 Fiber 而非线程。解析器把现代并发语法降级为普通调用和选择描述符，静态引用在 SSA 阶段产生捕获、逃逸、共享和所有权转移摘要。

**Tech Stack:** C11、Windows Fiber/Thread/Event/SRWLOCK、Mira parser、SSA Static Reference、x64 后端。

## Global Constraints

- 不为每个任务创建系统线程。
- 已开始 Fiber 不跨工作线程迁移。
- Channel 锁内不执行用户代码或切换 Fiber。
- 证据不足时保守关闭优化，不关闭并发功能。
- 现有 `parallel/join` 兼容。

---

### Task 1: 调度器核心

**Files:**
- Create: `runtime/rt_sched.c`
- Create: `runtime/rt_sched.h`
- Test: `tests/runtime_sched_test.c`

**Interfaces:**
- Produces: `mira_sched_init`, `mira_go_start`, `mira_task_yield`, `mira_task_join`, `mira_sched_wait_all`, `mira_sched_shutdown`。

- [ ] 写失败测试：10000 个任务恰好执行一次，工作线程数受限。
- [ ] 实现工作线程、本地队列、全局注入队列和任务计数。
- [ ] 实现线程亲和 Fiber 的首次启动、yield、完成和 join。
- [ ] 验证单线程确定性和多线程总数。

### Task 2: Channel

**Files:**
- Create: `runtime/rt_channel.c`
- Create: `runtime/rt_channel.h`
- Test: `tests/runtime_channel_test.c`

**Interfaces:**
- Produces: `mira_channel_new`, `mira_channel_send`, `mira_channel_recv`, `mira_channel_close`, `mira_channel_free`。

- [ ] 写容量 0/1/N 的阻塞、唤醒、顺序和关闭失败测试。
- [ ] 实现环形缓冲和发送/接收等待队列。
- [ ] 通过调度器 park/wake 接口挂起任务。
- [ ] 验证多生产者、多消费者和关闭竞争。

### Task 3: Select

**Files:**
- Create: `runtime/rt_select.c`
- Modify: `runtime/rt_channel.h`
- Test: `tests/runtime_select_test.c`

**Interfaces:**
- Produces: `MiraSelectCase`, `mira_channel_select`。

- [ ] 写立即就绪、default、阻塞、多 case 竞争和公平性失败测试。
- [ ] 实现随机起点扫描、原子认领和登记撤销。
- [ ] 验证每次只选择一个 case。

### Task 4: 编译器语法与调用链

**Files:**
- Modify: `mira.h`
- Modify: `parser/blocks.c`
- Modify: `parser/infix.c`
- Modify: `codegen/ssa_builder.c`
- Modify: `codegen/program.c`
- Modify: `main.c`
- Test: `tests/modern_go_channel.mira`
- Test: `tests/modern_select.mira`

**Interfaces:**
- Consumes: Tasks 1–3 的运行时接口。
- Produces: `go`、Channel 和 select 的现有 SSA 调用链。

- [ ] 先修复 `parallel([] {...})` 的 lambda 参数解析失败。
- [ ] 解析 `go [] {...}`、channel 调用和 select case。
- [ ] 将现有 `parallel/join` 转发到调度器。
- [ ] 验证 O0–O3 输出一致。

### Task 5: 静态引用并发事实

**Files:**
- Modify: `codegen/ir_ssa.h`
- Modify: `codegen/ssa_ref.c`
- Modify: `codegen/ssa_ref_vm.c`
- Modify: `codegen/decision.c`
- Modify: `codegen/decision.h`
- Test: `tests/ssa_ref_concurrency_test.c`

**Interfaces:**
- Produces: 捕获逃逸、并发只读、共享、唯一 Channel 转移和并发效果摘要。

- [ ] 写结构失败测试覆盖只读捕获、可变共享、唯一发送和效果屏障。
- [ ] 在 go/send/recv/close/select 节点传播事实。
- [ ] 智策只在证据充分时启用零拷贝和快路径。
- [ ] 验证 DCE/内存移动不跨越并发效果。

### Task 6: 全量与压力验证

**Files:**
- Create: `tests/run_concurrency_suite.ps1`
- Create: `tests/concurrency_stress.mira`

**Interfaces:**
- Produces: 功能、竞态、压力、线程数量、编译时间和回归证据。

- [ ] 运行调度器、Channel、select 和静态引用结构测试。
- [ ] 运行 10000 任务及多生产者/消费者压力测试。
- [ ] 运行 modern、结构体、SSA、寄存器、parallel 和 fib 回归。
- [ ] 检查 10 行样例预热后三次编译中位数低于 100 ms。
