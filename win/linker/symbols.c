/*
 * symbols.c ??符号管理、收集和 DLL 导入解析（哈希表版）
 */
#include "linker.h"
#include "../hash.h"
#include "elfdefs.h"

void ctx_init(LinkerCtx *ctx) {
	memset(ctx, 0, sizeof(*ctx));
	ctx->sym_cap = 512;
	ctx->symbols = calloc(ctx->sym_cap, sizeof(GlobalSymbol));
	ctx->import_cap = 128;
	ctx->imports = calloc(ctx->import_cap, sizeof(ImportEntry));
	ht_init(&ctx->sym_ht, 256);
	ht_init(&ctx->import_ht, 128);
}

GlobalSymbol *ctx_find_sym(LinkerCtx *ctx, const char *name) {
	/* A Mira object owns the process entry.  Its mainCRTStartup calls
	 * mira_main and then ExitProcess; selecting either the runtime C main or
	 * mira_main directly leaves a bare RET as the process termination path.
	 * (仅 Windows/COFF 路径需要此特判;ELF 用 _start 入口,不走此分支) */
#ifdef _WIN32
	if (strcmp(name, "main") == 0) {
		GlobalSymbol *mira_entry = (GlobalSymbol *)ht_get(&ctx->sym_ht, "mainCRTStartup");
		if (mira_entry && mira_entry->is_defined) return mira_entry;
	}
#endif
	return (GlobalSymbol *)ht_get(&ctx->sym_ht, name);
}

GlobalSymbol *ctx_add_sym(LinkerCtx *ctx, const char *name,
                          int obj_idx, int sec_idx,
                          uint32_t value, int is_defined) {
	GlobalSymbol *existing = ctx_find_sym(ctx, name);
	if (existing) {
		if (is_defined && !existing->is_defined) {
			existing->obj_idx = obj_idx;
			existing->sec_idx = sec_idx;
			existing->value = value;
			existing->is_defined = 1;
		}
		return existing;
	}
	if (ctx->sym_count >= ctx->sym_cap) {
		ctx->sym_cap *= 2;
		ctx->symbols = realloc(ctx->symbols, ctx->sym_cap * sizeof(GlobalSymbol));
		/* 閲嶅缓鍝堝笇琛紙鎸囬拡鍙兘鍙樹簡�?*/
		ht_free(&ctx->sym_ht);
		ht_init(&ctx->sym_ht, ctx->sym_cap);
		for (int i = 0; i < ctx->sym_count; i++)
			ht_set(&ctx->sym_ht, ctx->symbols[i].name, &ctx->symbols[i]);
	}
	GlobalSymbol *s = &ctx->symbols[ctx->sym_count++];
	s->name = strdup(name);
	s->obj_idx = obj_idx;
	s->sec_idx = sec_idx;
	s->value = value;
	s->is_defined = is_defined;
	s->is_referenced = 0;
	s->is_dynamic = 0;
	s->rva = 0;
	ht_set(&ctx->sym_ht, name, s);
	return s;
}

void ctx_add_import(LinkerCtx *ctx, const char *name, const char *dll) {
	if (ht_get(&ctx->import_ht, name)) return; /* 宸插瓨鍦?*/

	if (ctx->import_count >= ctx->import_cap) {
		ctx->import_cap *= 2;
		ctx->imports = realloc(ctx->imports, ctx->import_cap * sizeof(ImportEntry));
	}
	ImportEntry *e = &ctx->imports[ctx->import_count++];
	e->name = strdup(name);
	e->dll_name = strdup(dll);
	e->iat_rva = 0;
	ht_set(&ctx->import_ht, name, e);
}

#ifdef _WIN32
void collect_symbols(LinkerCtx *ctx, int obj_idx) {
	ObjFile *obj = &ctx->objs[obj_idx];
	unsigned char *referenced = calloc(obj->hdr->NumberOfSymbols ? obj->hdr->NumberOfSymbols : 1, 1);
	for (uint16_t si = 0; si < obj->hdr->NumberOfSections; ++si) {
		CoffSectionHeader *sh = &obj->sections[si];
		if (!sh->PointerToRelocations || !sh->NumberOfRelocations) continue;
		if ((size_t)sh->PointerToRelocations +
		    (size_t)sh->NumberOfRelocations * sizeof(CoffRelocation) > obj->size) continue;
		CoffRelocation *relocs = (CoffRelocation *)(obj->data + sh->PointerToRelocations);
		for (uint16_t ri = 0; ri < sh->NumberOfRelocations; ++ri)
			if (relocs[ri].SymbolTableIndex < obj->hdr->NumberOfSymbols)
				referenced[relocs[ri].SymbolTableIndex] = 1;
	}
	for (uint32_t i = 0; i < obj->hdr->NumberOfSymbols; i++) {
		CoffSymbol *sym = &obj->symbols[i];
		const char *name = coff_sym_name(obj, sym);

		if (sym->StorageClass == 3 && sym->SectionNumber > 0 &&
		    sym->Value == 0 && sym->NumberOfAuxSymbols > 0) {
			i += sym->NumberOfAuxSymbols;
			continue;
		}
		if (sym->StorageClass == 103) {
			i += sym->NumberOfAuxSymbols;
			continue;
		}
		if (sym->SectionNumber == -1) {
			i += sym->NumberOfAuxSymbols;
			continue;
		}

		int is_def = (sym->SectionNumber > 0);
		if (sym->StorageClass == 2 && sym->SectionNumber == 0) is_def = 0;
		if (sym->StorageClass == 105) is_def = 0;

		if (strlen(name) > 0 && (name[0] != '.' || strncmp(name, ".refptr.", 7) == 0 ||
		                         strncmp(name, ".weak.", 6) == 0)) {
			GlobalSymbol *global = ctx_add_sym(ctx, name, obj_idx, sym->SectionNumber, sym->Value, is_def);
			if (referenced[i]) global->is_referenced = 1;
		}
		i += sym->NumberOfAuxSymbols;
	}
	free(referenced);
}
#endif /* _WIN32 — collect_symbols(COFF 版) */

/* collect_symbols_elf — ELF 版符号收集,平行于 collect_symbols。
 *
 * ELF 符号表更简单(无 StorageClass/AuxSymbols):
 *   - st_shndx > 0(非 SHN_UNDEF/COMMON/ABS)→ 已定义
 *   - st_shndx == SHN_UNDEF → 外部引用(未定义)
 *   - st_shndx == SHN_COMMON → 待分配(里程碑1 暂不支持,当作未定义报错)
 *   - STB_LOCAL 符号跳过(只关心全局/弱符号)
 *
 * GlobalSymbol.sec_idx 对 ELF 存段头索引(与 COFF 的 1-based 一致,
 * 因为 ELF 段索引 0 是 NULL,实际数据段从 1 开始)。 */
void collect_symbols_elf(LinkerCtx *ctx, int obj_idx) {
	ObjFile *obj = &ctx->objs[obj_idx];
	if (obj->fmt != OBJ_FMT_ELF || obj->elf_symtab_idx < 0) return;

	const Elf64_Shdr *shdrs = (const Elf64_Shdr *)obj->elf_shdrs;
	const Elf64_Shdr *symtab_sh = &shdrs[obj->elf_symtab_idx];
	const Elf64_Sym *syms = (const Elf64_Sym *)(obj->data +
		elf_get_u64((const uint8_t *)&symtab_sh->sh_offset));
	uint32_t sym_count = (uint32_t)(elf_get_u64((const uint8_t *)&symtab_sh->sh_size) /
	                                ELF64_SYM_SIZE);

	/* 计算引用位图:遍历所有 .rela 段,记录被引用的符号索引 */
	unsigned char *referenced = calloc(sym_count ? sym_count : 1, 1);
	for (int si = 0; si < obj->elf_shnum; si++) {
		if (shdrs[si].sh_type != SHT_RELA) continue;
		const Elf64_Rela *relas = (const Elf64_Rela *)(obj->data +
			elf_get_u64((const uint8_t *)&shdrs[si].sh_offset));
		uint64_t rc = elf_get_u64((const uint8_t *)&shdrs[si].sh_size) / ELF64_RELA_SIZE;
		for (uint64_t r = 0; r < rc; r++) {
			uint32_t sidx = ELF64_R_SYM(elf_get_u64((const uint8_t *)&relas[r].r_info));
			if (sidx < sym_count) referenced[sidx] = 1;
		}
	}

	for (uint32_t i = 1; i < sym_count; i++) {  /* 跳过 [0] STN_UNDEF */
		uint8_t info = syms[i].st_info;
		uint8_t bind = ELF64_ST_BIND(info);
		uint16_t shndx = elf_get_u16((const uint8_t *)&syms[i].st_shndx);
		uint64_t value = elf_get_u64((const uint8_t *)&syms[i].st_value);

		/* 只收集全局和弱符号。局部符号对链接不可见。 */
		if (bind == STB_LOCAL) continue;

		const char *name = elf_sym_name(obj, (int)i);
		if (!name || !name[0]) continue;

		/* 跳过段类型符号(STT_SECTION) */
		uint8_t type = ELF64_ST_TYPE(info);
		if (type == STT_SECTION) continue;

		int is_def = (shndx != SHN_UNDEF && shndx < SHN_ABS);
		if (shndx == SHN_COMMON) is_def = 0;  /* COMMON 当未定义处理 */

		GlobalSymbol *gs = ctx_add_sym(ctx, name, obj_idx, shndx, (uint32_t)value, is_def);
		if (referenced[i]) gs->is_referenced = 1;
	}
	free(referenced);
}

/* resolve_imports_elf — ELF 版导入解析。
 *
 * 动态链接策略(自写链接器,链接过程零外部工具调用):
 *   未定义且被引用的符号不报错,标记 is_dynamic = 1。
 *   write_elf 为每个动态符号生成 PLT 入口 + .got.plt 槽,
 *   输出 .dynamic 段(DT_NEEDED = libc.so.6 / libm.so.6 / libpthread.so.0),
 *   由系统 ld.so 在进程启动时解析(DF_BIND_NOW 非懒解析)。
 *
 *   若符号在动态库中不存在,ld.so 启动时立即报错——与 Windows 上
 *   DLL 缺少导出类似,链接器不维护 libc 符号白名单。
 *
 *   唯一例外:程序入口 mira_main。它由 Mira 程序自身生成(program.c
 *   的入口序列 call mira_main),绝不可能来自动态库。空 main(函数体
 *   为空被 SSA 剪掉)时该符号缺失——与 Windows 原版一致,必须在
 *   编译期报「unresolved symbol」,而不是生成 PLT 让 ld.so 在进程
 *   启动时才失败(那会变成运行期段错误)。
 *
 *   另一类例外:非法 C 标识符名(如 '&&'、'-a')。它们只可能来自
 *   parser 对错误写法的解析(短路与/负号被当成标识符),任何动态库
 *   都不会导出这种名字。Windows 侧(COFF 链接器)对这类符号一律
 *   编译期报 unresolved;ELF 版若标动态,ld.so 要到进程启动才报
 *   symbol lookup error,行为漂移。因此对非法标识符同样编译期报错。 */
static int is_c_ident(const char *name) {
	if (!name || !name[0]) return 0;
	char c = name[0];
	if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_'))
		return 0;
	for (const char *p = name + 1; *p; p++) {
		c = *p;
		if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
		      (c >= '0' && c <= '9') || c == '_'))
			return 0;
	}
	return 1;
}

int resolve_imports_elf(LinkerCtx *ctx) {
	for (int i = 0; i < ctx->sym_count; i++) {
		GlobalSymbol *s = &ctx->symbols[i];
		if (s->is_defined || !s->is_referenced) continue;
		if (strcmp(s->name, "mira_main") == 0 ||
		    !is_c_ident(s->name)) {
			fprintf(stderr,
				"error: linker: unresolved symbol '%s'\n"
				"  This symbol is used but never defined.\n"
				"  If it is a variable, declare it with 'var'.\n"
				"  If it is a DLL function, use 'import-ext'.\n\n",
				s->name);
			return 0;
		}
		s->is_dynamic = 1;
	}
	return 1;
}

#ifdef _WIN32
int resolve_imports(LinkerCtx *ctx) {
	int unresolved = 0;
	for (int i = 0; i < ctx->sym_count; i++) {
		GlobalSymbol *s = &ctx->symbols[i];
		if (s->is_defined || !s->is_referenced) continue;

		const char *dll = find_dll_for(s->name);
		if (dll) {
			const char *func = import_func_name(s->name);
			ctx_add_import(ctx, func, dll);
		} else if (strncmp(s->name, "__imp_", 6) == 0) {
			const char *bare = s->name + 6;
			dll = find_dll_for(bare);
			if (dll) {
				ctx_add_import(ctx, import_func_name(s->name), dll);
			} else {
				const char *display = bare;
				mira_error_simple(2, "undefined symbol '%s'\n  Not a known function, variable, or DLL export.\n  Check spelling, or use 'import-ext' to load the required DLL.", display);
				unresolved++;
			}
		} else if (strncmp(s->name, "__intrinsic_", 12) == 0) {
			if (strcmp(s->name, "__intrinsic_setjmpex") == 0) {
				ctx_add_import(ctx, "_setjmpex", "msvcrt.dll");
			} else {
				mira_error_simple(2, "unresolved intrinsic '%s'", s->name);
				unresolved++;
			}
		}
	}
	return unresolved == 0;
}
#endif /* _WIN32 — resolve_imports(COFF 版) */
