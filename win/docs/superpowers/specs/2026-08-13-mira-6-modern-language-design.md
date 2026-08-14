# Mira 6.0 现代语言与标准库重构设计

日期：2026-08-13

## 目标

Mira 6.0 将源语言收敛为单一的现代中缀语法，并以不兼容升级的方式重建标准库。旧后缀语法、旧标准库 API 和相关兼容路径全部移除，不设置弃用期。

本次重构只改变源语言、模块接口和标准库组织，不推翻现有 IR、SSA、寄存器分配、机器码编码器或自写链接器。编译器内部可以继续使用栈式中间表示；用户不再接触栈语言模型。

## 成功标准

- 新建、示例、测试和标准库中的可执行 Mira 源码全部使用现代中缀语法。
- 编译器拒绝旧函数定义、后缀表达式、后缀变量读写和 `!syntax postfix`，并给出明确错误。
- Windows 与 Linux 源码和标准库接口一致。
- 新标准库具有稳定的模块边界、命名空间和一致命名规则。
- O0-O3 语义回归、现有差分正确性测试、标准库测试和性能基线全部通过。
- 删除兼容代码后，既有现代语法程序的运行性能和产物尺寸不发生实质退化。

## 语言模型

### 唯一公开语法

Mira 6.0 只支持现代中缀语法：

```mira
fn add(a: i64, b: i64) -> i64 {
    a + b
}

fn main() {
    mut value = add(20, 22);
    value += 1;
    print(value);
}
```

函数既支持末尾表达式返回，也支持显式提前 `return`。变量统一使用 `let`、`mut` 和 `const`；赋值统一使用 `=`、复合赋值以及 `++`/`--`。

### 删除的源语言能力

以下内容从语法、文档、测试和项目模板中删除：

- `!syntax postfix`
- `name: { args } ...` 函数定义
- `: name ... ;` 函数定义
- 后缀表达式求值
- `x @`、`x !` 变量读写
- 裸 `@`、`!`、`c@`、`c!`
- `dup`、`drop`、`swap`、`over`、`nip`、`rot`、`depth`
- `var` 和 `x: value` 形式的旧变量声明
- 所有仅用于兼容旧语法的解析路径

删除的是公共源语言能力，不要求把编译器内部栈式 IR 改成另一种 IR。

### 现代外部函数

旧写法：

```mira
extern lwmgl-clear: { r g b }
```

替换为：

```mira
extern fn lwmgl_clear(r: f64, g: f64, b: f64);
```

外部符号名不再依赖把 Mira 标识符中的连字符隐式替换成下划线。公共模块可以在普通 Mira 函数中包装外部函数。

## 模块与导入

### 语法

```mira
import std.math;
import std.random as rng;

fn main() {
    rng.seed(42);
    print(math.max(rng.range(1, 101), 50));
}
```

规则：

- `import std.math;` 解析到 `stdlib/std/math.mira`。
- 默认模块名为路径最后一段，本例为 `math`。
- `as` 指定当前文件内的模块别名。
- 导入不会把成员注入全局作用域；必须使用 `math.max()` 等限定名称。
- 同一规范路径只加载一次；循环导入产生明确错误并展示导入链。
- 相对文件导入保留现代形式，但同样建立模块命名空间，不再把定义直接拼入当前文件。
- Windows 专属模块在 Linux 上导入时产生编译错误。

### 名称解析

模块成员在编译前解析为稳定的限定符号，例如 `math.max`。限定名称贯穿函数索引、调用事实和链接符号生成，避免依赖文本替换。模块别名只影响源文件内的名称解析，不改变最终符号身份。

## 标准库布局

标准库根目录由原来的 `libs-mira/` 迁移为 `stdlib/std/`。旧库不机械翻译；只复用经过验证的算法和运行时入口。

### Prelude

无需导入：

```text
print(value)
println(value)
assert(condition)
panic(message)
to_int(value)
to_float(value)
to_string(value)
true
false
```

Prelude 只容纳写最小程序必需的能力。其他功能必须显式导入。

### `std.math`

首版公开：

```text
math.abs(x)
math.min(a, b)
math.max(a, b)
math.clamp(x, low, high)
math.sqrt(x)
math.pow(x, exponent)
math.PI
math.E
```

`floor`、`ceil`、`round` 只有在运行时与跨平台语义完成并验证后才加入，不提供空壳。

### `std.random`

```text
random.seed(seed)
random.int()
random.range(min, max)
random.float()
random.bool()
```

`range(min, max)` 使用半开区间 `[min, max)`；`max <= min` 触发明确运行时错误。同一平台和版本中，相同种子产生相同序列。跨版本序列稳定性不在 6.0 首版承诺范围内。

### `std.string`

```text
string.length(text)
string.concat(a, b)
string.equal(a, b)
string.contains(text, part)
string.find(text, part)
string.slice(text, start, length)
string.trim(text)
string.at(text, index)
string.from_int(value)
string.to_int(text)
```

`at` 返回字符的整数值；首版字符串仍沿用现有字节字符串表示，不宣称 Unicode 码点索引。越界和无效数字转换必须使用统一错误策略，不得静默访问非法内存。

### `std.time`

```text
time.now()
time.monotonic()
time.sleep(milliseconds)
```

`now` 返回 Unix 秒；`monotonic` 返回纳秒级单调计时值，不代表墙上时间。

### `std.io`

```text
io.print(value)
io.println(value)
io.read_line()
io.read_int()
```

Prelude 的 `print`、`println` 是这两个输出接口的快捷入口。

### `std.fs`

```text
fs.read(path)
fs.write(path, content)
fs.append(path, content)
fs.exists(path)
fs.remove(path)
```

目录遍历、权限和路径对象不在本轮范围内。

### `std.list`

```text
list.new()
list.length(values)
list.get(values, index)
list.set(values, index, value)
list.push(values, value)
list.free(values)
```

首版继续使用现有动态列表运行时。泛型、自动生命周期和新集合类型留待独立设计。

### `std.task`

```text
task.spawn(function)
task.join(handle)
task.yield()
task.wait_all()
```

旧 `parallel/join` 与 `go/join-task` 合并为单一任务模型。编译器仍可依据静态效果分析自动选择快速任务或可挂起 Fiber；该选择不进入公共 API。

### `std.channel`

```text
channel.new(capacity)
channel.send(ch, value)
channel.receive(ch)
channel.close(ch)
channel.free(ch)
```

容量 `0` 表示无缓冲通道。`select` 暂不进入首版公共 API，待其 Mira 源语法和公平性测试独立完成后加入。

### `std.memory`

```text
memory.allocate(size)
memory.free(ptr)
memory.copy(destination, source, size)
memory.zero(ptr, size)
memory.load_i64(ptr)
memory.store_i64(ptr, value)
memory.load_u8(ptr)
memory.store_u8(ptr, value)
```

这些是显式不安全的底层操作。参数顺序使用常见函数调用顺序，不继承旧后缀栈顺序。非法地址仍可能终止原生程序；文档必须明确这一点。

### `std.process`

```text
process.id()
process.exit(code)
process.env(name)
process.exec(command)
```

跨平台实现不一致时，在不支持的平台给出明确错误，不返回伪造结果。

### `std.windows`

```text
windows.message_box(text, title)
windows.clipboard_get()
windows.clipboard_set(text)
windows.console_title(title)
windows.console_color(color)
windows.cursor_move(x, y)
windows.beep(frequency, duration)
```

该模块只允许 Windows 目标导入。

### 非标准库扩展

LWMGL 移出标准库，作为独立扩展包 `lwmgl`：

```mira
import lwmgl;

fn main() {
    lwmgl.init(800, 600);
}
```

标准库不绑定特定图形 DLL。

## 编译器与运行时边界

公共函数通过 Mira 标准库包装内部运行时入口。内部入口使用保留前缀，例如：

```text
__mira_math_sqrt
__mira_string_length
__mira_channel_send
__mira_memory_load_i64
```

带 `__mira_` 前缀的名字不是公共 API，用户源码直接调用时应报错。这样运行时实现和 ABI 可以独立演进，而不破坏标准库接口。

对于性能敏感且无法用普通 Mira 高效表达的操作，包装函数允许由编译器内联或直接降低为运行时调用。包装层不得造成可测量的稳定运行时开销。

## 错误处理

- 旧语法产生针对性的迁移错误，而不是笼统的“未知标识符”。
- 未找到模块时展示请求名和搜索位置。
- 循环导入展示完整导入链。
- 重复模块别名、未知成员和平台不兼容分别报告。
- 标准库参数错误使用统一的 `panic`/运行时错误路径。
- `random.range` 非法范围、字符串越界和列表越界必须覆盖测试。

不提供自动源码转换器；项目尚无外部兼容负担，自动转换会延长旧模型寿命。

## 迁移与删除顺序

1. 为旧语法和旧 API 建立失败测试，为当前现代程序建立保护性回归。
2. 实现模块表、限定名称解析、现代导入语法和循环导入诊断。
3. 实现 `extern fn` 并迁移外部调用测试。
4. 建立 Prelude 与 `stdlib/std/`，先完成 `math`、`random`、`string`、`time`。
5. 包装并验证 `io`、`fs`、`list`、`task`、`channel`、`memory`、`process`、`windows`。
6. 迁移仍有价值的程序、benchmark、fuzz 输入和回归测试。
7. 删除旧标准库、旧项目模板、旧公共内建名称和后缀解析路径。
8. 同步 Windows/Linux 并运行完整验证。
9. 更新 README、语言手册、模块 API 文档和 6.0 迁移说明。

每一步保持编译器可构建；在现代标准库及测试迁移完成前，不提前删除其仍依赖的底层实现。

## 测试策略

### 解析与诊断

- 每一种新导入形式、别名、限定调用、循环导入和平台错误。
- `extern fn` 的参数、返回类型、符号解析和错误输入。
- 每一种旧语法都有明确拒绝测试。

### 标准库

- 每个公开函数至少包含正常、边界和错误路径测试。
- `random` 使用固定种子测试确定性和范围。
- `string` 覆盖空串、边界索引和无效转换。
- `task/channel` 覆盖阻塞、缓冲、关闭、join 和大量短任务。
- `memory` 覆盖 i64/u8 读写、复制和清零。

### 编译器正确性

- 所有现代测试在 O0、O1、O2、O3 下结果一致。
- 保留并扩展当前差分测试；生成器只生成 Mira 6.0 语法。
- Windows 原生验证 PE/Win64 ABI；Linux 原生验证 ELF/SysV ABI。

### 性能与产物

- 记录迁移前的现代程序编译时间、运行时间和产物大小。
- 模块包装不得引入稳定可观察的调用开销；必要时验证内联或直接降低。
- 删除旧解析路径后，现代源码的编译速度不得出现超过测量噪声的退化。

## 明确不在本次范围内

- 重写 SSA 或机器码后端
- 泛型和完整静态类型系统
- 自动内存管理新模型
- Unicode 字符串重设计
- 包管理器和在线仓库
- `select` 的新源语言设计
- 目录、网络、GUI 等新标准库能力
- 旧源码自动迁移工具

这些能力应分别设计，不能借本次删除兼容代码顺带加入。

## 发布

该变更发布为 Mira 6.0，版本说明明确标注完全不兼容。仓库只保留一份当前语言文档；历史后缀语法只存在于 Git 历史和 5.x Release 中，不继续出现在主分支教程和可执行示例里。
