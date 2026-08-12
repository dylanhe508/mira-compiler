# 智策并发挂起决策设计

## 目标

让 SSA 智策自动判断 `go` lambda 是否可能挂起。证明不可挂起的任务在 worker 调度 Fiber 上直接执行；其他任务继续使用可恢复 Fiber。

## 事实传播

`SsaFunctionEffect` 增加 `may_suspend`。Channel send/recv/select、join、yield、wait、sleep、间接调用和未知外部调用置位；直接用户函数通过调用图传播。已知纯运行时函数采用白名单，仅表示“不挂起”，不表示无副作用。

## 改写

当 `mira_go_start0` 的函数指针来自可解析的 `SSA_OP_LEA_FUNC`，且目标函数 `may_suspend=false`，智策把调用改为 `mira_go_start_fast0`。无法解析函数目标时保留普通路径。

## 运行时

Fast task 仍可返回 JoinHandle，但在 worker 的调度 Fiber 上直接调用，不创建任务 Fiber。错误分类会破坏调度，因此所有未知情况必须保守走 Fiber。

