# Mira Compiler

A self-built compiler for the Mira language (x86-64) with a complete toolchain
that does not depend on any external assembler or linker:

**lexer → parser → IR → SSA (register allocation) → x86-64 encoding → self-written linker**
(Windows: COFF/PE + Win64 ABI; Linux: ELF + SysV ABI).

The standard library is written in Mira itself (`libs-mira/`); the low-level
runtime support is written in C (`runtime/`, built and linked together with the compiler).

## Features

- **Optimization levels O0–O3 with identical semantics** (O3 default; SSA passes: mem2reg,
  dead code elimination, constant propagation, lea strength reduction, shadow space
  hoisting, affine folding, …)
- **Two platforms from one source tree**: Windows (COFF/PE, Win64 ABI) and Linux
  (ELF, SysV ABI)
- **Built-in concurrent runtime**: worker thread pool, lightweight tasks, channels,
  select, join, fibers (coroutines)
- **Fast compilation, tiny binaries**: ~0.3–1 s for 1000-function programs;
  typical output ~8 KB (a gcc reference build is 368 KB)
- **Self-contained executables**: compiler + linker in one binary, the output runs
  directly (plain PE on Windows, plain ELF on Linux)

## Directory layout

```
win/       Windows authoritative source tree (builds mira.exe)
linux/     Linux mirror source tree (manual mirror of win/, builds mira)
bench/     benchmarks and fuzz differential assets (internal use)
```

Each source tree contains: `main/lexer/parser/codegen/linker/runtime` +
`tests/` (regression cases) + `libs-mira/` (standard library) + `apps/` (example apps) + `docs/`.

## Building

Requires a C compiler (gcc/clang); no other dependencies.

### Windows (TDM-GCC or MinGW)

```bat
cd win
mingw32-make
:: output: mira.exe + runtime\*.obj
```

### Linux

```sh
cd linux
make
# output: mira + runtime/*.o
```

## Quick start

```mira
// hello.mira
fn main() {
    print("hello, mira!");
}
```

```sh
# Windows
mira.exe hello.mira && hello.exe
# Linux
./mira hello.mira && ./hello
```

Optimization levels `-O0/-O1/-O2/-O3` are semantically identical; O3 is the default:

```sh
mira.exe -O0 hello.mira
```

A complete iterative example (fib(40) = 165580141):

```mira
fn fib(n) {
    mut a = 0;
    mut b = 1;
    mut i = 0;
    while (i < n) {
        mut next = a + b;
        a = b;
        b = next;
        i = i + 1;
    }
    b
}

fn main() {
    print("fib(40) = ");
    print(fib(40));
}
```

## Language cheat sheet

| Syntax | Meaning |
| --- | --- |
| `fn name(args) { ... }` | Define a function |
| `var x = value;` | Declare a variable (immutable) |
| `mut x = value;` | Declare a mutable variable |
| `x = value;` | Assign to a variable (top-level bare assignment is not allowed; use `var`) |
| `x !` / `x @` | Write / read a variable (postfix) |
| `(expr) !` / `(expr) @` | Pointer store / load |
| `[...]` | List literal |
| `while / if / else / for i in 0..n` | Control flow |

## Testing

```sh
# per-tree regression
bash win/regress.sh      # Windows
bash linux/regress.sh    # Linux

# full test suite (400 checks: tests 55×4 + fuzz 40×4 + bench 5×4,
# checksums verified against gcc references)
bash win/fulltest.sh
```

The fuzz/bench sections of `fulltest.sh` depend on the `bench/` directory.

## Version

5.13.4 (`mira -v` output)

## Notes

- `win/` is the authoritative source tree; `linux/` is a manually synchronized
  mirror — core code changes must be applied to both trees
  (win uses CRLF, linux uses LF; compare with `diff --strip-trailing-cr`).
- `parser.c` / `codegen.c` are Unity aggregation shells (`#include`-ing their
  sub-files); both Makefiles declare the full sub-file dependencies explicitly,
  so touching a sub-file triggers correct incremental rebuilds.
- The compiler is fully self-built: no external assembler/linker is needed at
  compile time; the runtime is written in C, so building the compiler itself
  requires gcc.

---

# Mira 编译器（中文版）

自研的 Mira 语言编译器（x86-64），完整工具链不依赖外部汇编器/链接器：
**lexer → parser → IR → SSA（寄存器分配）→ x86-64 编码 → 自研链接器**
（Windows: COFF/PE + Win64 ABI；Linux: ELF + SysV ABI）。

运行时标准库以 Mira 语言编写（`libs-mira/`），底层支持由 C 编写的运行时
（`runtime/`，随编译器一起构建/链接）。

## 特性

- **O0–O3 优化级别**：语义完全一致，默认 O3（含 SSA 优化：mem2reg、死代码消除、常量传播、强度削减 lea、shadow space hoisting、affine 折叠等）
- **双平台**：Windows（COFF/PE, Win64 ABI）与 Linux（ELF, SysV ABI），同一份源码
- **内置并发运行时**：worker 线程池、轻量任务、channel、select、join、fiber（协程）
- **编译快、产物小**：1000 函数规模 ~0.3–1 秒；产物典型 ~8 KB（gcc 参考 368 KB）
- **可执行文件自包含**：编译器+链接器一体，输出即运行（Windows 下直接 PE，Linux 下直接 ELF）

## 目录结构

```
win/       Windows 权威源码树（构建出 mira.exe）
linux/     Linux 镜像源码树（win/ 的人工同步镜像，构建出 mira）
bench/     基准测试与 fuzz 差分资产（内部使用）
```

每个源码树的组成：`main/lexer/parser/codegen/linker/runtime` +
`tests/`（回归用例）+ `libs-mira/`（标准库）+ `apps/`（示例应用）+ `docs/`。

## 构建

需要 C 编译器（gcc/clang，无其他依赖）。

### Windows（TDM-GCC 或 MinGW）

```bat
cd win
mingw32-make
:: 产物: mira.exe + runtime\*.obj
```

### Linux

```sh
cd linux
make
# 产物: mira + runtime/*.o
```

## 快速上手

```mira
// hello.mira
fn main() {
    print("hello, mira!");
}
```

```sh
# Windows
mira.exe hello.mira && hello.exe
# Linux
./mira hello.mira && ./hello
```

优化级别 `-O0/-O1/-O2/-O3`，语义一致，默认 O3：

```sh
mira.exe -O0 hello.mira
```

一个完整的迭代示例（fib(40) = 165580141）：

```mira
fn fib(n) {
    mut a = 0;
    mut b = 1;
    mut i = 0;
    while (i < n) {
        mut next = a + b;
        a = b;
        b = next;
        i = i + 1;
    }
    b
}

fn main() {
    print("fib(40) = ");
    print(fib(40));
}
```

## 语言速记

| 语法 | 含义 |
| --- | --- |
| `fn name(args) { ... }` | 定义函数 |
| `var x = 值;` | 声明变量（不可变） |
| `mut x = 值;` | 声明可变变量 |
| `x = 值;` | 给变量赋值（顶层裸赋值不合法，须用 `var` 声明） |
| `x !` / `x @` | 写变量 / 读变量（postfix） |
| `(expr) !` / `(expr) @` | 指针 store / load |
| `[...]` | 列表字面量 |
| `while / if / else / for i in 0..n` | 流程控制 |

## 测试

```sh
# 各树回归
bash win/regress.sh      # Windows
bash linux/regress.sh    # Linux

# 全量测试（400 项: tests 55×4 + fuzz 40×4 + bench 5×4，对比 gcc 参考校验和）
bash win/fulltest.sh
```

`fulltest.sh` 的 fuzz/bench 段依赖 `bench/` 目录。

## 版本

5.13.4（`mira -v` 输出）

## 注意事项

- `win/` 为权威源码树，`linux/` 为人工同步镜像——修改核心代码需双树同步
  （win 端 CRLF、linux 端 LF，比较用 `diff --strip-trailing-cr`）。
- `parser.c` / `codegen.c` 是 Unity 聚合壳（`#include` 聚合子文件），
  两树 Makefile 已显式声明全部子文件依赖，改子文件会正确触发增量重编。
- 编译器本体自研：不依赖外部汇编器/链接器；运行时用 C 编写，
  构建编译器本体需要 gcc。
