# Mira 分片 Fast Queue 与任务窃取设计

## 目标

消除 fast task 提交对调度器全局队列锁的争用，并让空闲 worker 窃取尚未启动的 fast task。

## 架构

每个 worker 增加独立 inbox、SRWLOCK 和唤醒事件。外部 fast task 轮询分配到 inbox；owner 尚未绑定，因此其他 worker 可以从队尾窃取。任务一旦开始、创建 Fiber、yield 或 park，就绑定原 worker，只进入该 worker 的 ready queue，绝不迁移。

普通可挂起任务暂时保留现有全局 FIFO，以缩小正确性风险。worker 检查顺序为本地 ready、本地 fast inbox、其他 worker fast inbox、全局普通队列，然后等待。fast 提交不获取 scheduler.lock。

## 验证

结构测试断言 fast 提交的全局锁计数为零；压力测试验证任务恰好执行一次、多 worker 参与、join/Channel/select 无回归，并记录10万任务中位数。

