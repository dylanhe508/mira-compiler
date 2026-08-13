/* elf_writer.c - ELF ET_REL (.o) 文件生成。
 *
 * 平行于 coff_writer.c,消费同一个 EncodeResult + IrBuffer,输出
 * SysV x86-64 的 ELF 可重定位目标文件。设计要点:
 *
 *   - 自带 Elf64_* 结构定义(Windows 上无 <elf.h>),Linux 上也兼容。
 *   - 段:.text(SHF_ALLOC|EXECIN)、.data(SHF_ALLOC|WRITE)、
 *         .bss(SHF_ALLOC|WRITE, SHT_NOBITS, 无文件内容)、
 *         .symtab/.strtab/.shstrtab。
 *   - 符号表:ELF 要求 STB_LOCAL 必须排在 STB_GLOBAL 之前;段符号
 *     用 STB_LOCAL,其余按可见性 STB_GLOBAL/LOCAL。
 *   - 重定位:用 Elf64_Rela(显式 addend)。MIRA_RELOC_RIP32(=4)
 *     映射到 R_X86_64_PLT32,MIRA_RELOC_ABS64(=1)映射到 R_X86_64_64。
 *   - .data 段内的 ADDR64 重定位走 .rela.data,text 段的全部走
 *     .rela.text(与 coff_writer 的 is_rip_data 分组一致)。
 */
#include "ir.h"
#include "obj_reloc.h"
#include "../mira.h"
#include "../linker/linker.h"
#include "../linker/elfdefs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* === ELF64 结构与常量来自 elfdefs.h(跨平台共享) === */

/* ELF 符号表项大小 */
#define ELF64_SYM_SIZE   24
#define ELF64_RELA_SIZE  24
#define ELF64_SHDR_SIZE  64
#define ELF64_EHDR_SIZE  64

/* === 内部符号条目 === */
typedef struct {
	const char *name;
	uint64_t    value;
	int         shndx;     /* 段索引(1-based,0=UNDEF) */
	uint8_t     info;      /* bind<<4 | type */
} ElfSym;

/* === 字符串表辅助 === */
typedef struct {
	char  *buf;
	size_t len;
	size_t cap;
} StrTab;

static void strtab_init(StrTab *st) {
	st->cap = 256;
	st->buf = (char *)calloc(st->cap, 1);
	st->len = 1;  /* 第 0 字节是 NUL(无名符号) */
	st->buf[0] = '\0';
}

static uint32_t strtab_add(StrTab *st, const char *s) {
	if (!s || !s[0]) return 0;
	size_t slen = strlen(s) + 1;
	if (st->len + slen > st->cap) {
		while (st->len + slen > st->cap) st->cap *= 2;
		st->buf = (char *)realloc(st->buf, st->cap);
	}
	uint32_t off = (uint32_t)st->len;
	memcpy(st->buf + st->len, s, slen);
	st->len += slen;
	return off;
}

/* === 小端写入辅助 === */
static void put_u16(uint8_t *p, uint16_t v) { p[0]=v; p[1]=v>>8; }
static void put_u32(uint8_t *p, uint32_t v) {
	p[0]=v; p[1]=v>>8; p[2]=v>>16; p[3]=v>>24;
}
static void put_u64(uint8_t *p, uint64_t v) {
	p[0]=v; p[1]=v>>8; p[2]=v>>16; p[3]=v>>24;
	p[4]=v>>32; p[5]=v>>40; p[6]=v>>48; p[7]=v>>56;
}

/* 主写入函数 */
int elf_write_obj(EncodeResult *enc, IrBuffer *ir, const char *path) {
	FILE *fp = fopen(path, "wb");
	if (!fp) return -1;

	/* 判定段存在性 */
	int has_text = enc->text_len > 0;
	int has_data = enc->data_len > 0;
	int has_bss = enc->bss_size > 0;

	/* 收集符号。ELF 要求 STB_LOCAL 排在 STB_GLOBAL 之前。
	 * 顺序:段符号(LOCAL) → 已定义符号(按可见性) → 外部未定义(GLOBAL) */
	int max_syms = enc->sym_count + ir->extern_count + ir->global_count + 5;
	ElfSym *syms = (ElfSym *)calloc(max_syms, sizeof(ElfSym));
	/* ELF symbol table entry zero is reserved and must stay entirely zero. */
	int sym_count = 1;

	/* 段索引分配(0=NULL 段):
	 *   1=.text, 2=.data, 3=.bss, 后面是 .symtab/.strtab/.rela.* / .shstrtab
	 * 段索引必须在收集符号时就确定。 */
	int sec_text = has_text ? 1 : 0;
	int sec_data = has_data ? (has_text ? 2 : 1) : 0;
	int next_sec = has_text + has_data;
	int sec_bss  = has_bss ? next_sec + 1 : 0;

	/* (a) 段符号(STB_LOCAL, STT_SECTION) —— 段名在 .shstrtab,
	 * 但 ELF 符号名指向 .strtab,这里段符号用空名(st_name=0),
	 * 因为段引用通过 shndx 实现,不需要命名的段符号。 */
	/* 跳过段符号(coff 需要,elf 段引用靠 shndx)。直接放已定义符号。 */

	/* (b) 已定义符号 from enc->symbols */
	for (int i = 0; i < enc->sym_count; i++) {
		ElfSym *es = &syms[sym_count];
		es->name = enc->symbols[i].name;
		es->value = enc->symbols[i].offset;
		int sec = enc->symbols[i].section;
		if (sec == 1) { es->shndx = sec_text; es->info = (STB_GLOBAL<<4)|STT_FUNC; }
		else if (sec == 2) { es->shndx = sec_data; es->info = (STB_GLOBAL<<4)|STT_OBJECT; }
		else if (sec == 3) { es->shndx = sec_bss; es->info = (STB_GLOBAL<<4)|STT_OBJECT; }
		else continue;
		/* 检查是否全局(在 ir->globals 里) */
		int is_global = 0;
		for (int g = 0; g < ir->global_count; g++) {
			if (strcmp(ir->globals[g], es->name) == 0) { is_global = 1; break; }
		}
		if (!is_global) es->info = (STB_LOCAL<<4)|(es->info & 0xf);
		sym_count++;
	}

	/* (c) 全局声明但未定义的符号(外部函数) */
	for (int g = 0; g < ir->global_count; g++) {
		const char *name = ir->globals[g];
		int found = 0;
		for (int s = 0; s < sym_count; s++) {
			if (syms[s].name && strcmp(syms[s].name, name) == 0) { found = 1; break; }
		}
		if (!found) {
			syms[sym_count].name = name;
			syms[sym_count].value = 0;
			syms[sym_count].shndx = sec_text;  /* 假定在 text 段 */
			syms[sym_count].info = (STB_GLOBAL<<4)|STT_FUNC;
			sym_count++;
		}
	}

	/* (d) 外部引用符号(undeclared extern,带 dead-strip) */
	for (int e = 0; e < ir->extern_count; e++) {
		const char *name = ir->externs[e];
		/* dead-strip:只有被某个 reloc 引用才输出 */
		int referenced = 0;
		for (int r = 0; r < enc->reloc_count; r++) {
			if (enc->relocs[r].sym_name &&
			    strcmp(enc->relocs[r].sym_name, name) == 0) {
				referenced = 1; break;
			}
		}
		if (!referenced) continue;
		int found = 0;
		for (int s = 0; s < sym_count; s++) {
			if (syms[s].name && strcmp(syms[s].name, name) == 0) { found = 1; break; }
		}
		if (!found) {
			syms[sym_count].name = name;
			syms[sym_count].value = 0;
			syms[sym_count].shndx = SHN_UNDEF;
			syms[sym_count].info = (STB_GLOBAL<<4)|STT_FUNC;
			sym_count++;
		}
	}

	/* ELF 要求 STB_LOCAL 在前。把 LOCAL 的移到前面。 */
	int local_end = 1;
	for (int i = 1; i < sym_count; i++) {
		if ((syms[i].info >> 4) == STB_LOCAL) {
			if (i != local_end) {
				ElfSym tmp = syms[i];
				syms[i] = syms[local_end];
				syms[local_end] = tmp;
			}
			local_end++;
		}
	}
	/* 重排可能打乱了相对顺序,但对正确性无影响(符号按名解析)。 */

	/* 构建 .strtab(符号名字符串表) */
	StrTab strtab;
	strtab_init(&strtab);
	uint32_t *sym_names = (uint32_t *)calloc(sym_count, sizeof(uint32_t));
	for (int i = 1; i < sym_count; i++)
		sym_names[i] = strtab_add(&strtab, syms[i].name);

	/* 构建重定位表,分流 text/data */
	/* 同时收集 reloc 引用的符号索引 */
	typedef struct { uint64_t offset; uint32_t symidx; uint32_t type; int64_t addend; } RelOut;
	RelOut *text_relocs = NULL; int text_rc = 0;
	RelOut *data_relocs = NULL; int data_rc = 0;

	for (int r = 0; r < enc->reloc_count; r++) {
		IrReloc *rl = &enc->relocs[r];
		/* 找符号索引 */
		int sidx = -1;
		for (int s = 0; s < sym_count; s++) {
			if (syms[s].name && rl->sym_name &&
			    strcmp(syms[s].name, rl->sym_name) == 0) { sidx = s; break; }
		}
		if (sidx < 0 && rl->sym_name) {
			/* 与 coff_writer 对齐:reloc 引用的未声明符号自动补进
			 * 符号表(SHN_UNDEF)。之前这里直接 continue 会静默丢弃
			 * reloc——对未定义符号(如 '&&')的调用在链接后变成
			 * 空跳转,Windows 侧则正常报 unresolved symbol。 */
			if (sym_count >= max_syms) {
				max_syms *= 2;
				syms = (ElfSym *)realloc(syms, (size_t)max_syms * sizeof(ElfSym));
			}
			syms[sym_count].name = rl->sym_name;
			syms[sym_count].value = 0;
			syms[sym_count].shndx = SHN_UNDEF;
			syms[sym_count].info = (STB_GLOBAL<<4)|STT_FUNC;
			sym_names = (uint32_t *)realloc(sym_names, (size_t)(sym_count+1) * sizeof(uint32_t));
			sym_names[sym_count] = strtab_add(&strtab, rl->sym_name);
			sidx = sym_count;
			sym_count++;
		}
		if (sidx < 0) continue;  /* 找不到符号,跳过(不应发生) */

		/* 映射 reloc 类型 */
		uint32_t etype;
		if (rl->type == MIRA_RELOC_RIP32) etype = R_X86_64_PLT32;
		else if (rl->type == MIRA_RELOC_ABS64) etype = R_X86_64_64;
		else continue;

		int is_data = (rl->is_rip_data && rl->type == MIRA_RELOC_ABS64);
		RelOut ro = { rl->offset, (uint32_t)sidx, etype, 0 };

		/* PLT32/PC32 的 addend:call/jmp 的 disp32 字段在编码时已被
		 * 填入 -4(因为 RIP 相对偏移从下一条指令算起)。ELF Rela 的
		 * addend 应补偿这个:对 PLT32,addend = -4 + (已编码的值)。
		 * 但 encoder 已把最终 disp32 写入 text,所以 addend = -4
		 * 让链接器重算。简化:addend = 0,因为 Mira 的 encoder
		 * 用 RELA 风格(把全偏移编码进去了),链接器做 S+A-P 时
		 * A 取编码值。这里 addend=0 是最安全的初值。 */
		if (is_data) {
			data_relocs = realloc(data_relocs, (data_rc+1)*sizeof(RelOut));
			data_relocs[data_rc++] = ro;
		} else {
			text_relocs = realloc(text_relocs, (text_rc+1)*sizeof(RelOut));
			text_relocs[text_rc++] = ro;
		}
	}

	/* === 段布局计算 === */
	/* 段顺序:.text .data .bss .rela.text .rela.data .symtab .strtab .shstrtab */
	int nsections = 1;  /* NULL 段 */
	int idx_text=0, idx_data=0, idx_bss=0;
	int idx_rela_text=0, idx_rela_data=0, idx_symtab=0, idx_strtab=0, idx_shstrtab=0;

	if (has_text) { idx_text = nsections++; }
	if (has_data) { idx_data = nsections++; }
	if (has_bss)  { idx_bss = nsections++; }
	if (text_rc)  { idx_rela_text = nsections++; }
	if (data_rc)  { idx_rela_data = nsections++; }
	idx_symtab = nsections++;
	idx_strtab = nsections++;
	idx_shstrtab = nsections++;

	/* 回填段索引(段符号省略了,已定义符号的 shndx 用 sec_text/data/bss) */
	/* (上面已赋值) */

	/* 构建 .shstrtab */
	StrTab shstrtab;
	strtab_init(&shstrtab);
	uint32_t off_text = has_text ? strtab_add(&shstrtab, ".text") : 0;
	uint32_t off_data = has_data ? strtab_add(&shstrtab, ".data") : 0;
	uint32_t off_bss = has_bss ? strtab_add(&shstrtab, ".bss") : 0;
	uint32_t off_rela_text = strtab_add(&shstrtab, ".rela.text");
	uint32_t off_rela_data = strtab_add(&shstrtab, ".rela.data");
	uint32_t off_symtab = strtab_add(&shstrtab, ".symtab");
	uint32_t off_strtab = strtab_add(&shstrtab, ".strtab");
	uint32_t off_shstrtab = strtab_add(&shstrtab, ".shstrtab");

	/* 计算各段文件偏移 */
	uint64_t cur_off = ELF64_EHDR_SIZE;

	/* ELF 规范要求各段 sh_offset 按 sh_addralign 对齐(至少 8 字节)。
	 * 若不对齐,产生的 .o 会被链接器/readelf 等以未对齐方式访问(UB),
	 * 且 gcc 生成的 .o 全部对齐。统一把每个段起点对齐到 8 字节。 */
	#define ALIGN8(v) (((v) + 7) & ~(uint64_t)7)

	uint64_t text_off=0, text_sz=0;
	uint64_t data_off=0, data_sz=0;
	uint64_t bss_sz=0;
	uint64_t rela_text_off=0, rela_text_sz=0;
	uint64_t rela_data_off=0, rela_data_sz=0;
	uint64_t symtab_off=0, symtab_sz=0;
	uint64_t strtab_off=0;
	uint64_t shstrtab_off=0;

	if (has_text) {
		cur_off = ALIGN8(cur_off);
		text_off = cur_off;
		text_sz = enc->text_len;
		cur_off += text_sz;
	}
	if (has_data) {
		cur_off = ALIGN8(cur_off);
		data_off = cur_off;
		data_sz = enc->data_len;
		cur_off += data_sz;
	}
	/* .bss 无文件内容 */
	if (has_bss) bss_sz = enc->bss_size;

	if (text_rc) {
		cur_off = ALIGN8(cur_off);
		rela_text_off = cur_off;
		rela_text_sz = (uint64_t)text_rc * ELF64_RELA_SIZE;
		cur_off += rela_text_sz;
	}
	if (data_rc) {
		cur_off = ALIGN8(cur_off);
		rela_data_off = cur_off;
		rela_data_sz = (uint64_t)data_rc * ELF64_RELA_SIZE;
		cur_off += rela_data_sz;
	}
	cur_off = ALIGN8(cur_off);
	symtab_off = cur_off;
	symtab_sz = (uint64_t)sym_count * ELF64_SYM_SIZE;
	cur_off += symtab_sz;
	strtab_off = cur_off;
	cur_off += strtab.len;
	shstrtab_off = cur_off;
	cur_off += shstrtab.len;

	cur_off = ALIGN8(cur_off);
	uint64_t shoff = cur_off;
	uint64_t total = shoff + (uint64_t)nsections * ELF64_SHDR_SIZE;

	/* === 分配并填充整个文件缓冲 === */
	uint8_t *buf = (uint8_t *)calloc(total, 1);

	/* ELF Header */
	buf[0] = ELFMAG0; buf[1] = ELFMAG1; buf[2] = ELFMAG2; buf[3] = ELFMAG3;
	buf[4] = ELFCLASS64; buf[5] = ELFDATA2LSB; buf[6] = EV_CURRENT; buf[7] = ELFOSABI_NONE;
	/* e_ident[8..15] = 0 (padding) */
	put_u16(buf+16, ET_REL);
	put_u16(buf+18, EM_X86_64);
	put_u32(buf+20, EV_CURRENT);
	put_u64(buf+24, 0);    /* e_entry = 0 (可重定位) */
	put_u64(buf+32, 0);    /* e_phoff = 0 (无 program header) */
	put_u64(buf+40, shoff);
	put_u32(buf+48, 0);    /* e_flags */
	put_u16(buf+52, ELF64_EHDR_SIZE);
	put_u16(buf+54, 0);    /* e_phentsize */
	put_u16(buf+56, 0);    /* e_phnum */
	put_u16(buf+58, ELF64_SHDR_SIZE);
	put_u16(buf+60, (uint16_t)nsections);
	put_u16(buf+62, (uint16_t)idx_shstrtab);

	/* 段数据 */
	if (has_text) memcpy(buf + text_off, enc->text_code, text_sz);
	if (has_data) memcpy(buf + data_off, enc->data_buf, data_sz);

	/* 重定位(Rela) */
	if (text_rc) {
		uint8_t *p = buf + rela_text_off;
		for (int i = 0; i < text_rc; i++) {
			put_u64(p, text_relocs[i].offset);
			put_u64(p+8, ((uint64_t)text_relocs[i].symidx << 32) | text_relocs[i].type);
			put_u64(p+16, (uint64_t)text_relocs[i].addend);
			p += ELF64_RELA_SIZE;
		}
	}
	if (data_rc) {
		uint8_t *p = buf + rela_data_off;
		for (int i = 0; i < data_rc; i++) {
			put_u64(p, data_relocs[i].offset);
			put_u64(p+8, ((uint64_t)data_relocs[i].symidx << 32) | data_relocs[i].type);
			put_u64(p+16, (uint64_t)data_relocs[i].addend);
			p += ELF64_RELA_SIZE;
		}
	}

	/* 符号表 */
	{
		uint8_t *p = buf + symtab_off;
		for (int i = 0; i < sym_count; i++) {
			put_u32(p, sym_names[i]);
			p[4] = syms[i].info;
			p[5] = 0;  /* st_other */
			if (syms[i].shndx == SHN_UNDEF) put_u16(p+6, 0);
			else if (syms[i].shndx == SHN_ABS) put_u16(p+6, SHN_ABS);
			else put_u16(p+6, (uint16_t)syms[i].shndx);
			put_u64(p+8, syms[i].value);
			put_u64(p+16, 0);
			p += ELF64_SYM_SIZE;
		}
	}

	/* 字符串表 */
	memcpy(buf + strtab_off, strtab.buf, strtab.len);
	memcpy(buf + shstrtab_off, shstrtab.buf, shstrtab.len);

	/* 段头表 */
	{
		uint8_t *p = buf + shoff;
		/* [0] NULL 段:全零 */
		p += ELF64_SHDR_SIZE;

		/* 辅助宏:写一个段头 */
		#define WRITE_SHDR(name, type, flags, offset, size, link, info, align, entsize) do { \
			put_u32(p, name); put_u32(p+4, type); put_u64(p+8, flags); \
			put_u64(p+16, 0); put_u64(p+24, offset); put_u64(p+32, size); \
			put_u32(p+40, link); put_u32(p+44, info); \
			put_u64(p+48, align); put_u64(p+56, entsize); \
			p += ELF64_SHDR_SIZE; \
		} while(0)

		if (has_text)
			WRITE_SHDR(off_text, SHT_PROGBITS, SHF_ALLOC|SHF_EXECINSTR,
			           text_off, text_sz, 0, 0, 16, 0);
		if (has_data)
			WRITE_SHDR(off_data, SHT_PROGBITS, SHF_ALLOC|SHF_WRITE,
			           data_off, data_sz, 0, 0, 8, 0);
		if (has_bss)
			WRITE_SHDR(off_bss, SHT_NOBITS, SHF_ALLOC|SHF_WRITE,
			           0, bss_sz, 0, 0, 8, 0);
		if (text_rc)
			WRITE_SHDR(off_rela_text, SHT_RELA, 0,
			           rela_text_off, rela_text_sz, idx_symtab, idx_text, 8, ELF64_RELA_SIZE);
		if (data_rc)
			WRITE_SHDR(off_rela_data, SHT_RELA, 0,
			           rela_data_off, rela_data_sz, idx_symtab, idx_data, 8, ELF64_RELA_SIZE);
		/* .symtab: sh_info = 第一个非 LOCAL 符号的索引 */
		WRITE_SHDR(off_symtab, SHT_SYMTAB, 0,
		           symtab_off, symtab_sz, idx_strtab, local_end, 8, ELF64_SYM_SIZE);
		WRITE_SHDR(off_strtab, SHT_STRTAB, 0,
		           strtab_off, strtab.len, 0, 0, 1, 0);
		WRITE_SHDR(off_shstrtab, SHT_STRTAB, 0,
		           shstrtab_off, shstrtab.len, 0, 0, 1, 0);
		#undef WRITE_SHDR
	}

	fwrite(buf, 1, total, fp);
	fclose(fp);
	free(buf);

	free(syms);
	free(sym_names);
	free(text_relocs);
	free(data_relocs);
	free(strtab.buf);
	free(shstrtab.buf);
	return 0;
}

/* 内存版本(供 REPL 使用,与 coff_write_mem 平行) */
int elf_write_mem(EncodeResult *enc, IrBuffer *ir, uint8_t **out_buf, int *out_len) {
	const char *tmp = "_ir_tmp.o";
	if (elf_write_obj(enc, ir, tmp) != 0) return -1;
	FILE *fp = fopen(tmp, "rb");
	if (!fp) return -1;
	fseek(fp, 0, SEEK_END);
	long sz = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	*out_buf = (uint8_t *)malloc(sz);
	*out_len = (int)fread(*out_buf, 1, sz, fp);
	fclose(fp);
	remove(tmp);
	return 0;
}
