# Mira 并发吞吐优化设计

## 目标

在不改变 `join`、Channel、`yield/park` 语义的前提下，降低大量极小任务的提交与执行开销。基线为 100000 个任务约 1.63 秒；每次改动必须同时通过调度器、join、Channel、select 和 Mira 端到端回归。

## 设计

第一阶段拆分任务所有权：`mira_go_start` 创建不可 join 的 detached task，不分配 `MiraJoinHandle` 和 Windows Event；`mira_go_start_handle` 保留原有完整语义。完成计数仍由调度器统一维护。

第二阶段引入任务对象池。已完成的 detached task 回收到调度器空闲链表，提交时复用，关闭调度器时统一释放。Join task 在句柄生命周期验证稳定前不进入对象池。

第三阶段才考虑懒 Fiber。Windows Fiber 无法把已经在普通栈上执行的 C 函数原地转换为可恢复任务，因此不能在任务阻塞后无损“升级”。安全方案是由静态引用/智策证明任务不含 `yield/park/channel/join` 后，标记为不可挂起任务并直接执行；本轮不伪造这一证明。

## 正确性边界

- Detached task 不能被 join，但仍计入 `mira_sched_wait_all`。
- 可 join task 必须继续创建独立 Event 并保持引用计数。
- 所有可能阻塞的普通 `go` lambda 仍在 Fiber 上执行。
- 不改变 Channel 锁内禁止切换 Fiber 的规则。
- 性能目标以中位数为准，不用删除功能换成绩。

