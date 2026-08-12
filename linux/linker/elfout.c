/*
 * elfout.c — ELF 动态链接可执行文件输出(ET_EXEC,non-PIE)。
 *
 * 平行于 pe.c 的 write_pe,实现 ELF 链接器后端。链接过程零外部工具
 * 调用(不依赖 gcc/ld/llvm):未定义符号标记为动态符号,由本文件生成
 * PLT/GOT/.dynamic,系统 ld.so 在进程启动时解析(DF_BIND_NOW 非懒
 * 解析,运行时 PLT 直接命中真实地址,零动态开销)。
 *
 * 动态链接结构:
 *   - DT_NEEDED = libc.so.6 / libm.so.6 / libpthread.so.0(glibc 2.34+
 *     的 pthread 符号已在 libc 中,libpthread 仅为兼容老版本)
 *   - 每个动态符号:16 字节 PLT 入口 + 1 个 .got.plt 槽 + 1 条
 *     R_X86_64_JUMP_SLOT 重定位
 *   - 数据段对动态符号的 64 位绝对引用:分配 .got 槽 + R_X86_64_GLOB_DAT
 *   - .gnu.hash:ld.so 查找动态符号所需(glibc 2.36+ 强制要求)
 *   - PT_INTERP = "/lib64/ld-linux-x86-64.so.2"
 *
 * 文件布局(non-PIE,vaddr 0x400000 起):
 *   PT_LOAD 1 (R+X): ehdr + phdrs + .interp + .text + .rodata + .plt
 *                    + .dynsym/.dynstr/.gnu.hash/.rela.plt/.rela.dyn
 *                    (动态元数据段 ALLOC,ld.so 经 DT_* 虚拟地址访问)
 *   PT_LOAD 2 (R+W): .data + .bss + .got + .got.plt + .dynamic
 *   尾部(非 ALLOC): .symtab/.strtab/.shstrtab + section header table
 *
 * 段合并复用 pe.c 的 get_base_section / find_merged 逻辑,
 * 但段名白名单改为 ELF 风格(.text/.data/.bss/.rodata)。
 */
#include "linker.h"
#include "elfdefs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <sys/stat.h>
#endif

/* 基地址:non-PIE ELF 可执行文件的传统加载地址 */
#define ELF_BASE_ADDR    0x400000
/* 页大小(x86-64 = 4KB),program header 对齐 */
#define ELF_PAGE_ALIGN   0x1000
#define ELF_EMIT_SECTION_METADATA 0
/* 动态链接器路径 */
#define ELF_INTERP       "/lib64/ld-linux-x86-64.so.2"

/* 依赖的动态库(DT_NEEDED,顺序与 .dynstr 中一致) */
static const char *const DT_NEEDED_LIBS[] = {
	"libc.so.6", "libm.so.6", "libpthread.so.0"
};
#define DT_NEEDED_LIB_COUNT 3

/* 64 位 align_up */
static uint64_t align64(uint64_t v, uint64_t a) {
	return (v + a - 1) & ~(a - 1);
}

/* 在 Linux 上给输出文件加可执行权限 */
static void chmod_exec(const char *path) {
#ifndef _WIN32
	chmod(path, 0755);
#else
	(void)path;
#endif
}

/* GNU hash 函数(ld.so 的 dl_new_hash) */
static uint32_t elf_gnu_hash(const char *s) {
	uint32_t h = 5381;
	for (; *s; s++) h = h * 33 + (unsigned char)*s;
	return h;
}

/* 小质数表:取 >= n 的最小质数作为 .gnu.hash 桶数 */
static const uint32_t PRIMES[] = {
	1,2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71,73,79,83,89,97,
	101,103,107,109,113,127,131,137,139,149,151,157,163,167,173,179,181,191,
	193,197,199,211,223,227,229,233,239,241,251,257,263,269,271,277,281,283,
	293,307,311,313,317,331,337,347,349,353,359,367,373,379,383,389,397,401,
	409,419,421,431,433,439,443,449,457,461,463,467,479,487,491,499,503,509,
	521,523,541,547,557,563,569,571,577,587,593,599,601,607,613,617,619,631
};
static uint32_t next_prime(uint32_t n) {
	for (size_t i = 0; i < sizeof(PRIMES) / sizeof(PRIMES[0]); i++)
		if (PRIMES[i] >= n) return PRIMES[i];
	return n;
}

/* ====== ELF Obj 段迭代辅助 ====== */

static int elf_sec_count(const ObjFile *obj) {
	return obj->elf_shnum;
}

static const char *elf_sec_base_name(const ObjFile *obj, int si) {
	return elf_section_name(obj, si);
}

/* 返回第 si 个段的原始数据指针与大小。NOBITS 段返回 NULL。 */
static const uint8_t *elf_sec_data(const ObjFile *obj, int si, uint64_t *out_size) {
	const Elf64_Shdr *shdrs = (const Elf64_Shdr *)obj->elf_shdrs;
	if (si < 0 || si >= obj->elf_shnum) { *out_size = 0; return NULL; }
	if (shdrs[si].sh_type == SHT_NOBITS) { *out_size = 0; return NULL; }
	uint64_t off = elf_get_u64((const uint8_t *)&shdrs[si].sh_offset);
	*out_size = elf_get_u64((const uint8_t *)&shdrs[si].sh_size);
	return obj->data + off;
}

static int elf_sec_is_alloc(const ObjFile *obj, int si) {
	const Elf64_Shdr *shdrs = (const Elf64_Shdr *)obj->elf_shdrs;
	if (si < 0 || si >= obj->elf_shnum) return 0;
	return (elf_get_u64((const uint8_t *)&shdrs[si].sh_flags) & SHF_ALLOC) != 0;
}

static int elf_sec_is_nobits(const ObjFile *obj, int si) {
	const Elf64_Shdr *shdrs = (const Elf64_Shdr *)obj->elf_shdrs;
	if (si < 0 || si >= obj->elf_shnum) return 0;
	return shdrs[si].sh_type == SHT_NOBITS;
}

/* 读取 obj 符号表中第 symidx 个符号的属性。成功返回 1。
 * GCC 生成的 .o 里 static 函数/字符串常量是 STB_LOCAL,不进全局
 * 符号表,重定位需按段直接解析(见 write_elf 步骤 13)。 */
static int elf_sym_attrs(const ObjFile *obj, int symidx, uint8_t *bind,
                         uint8_t *type, uint16_t *shndx, uint64_t *value) {
	if (!obj || obj->fmt != OBJ_FMT_ELF || obj->elf_symtab_idx < 0) return 0;
	const Elf64_Shdr *shdrs = (const Elf64_Shdr *)obj->elf_shdrs;
	const Elf64_Shdr *symtab_sh = &shdrs[obj->elf_symtab_idx];
	const Elf64_Sym *syms = (const Elf64_Sym *)(obj->data +
		elf_get_u64((const uint8_t *)&symtab_sh->sh_offset));
	uint64_t sym_count = elf_get_u64((const uint8_t *)&symtab_sh->sh_size) /
	                     ELF64_SYM_SIZE;
	if ((uint64_t)symidx >= sym_count) return 0;
	const Elf64_Sym *sym = &syms[symidx];
	*bind = ELF64_ST_BIND(sym->st_info);
	*type = ELF64_ST_TYPE(sym->st_info);
	*shndx = elf_get_u16((const uint8_t *)&sym->st_shndx);
	*value = elf_get_u64((const uint8_t *)&sym->st_value);
	return 1;
}

/* ====== 合并段(piece 记录来源 obj+sec) ======
 * piece.sec_idx 对 ELF 存的是段头索引(0-based)。
 */

static void elf_add_piece(MergedSection *ms, int obj_idx, int sec_idx,
                          uint32_t size, uint32_t *out_offset) {
	if (ms->piece_count >= ms->piece_cap) {
		ms->piece_cap = ms->piece_cap ? ms->piece_cap * 2 : 32;
		ms->pieces = realloc(ms->pieces, ms->piece_cap * sizeof(*ms->pieces));
	}
	int pi = ms->piece_count++;
	ms->pieces[pi].obj_idx = obj_idx;
	ms->pieces[pi].sec_idx = sec_idx;
	ms->pieces[pi].offset = ms->size;
	ms->pieces[pi].size = size;
	*out_offset = ms->pieces[pi].offset;
	ms->size += size;
	ms->size = (uint32_t)align64(ms->size, 16);
}

/* ====== 动态符号表 ====== */
typedef struct {
	GlobalSymbol *gs;    /* 指向 ctx->symbols[] 中的未定义符号 */
	uint32_t name_off;   /* .dynstr 中名字偏移 */
	int      globdat;    /* 被 64 位绝对引用 → 需要 .got 槽 + GLOB_DAT */
	uint64_t got_slot;   /* .got 槽地址(globdat 时有效) */
	int      copy_reloc; /* 数据符号(non-PIE PC32 引用):copy relocation,
	                      * 引用静态指向 .bss 副本,ld.so 启动时 COPY 填入 */
	uint64_t copy_slot;  /* .bss 副本槽地址(copy_reloc 时有效) */
} DynSym;

/* 在 dyn 表中找符号,返回索引或 -1 */
static int dyn_find(const DynSym *dyn, int dyn_count, const GlobalSymbol *gs) {
	for (int i = 0; i < dyn_count; i++)
		if (dyn[i].gs == gs) return i;
	return -1;
}

/* 判断 .o 是否由 GCC 生成。GCC 必有 .note.GNU-stack/.comment/.eh_frame
 * 段;Mira 的 elf_writer 只产 .text/.data/.bss 与 .rela 及符号表段。
 * 两者重定位风格不同:
 *   GCC:addend 含 -4 修正(相对重定位位置 P,psABI 公式 S+A-P)
 *   Mira:addend=0,encoder 把 -4 修正预编码进 patch(相对 P+4)
 * 差 4,链接器按 obj 来源选择公式。 */
static int obj_is_gcc(const ObjFile *obj) {
	if (!obj || obj->fmt != OBJ_FMT_ELF) return 0;
	const Elf64_Shdr *shdrs = (const Elf64_Shdr *)obj->elf_shdrs;
	for (int i = 1; i < obj->elf_shnum; i++) {
		uint32_t type = elf_get_u32((const uint8_t *)&shdrs[i].sh_type);
		if (type == SHT_NOTE) return 1;
		const char *name = elf_section_name(obj, i);
		if (name && (strcmp(name, ".comment") == 0 ||
		             strcmp(name, ".eh_frame") == 0))
			return 1;
	}
	return 0;
}

/* GCC .o 中引用动态数据符号的 PC32 重定位(non-PIE 下 GCC 对
 * stdout/stderr 等未定义数据生成 PC32)。libc 加载在 0x7f... 高位,
 * 32 位相对偏移必然溢出,不能进 .rela.dyn 由 ld.so 补;
 * 标准做法是 copy relocation(GNU ld 同款):链接器在 .bss 分配
 * 副本槽,引用静态指向副本(近地址不溢出),.rela.dyn 生成
 * R_X86_64_COPY,ld.so 启动时把库符号值拷进副本。 */


/* ====== 主链接函数 ====== */
int write_elf(LinkerCtx *ctx, const char *out_path) {
	/* -----------------------------------------------------------
	 * 步骤 1:合并段。把所有 obj 的 SHF_ALLOC 段按 base name 归并。
	 * ----------------------------------------------------------- */
	MergedSection merged[16];
	int merged_count = 0;
	memset(merged, 0, sizeof(merged));

	for (int oi = 0; oi < ctx->obj_count; oi++) {
		ObjFile *obj = &ctx->objs[oi];
		if (obj->fmt != OBJ_FMT_ELF) continue;
		int nsec = elf_sec_count(obj);
		for (int si = 1; si < nsec; si++) {
			if (!elf_sec_is_alloc(obj, si)) continue;
			const char *sname = elf_sec_base_name(obj, si);
			char base[32] = {0};
			get_base_section(sname, base, sizeof(base));
			/* 白名单 */
			if (strcmp(base, ".text") != 0 &&
			    strcmp(base, ".data") != 0 &&
			    strcmp(base, ".bss") != 0 &&
			    strcmp(base, ".rodata") != 0)
				continue;

			uint64_t sec_size = 0;
			elf_sec_data(obj, si, &sec_size);
			if (elf_sec_is_nobits(obj, si)) {
				const Elf64_Shdr *shdrs = (const Elf64_Shdr *)obj->elf_shdrs;
				sec_size = elf_get_u64((const uint8_t *)&shdrs[si].sh_size);
			}
			if (sec_size == 0) continue;

			MergedSection *ms = find_merged(merged, merged_count, base);
			if (!ms) {
				if (merged_count >= 16) {
					mira_error_simple(2, "linker: 段数超过上限 16");
					return 0;
				}
				ms = &merged[merged_count++];
				strncpy(ms->name, base, 7);
				ms->name[7] = '\0';
			}
			/* 按 ELF 段的 sh_addralign 对齐 piece 起点(GCC 的 .data/.bss
			 * 可能含 16/32/64 字节对齐变量,顺序拼接会让 movaps 等
			 * 指令 #GP;上限 4096 = 页)。GCC 符号 st_value 是相对本段
			 * 起点的偏移,段起点对齐后符号地址 = 段基址 + 偏移 自动对齐。 */
			const Elf64_Shdr *shs = (const Elf64_Shdr *)obj->elf_shdrs;
			uint64_t addralign = elf_get_u64((const uint8_t *)&shs[si].sh_addralign);
			if (addralign == 0) addralign = 1;
			if (addralign > 4096) addralign = 4096;
			if (addralign > ms->max_align) ms->max_align = (uint32_t)addralign;
			ms->size = (uint32_t)align64(ms->size, addralign);
			uint32_t off;
			elf_add_piece(ms, oi, si, (uint32_t)sec_size, &off);
		}
	}

	/* 复制段数据到合并缓冲区 */
	for (int i = 0; i < merged_count; i++) {
		merged[i].data = calloc(1, merged[i].size ? merged[i].size : 1);
		for (int j = 0; j < merged[i].piece_count; j++) {
			int oi = merged[i].pieces[j].obj_idx;
			int si = merged[i].pieces[j].sec_idx;
			ObjFile *obj = &ctx->objs[oi];
			uint64_t sz = 0;
			const uint8_t *src = elf_sec_data(obj, si, &sz);
			if (sz > 0 && src)
				memcpy(merged[i].data + merged[i].pieces[j].offset, src, sz);
		}
	}

	/* -----------------------------------------------------------
	 * 步骤 2:收集动态符号(resolve_imports_elf 已标记 is_dynamic)。
	 * ----------------------------------------------------------- */
	DynSym *dyn = calloc(ctx->sym_count ? ctx->sym_count : 1, sizeof(DynSym));
	int dyn_count = 0;
	for (int i = 0; i < ctx->sym_count; i++) {
		GlobalSymbol *s = &ctx->symbols[i];
		if (!s->is_defined && s->is_dynamic)
			dyn[dyn_count++].gs = s;
	}

	/* -----------------------------------------------------------
	 * 步骤 3:预扫描所有 .rela,确定哪些动态符号被绝对/GOT 引用
	 * (需要 .got 槽 + R_X86_64_GLOB_DAT,由 ld.so 填真实地址)。
	 *   R_X86_64_64        → GOT 槽
	 *   R_X86_64_GOTPCREL* → GOT 槽(GCC PIC 残留;Makefile 用
	 *                        -fno-pie 后正常不会出现)
 * GCC .o 的 R_X86_64_PC32 引用动态数据符号(stdout 等)无法
 * 静态解析(non-PIE 下 PC32 相对偏移必然溢出)→ copy relocation:
 * 链接器在 .bss 分配副本槽,引用静态指向槽,ld.so 启动时把
 * 库中的符号值拷入槽(R_X86_64_COPY)。
	 * ----------------------------------------------------------- */
	int globdat_count = 0;
	int copy_count = 0;
	for (int oi = 0; oi < ctx->obj_count; oi++) {
		ObjFile *obj = &ctx->objs[oi];
		if (obj->fmt != OBJ_FMT_ELF) continue;
		int is_gcc = obj_is_gcc(obj);
		const Elf64_Shdr *shdrs = (const Elf64_Shdr *)obj->elf_shdrs;
		for (int si = 0; si < obj->elf_shnum; si++) {
			if (shdrs[si].sh_type != SHT_RELA) continue;
			const Elf64_Rela *relas = (const Elf64_Rela *)(obj->data +
				elf_get_u64((const uint8_t *)&shdrs[si].sh_offset));
			uint64_t rela_count = elf_get_u64((const uint8_t *)&shdrs[si].sh_size) /
			                      ELF64_RELA_SIZE;
			for (uint64_t r = 0; r < rela_count; r++) {
				uint64_t r_info = elf_get_u64((const uint8_t *)&relas[r].r_info);
				uint32_t symidx = ELF64_R_SYM(r_info);
				uint32_t rtype = ELF64_R_TYPE(r_info);
				if (symidx == 0) continue;
				const char *symname = elf_sym_name(obj, (int)symidx);
				if (!symname || !symname[0]) continue;
				GlobalSymbol *gs = ctx_find_sym(ctx, symname);
				if (!gs || gs->is_defined) continue;
				int d = dyn_find(dyn, dyn_count, gs);
				if (d < 0) continue;
				if (rtype == R_X86_64_64 || rtype == R_X86_64_GOTPCREL ||
				    rtype == R_X86_64_GOTPCRELX) {
					if (!dyn[d].globdat) {
						dyn[d].globdat = 1;
						globdat_count++;
					}
				} else if (is_gcc && rtype == R_X86_64_PC32) {
					/* 数据符号引用 → copy relocation(见 DynSym 注释) */
					if (!dyn[d].copy_reloc) {
						dyn[d].copy_reloc = 1;
						copy_count++;
					}
				}
			}
		}
	}

	/* -----------------------------------------------------------
	 * 步骤 4:构建 .dynstr(依赖库名 + 动态符号名)。
	 *   .dynsym 大小 = (n+1) * 24;.gnu.hash / .rela.plt / .rela.dyn
	 *   大小均可在布局前确定。
	 * ----------------------------------------------------------- */
	char *dynstr = NULL;
	uint32_t dynstr_len = 0;
	uint32_t needed_off[DT_NEEDED_LIB_COUNT] = {0};
	{
		int cap = 256;
		dynstr = calloc(cap, 1);
		dynstr[dynstr_len++] = '\0';
		for (int i = 0; i < DT_NEEDED_LIB_COUNT; i++) {
			needed_off[i] = dynstr_len;
			int slen = (int)strlen(DT_NEEDED_LIBS[i]) + 1;
			while (dynstr_len + slen > (uint32_t)cap) {
				cap *= 2;
				dynstr = realloc(dynstr, cap);
			}
			memcpy(dynstr + dynstr_len, DT_NEEDED_LIBS[i], slen);
			dynstr_len += slen;
		}
		for (int i = 0; i < dyn_count; i++) {
			dyn[i].name_off = dynstr_len;
			int slen = (int)strlen(dyn[i].gs->name) + 1;
			while (dynstr_len + slen > (uint32_t)cap) {
				cap *= 2;
				dynstr = realloc(dynstr, cap);
			}
			memcpy(dynstr + dynstr_len, dyn[i].gs->name, slen);
			dynstr_len += slen;
		}
	}
	uint64_t dynsym_size = (uint64_t)(dyn_count + 1) * ELF64_SYM_SIZE;

	/* .gnu.hash 布局:
	 *   [0] nbuckets  [4] symoffset(=0:所有动态符号都是全局)  [8] bloom_size
	 *   [12] bloom_shift  [16..] bloom  [..] buckets  [..] chain
	 * 符号索引从 1 开始(0 = STN_UNDEF),chain 下标 = 符号索引 - 1。 */
	uint32_t gh_nbuckets = next_prime(dyn_count > 0 ? (uint32_t)dyn_count : 1);
	const uint32_t GH_BLOOM_SIZE = 1;
	const uint32_t GH_BLOOM_SHIFT = 5;
	uint64_t gh_bloom = 0;
	uint32_t *gh_buckets = calloc(gh_nbuckets, sizeof(uint32_t));
	uint32_t *gh_chain = calloc(dyn_count ? dyn_count : 1, sizeof(uint32_t));
	for (int i = 0; i < dyn_count; i++) {
		uint32_t h = elf_gnu_hash(dyn[i].gs->name);
		uint32_t b = h % gh_nbuckets;
		gh_bloom |= (1ULL << (h & 63)) | (1ULL << ((h >> GH_BLOOM_SHIFT) & 63));
		if (gh_buckets[b] == 0) gh_buckets[b] = (uint32_t)(i + 1);
	}
	/* chain:桶内后继(按符号索引升序)。链末 = 1(0<<1|1)。 */
	for (int i = 0; i < dyn_count; i++) {
		uint32_t h = elf_gnu_hash(dyn[i].gs->name);
		uint32_t b = h % gh_nbuckets;
		uint32_t next = 0;
		for (int j = i + 1; j < dyn_count; j++) {
			uint32_t hj = elf_gnu_hash(dyn[j].gs->name);
			if (hj % gh_nbuckets == b) { next = (uint32_t)(j + 1); break; }
		}
		gh_chain[i] = (next << 1) | 1;
	}
	uint64_t gnuhash_size = 16 + GH_BLOOM_SIZE * 8 +
	                        (uint64_t)gh_nbuckets * 4 +
	                        (uint64_t)dyn_count * 4;

	/* .rela.plt:每动态符号一条 JUMP_SLOT;.rela.dyn:GLOB_DAT + COPY */
	uint64_t relaplt_size = (uint64_t)dyn_count * ELF64_RELA_SIZE;
	uint64_t reladyn_size = (uint64_t)(globdat_count + copy_count) *
	                        ELF64_RELA_SIZE;

	/* -----------------------------------------------------------
	 * 步骤 5:分配虚拟地址与文件偏移。
	 *   PT_LOAD 1 (R+X): ehdr + 5 phdr + .interp + .text + .rodata
	 *                    + .plt + .dynsym/.dynstr/.gnu.hash/
	 *                    .rela.plt/.rela.dyn(ALLOC 元数据)
	 *   PT_LOAD 2 (R+W): .data + .bss + .got + .got.plt + .dynamic
	 * ----------------------------------------------------------- */
	MergedSection *ms_text   = find_merged(merged, merged_count, ".text");
	MergedSection *ms_rodata = find_merged(merged, merged_count, ".rodata");
	MergedSection *ms_data   = find_merged(merged, merged_count, ".data");
	MergedSection *ms_bss    = find_merged(merged, merged_count, ".bss");

	/* 输出段布局元数据(名字,类型,flags,对齐) */
	enum { SEG_INTERP, SEG_TEXT, SEG_RODATA, SEG_PLT, SEG_DYNSYM, SEG_DYNSTR,
	       SEG_GNUHASH, SEG_RELAPLT, SEG_RELADYN, SEG_DATA, SEG_BSS,
	       SEG_GOT, SEG_GOTPLT, SEG_DYNAMIC, SEG_SYMTAB, SEG_STRTAB,
	       SEG_SHSTRTAB, SEG_COUNT };
	/* 每个输出段的 rva(虚拟地址)与 file_offset(0 = 无文件内容) */
	uint64_t seg_rva[SEG_COUNT];
	uint64_t seg_off[SEG_COUNT];
	uint64_t seg_size[SEG_COUNT];
	memset(seg_rva, 0, sizeof(seg_rva));
	memset(seg_off, 0, sizeof(seg_off));
	memset(seg_size, 0, sizeof(seg_size));

	const char *interp_str = ELF_INTERP;
	int interp_len = (int)strlen(interp_str) + 1;
	int interp_pad = (int)align64(interp_len, 16);

	/* PLT 大小:PLT0(16) + 每动态符号 16 */
	uint64_t plt_size = dyn_count > 0 ? 16 + (uint64_t)dyn_count * 16 : 0;
	/* .got.plt:3 个固定槽 + 每符号 1 个 */
	uint64_t gotplt_size = dyn_count > 0 ? 8 * (3 + (uint64_t)dyn_count) : 0;
	/* .got:每个 GLOB_DAT 符号 1 个槽 */
	uint64_t got_size = (uint64_t)globdat_count * 8;

	uint64_t cur_addr = ELF_BASE_ADDR;
	uint64_t cur_file = 0;

	/* ELF 头 + 5 个 program header */
	cur_addr += ELF64_EHDR_SIZE + 5 * ELF64_PHDR_SIZE;
	cur_file += ELF64_EHDR_SIZE + 5 * ELF64_PHDR_SIZE;
	cur_addr = align64(cur_addr, 16);
	cur_file = align64(cur_file, 16);

	/* .interp(必须位于第一个可加载段内) */
	seg_rva[SEG_INTERP] = cur_addr;
	seg_off[SEG_INTERP] = cur_file;
	seg_size[SEG_INTERP] = interp_len;
	cur_addr += interp_pad;
	cur_file += interp_pad;

	/* .text */
	if (ms_text && ms_text->size > 0) {
		uint64_t tal = ms_text->max_align ? ms_text->max_align : 16;
		cur_addr = align64(cur_addr, tal);
		cur_file = align64(cur_file, tal);
		ms_text->rva = (uint32_t)cur_addr;
		ms_text->file_offset = (uint32_t)cur_file;
		ms_text->raw_size = (uint32_t)align64(ms_text->size, 16);
		seg_rva[SEG_TEXT] = cur_addr;
		seg_off[SEG_TEXT] = cur_file;
		seg_size[SEG_TEXT] = ms_text->size;
		cur_addr += ms_text->raw_size;
		cur_file += ms_text->raw_size;
	}
	/* .rodata(和 .text 在同一 PT_LOAD) */
	if (ms_rodata && ms_rodata->size > 0) {
		uint64_t tal = ms_rodata->max_align ? ms_rodata->max_align : 16;
		cur_addr = align64(cur_addr, tal);
		cur_file = align64(cur_file, tal);
		ms_rodata->rva = (uint32_t)cur_addr;
		ms_rodata->file_offset = (uint32_t)cur_file;
		ms_rodata->raw_size = (uint32_t)align64(ms_rodata->size, 16);
		seg_rva[SEG_RODATA] = cur_addr;
		seg_off[SEG_RODATA] = cur_file;
		seg_size[SEG_RODATA] = ms_rodata->size;
		cur_addr += ms_rodata->raw_size;
		cur_file += ms_rodata->raw_size;
	}
	/* .plt */
	if (plt_size > 0) {
		seg_rva[SEG_PLT] = cur_addr;
		seg_off[SEG_PLT] = cur_file;
		seg_size[SEG_PLT] = plt_size;
		cur_addr += plt_size;
		cur_file += plt_size;
	}
	/* 动态元数据段(ALLOC,8 对齐)——ld.so 经 DT_* 虚拟地址访问 */
	cur_addr = align64(cur_addr, 8);
	cur_file = align64(cur_file, 8);
	seg_rva[SEG_DYNSYM] = cur_addr;
	seg_off[SEG_DYNSYM] = cur_file;
	seg_size[SEG_DYNSYM] = dynsym_size;
	cur_addr += dynsym_size;
	cur_file += dynsym_size;

	seg_rva[SEG_DYNSTR] = cur_addr;
	seg_off[SEG_DYNSTR] = cur_file;
	seg_size[SEG_DYNSTR] = dynstr_len;
	cur_addr += dynstr_len;
	cur_file += dynstr_len;

	seg_rva[SEG_GNUHASH] = cur_addr;
	seg_off[SEG_GNUHASH] = cur_file;
	seg_size[SEG_GNUHASH] = gnuhash_size;
	cur_addr += gnuhash_size;
	cur_file += gnuhash_size;

	seg_rva[SEG_RELAPLT] = cur_addr;
	seg_off[SEG_RELAPLT] = cur_file;
	seg_size[SEG_RELAPLT] = relaplt_size;
	cur_addr += relaplt_size;
	cur_file += relaplt_size;

	seg_rva[SEG_RELADYN] = cur_addr;
	seg_off[SEG_RELADYN] = cur_file;
	seg_size[SEG_RELADYN] = reladyn_size;
	cur_addr += reladyn_size;
	cur_file += reladyn_size;

	/* 记录 PT_LOAD 1 的范围(文件尾) */
	uint64_t load1_file_end = cur_file;

	/* Keep writable data on its own file and virtual page.  Sharing a file
	 * page between RX and RW PT_LOADs can lose initialized data when the ELF
	 * loader clears the tail of the preceding segment. */
	cur_addr = align64(cur_addr, ELF_PAGE_ALIGN);
	cur_file = align64(cur_file, ELF_PAGE_ALIGN);
	uint64_t load2_vaddr = cur_addr;
	uint64_t load2_file_off = cur_file;
	uint64_t load2_file_end = cur_file;

	/* .data */
	if (ms_data && ms_data->size > 0) {
		ms_data->rva = (uint32_t)cur_addr;
		ms_data->file_offset = (uint32_t)cur_file;
		ms_data->raw_size = (uint32_t)align64(ms_data->size, 16);
		seg_rva[SEG_DATA] = cur_addr;
		seg_off[SEG_DATA] = cur_file;
		seg_size[SEG_DATA] = ms_data->size;
		cur_addr += ms_data->size;
		cur_file += ms_data->raw_size;
		load2_file_end = cur_file;
	}
	/* .got(GLOB_DAT 槽,8 对齐) */
	if (got_size > 0) {
		cur_addr = align64(cur_addr, 8);
		cur_file = align64(cur_file, 8);
		seg_rva[SEG_GOT] = cur_addr;
		seg_off[SEG_GOT] = cur_file;
		seg_size[SEG_GOT] = got_size;
		cur_addr += got_size;
		cur_file += got_size;
		load2_file_end = cur_file;
	}
	/* .got.plt(8 对齐) */
	if (gotplt_size > 0) {
		cur_addr = align64(cur_addr, 8);
		cur_file = align64(cur_file, 8);
		seg_rva[SEG_GOTPLT] = cur_addr;
		seg_off[SEG_GOTPLT] = cur_file;
		seg_size[SEG_GOTPLT] = gotplt_size;
		cur_addr += gotplt_size;
		cur_file += gotplt_size;
		load2_file_end = cur_file;
	}
	/* .dynamic(8 对齐;始终生成,最少 DT_NULL 一项) */
	{
		cur_addr = align64(cur_addr, 8);
		cur_file = align64(cur_file, 8);
		seg_rva[SEG_DYNAMIC] = cur_addr;
		seg_off[SEG_DYNAMIC] = cur_file;
		/* 固定条目:3×NEEDED + SYMTAB/SYMENT/STRTAB/STRSZ/GNU_HASH/FLAGS
		 * 条件条目:dyn_count>0 → PLTGOT/PLTRELSZ/PLTREL/JMPREL;
		 *          globdat/copy>0 → RELA/RELASZ/RELAENT;末尾 DT_NULL */
		uint64_t dynamic_size =
			(uint64_t)(9 + (dyn_count > 0 ? 4 : 0) +
			           (globdat_count > 0 || copy_count > 0 ? 3 : 0) + 1) * 16;
		seg_size[SEG_DYNAMIC] = dynamic_size;
		cur_addr += dynamic_size;
		cur_file += dynamic_size;
		load2_file_end = cur_file;
	}
	/* .bss(无文件内容,放最后——有初始内容的段必须在 filesz 内)。
	 * 基址必须按 max_align 对齐:前面 .got/.got.plt/.dynamic 只 8 对齐,
	 * 若 .bss 含 16 字节对齐变量(如 rt_sched_posix 的调度器结构),
	 * 基址 8 对齐会让 movaps 等 16 字节存取 #GP。
	 * 尾部追加 copy relocation 副本槽区(每符号 8 字节,16 对齐)。 */
	uint64_t copy_bss_size = (uint64_t)copy_count * 8;
	if (ms_bss && ms_bss->size > 0) {
		uint64_t tal = ms_bss->max_align ? ms_bss->max_align : 16;
		cur_addr = align64(cur_addr, tal);
		ms_bss->rva = (uint32_t)cur_addr;
		ms_bss->file_offset = 0;
		ms_bss->raw_size = 0;
		seg_rva[SEG_BSS] = cur_addr;
		seg_off[SEG_BSS] = 0;
		seg_size[SEG_BSS] = ms_bss->size + copy_bss_size;
		cur_addr += ms_bss->size + copy_bss_size;
	} else if (copy_bss_size > 0) {
		cur_addr = align64(cur_addr, 16);
		seg_rva[SEG_BSS] = cur_addr;
		seg_off[SEG_BSS] = 0;
		seg_size[SEG_BSS] = copy_bss_size;
		cur_addr += copy_bss_size;
	}

	/* PT_LOAD 2 范围 */
	uint64_t load2_memsz = cur_addr - load2_vaddr;
	uint64_t load2_filesz = load2_file_end - load2_file_off;

	/* -----------------------------------------------------------
	 * 步骤 6:计算全局符号地址(静态已定义符号)。
	 * ----------------------------------------------------------- */
	for (int i = 0; i < ctx->sym_count; i++) {
		GlobalSymbol *s = &ctx->symbols[i];
		if (!s->is_defined || s->obj_idx < 0) continue;
		ObjFile *obj = &ctx->objs[s->obj_idx];
		if (obj->fmt != OBJ_FMT_ELF) continue;
		if (s->sec_idx <= 0) continue;

		const char *sname = elf_sec_base_name(obj, s->sec_idx);
		char base[32] = {0};
		get_base_section(sname, base, sizeof(base));
		MergedSection *ms = find_merged(merged, merged_count, base);
		if (!ms) continue;

		uint32_t piece_off = 0;
		for (int j = 0; j < ms->piece_count; j++) {
			if (ms->pieces[j].obj_idx == s->obj_idx &&
			    ms->pieces[j].sec_idx == s->sec_idx) {
				piece_off = ms->pieces[j].offset;
				s->rva = ms->rva + piece_off + s->value;
				break;
			}
		}
	}

	#if ELF_EMIT_SECTION_METADATA
	/* -----------------------------------------------------------
	 * 步骤 7:section header 段索引分配(先于 .symtab 构建,符号
	 * 的 shndx 需要引用这些索引)。
	 *   0=NULL, 然后按 SEG_* 顺序(存在者), .shstrtab 恒存在。
	 * ----------------------------------------------------------- */
	uint8_t seg_present[SEG_COUNT];
	memset(seg_present, 0, sizeof(seg_present));
	seg_present[SEG_INTERP] = 1;
	if (ms_text && ms_text->size > 0) seg_present[SEG_TEXT] = 1;
	if (ms_rodata && ms_rodata->size > 0) seg_present[SEG_RODATA] = 1;
	if (plt_size > 0) seg_present[SEG_PLT] = 1;
	seg_present[SEG_DYNSYM] = 1;
	seg_present[SEG_DYNSTR] = 1;
	seg_present[SEG_GNUHASH] = 1;
	if (dyn_count > 0) seg_present[SEG_RELAPLT] = 1;
	if (globdat_count > 0 || copy_count > 0) seg_present[SEG_RELADYN] = 1;
	if (ms_data && ms_data->size > 0) seg_present[SEG_DATA] = 1;
	if ((ms_bss && ms_bss->size > 0) || copy_bss_size > 0) seg_present[SEG_BSS] = 1;
	if (got_size > 0) seg_present[SEG_GOT] = 1;
	if (gotplt_size > 0) seg_present[SEG_GOTPLT] = 1;
	seg_present[SEG_DYNAMIC] = 1;
	seg_present[SEG_SYMTAB] = 1;
	seg_present[SEG_STRTAB] = 1;
	seg_present[SEG_SHSTRTAB] = 1;

	static const char *const SEG_NAMES[SEG_COUNT] = {
		[SEG_INTERP]   = ".interp",
		[SEG_TEXT]     = ".text",
		[SEG_RODATA]   = ".rodata",
		[SEG_PLT]      = ".plt",
		[SEG_DYNSYM]   = ".dynsym",
		[SEG_DYNSTR]   = ".dynstr",
		[SEG_GNUHASH]  = ".gnu.hash",
		[SEG_RELAPLT]  = ".rela.plt",
		[SEG_RELADYN]  = ".rela.dyn",
		[SEG_DATA]     = ".data",
		[SEG_BSS]      = ".bss",
		[SEG_GOT]      = ".got",
		[SEG_GOTPLT]   = ".got.plt",
		[SEG_DYNAMIC]  = ".dynamic",
		[SEG_SYMTAB]   = ".symtab",
		[SEG_STRTAB]   = ".strtab",
		[SEG_SHSTRTAB] = ".shstrtab",
	};
	/* 段索引表:seg_shndx[SEG_*] = section header 中的索引 */
	uint16_t seg_shndx[SEG_COUNT];
	memset(seg_shndx, 0, sizeof(seg_shndx));
	int n_sections = 1;  /* [0] = NULL */
	for (int s = 0; s < SEG_COUNT; s++)
		if (seg_present[s]) seg_shndx[s] = (uint16_t)n_sections++;
	int sx_shstrtab = seg_shndx[SEG_SHSTRTAB];

	/* -----------------------------------------------------------
	 * 步骤 8:分配非 ALLOC 尾部元数据文件偏移(8 对齐)。
	 *   .symtab/.strtab/.shstrtab(调试用,section header 访问)。
	 * ----------------------------------------------------------- */
	/* 静态符号表 .symtab/.strtab(与 COFF 版一致) */
	typedef struct { const char *name; uint64_t value; uint16_t shndx; uint8_t info; } OutSym;
	OutSym *out_syms = calloc(ctx->sym_count + 2, sizeof(OutSym));
	int out_sym_count = 0;
	out_syms[out_sym_count++] = (OutSym){"", 0, SHN_UNDEF, 0};
	for (int i = 0; i < ctx->sym_count; i++) {
		GlobalSymbol *s = &ctx->symbols[i];
		if (!s->is_defined || s->rva == 0) continue;
		OutSym os;
		os.name = s->name;
		os.value = s->rva;
		os.info = (STB_GLOBAL << 4) | STT_FUNC;
		os.shndx = SHN_ABS;
		if (ms_text && s->rva >= ms_text->rva && s->rva < ms_text->rva + ms_text->size)
			{ os.shndx = seg_shndx[SEG_TEXT]; os.info = (STB_GLOBAL<<4)|STT_FUNC; }
		else if (ms_rodata && s->rva >= ms_rodata->rva && s->rva < ms_rodata->rva + ms_rodata->size)
			{ os.shndx = seg_shndx[SEG_RODATA]; os.info = (STB_GLOBAL<<4)|STT_OBJECT; }
		else if (ms_data && s->rva >= ms_data->rva && s->rva < ms_data->rva + ms_data->size)
			{ os.shndx = seg_shndx[SEG_DATA]; os.info = (STB_GLOBAL<<4)|STT_OBJECT; }
		else if (ms_bss && s->rva >= ms_bss->rva && s->rva < ms_bss->rva + ms_bss->size)
			{ os.shndx = seg_shndx[SEG_BSS]; os.info = (STB_GLOBAL<<4)|STT_OBJECT; }
		out_syms[out_sym_count++] = os;
	}
	/* 构建 .strtab */
	char *strtab = NULL;
	int strtab_len = 0;
	{
		int cap = 256;
		strtab = calloc(cap, 1);
		strtab[strtab_len++] = '\0';
		for (int i = 0; i < out_sym_count; i++) {
			if (!out_syms[i].name || !out_syms[i].name[0]) continue;
			int slen = (int)strlen(out_syms[i].name) + 1;
			while (strtab_len + slen > cap) { cap *= 2; strtab = realloc(strtab, cap); }
			memcpy(strtab + strtab_len, out_syms[i].name, slen);
			strtab_len += slen;
		}
	}
	uint32_t *sym_strtab_off = calloc(out_sym_count, sizeof(uint32_t));
	{
		int pos = 1;
		for (int i = 0; i < out_sym_count; i++) {
			if (!out_syms[i].name || !out_syms[i].name[0]) { sym_strtab_off[i] = 0; continue; }
			sym_strtab_off[i] = (uint32_t)pos;
			pos += (int)strlen(out_syms[i].name) + 1;
		}
	}
	uint64_t symtab_size = (uint64_t)out_sym_count * ELF64_SYM_SIZE;

	uint64_t meta_off = align64(load2_file_end, 8);
	seg_off[SEG_SYMTAB] = meta_off;
	seg_size[SEG_SYMTAB] = symtab_size;
	meta_off += symtab_size;

	seg_off[SEG_STRTAB] = meta_off;
	seg_size[SEG_STRTAB] = strtab_len;
	meta_off += strtab_len;

	/* 构建 .shstrtab */
	char *shstrtab = NULL;
	uint32_t shstrtab_len = 0;
	uint32_t shstr_off[SEG_COUNT] = {0};
	{
		int cap = 256;
		shstrtab = calloc(cap, 1);
		shstrtab[shstrtab_len++] = '\0';
		for (int s = 0; s < SEG_COUNT; s++) {
			if (!seg_present[s]) continue;
			shstr_off[s] = shstrtab_len;
			int slen = (int)strlen(SEG_NAMES[s]) + 1;
			while (shstrtab_len + slen > (uint32_t)cap) {
				cap *= 2;
				shstrtab = realloc(shstrtab, cap);
			}
			memcpy(shstrtab + shstrtab_len, SEG_NAMES[s], slen);
			shstrtab_len += slen;
		}
	}
	seg_off[SEG_SHSTRTAB] = meta_off;
	seg_size[SEG_SHSTRTAB] = shstrtab_len;
	meta_off += shstrtab_len;
	uint64_t shdr_off = align64(meta_off, 8);

	/* Executable loading is driven entirely by the program header table.
	 * Keep the release image sectionless: the static symbol/string tables and
	 * section headers are useful to tooling, but are not required by ld.so. */
	uint64_t total = shdr_off + (uint64_t)n_sections * ELF64_SHDR_SIZE;
	#else
	uint64_t total = load2_file_end;
	#endif

	/* -----------------------------------------------------------
	 * 步骤 9:分配 .got 槽地址(布局已定)。
	 * ----------------------------------------------------------- */
	{
		uint64_t slot_addr = seg_rva[SEG_GOT];
		for (int i = 0; i < dyn_count; i++)
			if (dyn[i].globdat) {
				dyn[i].got_slot = slot_addr;
				slot_addr += 8;
			}
	}

	/* 步骤 9.5:分配 .bss copy 副本槽地址(.bss 段尾部,16 对齐)。
	 * 槽内容由 ld.so 启动时 COPY 重定位填入,链接期留 0。 */
	{
		uint64_t slot_addr = seg_rva[SEG_BSS] +
		                     (ms_bss ? (uint64_t)align64(ms_bss->size, 16) : 0);
		for (int i = 0; i < dyn_count; i++)
			if (dyn[i].copy_reloc) {
				dyn[i].copy_slot = slot_addr;
				slot_addr += 8;
			}
	}

	/* -----------------------------------------------------------
	 * 步骤 10:构建 .dynamic 条目(顺序任意,以 DT_NULL 结尾)。
	 * ----------------------------------------------------------- */
	typedef struct { int64_t tag; uint64_t val; } DynEnt;
	DynEnt *dynent = calloc(3 + 13 + 3 + 1, sizeof(DynEnt));
	int dynent_count = 0;
	for (int i = 0; i < DT_NEEDED_LIB_COUNT; i++)
		dynent[dynent_count++] = (DynEnt){ DT_NEEDED, needed_off[i] };
	dynent[dynent_count++] = (DynEnt){ DT_PLTGOT, seg_rva[SEG_GOTPLT] };
	dynent[dynent_count++] = (DynEnt){ DT_PLTRELSZ, seg_size[SEG_RELAPLT] };
	dynent[dynent_count++] = (DynEnt){ DT_PLTREL, DT_RELA };
	dynent[dynent_count++] = (DynEnt){ DT_JMPREL, seg_rva[SEG_RELAPLT] };
	dynent[dynent_count++] = (DynEnt){ DT_SYMTAB, seg_rva[SEG_DYNSYM] };
	dynent[dynent_count++] = (DynEnt){ DT_SYMENT, ELF64_SYM_SIZE };
	dynent[dynent_count++] = (DynEnt){ DT_STRTAB, seg_rva[SEG_DYNSTR] };
	dynent[dynent_count++] = (DynEnt){ DT_STRSZ, seg_size[SEG_DYNSTR] };
	dynent[dynent_count++] = (DynEnt){ DT_GNU_HASH, seg_rva[SEG_GNUHASH] };
	dynent[dynent_count++] = (DynEnt){ DT_FLAGS, DF_BIND_NOW };
	if (globdat_count > 0 || copy_count > 0) {
		dynent[dynent_count++] = (DynEnt){ DT_RELA, seg_rva[SEG_RELADYN] };
		dynent[dynent_count++] = (DynEnt){ DT_RELASZ, seg_size[SEG_RELADYN] };
		dynent[dynent_count++] = (DynEnt){ DT_RELAENT, ELF64_RELA_SIZE };
	}
	dynent[dynent_count++] = (DynEnt){ DT_NULL, 0 };

	/* -----------------------------------------------------------
	 * 步骤 11:构建 PLT 代码(需要 .got.plt / .plt 地址)。
	 *   标准布局(懒解析兼容;BIND_NOW 下 ld.so 启动时直接填 GOT):
	 *     PLT0: ff 35 d32 (pushq GOT+8)  ff 25 d32 (jmpq *GOT+16)  0f 1f 40 00
	 *     PLTn: ff 25 d32 (jmpq *GOT[3+n])  68 i32 (pushq reloc_idx)  e9 d32 (jmp PLT0)
	 * ----------------------------------------------------------- */
	uint8_t *plt_buf = NULL;
	uint64_t *gotplt_buf = NULL;
	if (dyn_count > 0) {
		plt_buf = calloc(plt_size, 1);
		gotplt_buf = calloc(gotplt_size, 1);
		uint64_t plt0 = seg_rva[SEG_PLT];
		uint64_t gotplt = seg_rva[SEG_GOTPLT];
		/* GOT[0] = .dynamic 地址;GOT[1]/GOT[2] 由 ld.so 填写 */
		gotplt_buf[0] = seg_rva[SEG_DYNAMIC];
		gotplt_buf[1] = 0;
		gotplt_buf[2] = 0;
		/* PLT0 */
		plt_buf[0] = 0xff; plt_buf[1] = 0x35;
		uint32_t d1u = (uint32_t)(int64_t)(gotplt + 8) - (int64_t)(plt0 + 6);
		plt_buf[2] = d1u; plt_buf[3] = d1u >> 8; plt_buf[4] = d1u >> 16; plt_buf[5] = d1u >> 24;
		plt_buf[6] = 0xff; plt_buf[7] = 0x25;
		uint32_t d2u = (uint32_t)(int64_t)(gotplt + 16) - (int64_t)(plt0 + 12);
		plt_buf[8] = d2u; plt_buf[9] = d2u >> 8; plt_buf[10] = d2u >> 16; plt_buf[11] = d2u >> 24;
		plt_buf[12] = 0x0f; plt_buf[13] = 0x1f; plt_buf[14] = 0x40; plt_buf[15] = 0x00;
		/* 每符号一个 PLT 入口 */
		for (int i = 0; i < dyn_count; i++) {
			uint64_t pltn = plt0 + 16 + 16 * (uint64_t)i;
			uint8_t *p = plt_buf + 16 + 16 * i;
			/* GOT 槽初始值 = PLTn+6(懒解析入口);BIND_NOW 下被覆盖 */
			gotplt_buf[3 + i] = pltn + 6;
			p[0] = 0xff; p[1] = 0x25;
			uint32_t ddu = (uint32_t)(int64_t)(gotplt + 8 * (3 + (uint64_t)i)) - (int64_t)(pltn + 6);
			p[2] = ddu; p[3] = ddu >> 8; p[4] = ddu >> 16; p[5] = ddu >> 24;
			p[6] = 0x68;
			uint32_t ri = (uint32_t)i;
			p[7] = ri; p[8] = ri >> 8; p[9] = ri >> 16; p[10] = ri >> 24;
			p[11] = 0xe9;
			/* e9 在 pltn+11,5 字节,rel32 相对下一条指令 = pltn+16 */
			uint32_t dju = (uint32_t)(int64_t)plt0 - (int64_t)(pltn + 16);
			p[12] = dju; p[13] = dju >> 8; p[14] = dju >> 16; p[15] = dju >> 24;
		}
	}

	/* -----------------------------------------------------------
	 * 步骤 12:构建 .rela.plt / .rela.dyn 重定位表。
	 * ----------------------------------------------------------- */
	uint8_t *relaplt_buf = NULL;
	uint8_t *reladyn_buf = NULL;
	if (dyn_count > 0) {
		relaplt_buf = calloc(relaplt_size, 1);
		for (int i = 0; i < dyn_count; i++) {
			/* 数据符号(copy relocation)不是函数,不建 PLT 入口;
			 * 但 .plt 段仍按 dyn_count 布局(PLTn 与 dyn[n] 对齐),
			 * 其 JUMP_SLOT 槽无引用,浪费 16 字节无害。 */
			if (dyn[i].copy_reloc) continue;
			uint8_t *p = relaplt_buf + (uint64_t)i * ELF64_RELA_SIZE;
			/* offset = GOT 槽地址;info = (dynsym 索引<<32)|JUMP_SLOT;addend = 0 */
			elf_put_u64(p, seg_rva[SEG_GOTPLT] + 8 * (3 + (uint64_t)i));
			elf_put_u64(p + 8, ELF64_R_INFO((uint64_t)i + 1, R_X86_64_JUMP_SLOT));
			elf_put_u64(p + 16, 0);
		}
	}
	if (globdat_count > 0) {
		reladyn_buf = calloc(reladyn_size, 1);
		int gi = 0;
		for (int i = 0; i < dyn_count; i++) {
			if (!dyn[i].globdat) continue;
			uint8_t *p = reladyn_buf + (uint64_t)gi * ELF64_RELA_SIZE;
			elf_put_u64(p, dyn[i].got_slot);
			elf_put_u64(p + 8, ELF64_R_INFO((uint64_t)i + 1, R_X86_64_GLOB_DAT));
			elf_put_u64(p + 16, 0);
			gi++;
		}
	}
	/* COPY 条目:ld.so 启动时把库数据符号(stdout/stderr 等)的值
	 * 拷进 .bss 副本槽(offset=副本槽地址,sym=库符号,addend=0)。 */
	if (copy_count > 0) {
		if (!reladyn_buf)
			reladyn_buf = calloc(reladyn_size, 1);
		int gi = globdat_count;
		for (int i = 0; i < dyn_count; i++) {
			if (!dyn[i].copy_reloc) continue;
			uint8_t *p = reladyn_buf + (uint64_t)gi * ELF64_RELA_SIZE;
			elf_put_u64(p, dyn[i].copy_slot);
			elf_put_u64(p + 8, ELF64_R_INFO((uint64_t)i + 1, R_X86_64_COPY));
			elf_put_u64(p + 16, 0);
			gi++;
		}
	}

	/* -----------------------------------------------------------
	 * 步骤 13:应用重定位。
	 *   Mira 的 elf_writer 把 Rela addend 设为 0,encoder 在 patch
	 *   位置填 0(外部调用)或预计算值(内部调用)。因此读取 patch
	 *   位置的当前编码值作为额外 addend(COFF 隐式 addend 风格)。
	 *   动态符号:
	 *     PLT32/PC32 → S = 该符号 PLT 入口地址(静态已知)
	 *     R_X86_64_64 → 引用改为指向 .got 槽(ld.so 填真实地址)
	 * ----------------------------------------------------------- */
	for (int oi = 0; oi < ctx->obj_count; oi++) {
		ObjFile *obj = &ctx->objs[oi];
		if (obj->fmt != OBJ_FMT_ELF) continue;
		int is_gcc = obj_is_gcc(obj);

		const Elf64_Shdr *shdrs = (const Elf64_Shdr *)obj->elf_shdrs;
		for (int si = 0; si < obj->elf_shnum; si++) {
			if (shdrs[si].sh_type != SHT_RELA) continue;

			int target_si = (int)elf_get_u32((const uint8_t *)&shdrs[si].sh_info);
			const char *target_name = elf_sec_base_name(obj, target_si);
			char target_base[32] = {0};
			get_base_section(target_name, target_base, sizeof(target_base));
			MergedSection *ms = find_merged(merged, merged_count, target_base);
			if (!ms) continue;

			uint32_t piece_off = 0;
			int found_piece = 0;
			for (int j = 0; j < ms->piece_count; j++) {
				if (ms->pieces[j].obj_idx == oi &&
				    ms->pieces[j].sec_idx == target_si) {
					piece_off = ms->pieces[j].offset;
					found_piece = 1;
					break;
				}
			}
			if (!found_piece) continue;

			const Elf64_Rela *relas = (const Elf64_Rela *)(obj->data +
				elf_get_u64((const uint8_t *)&shdrs[si].sh_offset));
			uint64_t rela_count = elf_get_u64((const uint8_t *)&shdrs[si].sh_size) /
			                      ELF64_RELA_SIZE;

			for (uint64_t r = 0; r < rela_count; r++) {
				const Elf64_Rela *rel = &relas[r];
				uint64_t r_info = elf_get_u64((const uint8_t *)&rel->r_info);
				uint32_t symidx = ELF64_R_SYM(r_info);
				uint32_t rtype = ELF64_R_TYPE(r_info);
				int64_t addend = (int64_t)elf_get_u64((const uint8_t *)&rel->r_addend);

				uint32_t r_offset = (uint32_t)elf_get_u64((const uint8_t *)&rel->r_offset);
				uint32_t patch_local = piece_off + r_offset;
				if ((uint64_t)patch_local + 8 > ms->size) continue;
				uint64_t patch_addr = ms->rva + patch_local;

				/* 求符号地址 S */
				uint64_t S = 0;
				int has_S = 0;
				if (symidx == 0) {
					has_S = 1;  /* 段相对重定位,S=0 */
				} else {
					uint8_t sbind = STB_GLOBAL, stype = STT_NOTYPE;
					uint16_t sshndx = SHN_UNDEF;
					uint64_t sval = 0;
					elf_sym_attrs(obj, (int)symidx, &sbind, &stype, &sshndx, &sval);
					if (sbind == STB_LOCAL || stype == STT_SECTION) {
						/* 局部符号(static 函数/字符串/段符号):本文件内
						 * 直接按段解析,不进全局符号表。 */
						if (sshndx != SHN_UNDEF && sshndx < SHN_ABS) {
							const char *lname = elf_sec_base_name(obj, sshndx);
							char lbase[32] = {0};
							get_base_section(lname, lbase, sizeof(lbase));
							MergedSection *lms = find_merged(merged, merged_count, lbase);
							if (lms) {
								for (int j = 0; j < lms->piece_count; j++) {
									if (lms->pieces[j].obj_idx == oi &&
									    lms->pieces[j].sec_idx == sshndx) {
										S = (uint64_t)lms->rva +
										    lms->pieces[j].offset + sval;
										has_S = 1;
										break;
									}
								}
							}
						}
						/* 解析失败:has_S 保持 0,落入统一错误路径 */
					} else {
						/* 全局/弱符号:查合并后的符号表 */
						const char *symname = elf_sym_name(obj, symidx);
						if (symname && symname[0]) {
							GlobalSymbol *gs = ctx_find_sym(ctx, symname);
							if (gs && gs->is_defined) {
								S = gs->rva;
								has_S = 1;
							} else if (gs && !gs->is_defined && gs->is_dynamic) {
								int d = dyn_find(dyn, dyn_count, gs);
								if (d >= 0) {
									if (dyn[d].copy_reloc) {
										/* 数据符号:PC32 引用静态指向
										 * .bss 副本(近地址不溢出) */
										S = dyn[d].copy_slot;
									} else {
										/* PLT 入口地址:PLT0 之后第 d 项 */
										S = seg_rva[SEG_PLT] + 16 + 16 * (uint64_t)d;
										if (rtype == R_X86_64_64 ||
										    rtype == R_X86_64_GOTPCREL ||
										    rtype == R_X86_64_GOTPCRELX)
											S = dyn[d].got_slot;  /* 指向 GOT 槽 */
									}
									has_S = 1;
								}
							}
						}
					}
				}
				if (!has_S) {
					const char *symname = elf_sym_name(obj, symidx);
					fprintf(stderr,
						"error: linker: 无法解析的符号 '%s'\n", symname ? symname : "?");
					for (int i = 0; i < merged_count; i++) {
						free(merged[i].data);
						free(merged[i].pieces);
					}
					free(dyn); free(dynstr); free(gh_buckets); free(gh_chain);
					#if ELF_EMIT_SECTION_METADATA
					free(out_syms); free(strtab); free(sym_strtab_off); free(shstrtab);
					#endif
					free(dynent); free(plt_buf); free(gotplt_buf);
					free(relaplt_buf); free(reladyn_buf);
					return 0;
				}

				/* copy relocation 符号的引用已静态补到 .bss 副本
				 * (S 解析时处理),不再需要运行时补丁,走正常分支。 */

				switch (rtype) {
				case R_X86_64_PLT32:
				case R_X86_64_PC32:
				case R_X86_64_GOTPCREL:
				case R_X86_64_GOTPCRELX: {
					/* 按 obj 来源区分公式(差 4,见 obj_is_gcc 注释):
					 *   GCC:addend 含 -4 修正 → S+A-P
					 *   Mira:patch 预编码 → S+A+enc-(P+4) */
					/* 补丁点在指令流内(disp32/rel32 字段),偏移不保证
					 * 对齐,memcpy 读写避免未对齐访问 UB */
					int32_t encoded, val32;
					memcpy(&encoded, ms->data + patch_local, 4);
					int64_t val;
					if (is_gcc)
						val = (int64_t)S + addend + encoded - (int64_t)patch_addr;
					else
						val = (int64_t)S + addend + encoded -
						      (int64_t)(patch_addr + 4);
					val32 = (int32_t)val;
					memcpy(ms->data + patch_local, &val32, 4);
					break;
				}
				case R_X86_64_64: {
					int64_t encoded;
					memcpy(&encoded, ms->data + patch_local, 8);
					int64_t val64 = (int64_t)S + addend + encoded;
					memcpy(ms->data + patch_local, &val64, 8);
					break;
				}
				case R_X86_64_32S:
				case R_X86_64_32: {
					/* mov $sym, reg:32 位绝对。仅限静态已定义符号
					 * 或 copy relocation 符号(已指向 .bss 副本,低地址
					 * 可表达);其余动态符号地址未知且常 >2GB,无法表达。 */
					const char *sname = elf_sym_name(obj, symidx);
					GlobalSymbol *gs2 = sname ? ctx_find_sym(ctx, sname) : NULL;
					int is_copy = 0;
					if (gs2 && !gs2->is_defined && gs2->is_dynamic) {
						int d2 = dyn_find(dyn, dyn_count, gs2);
						if (d2 >= 0) is_copy = dyn[d2].copy_reloc;
					}
					if (gs2 && !gs2->is_defined && !is_copy) {
						fprintf(stderr, "warning: R_X86_64_32%s 引用动态符号 '%s'(跳过)\n",
							rtype == R_X86_64_32S ? "S" : "", sname ? sname : "?");
						break;
					}
					int32_t val32 = (int32_t)((int64_t)S + addend);
					memcpy(ms->data + patch_local, &val32, 4);
					break;
				}
				default:
					fprintf(stderr, "warning: 不支持的重定位类型 %u\n", rtype);
					break;
				}
			}
		}
	}

	/* -----------------------------------------------------------
	 * 步骤 14:确定入口点 _start。
	 * ----------------------------------------------------------- */
	uint64_t entry = 0;
	GlobalSymbol *start_sym = ctx_find_sym(ctx, "_start");
	if (start_sym && start_sym->is_defined) {
		entry = start_sym->rva;
	}
	if (entry == 0) {
		mira_error_simple(2, "linker: 找不到入口符号 '_start'");
		for (int i = 0; i < merged_count; i++) {
			free(merged[i].data);
			free(merged[i].pieces);
		}
		free(dyn); free(dynstr); free(gh_buckets); free(gh_chain);
		#if ELF_EMIT_SECTION_METADATA
		free(out_syms); free(strtab); free(sym_strtab_off); free(shstrtab);
		#endif
		free(dynent); free(plt_buf); free(gotplt_buf);
		free(relaplt_buf); free(reladyn_buf);
		return 0;
	}

	/* -----------------------------------------------------------
	 * 步骤 15:写入文件缓冲。
	 * ----------------------------------------------------------- */
	uint8_t *buf = calloc(total, 1);

	/* ELF Header */
	{
		Elf64_Ehdr *eh = (Elf64_Ehdr *)buf;
		eh->e_ident[0]=ELFMAG0; eh->e_ident[1]=ELFMAG1;
		eh->e_ident[2]=ELFMAG2; eh->e_ident[3]=ELFMAG3;
		eh->e_ident[4]=ELFCLASS64; eh->e_ident[5]=ELFDATA2LSB;
		eh->e_ident[6]=EV_CURRENT; eh->e_ident[7]=ELFOSABI_NONE;
		elf_put_u16((uint8_t*)&eh->e_type, ET_EXEC);
		elf_put_u16((uint8_t*)&eh->e_machine, EM_X86_64);
		elf_put_u32((uint8_t*)&eh->e_version, EV_CURRENT);
		elf_put_u64((uint8_t*)&eh->e_entry, entry);
		elf_put_u64((uint8_t*)&eh->e_phoff, ELF64_EHDR_SIZE);
		#if ELF_EMIT_SECTION_METADATA
		elf_put_u64((uint8_t*)&eh->e_shoff, shdr_off);
		#else
		elf_put_u64((uint8_t*)&eh->e_shoff, 0);
		#endif
		elf_put_u16((uint8_t*)&eh->e_ehsize, ELF64_EHDR_SIZE);
		elf_put_u16((uint8_t*)&eh->e_phentsize, ELF64_PHDR_SIZE);
		elf_put_u16((uint8_t*)&eh->e_phnum, 5);
		#if ELF_EMIT_SECTION_METADATA
		elf_put_u16((uint8_t*)&eh->e_shentsize, ELF64_SHDR_SIZE);
		elf_put_u16((uint8_t*)&eh->e_shnum, (uint16_t)n_sections);
		elf_put_u16((uint8_t*)&eh->e_shstrndx, (uint16_t)sx_shstrtab);
		#else
		elf_put_u16((uint8_t*)&eh->e_shentsize, 0);
		elf_put_u16((uint8_t*)&eh->e_shnum, 0);
		elf_put_u16((uint8_t*)&eh->e_shstrndx, SHN_UNDEF);
		#endif
	}

	/* Program Headers */
	{
		uint8_t *p = buf + ELF64_EHDR_SIZE;
		/* PT_LOAD 1: R+X(ehdr+phdrs+interp+text+rodata+plt+动态元数据)。
		 * copy relocation 不向 .text 写入,保持纯 R+X。 */
		elf_put_u32(p, PT_LOAD);
		elf_put_u32(p + 4, PF_R | PF_X);
		elf_put_u64(p + 8, 0);
		elf_put_u64(p + 16, ELF_BASE_ADDR);
		elf_put_u64(p + 24, ELF_BASE_ADDR);
		elf_put_u64(p + 32, load1_file_end);
		elf_put_u64(p + 40, load1_file_end);
		elf_put_u64(p + 48, ELF_PAGE_ALIGN);
		/* PT_LOAD 2: R+W(data+bss+got+got.plt+dynamic) */
		p += ELF64_PHDR_SIZE;
		elf_put_u32(p, PT_LOAD);
		elf_put_u32(p + 4, PF_R | PF_W);
		elf_put_u64(p + 8, load2_file_off);
		elf_put_u64(p + 16, load2_vaddr);
		elf_put_u64(p + 24, load2_vaddr);
		elf_put_u64(p + 32, load2_filesz);
		elf_put_u64(p + 40, load2_memsz);
		elf_put_u64(p + 48, ELF_PAGE_ALIGN);
		/* PT_INTERP */
		p += ELF64_PHDR_SIZE;
		elf_put_u32(p, PT_INTERP);
		elf_put_u32(p + 4, PF_R);
		elf_put_u64(p + 8, seg_off[SEG_INTERP]);
		elf_put_u64(p + 16, seg_rva[SEG_INTERP]);
		elf_put_u64(p + 24, seg_rva[SEG_INTERP]);
		elf_put_u64(p + 32, interp_len);
		elf_put_u64(p + 40, interp_len);
		elf_put_u64(p + 48, 1);
		/* PT_DYNAMIC */
		p += ELF64_PHDR_SIZE;
		elf_put_u32(p, PT_DYNAMIC);
		elf_put_u32(p + 4, PF_R | PF_W);
		elf_put_u64(p + 8, seg_off[SEG_DYNAMIC]);
		elf_put_u64(p + 16, seg_rva[SEG_DYNAMIC]);
		elf_put_u64(p + 24, seg_rva[SEG_DYNAMIC]);
		elf_put_u64(p + 32, seg_size[SEG_DYNAMIC]);
		elf_put_u64(p + 40, seg_size[SEG_DYNAMIC]);
		elf_put_u64(p + 48, 8);
		/* PT_GNU_STACK:无 PF_X → 栈不可执行 */
		p += ELF64_PHDR_SIZE;
		elf_put_u32(p, PT_GNU_STACK);
		elf_put_u32(p + 4, PF_R | PF_W);
		elf_put_u64(p + 8, 0);
		elf_put_u64(p + 16, 0);
		elf_put_u64(p + 24, 0);
		elf_put_u64(p + 32, 0);
		elf_put_u64(p + 40, 0);
		elf_put_u64(p + 48, 16);
	}

	/* 可加载段数据 */
	memcpy(buf + seg_off[SEG_INTERP], interp_str, interp_len);
	if (ms_text && ms_text->size > 0)
		memcpy(buf + ms_text->file_offset, ms_text->data, ms_text->size);
	if (ms_rodata && ms_rodata->size > 0)
		memcpy(buf + ms_rodata->file_offset, ms_rodata->data, ms_rodata->size);
	if (plt_size > 0)
		memcpy(buf + seg_off[SEG_PLT], plt_buf, plt_size);

	/* .dynsym */
	{
		uint8_t *p = buf + seg_off[SEG_DYNSYM];
		memset(p, 0, ELF64_SYM_SIZE);  /* [0] STN_UNDEF */
		p += ELF64_SYM_SIZE;
		for (int i = 0; i < dyn_count; i++) {
			elf_put_u32(p, dyn[i].name_off);
			uint8_t type = (dyn[i].globdat || dyn[i].copy_reloc) ? STT_OBJECT : STT_FUNC;
			p[4] = (STB_GLOBAL << 4) | type;
			p[5] = 0;  /* STV_DEFAULT */
			/* copy relocation 符号在 .dynsym 中保持 SHN_UNDEF:
			 * ld.so 据此在依赖库中查找定义,再把值拷入 .bss 副本槽;
			 * st_size 必须等于目标大小(ld.so 做 size 检查)。 */
			elf_put_u16(p + 6, SHN_UNDEF);
			elf_put_u64(p + 8, 0);
			elf_put_u64(p + 16, dyn[i].copy_reloc ? 8 : 0);
			p += ELF64_SYM_SIZE;
		}
	}
	memcpy(buf + seg_off[SEG_DYNSTR], dynstr, dynstr_len);

	/* .gnu.hash */
	{
		uint8_t *p = buf + seg_off[SEG_GNUHASH];
		elf_put_u32(p, gh_nbuckets);
		elf_put_u32(p + 4, 0);                  /* symoffset */
		elf_put_u32(p + 8, GH_BLOOM_SIZE);
		elf_put_u32(p + 12, GH_BLOOM_SHIFT);
		elf_put_u64(p + 16, gh_bloom);
		p += 24;
		for (uint32_t i = 0; i < gh_nbuckets; i++) {
			elf_put_u32(p, gh_buckets[i]);
			p += 4;
		}
		for (int i = 0; i < dyn_count; i++) {
			elf_put_u32(p, gh_chain[i]);
			p += 4;
		}
	}

	/* .rela.plt / .rela.dyn */
	if (relaplt_buf) memcpy(buf + seg_off[SEG_RELAPLT], relaplt_buf, relaplt_size);
	if (reladyn_buf) memcpy(buf + seg_off[SEG_RELADYN], reladyn_buf, reladyn_size);

	if (ms_data && ms_data->size > 0)
		memcpy(buf + ms_data->file_offset, ms_data->data, ms_data->size);
	if (got_size > 0)
		memset(buf + seg_off[SEG_GOT], 0, got_size);
	if (gotplt_size > 0)
		memcpy(buf + seg_off[SEG_GOTPLT], gotplt_buf, gotplt_size);

	/* .dynamic */
	{
		uint8_t *p = buf + seg_off[SEG_DYNAMIC];
		for (int i = 0; i < dynent_count; i++) {
			elf_put_u64(p, (uint64_t)dynent[i].tag);
			elf_put_u64(p + 8, dynent[i].val);
			p += 16;
		}
	}

	#if ELF_EMIT_SECTION_METADATA
	{
		uint8_t *p = buf + seg_off[SEG_SYMTAB];
		for (int i = 0; i < out_sym_count; i++) {
			elf_put_u32(p, sym_strtab_off[i]);
			p[4] = out_syms[i].info;
			p[5] = 0;
			elf_put_u16(p + 6, out_syms[i].shndx);
			elf_put_u64(p + 8, out_syms[i].value);
			elf_put_u64(p + 16, 0);
			p += ELF64_SYM_SIZE;
		}
		memcpy(buf + seg_off[SEG_STRTAB], strtab, strtab_len);
		memcpy(buf + seg_off[SEG_SHSTRTAB], shstrtab, shstrtab_len);
	}

	/* Section Header Table */
	{
		uint8_t *p = buf + shdr_off;
		memset(p, 0, ELF64_SHDR_SIZE);  /* [0] NULL */
		p += ELF64_SHDR_SIZE;

		#define WRITE_SHDR(noff, type, flags, addr, off, sz, link, info, al, ent) do { \
			elf_put_u32(p, noff); elf_put_u32(p+4, type); \
			elf_put_u64(p+8, flags); elf_put_u64(p+16, addr); \
			elf_put_u64(p+24, off); elf_put_u64(p+32, sz); \
			elf_put_u32(p+40, link); elf_put_u32(p+44, info); \
			elf_put_u64(p+48, al); elf_put_u64(p+56, ent); \
			p += ELF64_SHDR_SIZE; } while(0)

		for (int s = 0; s < SEG_COUNT; s++) {
			if (!seg_present[s]) continue;
			uint32_t type = SHT_PROGBITS, flags = 0;
			uint32_t link = 0, info = 0;
			uint64_t align = 1, entsize = 0;
			switch (s) {
			case SEG_INTERP: type = SHT_PROGBITS; flags = SHF_ALLOC; align = 1; break;
			case SEG_TEXT:   type = SHT_PROGBITS; flags = SHF_ALLOC|SHF_EXECINSTR;
				align = ms_text ? ms_text->max_align : 16;
				if (align < 16) align = 16; break;
			case SEG_RODATA: type = SHT_PROGBITS; flags = SHF_ALLOC;
				align = ms_rodata ? ms_rodata->max_align : 8;
				if (align < 8) align = 8; break;
			case SEG_PLT:    type = SHT_PROGBITS; flags = SHF_ALLOC|SHF_EXECINSTR; align = 16; break;
			case SEG_DYNSYM:
				type = SHT_SYMTAB; flags = SHF_ALLOC; align = 8; entsize = ELF64_SYM_SIZE;
				link = seg_shndx[SEG_DYNSTR]; info = 1; break;
			case SEG_DYNSTR: type = SHT_STRTAB; flags = SHF_ALLOC; align = 1; break;
			case SEG_GNUHASH: type = SHT_PROGBITS; flags = SHF_ALLOC; align = 8; break;
			case SEG_RELAPLT:
				type = SHT_RELA; flags = SHF_ALLOC; align = 8; entsize = ELF64_RELA_SIZE;
				link = seg_shndx[SEG_DYNSYM]; info = seg_shndx[SEG_PLT]; break;
			case SEG_RELADYN:
				type = SHT_RELA; flags = SHF_ALLOC; align = 8; entsize = ELF64_RELA_SIZE;
				link = seg_shndx[SEG_DYNSYM]; info = 0; break;
			case SEG_DATA:   type = SHT_PROGBITS; flags = SHF_ALLOC|SHF_WRITE;
				align = ms_data ? ms_data->max_align : 8;
				if (align < 8) align = 8; break;
			case SEG_BSS:    type = SHT_NOBITS;   flags = SHF_ALLOC|SHF_WRITE;
				align = ms_bss ? ms_bss->max_align : 8;
				if (align < 8) align = 8; break;
			case SEG_GOT:    type = SHT_PROGBITS; flags = SHF_ALLOC|SHF_WRITE; align = 8; break;
			case SEG_GOTPLT: type = SHT_PROGBITS; flags = SHF_ALLOC|SHF_WRITE; align = 8; break;
			case SEG_DYNAMIC:
				type = SHT_DYNAMIC; flags = SHF_ALLOC|SHF_WRITE;
				link = seg_shndx[SEG_DYNSTR];
				align = 8; entsize = 16; break;
			case SEG_SYMTAB:
				type = SHT_SYMTAB; align = 8; entsize = ELF64_SYM_SIZE;
				link = seg_shndx[SEG_STRTAB]; info = 1; break;
			case SEG_STRTAB: type = SHT_STRTAB; align = 1; break;
			case SEG_SHSTRTAB: type = SHT_STRTAB; align = 1; break;
			default: break;
			}
			WRITE_SHDR(shstr_off[s], type, flags, seg_rva[s], seg_off[s],
			           seg_size[s], link, info, align, entsize);
		}
		#undef WRITE_SHDR
	}
	#endif

	/* 写文件 */
	FILE *out = fopen(out_path, "wb");
	if (!out) {
		fprintf(stderr, "error: cannot create '%s'\n", out_path);
	} else {
		fwrite(buf, 1, total, out);
		fclose(out);
		chmod_exec(out_path);
	}

	/* 清理 */
	free(buf);
	#if ELF_EMIT_SECTION_METADATA
	free(out_syms);
	free(strtab);
	free(sym_strtab_off);
	free(shstrtab);
	#endif
	free(dyn);
	free(dynstr);
	free(gh_buckets);
	free(gh_chain);
	free(dynent);
	free(plt_buf);
	free(gotplt_buf);
	free(relaplt_buf);
	free(reladyn_buf);
	for (int i = 0; i < merged_count; i++) {
		free(merged[i].data);
		free(merged[i].pieces);
	}
	return 1;
}
