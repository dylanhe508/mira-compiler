/*
 * elf.c — ELF ET_REL (.o) 目标文件加载与符号查询。
 *
 * 平行于 coff.c 的 load_obj / coff_sym_name / section_name。
 * 仅做"解析到内存结构"的工作,不做任何链接逻辑(链接由 elfout.c 完成)。
 *
 * 设计:
 *   - 不拷贝段数据,所有指针指向 obj->data 内部(加载时一次 fread)。
 *   - 段头/符号表/字符串表直接 cast 到 Elf64_* 结构(小端布局已对齐)。
 *   - 验证 ELF magic、class64、machine=X86_64、type=ET_REL。
 */
#include "linker.h"
#include "elfdefs.h"
#include <stdio.h>
#include <string.h>

/* 从 obj 中定位 .symtab 及其关联的 .strtab 段。
 * ELF 的 .symtab 段头 sh_link 指向 .strtab 的段索引。 */
static void elf_locate_symtab(ObjFile *obj) {
	const Elf64_Ehdr *eh = (const Elf64_Ehdr *)obj->elf_ehdr;
	const Elf64_Shdr *shdrs = (const Elf64_Shdr *)obj->elf_shdrs;

	obj->elf_symtab_idx = -1;
	obj->elf_strtab_idx = -1;

	for (int i = 0; i < obj->elf_shnum; i++) {
		if (shdrs[i].sh_type == SHT_SYMTAB) {
			obj->elf_symtab_idx = i;
			obj->elf_strtab_idx = (int)shdrs[i].sh_link;
			break;
		}
	}
	(void)eh;
}

/* 加载 ELF 目标文件到 ObjFile 结构。
 * 成功返回 1,失败返回 0。
 * obj->fmt 设为 OBJ_FMT_ELF,data/size 填入完整文件内容。 */
int load_obj_elf(ObjFile *obj, const char *path) {
	memset(obj, 0, sizeof(*obj));
	obj->fmt = OBJ_FMT_ELF;

	FILE *f = fopen(path, "rb");
	if (!f) {
		fprintf(stderr, "error: cannot open object file '%s'\n", path);
		return 0;
	}
	fseek(f, 0, SEEK_END);
	obj->size = (size_t)ftell(f);
	fseek(f, 0, SEEK_SET);
	obj->data = (uint8_t *)malloc(obj->size);
	if (!obj->data) { fclose(f); return 0; }
	if (fread(obj->data, 1, obj->size, f) != obj->size) {
		fclose(f);
		fprintf(stderr, "error: short read on '%s'\n", path);
		return 0;
	}
	fclose(f);

	/* 验证 ELF 头 */
	if (obj->size < ELF64_EHDR_SIZE) {
		fprintf(stderr, "error: '%s' too small to be ELF\n", path);
		return 0;
	}
	const uint8_t *d = obj->data;
	if (d[0] != ELFMAG0 || d[1] != ELFMAG1 ||
	    d[2] != ELFMAG2 || d[3] != ELFMAG3) {
		fprintf(stderr, "error: '%s' is not an ELF file\n", path);
		return 0;
	}
	if (d[4] != ELFCLASS64) {
		fprintf(stderr, "error: '%s' is not ELF64\n", path);
		return 0;
	}

	const Elf64_Ehdr *eh = (const Elf64_Ehdr *)d;
	if (elf_get_u16((const uint8_t *)&eh->e_type) != ET_REL) {
		fprintf(stderr, "error: '%s' is not ET_REL (got type %u)\n",
		        path, elf_get_u16((const uint8_t *)&eh->e_type));
		return 0;
	}
	if (elf_get_u16((const uint8_t *)&eh->e_machine) != EM_X86_64) {
		fprintf(stderr, "error: '%s' is not x86-64 ELF\n", path);
		return 0;
	}

	obj->elf_ehdr  = (void *)eh;
	uint16_t shnum = elf_get_u16((const uint8_t *)&eh->e_shnum);
	uint16_t shoff = elf_get_u16((const uint8_t *)&eh->e_shoff) >> 16;
	(void)shoff;  /* 防止未使用警告 */

	/* 段头表紧跟在 ELF 头后面 */
	obj->elf_shdrs = (void *)(d + elf_get_u64((const uint8_t *)&eh->e_shoff));
	obj->elf_shnum = shnum;
	obj->elf_shstrndx = elf_get_u16((const uint8_t *)&eh->e_shstrndx);

	elf_locate_symtab(obj);
	return 1;
}

/* 返回段名(指向 obj 内部数据,无需释放)。
 * shidx 为段头表索引(0 = NULL 段)。 */
const char *elf_section_name(const ObjFile *obj, int shidx) {
	if (!obj || obj->fmt != OBJ_FMT_ELF) return "";
	if (shidx < 0 || shidx >= obj->elf_shnum) return "";
	const Elf64_Shdr *shdrs = (const Elf64_Shdr *)obj->elf_shdrs;
	const Elf64_Shdr *shstrtab_sh = &shdrs[obj->elf_shstrndx];
	const char *shstrtab = (const char *)(obj->data +
		elf_get_u64((const uint8_t *)&shstrtab_sh->sh_offset));
	return shstrtab + elf_get_u32((const uint8_t *)&shdrs[shidx].sh_name);
}

/* 返回符号名(指向 obj 内部数据,无需释放)。
 * symidx 为符号表条目索引(0 = 第一个符号,ELF 要求为 STN_UNDEF)。 */
const char *elf_sym_name(const ObjFile *obj, int symidx) {
	if (!obj || obj->fmt != OBJ_FMT_ELF) return "";
	if (obj->elf_symtab_idx < 0) return "";
	const Elf64_Shdr *shdrs = (const Elf64_Shdr *)obj->elf_shdrs;
	const Elf64_Shdr *symtab_sh = &shdrs[obj->elf_symtab_idx];
	const Elf64_Sym *syms = (const Elf64_Sym *)(obj->data +
		elf_get_u64((const uint8_t *)&symtab_sh->sh_offset));
	const Elf64_Shdr *strtab_sh = &shdrs[obj->elf_strtab_idx];
	const char *strtab = (const char *)(obj->data +
		elf_get_u64((const uint8_t *)&strtab_sh->sh_offset));

	uint32_t sym_count = (uint32_t)(elf_get_u64((const uint8_t *)&symtab_sh->sh_size) /
	                                ELF64_SYM_SIZE);
	if ((uint32_t)symidx >= sym_count) return "";
	uint32_t name_off = elf_get_u32((const uint8_t *)&syms[symidx].st_name);
	return strtab + name_off;
}
