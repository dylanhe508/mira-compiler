# Makefile for Mira (Linux x86-64 / SysV ABI)
#
# 构建两套产物:
#   mira        —— 编译器+链接器(GCC 编译,目标 SysV ABI)
#   runtime/*.o —— 运行时目标文件(供 mira 链接用户程序)
#
# 里程碑3 之后:运行时含 POSIX 版并发实现(rt_sched/rt_channel/rt_fiber/rt_sync)。
# main.c 的 mod_names 硬编码模块名 rt_sched.o/rt_channel.o/rt_fiber.o/rt_sync.o,
# 因此 POSIX 源文件通过显式规则编译成对应的模块名。

CC      = gcc
# -fno-pie:非 PIC 代码,全局数据引用用 PC32 直接寻址(不产 GOTPCREL,
# 未定义数据符号进 .rela.dyn 由自写链接器处理);PIC 残留(GOTPCREL)
# 链接器也有防御支持。
CFLAGS  = -O2 -g -Wall -fno-pie -D_POSIX_C_SOURCE=200809L -I.
DEPFLAGS = -MMD -MP
# -no-pie:Ubuntu 发行版默认 PIE 链接,必须显式关掉以匹配 -fno-pie 的编译产物
LDFLAGS = -no-pie -lpthread -lm

# === 编译器源文件(平台无关核心 + SysV ABI 后端 + ELF 链接器)===
# 不含:abi_win64.c, coff_writer.c, linker/coff.c, linker/dll_map.c, linker/pe.c
COMPILER_SRCS = \
	main.c lexer.c parser.c typecheck.c codegen.c hash.c memory.c error.c \
	codegen/decision.c codegen/stdlib_builtins.c codegen/ir.c codegen/ir_opt.c \
	codegen/ir_cfg_cleanup.c codegen/ir_dump.c \
	codegen/encoder.c codegen/elf_writer.c codegen/abi_sysv.c \
	codegen/ssa_builder.c codegen/ssa_function_index.c codegen/ssa_lower.c codegen/ssa_dom.c \
	codegen/ssa_opt.c codegen/ssa_affine.c codegen/ssa_module_facts.c codegen/ssa_vm_profile.c \
	codegen/ssa_ref.c codegen/ssa_ref_vm.c \
	codegen/ssa_mem2reg.c codegen/ssa_regalloc.c \
	codegen/target.c \
	linker/linker.c linker/symbols.c linker/elf.c linker/elfout.c

COMPILER_OBJS = $(COMPILER_SRCS:.c=.o)
COMPILER_DEPS = $(COMPILER_OBJS:.o=.d)

# === 运行时目标文件 ===
# 平台无关核心 + POSIX 实现。rt_fiber.o 由 C 实现与汇编切换器合并而成,
# 其余 POSIX 模块通过显式规则映射到 main.c mod_names 硬编码的模块名。
RUNTIME_OBJS = \
	runtime/rt_core.o runtime/rt_print.o runtime/rt_mem.o \
	runtime/rt_string.o runtime/rt_math.o runtime/rt_collection.o \
	runtime/rt_file.o runtime/rt_io.o runtime/rt_struct.o \
	runtime/rt_debug.o runtime/rt_time.o runtime/rt_error.o \
	runtime/rt_sync.o runtime/rt_sched.o runtime/rt_channel.o \
	runtime/rt_fiber.o
RUNTIME_DEPS = $(RUNTIME_OBJS:.o=.d) \
	runtime/rt_fiber_posix.d runtime/rt_fiber_x86_64.d
DEPS = $(COMPILER_DEPS) $(RUNTIME_DEPS)

.PHONY: all clean runtime

# 注意:make 的默认目标是第一个目标,codegen.o 的显式规则必须放在
# all 之后(见文件末尾),否则 make 无参数时只构建 codegen.o。
all: mira runtime

# 编译器
mira: $(COMPILER_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# 运行时目标文件
runtime: $(RUNTIME_OBJS)

# 通用编译规则
%.o: %.c
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

%.o: %.S
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

# POSIX 并发运行时:源文件名与模块名不同(main.c mod_names 硬编码),
# 显式命名规则把 *_posix.c 编成对应模块名。
runtime/rt_sync.o: runtime/rt_sync_posix.c
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

runtime/rt_sched.o: runtime/rt_sched_posix.c
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

runtime/rt_channel.o: runtime/rt_channel_posix.c
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

# fiber:POSIX 实现(C)+ x86-64 协程切换(汇编)合并成一个模块
runtime/rt_fiber.o: runtime/rt_fiber_posix.c runtime/rt_fiber_x86_64.S
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o runtime/rt_fiber_posix.o runtime/rt_fiber_posix.c
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o runtime/rt_fiber_x86_64.o runtime/rt_fiber_x86_64.S
	$(CC) $(CFLAGS) -r -o $@ runtime/rt_fiber_posix.o runtime/rt_fiber_x86_64.o

# codegen.c 只是壳:实际代码由 codegen/index.c 以 #include 聚合
# (state/emit/literal/block/list_literal/loops/words + 末尾 program.c),
# words.c 再聚合 11 个 words_*.c 子模块。
# 隐式规则 %.o:%.c 只跟踪 codegen.c,不追踪这些隐藏依赖——改 program.c
# 或任意 words_*.c 不重编 codegen.o,必须显式声明依赖。此规则必须位于
# all 之后(见上)。
CODEGEN_DEPS = codegen/index.c codegen/state.c codegen/emit.c \
	codegen/literal.c codegen/block.c codegen/list_literal.c \
	codegen/loops.c codegen/words.c codegen/program.c codegen/codegen.h \
	codegen/words_arith.c codegen/words_async.c codegen/words_compare.c \
	codegen/words_data.c codegen/words_debug.c codegen/words_file.c \
	codegen/words_io.c codegen/words_math.c codegen/words_memory.c \
	codegen/words_string.c codegen/words_winapi.c codegen/stdlib_builtins.h typecheck.h

codegen.o: codegen.c $(CODEGEN_DEPS)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ codegen.c

codegen/ssa_builder.o: codegen/ssa_builder.c codegen/stdlib_builtins.h typecheck.h
codegen/ssa_ref.o: codegen/ssa_ref.c codegen/stdlib_builtins.h
codegen/stdlib_builtins.o: codegen/stdlib_builtins.c codegen/stdlib_builtins.h

# parser.c 同样是壳:parser/index.c 以 #include 聚合
# (helpers/infix/parse_one/blocks + parser.h)。同理由显式规则声明依赖。
PARSER_DEPS = parser/index.c parser/modules.c parser/helpers.c parser/infix.c \
	parser/parse_one.c parser/blocks.c parser/parser.h typecheck.h

parser.o: parser.c $(PARSER_DEPS)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ parser.c

clean:
	rm -f $(COMPILER_OBJS) $(RUNTIME_OBJS) $(DEPS) mira \
		runtime/rt_fiber_posix.o runtime/rt_fiber_x86_64.o

-include $(DEPS)
