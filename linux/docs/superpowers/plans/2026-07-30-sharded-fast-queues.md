# Sharded Fast Queues Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 fast task 从全局调度锁迁移到 worker 分片 inbox，并支持安全窃取。

**Architecture:** 每 worker 独立 FIFO，外部轮询投递，空闲 worker 只窃取未启动任务；Fiber ready 队列保持亲和。

**Tech Stack:** C11、Windows SRWLOCK/Event、Mira scheduler。

## Global Constraints

- 只窃取尚未启动且无 owner 的 fast task。
- parked/yielded Fiber 永不跨线程。
- 普通任务行为不变。

---

### Task 1: 分片投递

**Files:**
- Modify: `runtime/rt_sched.c`
- Modify: `runtime/rt_sched.h`
- Test: `tests/runtime_sched_sharded_test.c`

- [ ] 写100000 fast task正确性及全局快任务锁计数为0的失败测试。
- [ ] 增加worker inbox、轮询投递和独立唤醒。
- [ ] 运行fast、join、Channel回归。

### Task 2: 安全窃取

**Files:**
- Modify: `runtime/rt_sched.c`
- Test: `tests/runtime_sched_steal_test.c`

- [ ] 写倾斜任务分布测试并要求多个worker完成。
- [ ] 空闲worker扫描其他inbox，仅弹出owner为空的任务。
- [ ] 验证任务恰好执行一次和Fiber线程亲和。

### Task 3: 性能验收

**Files:**
- Test: `tests/concurrency_fast_compare.c`

- [ ] 运行五次10万任务基准并计算中位数。
- [ ] 运行11组并发回归、O0-O3、静态引用和fib。

