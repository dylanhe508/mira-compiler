# Mira pressure 算术优化设计

## 目标与基线

以正式编译器 `win/mira.exe` 的最新重建版本为唯一性能基线。在 Windows
x86-64 上，`bench/bench_pressure.mira` 使用 `-O3`、3 次预热和 31 次正式
样本时，中位数为 21.627 ms；同机 GCC 10.3.0 `-O3` 中位数为 17.820 ms。
两者 checksum 均为 69441320302500000。

目标是让 Mira 的 pressure 中位数至少追平 GCC，同时满足：

- 不针对 benchmark 名、函数名或特定常量写规则；
- 其他运行基准不得出现可复现退化；
- 小程序预热编译中位数退化不得超过 1%；
- O0-O3 语义一致；
- 所有新优化先有失败测试，再实现，再跑完整回归。

## 已确认事实

最新正式编译器已启用 call-frame hoist，`run_pressure` 热循环不再逐次分配
Win64 shadow space。`pressure_call` 的高寄存器压力也已由现有图着色/线性
扫描系统控制到约一个实际 spill，因此本设计不修改寄存器分配器。

剩余差异主要在纯整数算术：Mira 最终 IR 仍包含约 24 条常数 `imul`，而 GCC
利用表达式重关联、LEA、移位和加减生成更短的依赖链。Mira 已有循环归纳变量
强度削弱、多因子仿射递推、范围分析、静态引用事实和智策成本模型；新工作必须
复用这些基础，而不是新增一套相互竞争的优化框架。

## 架构

### 1. 通用非循环仿射事实

从现有仿射证明逻辑中抽取一个有界、非递归的 SSA 分析。它为纯整数 SSA 值产生：

## 2026-08-08 measured evidence

- Candidate: `win/out/mira-pressure-candidate.exe`, Mira 5.13.4, TDM-GCC
  10.3.0, commit `772a5ce`.
- `bench_pressure` checksum: `69441320302500000`. With three warmups and 101
  samples, candidate median was 4,260,700 ns and GCC `-O3` was 17,498,400 ns
  (ratio 0.2435).
- Runtime ratios versus formal Mira: fib 0.9983 (31 samples), stencil 0.9906
  (31), branch 0.9993 (101), vector_add 0.9908 (101). Checksums matched.
- Complete compile/link ratios after three warmups and 101 samples: fib 0.9987
  (16.208/16.229 ms), pressure 0.9637 (14.630/15.181 ms).
- Focused Windows O0-O3 regressions passed: affine collapse/profitability,
  multi-factor affine, induction strength, dynamic slots, multiply strength,
  div/rem reuse, PHI/inlining, recursion, and nonvolatile-call ABI.
- Fresh final build and structural tests exited zero. Enabled O3 has one
  `imul`; `MIRA_DECISION_DISABLE=affine-collapse` retains 23.
- After the final legality fixes, pressure measured 4,284,400 ns versus GCC
  17,787,500 ns (ratio 0.2409, about 4.15x faster). Final complete compile/link
  ratios were fib 1.0050 (21.444/21.337 ms) and pressure 0.9823
  (19.349/19.698 ms), each after three warmups and 101 samples.
- Linux/SysV built successfully and produced the same pressure checksum. A
  seeded 200-program O0-O3 fuzz run found 0 ICEs, 0 runtime-output differences,
  and 23 O0 stack-underflow rejections; the formal compiler reproduces those
  rejections on the same generated inputs. The retained mira2c corpus reported
  0 output mismatches and 0 GCC failures for every valid case.
- Acceptance gates are complete. The formal `win/mira.exe` has not been replaced.

```text
base_vreg      唯一非常量基础值
coefficient    64 位回绕系数
constant       64 位回绕偏移
instruction_count
proof_complete
```

第一版只传播 `IMM`、`COPY`、`ADD`、`SUB` 和一侧为常量的 `MUL`。出现第二个
不同基础值、调用、内存、所有权效果、未知操作或预算耗尽时，结果为未证明。
分析使用按 VReg 编号的数组和有界固定点，复杂度为 O(指令数)，不做组合搜索。

### 2. 位掩码证明

复用整数范围/位事实，只接受严格可证明的掩码覆盖。第一版仅处理相同的
`2^n-1` 外层掩码，并要求被消除的内层值到外层掩码之间只有模 2^64 的
ADD、SUB 和 MUL。任何调用、比较、移位、除法、内存或不同掩码都拒绝。

掩码证明是独立合法性条件；仿射收益不能绕过它。

### 3. 智策提交

SSA 分析只提供事实，不直接改写。智策比较 KEEP 与 AFFINE_COLLAPSE：

- 旧、新动态指令估计；
- 旧、新代码体积；
- 寄存器压力变化；
- 热度和可信度；
- 当前函数代码增长预算。

只有证明完整、预计动态指令严格减少、代码体积不增加且寄存器压力不增加时
才提交。无法证明收益时保持原 SSA。调试模式输出选择、评分和拒绝原因。

### 4. 后端边界

SSA 归一化只把区域折叠为 `base*K+C`，不在同一改动中扩展机器级常数乘法
搜索。现有 lowering 和机器级 strength reduction 负责最后一次乘法。只有在
SSA 折叠通过全部验收但仍未追平 GCC 时，才启动第二阶段的目标成本指令选择。

## 测试

### 结构测试

先构造失败测试，覆盖：

- 长于当前递归预算的单基础值加法链；
- 正负系数、正负常数和 64 位回绕；
- 两个基础值时拒绝；
- 非常量乘法时拒绝；
- 调用、内存和所有权边界时拒绝；
- 相同末端掩码可证明覆盖；
- 不同或不可证明掩码时保持 IR 摘要不变；
- 智策判断无收益时保持 IR 摘要不变。

### 集成与性能

1. 重建候选编译器，不覆盖正式 `mira.exe`。
2. 运行五项 benchmark 的 O0-O3 checksum 回归。
3. 运行现有仿射、动态槽、除法、Phi/内联、递归、ABI 和极端应用回归。
4. 运行 fuzz/mira2c 差分测试。
5. Windows 与 Linux 构建通过。
6. pressure 3 次预热、101 次正式样本；中位数至少追平 GCC。
7. 其他基准若疑似变化落在 ±1% 内，使用 101 次复测；不接受可复现退化。
8. 小程序编译时间同样使用预热和 101 次复测，退化不得超过 1%。

若正确性通过但性能门槛失败，优化默认不合入，不用其他基准的退化交换
pressure 收益。
