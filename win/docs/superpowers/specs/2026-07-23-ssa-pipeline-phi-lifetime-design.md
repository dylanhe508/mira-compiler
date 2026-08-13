# Mira SSA 流水线与 PHI 生命周期重构设计

## 目标

将 PHI 保留到模块级内联和 SSA 优化结束，使内联后的循环仍是合法单定义
SSA；随后统一销毁 PHI，再进入寄存器分配和 lowering。

## 新顺序

```text
SSA 构建 / mem2reg
→ 静态引用与初次智策
→ 模块内联
→ CFG、支配、def/use、循环事实重建
→ 静态引用与智策刷新
→ 函数 SSA 固定点优化
→ SSA 合法性验证
→ 全模块统一销毁 PHI
→ var_reg_map / 寄存器分配 / lowering
```

## 阶段一范围

- `ssa_build()` 不再调用 `ssa_optimize_function()`，也不销毁 PHI。
- 暴露模块级 `ssa_destroy_phis_module()`，只由 `program.c` 在
  `ssa_optimize_module()` 完成后调用。
- 模块优化器不再因块名以 `inl_` 开头而跳过函数；改为以单定义验证决定
  能否执行只适用于 SSA 的 pass。
- 内联后必须重建 CFG/支配信息，并重新分析静态引用、寄存器压力、智策和循环。
- CFG 重建失败或函数不是合法单定义 SSA 时，只跳过要求 SSA 的优化；仍必须在
  lowering 前可靠销毁原有 PHI。
- 阶段一不加入归纳变量强度削弱，不改变优化准则含义。

## 安全要求

- 所有 PHI 的每一对 `(value, predecessor)` 必须对应 header 的真实前驱。
- PHI 销毁只能在前驱终结指令前插入 COPY，并保持 CFG 不变。
- 内联克隆必须为 VReg、block 目标和 PHI predecessor 建立一致映射。
- block id 在删除/插入后必须连续为 `[0, block_count)`。
- 重算支配关系前释放旧的 idom、dom_children 和 dominance frontier。
- `vreg_defs` 必须从当前指令链重建，不能信任已摘链指令留下的陈旧指针。
- 不得新增全局变量槽、BSS 容量或隐藏内存状态。

## 验收

- 编译器重新构建成功。
- 六个极端应用 O0–O3 全部通过。
- `regression_branch_phi`、`regression_deep_small`、
  `regression_signed_pow2_div`、`regression_induction_strength` O0–O3 全通过。
- 长矩阵输出保持 `-7778248811425506175`。
- 阶段一长矩阵中位数不得比当前 `64.3 ms` 退化超过 5%；超过则停止阶段二并
  定位流水线变化。
- 十行程序预热后编译中位数保持低于 100 ms。

