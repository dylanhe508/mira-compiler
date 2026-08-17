CC = gcc
CFLAGS = -O2 -I.
# -mno-stack-arg-probe: TDM-GCC 大栈帧会引用 ___chkstk_ms,
# 自写链接器不认识该符号(8-8 已踩坑),必须禁用栈探测。
RUNTIME_CFLAGS = -O2 -I. -D__USE_MINGW_ANSI_STDIO=0 -D_WIN32_WINNT=0x0600 -mno-stack-arg-probe

COMPILER_SRCS = \
	main.c cli.c lexer.c parser.c typecheck.c codegen.c hash.c memory.c error.c \
	codegen/decision.c codegen/stdlib_builtins.c codegen/ir.c codegen/ir_opt.c codegen/ir_cfg_cleanup.c \
	codegen/ir_dump.c codegen/encoder.c codegen/coff_writer.c codegen/elf_writer.c \
	codegen/abi_win64.c codegen/ssa_builder.c codegen/ssa_function_index.c \
	codegen/ssa_lower.c codegen/ssa_dom.c codegen/ssa_opt.c codegen/ssa_affine.c \
	codegen/ssa_module_facts.c codegen/ssa_vm_profile.c codegen/ssa_ref.c \
	codegen/ssa_ref_vm.c codegen/ssa_mem2reg.c codegen/ssa_regalloc.c \
	codegen/target.c linker/coff.c linker/dll_map.c linker/linker.c linker/pe.c \
	linker/elf.c linker/elfout.c linker/symbols.c

RUNTIME_NAMES = rt_core rt_print rt_time rt_mem rt_string rt_collection \
	rt_file rt_math rt_error rt_debug rt_io rt_struct rt_win rt_sched rt_channel
RUNTIME_OBJS = $(addprefix runtime/,$(addsuffix .obj,$(RUNTIME_NAMES))) \
	runtime/rt_chkstk_x86_64.obj

.PHONY: all runtime clean

# parser.c / codegen.c 只是壳:实际代码由 index.c 以 #include 聚合
# (parser: helpers/infix/parse_one/blocks + parser.h;
#  codegen: state/emit/literal/block/list_literal/loops/words/program,
#  words 再聚合 11 个 words_*.c)。只跟踪壳本身时改子文件不重编,
# 必须显式追加依赖。
PARSER_DEPS = parser/index.c parser/modules.c parser/helpers.c parser/infix.c \
	parser/parse_one.c parser/blocks.c parser/parser.h typecheck.h
CODEGEN_DEPS = codegen/index.c codegen/state.c codegen/emit.c \
	codegen/literal.c codegen/block.c codegen/list_literal.c \
	codegen/loops.c codegen/words.c codegen/program.c codegen/codegen.h \
	codegen/words_arith.c codegen/words_async.c codegen/words_compare.c \
	codegen/words_data.c codegen/words_debug.c codegen/words_file.c \
	codegen/words_io.c codegen/words_math.c codegen/words_memory.c \
	codegen/words_string.c codegen/words_winapi.c codegen/stdlib_builtins.h typecheck.h

all: mira.exe runtime

mira.exe: $(COMPILER_SRCS) $(PARSER_DEPS) $(CODEGEN_DEPS)
	$(CC) $(CFLAGS) $(COMPILER_SRCS) -ldbghelp -o $@

runtime: $(RUNTIME_OBJS)

runtime/%.obj: runtime/%.c
	$(CC) $(RUNTIME_CFLAGS) -c $< -o $@

runtime/rt_chkstk_x86_64.obj: runtime/rt_chkstk_x86_64.S
	$(CC) $(RUNTIME_CFLAGS) -c $< -o $@

clean:
	del /Q mira.exe runtime\*.obj 2>NUL || exit 0
