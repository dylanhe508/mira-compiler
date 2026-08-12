# Mira 结构体成员与方法实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 Mira 现代语法实现 `value.field`、字段赋值、`impl Type`、`self`、`mut self` 和 `value.method()`，同时保持现有 SSA 与后端不变。

**Architecture:** 在解析器维护结构体绑定类型、可变性和方法元数据，将成员操作静态展开为现有构造器、字段访问器、列表读写和普通直接调用。解释器、SSA、寄存器分配和编码器只接收已有 IR。

**Tech Stack:** C11、Mira 现代语法解析器、现有 postfix IR、SSA、本机 x64 后端、Windows PowerShell 测试。

## Global Constraints

- 旧的 `Point.x(point)` 语法继续工作。
- 不新增运行时动态分派、继承、虚函数、重载或泛型方法。
- `let` 实例只读；`mut` 实例可修改。
- 普通 `self` 只读；`mut self` 修改原实例。
- `-O0` 至 `-O3` 语义一致，解释执行与本机编译一致。
- 10 行级程序预热后三次编译中位数低于 100 ms。

---

### Task 1: 建立结构体类型、绑定与方法元数据

**Files:**
- Modify: `mira.h`
- Modify: `parser/parser.h`
- Modify: `parser/helpers.c`
- Test: `tests/modern_member_errors.mira`

**Interfaces:**
- Consumes: `Program`, `StructDef`, 现有变量槽和 `Def` 表。
- Produces: `MethodDef`, `VarBindingInfo`；`prog_find_method()`、`prog_set_var_binding()`、`prog_get_var_binding()`。

- [ ] 在 `StructDef` 旁增加方法元数据，记录所属结构体、源码方法名、内部定义名以及 `mut_self`。
- [ ] 在 `Program` 增加按变量槽索引的结构体类型与可变性数组；扩容必须与 `prog_add_var()` 同步。
- [ ] 实现精确名称查找和重复方法检测。
- [ ] 编译仅包含 `impl Missing`、重复方法和未知接收者的负例，确认当前版本失败位置不满足新诊断。
- [ ] 构建 `mira-candidate.exe`，保证旧测试仍能解析。

### Task 2: 解析 `impl`、`self` 和静态方法定义

**Files:**
- Modify: `parser/index.c`
- Modify: `parser/blocks.c`
- Test: `tests/modern_methods_read.mira`
- Test: `tests/modern_member_errors.mira`

**Interfaces:**
- Consumes: Task 1 的方法和绑定元数据。
- Produces: 内部普通 `Def`，名称为长度安全生成的结构体限定名；第一个参数固定映射为 `self`。

- [ ] 添加失败用例：`impl Point { fn sum(self) ... }` 与 `p.sum()` 输出 42。
- [ ] 解析 `impl Type { fn ... }`，复用现代 `fn` 参数、返回类型和函数体解析。
- [ ] 接受首参数 `self` 或 `mut self`，禁止其他位置使用 `self`。
- [ ] 方法内部定义以结构体限定名注册，重复方法在解析期报错。
- [ ] 运行只读方法正例并确认从红变绿。

### Task 3: 解析字段读取和方法调用

**Files:**
- Modify: `parser/infix.c`
- Modify: `parser/parse_one.c`
- Modify: `parser/blocks.c`
- Test: `tests/modern_methods_read.mira`
- Test: `tests/modern_member_chain.mira`

**Interfaces:**
- Consumes: 接收者变量槽的 `VarBindingInfo` 与 `prog_find_method()`。
- Produces: `p.x` 对应的现有 `Point.x(p)` postfix 链；`p.sum(a)` 对应的 `p a Point$sum` 直接调用链。

- [ ] 为 `let p = Point(...)` 和 `mut p = Point(...)` 推断并记录 `Point` 类型。
- [ ] 在分流一般 ID/函数调用之前识别单层 `receiver.member`。
- [ ] 字段读取展开到已有 getter，方法调用把接收者置于显式参数之前。
- [ ] 对未知字段、未知方法和无法确定类型给出含接收者与成员名的错误。
- [ ] 验证嵌套算术、方法参数和新旧访问形式输出一致。

### Task 4: 字段写入、复合赋值与可变方法

**Files:**
- Modify: `parser/blocks.c`
- Modify: `parser/index.c`
- Test: `tests/modern_methods_mut.mira`
- Test: `tests/modern_member_errors.mira`

**Interfaces:**
- Consumes: 现有 `list-get`、`list-set`、结构体字段偏移和绑定可变性。
- Produces: `p.x = v`、`p.x += v` 及 `p.move_x(v)` 的普通 postfix IR。

- [ ] 添加 `mut p` 字段直接写、复合赋值和 `mut self` 方法用例。
- [ ] 将字段写入降级为现有 `list-set`，复合赋值为读取、运算、写回。
- [ ] `mut self` 使用原对象引用，禁止构造副本。
- [ ] 拒绝修改 `let`、在普通 `self` 方法写字段、用只读实例调用 `mut self` 方法。
- [ ] 验证所有正负例诊断和结果。

### Task 5: 全链路回归与编译预算

**Files:**
- Modify: `tests/modern_struct_syntax.mira`
- Create: `tests/run_modern_members.ps1`

**Interfaces:**
- Consumes: Tasks 1–4 的解析与展开行为。
- Produces: O0–O3、解释器、本机执行和编译时长证据。

- [ ] 运行所有 `tests/modern_*.mira`，记录每个期望输出。
- [ ] 对成员与方法正例执行 `-O0`、`-O1`、`-O2`、`-O3`，要求输出完全一致。
- [ ] 用解释器运行相同用例并与本机结果比较。
- [ ] 运行已有结构体、SSA、寄存器和 `bench_fib.mira` 回归。
- [ ] 对 10 行成员方法样例预热后编译三次，中位数必须低于 100 ms。
- [ ] 检查生成 IR，确认没有新增动态方法表或间接调用。
