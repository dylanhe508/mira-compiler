/*
 * elfdefs.h — ELF64 结构定义与常量(跨平台,无 <elf.h> 依赖)。
 *
 * 被 codegen/elf_writer.c、linker/elf.c、linker/elfout.c 共享,
 * 避免在多处重复定义 Elf64_* 结构。
 *
 * 所有结构按小端手动布局(x86-64 固定小端)。
 */
#ifndef MIRA_ELFDEFS_H
#define MIRA_ELFDEFS_H

#include <stdint.h>

/* === ELF 文件标识 === */
#define EI_NIDENT    16
#define ELFMAG0      0x7f
#define ELFMAG1      'E'
#define ELFMAG2      'L'
#define ELFMAG3      'F'
#define ELFCLASS64   2
#define ELFDATA2LSB  1    /* x86-64 小端 */
#define EV_CURRENT   1
#define ELFOSABI_NONE 0

/* === e_type === */
#define ET_REL       1    /* 可重定位目标文件 */
#define ET_EXEC      2    /* 可执行文件 */

#define EM_X86_64    62

/* === 段类型(sh_type) === */
#define SHT_NULL     0
#define SHT_PROGBITS 1
#define SHT_SYMTAB   2
#define SHT_STRTAB   3
#define SHT_RELA     4
#define SHT_NOBITS   8
#define SHT_NOTE     7
#define SHT_DYNAMIC  6

/* === 段标志(sh_flags) === */
#define SHF_WRITE      0x1
#define SHF_ALLOC      0x2
#define SHF_EXECINSTR  0x4

/* === 符号绑定(st_info >> 4) === */
#define STB_LOCAL    0
#define STB_GLOBAL   1
#define STB_WEAK     2

/* === 符号类型(st_info & 0xf) === */
#define STT_NOTYPE   0
#define STT_OBJECT   1
#define STT_FUNC     2
#define STT_SECTION  3

/* === 特殊段索引(st_shndx) === */
#define SHN_UNDEF    0
#define SHN_ABS      0xfff1
#define SHN_COMMON   0xfff2

/* === x86-64 重定位类型 === */
#define R_X86_64_64       1    /* S + A(绝对地址 64 位) */
#define R_X86_64_PC32     2    /* S + A - P(RIP 相对 32 位) */
#define R_X86_64_PLT32    4    /* L + A - P(call/jmp,静态链接时 L=S) */
#define R_X86_64_GLOB_DAT 6    /* 动态:ld.so 把符号地址写入 GOT 槽 */
#define R_X86_64_JUMP_SLOT 7   /* 动态:ld.so 把符号地址写入 .got.plt 槽 */
#define R_X86_64_COPY     5    /* 动态:ld.so 把库数据符号值拷入 .bss 副本槽 */
#define R_X86_64_PC64     24
#define R_X86_64_32S      10    /* S + A(32 位符号扩展,mov $sym,reg) */
#define R_X86_64_32       11    /* S + A(32 位绝对,mov $sym,reg/读变量) */
#define R_X86_64_GOTPCREL 9
#define R_X86_64_GOTPCRELX 41   /* GOTPCREL + 可优化提示(处理同 9) */

/* === Program Header(p_type) === */
#define PT_NULL      0
#define PT_LOAD      1          /* 可加载段 */
#define PT_DYNAMIC   2          /* 动态链接段 */
#define PT_INTERP    3          /* 解释器路径 */
#define PT_GNU_STACK 0x6474e551 /* 栈属性(无 PF_X = 栈不可执行) */

/* === Program Header 权限标志(p_flags) === */
#define PF_X       1          /* 可执行 */
#define PF_W       2          /* 可写 */
#define PF_R       4          /* 可读 */

/* === Dynamic 段标签(d_tag) === */
#define DT_NULL      0
#define DT_NEEDED    1    /* d_val = 依赖库名在 .dynstr 中的偏移 */
#define DT_PLTRELSZ  2    /* .rela.plt 大小 */
#define DT_PLTGOT    3    /* .got.plt 地址 */
#define DT_STRTAB    5    /* .dynstr 地址 */
#define DT_SYMTAB    6    /* .dynsym 地址 */
#define DT_RELA      7    /* .rela.dyn 地址 */
#define DT_RELASZ    8    /* .rela.dyn 大小 */
#define DT_RELAENT   9    /* Rela 条目大小 = 24 */
#define DT_STRSZ     10   /* .dynstr 大小 */
#define DT_SYMENT    11   /* Sym 条目大小 = 24 */
#define DT_PLTREL    20   /* JUMP_SLOT 表条目类型 = DT_RELA */
#define DT_JMPREL    23   /* .rela.plt 地址 */
#define DT_FLAGS     30
#define DT_GNU_HASH  0x6ffffef5

/* DT_FLAGS 值:强制启动时解析所有 JUMP_SLOT(非懒解析),
 * 运行时 PLT 直接命中真实地址,零动态开销。
 * (DF_ORIGIN=1,DF_BIND_NOW=0x8,勿混淆) */
#define DF_BIND_NOW  0x8

/* === ELF64 结构(小端布局) === */
typedef struct {
	uint8_t  e_ident[EI_NIDENT];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint64_t e_entry;
	uint64_t e_phoff;
	uint64_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
} Elf64_Ehdr;

typedef struct {
	uint32_t p_type;
	uint32_t p_flags;
	uint64_t p_offset;
	uint64_t p_vaddr;
	uint64_t p_paddr;
	uint64_t p_filesz;
	uint64_t p_memsz;
	uint64_t p_align;
} Elf64_Phdr;

typedef struct {
	uint32_t sh_name;
	uint32_t sh_type;
	uint64_t sh_flags;
	uint64_t sh_addr;
	uint64_t sh_offset;
	uint64_t sh_size;
	uint32_t sh_link;
	uint32_t sh_info;
	uint64_t sh_addralign;
	uint64_t sh_entsize;
} Elf64_Shdr;

typedef struct {
	uint32_t st_name;
	uint8_t  st_info;     /* bind<<4 | type */
	uint8_t  st_other;
	uint16_t st_shndx;
	uint64_t st_value;
	uint64_t st_size;
} Elf64_Sym;

typedef struct {
	uint64_t r_offset;
	uint64_t r_info;      /* sym<<32 | type */
	int64_t  r_addend;
} Elf64_Rela;

/* === 结构体大小常量 === */
#define ELF64_EHDR_SIZE   64
#define ELF64_PHDR_SIZE   56
#define ELF64_SHDR_SIZE   64
#define ELF64_SYM_SIZE    24
#define ELF64_RELA_SIZE   24

/* === Rela 辅助宏 === */
#define ELF64_R_SYM(i)    ((uint32_t)((i) >> 32))
#define ELF64_R_TYPE(i)   ((uint32_t)((i) & 0xffffffff))
#define ELF64_R_INFO(s,t) (((uint64_t)(s) << 32) | (uint32_t)(t))

/* === Sym 辅助宏 === */
#define ELF64_ST_BIND(i)  ((i) >> 4)
#define ELF64_ST_TYPE(i)  ((i) & 0xf)

/* === 小端读取/写入辅助(供 elf.c / elfout.c 使用) === */
static inline uint16_t elf_get_u16(const uint8_t *p) {
	return (uint16_t)(p[0] | (p[1] << 8));
}
static inline uint32_t elf_get_u32(const uint8_t *p) {
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
	       ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static inline uint64_t elf_get_u64(const uint8_t *p) {
	return (uint64_t)elf_get_u32(p) | ((uint64_t)elf_get_u32(p + 4) << 32);
}
static inline void elf_put_u16(uint8_t *p, uint16_t v) {
	p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
}
static inline void elf_put_u32(uint8_t *p, uint32_t v) {
	p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
	p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static inline void elf_put_u64(uint8_t *p, uint64_t v) {
	elf_put_u32(p, (uint32_t)v);
	elf_put_u32(p + 4, (uint32_t)(v >> 32));
}

#endif /* MIRA_ELFDEFS_H */
