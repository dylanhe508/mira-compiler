/*
 * linker.h �?Mira 鑷畾涔?PE 閾炬帴鍣ㄥご鏂囦�?
 */
#ifndef LINKER_H
#define LINKER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../hash.h"

/* 鈹€鈹€鈹€ COFF 缁撴�?鈹€鈹€鈹€ */

#pragma pack(push, 1)

typedef struct {
	uint16_t Machine;
	uint16_t NumberOfSections;
	uint32_t TimeDateStamp;
	uint32_t PointerToSymbolTable;
	uint32_t NumberOfSymbols;
	uint16_t SizeOfOptionalHeader;
	uint16_t Characteristics;
} CoffHeader;

typedef struct {
	char     Name[8];
	uint32_t VirtualSize;
	uint32_t VirtualAddress;
	uint32_t SizeOfRawData;
	uint32_t PointerToRawData;
	uint32_t PointerToRelocations;
	uint32_t PointerToLinenumbers;
	uint16_t NumberOfRelocations;
	uint16_t NumberOfLinenumbers;
	uint32_t Characteristics;
} CoffSectionHeader;

typedef struct {
	union {
		char ShortName[8];
		struct { uint32_t Zeroes; uint32_t Offset; } LongName;
	} Name;
	uint32_t Value;
	int16_t  SectionNumber;
	uint16_t Type;
	uint8_t  StorageClass;
	uint8_t  NumberOfAuxSymbols;
} CoffSymbol;

typedef struct {
	uint32_t VirtualAddress;
	uint32_t SymbolTableIndex;
	uint16_t Type;
} CoffRelocation;

/* PE 缁撴�?*/
typedef struct {
	uint16_t Magic;
	uint8_t  MajorLinkerVersion;
	uint8_t  MinorLinkerVersion;
	uint32_t SizeOfCode;
	uint32_t SizeOfInitializedData;
	uint32_t SizeOfUninitializedData;
	uint32_t AddressOfEntryPoint;
	uint32_t BaseOfCode;
	uint64_t ImageBase;
	uint32_t SectionAlignment;
	uint32_t FileAlignment;
	uint16_t MajorOperatingSystemVersion;
	uint16_t MinorOperatingSystemVersion;
	uint16_t MajorImageVersion;
	uint16_t MinorImageVersion;
	uint16_t MajorSubsystemVersion;
	uint16_t MinorSubsystemVersion;
	uint32_t Win32VersionValue;
	uint32_t SizeOfImage;
	uint32_t SizeOfHeaders;
	uint32_t CheckSum;
	uint16_t Subsystem;
	uint16_t DllCharacteristics;
	uint64_t SizeOfStackReserve;
	uint64_t SizeOfStackCommit;
	uint64_t SizeOfHeapReserve;
	uint64_t SizeOfHeapCommit;
	uint32_t LoaderFlags;
	uint32_t NumberOfRvaAndSizes;
} PE64OptionalHeader;

typedef struct {
	uint32_t VirtualAddress;
	uint32_t Size;
} DataDirectory;

typedef struct {
	char     Name[8];
	uint32_t VirtualSize;
	uint32_t VirtualAddress;
	uint32_t SizeOfRawData;
	uint32_t PointerToRawData;
	uint32_t PointerToRelocations;
	uint32_t PointerToLinenumbers;
	uint16_t NumberOfRelocations;
	uint16_t NumberOfLinenumbers;
	uint32_t Characteristics;
} PeSectionHeader;

#pragma pack(pop)

/* IMAGE_REL_AMD64 类型 */
#define IMAGE_REL_AMD64_ABSOLUTE  0x0000
#define IMAGE_REL_AMD64_ADDR64    0x0001
#define IMAGE_REL_AMD64_ADDR32    0x0002
#define IMAGE_REL_AMD64_ADDR32NB  0x0003
#define IMAGE_REL_AMD64_REL32     0x0004
#define IMAGE_REL_AMD64_REL32_1   0x0005
#define IMAGE_REL_AMD64_REL32_2   0x0006
#define IMAGE_REL_AMD64_REL32_3   0x0007
#define IMAGE_REL_AMD64_REL32_4   0x0008
#define IMAGE_REL_AMD64_REL32_5   0x0009
#define IMAGE_REL_AMD64_SECTION   0x000A
#define IMAGE_REL_AMD64_SECREL    0x000B

/* 鈹€鈹€鈹€ 閾炬帴鍣ㄥ唴閮ㄧ粨鏋?鈹€鈹€鈹€ */

/* 目标文件格式(COFF=Windows / ELF=Linux)。决定加载与输出路径。 */
typedef enum { OBJ_FMT_COFF, OBJ_FMT_ELF } ObjFormat;

/* 已加载的 obj 文件。
 * COFF 字段(hdr/sections/symbols/strtab)仅在 OBJ_FMT_COFF 下有效;
 * ELF 字段(elf_*)仅在 OBJ_FMT_ELF 下有效。
 * data/size 两种格式共用(整个文件的原始字节)。 */
typedef struct {
	uint8_t *data;
	size_t   size;
	ObjFormat fmt;        /* 文件格式 */
	/* === COFF 字段 === */
	CoffHeader *hdr;
	CoffSectionHeader *sections;
	CoffSymbol *symbols;
	char *strtab;
	uint32_t strtab_size;
	/* === ELF 字段(指向 data 内部,无需独立释放) === */
	void *elf_ehdr;           /* Elf64_Ehdr* */
	void *elf_shdrs;          /* Elf64_Shdr*[e_shnum] */
	int   elf_shnum;          /* 段数 */
	int   elf_shstrndx;       /* 段名字符串表索引 */
	int   elf_symtab_idx;     /* .symtab 段索引(-1=无) */
	int   elf_strtab_idx;     /* 关联的 .strtab 段索引 */
} ObjFile;

/* 鍏ㄥ眬绗﹀彿琛?*/
typedef struct {
	char *name;
	int   obj_idx;    /* 来自哪个 obj (-1 = 链接器生?? */
	int   sec_idx;    /* 鍦ㄨ�?obj 涓�?section 绱㈠�?(1-based) */
	uint32_t value;   /* section 鍐呭亸绉?*/
	int   is_defined;
	int   is_referenced; /* at least one COFF relocation uses this symbol */
	int   is_dynamic;    /* ELF:未定义且被引用 → 由 ld.so 在 libc 等动态库中解析 */
	uint32_t rva;     /* 閾炬帴鍚庣殑鍦板�?*/
} GlobalSymbol;

/* DLL 瀵煎叆椤?*/
typedef struct {
	char *name;        /* 鍑芥暟鍚?(鍘绘帀 __imp_ 鍓嶇�? */
	char *dll_name;    /* 鎵€�?DLL */
	uint32_t iat_rva;  /* IAT 涓�?RVA */
} ImportEntry;

/* 閾炬帴鍣ㄤ笂涓嬫�?*/
typedef struct {
	ObjFile *objs;
	int obj_count;
	GlobalSymbol *symbols;
	int sym_count;
	int sym_cap;
	HashTable sym_ht;       /* 绗﹀彿鍚?�?GlobalSymbol* */
	ImportEntry *imports;
	int import_count;
	int import_cap;
	HashTable import_ht;    /* 瀵煎叆鍑芥暟�?�?ImportEntry* */
} LinkerCtx;

/* 鍚堝苟鐨?section */
typedef struct {
	char name[8];
	uint8_t *data;
	uint32_t size;
	uint32_t raw_size;
	uint32_t rva;
	uint32_t file_offset;
	uint32_t max_align;   /* 所有 piece 的 sh_addralign 最大值(上限 4096) */
	uint32_t characteristics;
	struct {
		int obj_idx;
		int sec_idx;       /* 1-based */
		uint32_t offset;
		uint32_t size;
	} *pieces;
	int piece_count;
	int piece_cap;
} MergedSection;

/* 鈹€鈹€鈹€ dll_map.c 鈹€鈹€鈹€ */
const char *find_dll_for(const char *funcname);
const char *rename_import(const char *name);
const char *import_func_name(const char *sym);

/* 鈹€鈹€鈹€ coff.c 鈹€鈹€鈹€ */
const char *coff_sym_name(const ObjFile *obj, const CoffSymbol *sym);
int         load_obj(ObjFile *obj, const char *path);
const char *section_name(const ObjFile *obj, const CoffSectionHeader *sec);

/* 鈹€鈹€鈹€ symbols.c 鈹€鈹€鈹€ */
void          ctx_init(LinkerCtx *ctx);
GlobalSymbol *ctx_find_sym(LinkerCtx *ctx, const char *name);
GlobalSymbol *ctx_add_sym(LinkerCtx *ctx, const char *name,
                          int obj_idx, int sec_idx,
                          uint32_t value, int is_defined);
void          ctx_add_import(LinkerCtx *ctx, const char *name, const char *dll);
void          collect_symbols(LinkerCtx *ctx, int obj_idx);
void          collect_symbols_elf(LinkerCtx *ctx, int obj_idx);
int           resolve_imports(LinkerCtx *ctx);
int           resolve_imports_elf(LinkerCtx *ctx);

/* 鈹€鈹€鈹€ pe.c 鈹€鈹€鈹€ */
uint32_t       align_up(uint32_t val, uint32_t align);
void           get_base_section(const char *full_name, char *out, size_t out_size);
MergedSection *find_merged(MergedSection *secs, int count, const char *name);
int            write_pe(LinkerCtx *ctx, const char *out_path);

/* 鈹€鈹€鈹€ elf.c(ELF 目标文件加载,平行于 coff.c)鈹€鈹€鈹€ */
int         load_obj_elf(ObjFile *obj, const char *path);
const char *elf_sym_name(const ObjFile *obj, int symidx);
const char *elf_section_name(const ObjFile *obj, int shidx);

/* 鈹€鈹€鈹€ elfout.c(静态 ELF 可执行输出,平行于 pe.c write_pe)鈹€鈹€鈹€ */
int write_elf(LinkerCtx *ctx, const char *out_path);

/* 鈹€鈹€鈹€ linker.c (鍏ュ�? 鈹€鈹€鈹€ */
int linker_run(const char **obj_paths, int obj_count, const char *out_path);

/* error.c �?统一错误输出 */
void mira_error_simple(int exit_code, const char *fmt, ...);

#endif /* LINKER_H */
