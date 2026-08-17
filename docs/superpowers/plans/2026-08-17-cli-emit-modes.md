# Mira 命令行输出模式实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**目标：** 让 `-S/--emit=asm` 输出可被 GNU assembler 接受的 Intel x86-64 汇编，让 `--emit=ir` 明确输出内部 IR，让 `-c/--emit=obj` 只生成目标文件，并统一 `-o` 与参数诊断。

**架构：** 保留 Mira 现有的直接机器码编码器和自研链接器。把源文件到最终 `IrBuffer` 的过程收敛为唯一流水线，然后由 IR、ASM、OBJ、EXE 四个终点消费同一个最终结果；ASM 写出器独立放在 `codegen/asm_writer.c`，命令解析独立放在 `cli.c`。

**技术栈：** C11、Mira `IrBuffer`/自研 x86-64 编码器、GNU assembler Intel 语法、PowerShell 回归脚本、Windows COFF 与 Linux ELF。

## 全局约束

- 普通 `mira input.mira` 仍直接编码机器码并用自研链接器生成可执行文件。
- 普通编译不得调用外部汇编器；GNU assembler 只用于测试输出 `.s` 是否有效。
- 当前默认优化级别以源码变量 `mira_opt_level` 为准，当前值是 `2`。
- `-S` 改为 ASM；检查内部 IR 的仓库脚本必须迁移到 `--emit=ir`。
- Windows 与 Linux 的平台无关文件逐项镜像；保留 `ssa_lower.c` 等既有 ABI 差异。
- 原始目录 `E:\mira\mira` 不修改；只修改当前隔离仓库。
- 每项生产改动必须先有真实 RED，再取得 focused GREEN，最后才提交。

---

## 文件结构

- 新建 `win/cli.h`、`win/cli.c`：只负责把 argv 解析为稳定的 `MiraCliOptions`，不编译、不写文件。
- 新建 `linux/cli.h`、`linux/cli.c`：与 Windows 平台无关内容镜像。
- 新建 `win/codegen/asm_writer.h`、`win/codegen/asm_writer.c`：把最终 `IrBuffer` 写成 GNU Intel 汇编。
- 新建 `linux/codegen/asm_writer.h`、`linux/codegen/asm_writer.c`：平台无关逻辑镜像，必要的符号/段差异由参数决定。
- 修改 `win/main.c`、`linux/main.c`：共享编译流水线和四种输出终点，不再自行猜测“最后一个非选项参数”。
- 修改两端 `codegen/ir_dump.c`：只保留内部 IR 调试格式，并对未知 opcode 返回失败。
- 修改两端 `codegen/ir.h`：声明两个写出接口需要的状态/错误类型。
- 修改两端 `Makefile`：加入 `cli.c` 和 `codegen/asm_writer.c`。
- 新建两端 `tests/cli_emit_modes.mira`、`tests/run_cli_emit_modes.ps1`：CLI 与输出模式端到端测试。
- 修改现有依赖 `-S` 的 PowerShell 测试：改用 `--emit=ir ... -o ...`。
- 修改 `README.md` 与两端 CLI 帮助：展示真实用法和默认优化级别。

---

### 任务 1：建立确定性的命令解析器

**文件：**
- 新建：`win/cli.h`
- 新建：`win/cli.c`
- 新建：`win/tests/cli_parse_test.c`
- 新建：`win/tests/run_cli_parse.ps1`
- 修改：`win/Makefile`
- 镜像：对应 `linux/` 文件

**接口：**
- 输入：`int argc, char **argv`
- 输出：`bool mira_cli_parse(int argc, char **argv, MiraCliOptions *out, char *error, size_t error_size)`
- 后续任务只读取 `MiraCliOptions`，不得再次扫描 argv。

- [ ] **步骤 1：写出命令解析器接口和失败测试**

`cli.h` 固定以下类型：

```c
#ifndef MIRA_CLI_H
#define MIRA_CLI_H
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    MIRA_COMMAND_COMPILE,
    MIRA_COMMAND_LINK,
    MIRA_COMMAND_NEW,
    MIRA_COMMAND_HELP,
    MIRA_COMMAND_VERSION
} MiraCommand;

typedef enum {
    MIRA_EMIT_EXE,
    MIRA_EMIT_ASM,
    MIRA_EMIT_IR,
    MIRA_EMIT_OBJ
} MiraEmitKind;

typedef struct {
    MiraCommand command;
    MiraEmitKind emit;
    const char *input;
    const char *output;
    const char *project_name;
    const char *march;
    const char *target;
    const char *link_inputs[64];
    int link_input_count;
    int opt_level;
    int avx2_override;
} MiraCliOptions;

bool mira_cli_parse(int argc, char **argv, MiraCliOptions *out,
                    char *error, size_t error_size);
#endif
```

`cli_parse_test.c` 必须直接断言：

```c
parse_ok("mira -S a.mira", MIRA_EMIT_ASM, "a.mira", NULL, 2);
parse_ok("mira --emit=ir a.mira -o a.ir -O3", MIRA_EMIT_IR, "a.mira", "a.ir", 3);
parse_ok("mira -c -O0 a.mira", MIRA_EMIT_OBJ, "a.mira", NULL, 0);
parse_ok("mira a.mira -o app.exe", MIRA_EMIT_EXE, "a.mira", "app.exe", 2);
parse_error("mira --emit=wat a.mira", "unknown emit mode 'wat'");
parse_error("mira -S -c a.mira", "conflicting emit modes");
parse_error("mira -o", "option '-o' requires a value");
parse_error("mira a.mira b.mira", "multiple input files");
parse_error("mira --mystery a.mira", "unknown option '--mystery'");
```

- [ ] **步骤 2：运行测试并确认 RED**

运行：

```powershell
powershell -ExecutionPolicy Bypass -File .\win\tests\run_cli_parse.ps1
```

预期：C 测试因 `mira_cli_parse` 尚未实现而编译或链接失败。

- [ ] **步骤 3：实现最小解析状态机**

`cli.c` 使用单次从左到右扫描；默认值固定为：

```c
*out = (MiraCliOptions){
    .command = MIRA_COMMAND_COMPILE,
    .emit = MIRA_EMIT_EXE,
    .opt_level = 2,
    .avx2_override = -1
};
```

解析规则必须完整覆盖：

```text
-S / --emit=asm       -> MIRA_EMIT_ASM
--emit=ir             -> MIRA_EMIT_IR
-c / --emit=obj       -> MIRA_EMIT_OBJ
-o VALUE              -> output
-O0 .. -O3            -> opt_level
-march=VALUE           -> march
-mavx2/-mno-avx2      -> avx2_override 1/0
--target=VALUE         -> target
-h/--help/-help       -> HELP
-v/--version/-version -> VERSION
-n/--new NAME         -> NEW
-l OBJ... [-o VALUE]  -> LINK
```

用一个 `set_emit()` helper 拒绝重复且不同的模式；任何以 `-` 开头但不在表中的
参数立即写入 `error` 并返回 `false`。编译命令只允许一个非选项输入。

- [ ] **步骤 4：运行 focused GREEN 并镜像**

运行两端 C 测试；预期均输出：

```text
CLI PARSE PASS
```

随后比较归一化换行后的 `cli.h`、`cli.c`、`cli_parse_test.c`，预期零差异。

- [ ] **步骤 5：提交**

```text
git add win/cli.* linux/cli.* win/tests/cli_parse_test.c linux/tests/cli_parse_test.c win/tests/run_cli_parse.ps1 linux/tests/run_cli_parse.ps1 win/Makefile linux/Makefile
git commit -m "feat(cli): parse explicit output modes"
```

---

### 任务 2：让所有产物共享最终机器 IR

**文件：**
- 修改：`win/main.c`
- 新建：`win/tests/final_ir_pipeline.mira`
- 修改：`win/tests/run_cli_emit_modes.ps1`
- 镜像：对应 `linux/` 文件

**接口：**
- 产生：`static void finalize_machine_ir(IrBuffer *ir)`
- 产生：`static bool compile_file_to_final_ir(const char *path, MiraCompileUnit *out)`
- 产生：`static void compile_unit_dispose(MiraCompileUnit *unit)`
- 消费：任务 3–5 的 IR、ASM、OBJ、EXE 写出路径。

- [ ] **步骤 1：写共享流水线 RED**

使用会触发 late IR 优化的固定程序，并分别执行：

```powershell
& $Mira --emit=ir final_ir_pipeline.mira -o from_ir.ir -O3
& $Mira -O3 final_ir_pipeline.mira
```

测试要求 `from_ir.ir` 不再包含 late pass 应删除的 `add reg, 0`，并含 O3 调度后
的稳定模式。当前编译器不认识 `--emit=ir`，预期真实 RED 为非零退出。

- [ ] **步骤 2：提取唯一 finalizer**

从 `compile_file_obj()` 中抽出且只保留一份：

```c
static void finalize_machine_ir(IrBuffer *ir) {
    if (mira_opt_level >= 2) {
        ir_opt_constant_fold(ir);
        ir_opt_strength_reduce(ir);
        ir_opt_const_fold_div(ir);
        ir_opt_redundant_load(ir);
        ir_opt_peephole(ir);
    }
    if (mira_opt_level >= 3)
        ir_opt_ilp_schedule(ir);
}
```

在 `main.c` 中定义并使用同一生命周期对象：

```c
typedef struct {
    IrBuffer *ir;
    char *source;
} MiraCompileUnit;
```

源文件读取、parser、typecheck、`codegen()` 与 `finalize_machine_ir()` 必须共同位于
`compile_file_to_final_ir()` 路径中。它把 `cg->ir` 地址和仍需保活的源码交给
`MiraCompileUnit`；`compile_unit_dispose()` 在产物写出后释放源码和导入路径。
OBJ、EXE、ASM 和 IR 不能各自再调用 finalizer。

- [ ] **步骤 3：接入 `MiraCliOptions` 并实现临时 `--emit=ir` 终点**

`main()` 只调用一次 `mira_cli_parse()`，然后设置：

```c
mira_opt_level = options.opt_level;
apply_target_options(&options);
```

IR 终点先继续调用现有：

```c
FILE *out = fopen(output_path, "w");
if (!out) mira_error_simple(1, "cannot write '%s'", output_path);
ir_dump(final_ir, out);
fclose(out);
```

- [ ] **步骤 4：验证共享流水线 GREEN 与普通编译哈希不变**

先在改动前记录 `regression_phi_inline.mira` O0/O3 的 EXE SHA-256 与 OBJ 的节
内容；改动后重新 clean build 并比较。EXE 哈希必须相同。COFF OBJ 的文件头
偏移 4–7 是现有写出器设置的 `TimeDateStamp`，应将该字段归零后比较，或直接
比较代码、数据和重定位节，不能用未经归一化的原始文件哈希制造假失败。

运行 `run_cli_emit_modes.ps1 -Group pipeline`，预期：

```text
FINAL IR PIPELINE PASS
```

- [ ] **步骤 5：提交**

```text
git add win/main.c linux/main.c win/tests/final_ir_pipeline.mira linux/tests/final_ir_pipeline.mira win/tests/run_cli_emit_modes.ps1 linux/tests/run_cli_emit_modes.ps1
git commit -m "refactor(cli): share finalized machine IR"
```

---

### 任务 3：迁移内部 IR 接口和既有测试

**文件：**
- 修改：`win/codegen/ir_dump.c`
- 修改：`win/codegen/ir.h`
- 修改：`win/tests/run_hot_loop_codegen.ps1`
- 修改：`win/tests/run_gradual_types.ps1`
- 修改：`win/tests/run_removed_cli_features.ps1`
- 镜像：对应 `linux/` 文件

**接口：**
- 产生：`bool ir_dump(const IrBuffer *ir, FILE *out, IrOpcode *unsupported)`
- 消费：`--emit=ir` 终点。

- [x] **步骤 1：写未知 opcode RED 与迁移测试**

新增 C 测试构造：

```c
IrBuffer ir = {0};
IrInst bad = {0};
bad.IrNode = IR_OPCODE_COUNT;
ir.text = &bad;
ir.text_count = 1;
IrOpcode unsupported = 0;
assert(!ir_dump(&ir, sink, &unsupported));
assert(unsupported == IR_OPCODE_COUNT);
```

现有 `ir_dump()` 会输出注释并成功，预期测试 RED。

- [x] **步骤 2：让 IR dump 明确失败**

将默认分支从：

```c
fprintf(out, "  ; unknown opcode %d\n", inst->IrNode);
```

改为设置 `*unsupported` 并返回 `false`。文本、data、bss 三段统一传播失败；文件
打开和关闭仍由调用方负责。

- [x] **步骤 3：迁移所有内部 IR 调用**

把：

```powershell
& $Mira -S $fixture $irPath -O3
```

统一改成：

```powershell
& $Mira --emit=ir $fixture -o $irPath -O3
```

`run_removed_cli_features.ps1` 不再把 `-S` 当“被保留的旧 IR”；它改为断言
`--emit=ir` 文件以 `;; Mira IR dump` 开头。

- [x] **步骤 4：运行所有 IR 形状测试**

至少运行 hot-loop、gradual SSA、removed-cli、affine、divrem、cross-branch 系列。
预期所有脚本 PASS，且 `rg` 不再找到把 `-S` 输出当 IR 解析的活跃测试。

- [ ] **步骤 5：提交**

```text
git commit -m "test(cli): move IR inspection to explicit emit mode"
```

---

### 任务 4：实现 GNU Intel 汇编写出器

**文件：**
- 新建：`win/codegen/asm_writer.h`
- 新建：`win/codegen/asm_writer.c`
- 新建：`win/tests/asm_writer_test.c`
- 新建：`win/tests/asm_emit_full.mira`
- 修改：`win/tests/run_cli_emit_modes.ps1`
- 修改：`win/Makefile`
- 镜像：对应 `linux/` 文件

**接口：**
- 产生：`bool ir_write_gas_intel(const IrBuffer *ir, FILE *out, IrOpcode *unsupported)`
- 产生：`bool ir_asm_supports_opcode(IrOpcode opcode)`
- 消费：`-S` 与 `--emit=asm` 终点。

- [ ] **步骤 1：写 opcode 完整性和端到端 RED**

`asm_writer_test.c` 遍历：

```c
for (int op = 0; op < IR_OPCODE_COUNT; ++op) {
    if (!ir_asm_supports_opcode((IrOpcode)op)) {
        fprintf(stderr, "unsupported opcode %d\n", op);
        return 1;
    }
}
puts("ASM WRITER OPCODE COVERAGE PASS");
```

端到端测试执行：

```powershell
& $Mira -S asm_emit_full.mira -o short.s -O3
& $Mira --emit=asm asm_emit_full.mira -o long.s -O3
```

要求两文件 SHA-256 相同，并由 `gcc -c short.s -o short.obj` 成功汇编。当前没有
写出器，预期 RED。

- [ ] **步骤 2：实现文件级 GNU Intel 结构**

输出头和段指令固定为：

```asm
.intel_syntax noprefix
.section .data
.section .bss
.section .text
```

映射必须精确使用：

```text
extern X       -> .extern X
global X       -> .globl X
section .data  -> .section .data
db             -> .byte
dq             -> .quad
resq N         -> .zero N * 8
align 32       -> .p2align 5
[rel symbol]   -> [rip + symbol]
qword [addr]   -> QWORD PTR [addr]
byte [addr]    -> BYTE PTR [addr]
```

- [ ] **步骤 3：实现穷尽指令映射**

writer 的 switch 必须覆盖 `IR_MOV_REG_REG` 到 `IR_LEA_IDX` 的每个枚举项。
一对一指令使用 opcode 表返回助记符；特殊项使用专门 formatter。至少明确补齐旧
`ir_dump.c` 缺失的四项：

```c
case IR_IMUL_WIDE_REG:
    fprintf(out, "  imul %s\n", reg_name(inst->src));
    return true;
case IR_SAR_REG_IMM:
    fprintf(out, "  sar %s, %lld\n", reg_name(inst->dst), (long long)inst->imm);
    return true;
case IR_VFMADD132SD:
    fprintf(out, "  vfmadd132sd %s, %s, %s\n",
            reg_name(inst->dst), reg_name(inst->src), reg_name(inst->src2));
    return true;
case IR_LEA_IDX:
    fprintf(out, "  lea %s, [%s + %s]\n",
            reg_name(inst->dst), reg_name(inst->src), reg_name((IrReg)inst->imm));
    return true;
```

任何越界 opcode 返回 `false`，调用方用：

```c
mira_error_simple(1, "cannot emit assembly for opcode %d to '%s'",
                  (int)unsupported, output_path);
```

- [ ] **步骤 4：验证汇编可消费且别名稳定**

运行 opcode C 测试、端到端脚本以及 O0–O3 的整数、f64、字符串、分支、调用、
BSS 和 AVX fixture。预期 `.s` 均能由宿主 `gcc -c` 接受；`-S` 与长选项哈希相同。

- [ ] **步骤 5：提交**

```text
git commit -m "feat(cli): emit GNU Intel assembly"
```

---

### 任务 5：完成 OBJ/EXE、`-o`、帮助与项目文档

**文件：**
- 修改：`win/main.c`
- 修改：`linux/main.c`
- 修改：`win/tests/run_cli_emit_modes.ps1`
- 修改：`linux/tests/run_cli_emit_modes.ps1`
- 修改：`README.md`

**接口：**
- 消费：`MiraCliOptions` 与任务 2 的最终 `IrBuffer`
- 产生：四种模式一致的默认文件名和 `-o` 行为。

- [ ] **步骤 1：写默认名称、`-o` 与诊断 RED**

脚本分别断言：

```text
mira a.mira                       -> a.exe / a
mira a.mira -o custom.exe         -> custom.exe
mira -S a.mira                    -> a.s
mira --emit=ir a.mira             -> a.ir
mira -c a.mira                    -> a.obj / a.o
mira --emit=obj a.mira -o x.obj   -> x.obj
```

同时逐字匹配缺少 `-o` 值、冲突 emit、多个输入和未知选项诊断。

- [ ] **步骤 2：实现四种终点**

主分派必须是单一 switch：

```c
switch (options.emit) {
case MIRA_EMIT_IR:  return write_ir(unit.ir, output_path);
case MIRA_EMIT_ASM: return write_asm(unit.ir, output_path);
case MIRA_EMIT_OBJ: return write_object(unit.ir, output_path);
case MIRA_EMIT_EXE: return write_executable(unit.ir, output_path);
}
```

OBJ 模式不调用 `linker_run()`；EXE 模式继续选择性加入运行时对象。成功消息分别为：

```text
IR written to <path>
Assembly written to <path>
Object written to <path>
Executable written to <path>
```

- [ ] **步骤 3：重写帮助文本并更新 README**

帮助按以下分组输出：

```text
Usage: mira [options] <file.mira>

Output modes:
  -S, --emit=asm     Emit GNU Intel assembly
      --emit=ir      Emit Mira internal IR
  -c, --emit=obj     Emit an object file without linking
  -o <path>          Set output path

Optimization:
  -O0|-O1|-O2|-O3   Set optimization level (default: -O%d)
```

默认值由 `mira_opt_level` 格式化。README 删除“默认 O3”，改成真实默认 O2，并
新增四种输出示例及“普通编译不依赖外部 assembler”的说明。

- [ ] **步骤 4：运行 focused GREEN**

`run_cli_emit_modes.ps1 -Group all` 必须覆盖 aliases、names、errors、pipeline、asm、
obj、exe，最终输出：

```text
CLI EMIT MODES PASS
```

- [ ] **步骤 5：提交**

```text
git commit -m "feat(cli): finish explicit artifact outputs"
```

---

### 任务 6：双平台同步和最终回归

**文件：**
- 修改：`win/tests/run_cli_emit_modes.ps1`
- 修改：`linux/tests/run_cli_emit_modes.ps1`
- 新建：`.superpowers/sdd/cli-emit-modes-report.md`

**接口：**
- 输入：任务 1–5 的完整实现。
- 输出：可复现的验收报告和干净工作树。

- [ ] **步骤 1：执行 Windows clean build 与全 CLI 回归**

运行：

```powershell
mingw32-make -C win clean
mingw32-make -C win mira.exe
powershell -ExecutionPolicy Bypass -File .\win\tests\run_cli_parse.ps1
powershell -ExecutionPolicy Bypass -File .\win\tests\run_cli_emit_modes.ps1 -Group all
```

预期全部 exit 0。

- [ ] **步骤 2：执行语言与优化回归**

至少运行 gradual all、modules O0–O3、float O0–O3、infix、short-circuit、
branch-return、stdlib core/data、hot-loop、divrem、affine、cross-branch。所有既有
golden 输出必须不变。

- [ ] **步骤 3：执行对象、汇编和产物稳定性验证**

验证：

- O0–O3 的 `.s` 都能被宿主 assembler 接受；
- `-c` 目标文件能被现有 linker 模式消费；
- 普通代表程序的运行输出、大小和 EXE SHA 与任务 2 记录一致；COFF OBJ 按任务 2
  的时间戳归一化规则比较；
- 未跟踪或已跟踪目录中没有遗留 `.exe/.obj/.o/.s/.ir` 测试产物。

- [ ] **步骤 4：核对 Windows/Linux 镜像**

对本次所有平台无关文件归一化 CRLF/LF 后做 SHA-256 manifest。预期零差异；
平台相关差异必须在报告中逐项说明。若 WSL 仍因 `E_ACCESSDENIED` 不可用，只能
报告 Linux 源码/语法/宿主编译检查，不能声称完成原生 Linux 运行。

- [ ] **步骤 5：写报告、检查并提交**

报告记录所有命令、exit code、关键 stdout、产物 hash/size、镜像 manifest 和
Linux 环境限制。随后运行：

```text
git diff --check
git status --short
git ls-files | rg '\.(exe|obj|o|s|ir)$'
```

仅报告文件存在预期改动，生成产物跟踪数为 0。提交：

```text
git commit -m "test(cli): verify explicit emit modes"
```
