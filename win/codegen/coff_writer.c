/* coff_writer.c �?COFF .obj 鏂囦欢鐢熸垚�?
 *
 * �?EncodeResult (鏈哄櫒鐮?+ 鏁版�?+ 閲嶅畾浣?+ 绗﹀�? 鍐欎�?COFF 鐩爣鏂囦欢�?
 * 涓庣幇鏈?Mira 閾炬帴鍣ㄥ吋瀹广�?
 */
#include "ir.h"
#include "obj_reloc.h"
#include "../mira.h"
#include "../linker/linker.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* COFF 甯搁�?*/
#define IMAGE_FILE_MACHINE_AMD64       0x8664
#define IMAGE_SCN_CNT_CODE             0x00000020
#define IMAGE_SCN_CNT_INITIALIZED_DATA 0x00000040
#define IMAGE_SCN_CNT_UNINITIALIZED_DATA 0x00000080
#define IMAGE_SCN_MEM_EXECUTE          0x20000000
#define IMAGE_SCN_MEM_READ             0x40000000
#define IMAGE_SCN_MEM_WRITE            0x80000000
#define IMAGE_SCN_ALIGN_16BYTES        0x00500000

#define IMAGE_SYM_CLASS_EXTERNAL       2
#define IMAGE_SYM_CLASS_STATIC         3
#define IMAGE_SYM_DTYPE_FUNCTION       0x20

#ifndef IMAGE_REL_AMD64_REL32
#define IMAGE_REL_AMD64_REL32          4
#endif
#ifndef IMAGE_REL_AMD64_ADDR64
#define IMAGE_REL_AMD64_ADDR64         1
#endif

/* 鍐欏叆宸ュ叿 */
static void write_buf(FILE *f, const void *data, int len) {
	fwrite(data, 1, len, f);
}

static void write_u16(FILE *f, uint16_t v) { fwrite(&v, 2, 1, f); }
static void write_u32(FILE *f, uint32_t v) { fwrite(&v, 4, 1, f); }

int coff_write_obj(EncodeResult *enc, IrBuffer *ir, const char *path) {
	FILE *f = fopen(path, "wb");
	if (!f) { fprintf(stderr, "coff_writer: cannot open %s\n", path); return 1; }

	/* 璁＄畻鍚勯儴鍒嗗ぇ灏?*/
	int num_sections = 0;
	bool has_text = enc->text_len > 0;
	bool has_data = enc->data_len > 0;
	bool has_bss = enc->bss_size > 0;
	if (has_text) num_sections++;
	if (has_data) num_sections++;
	if (has_bss) num_sections++;

	int text_sec = 0, data_sec = 0, bss_sec = 0;
	int sec_idx = 1;
	if (has_text) text_sec = sec_idx++;
	if (has_data) data_sec = sec_idx++;
	if (has_bss) bss_sec = sec_idx++;

	/* 鏀堕泦鎵€鏈夌鍙?*/
	typedef struct {
		char *name;
		uint32_t value;
		int16_t section;
		uint16_t type;
		uint8_t storage_class;
	} SymEntry;

	int sym_cap = 128;
	int sym_count = 0;
	SymEntry *syms = calloc(sym_cap, sizeof(SymEntry));

	/* 娈电鍙?(COFF 瑕佹眰姣忎釜娈垫湁绗﹀�? */
	if (has_text) {
		if (sym_count >= sym_cap) { sym_cap *= 2; syms = realloc(syms, sym_cap * sizeof(SymEntry)); }
		syms[sym_count].name = strdup(".text");
		syms[sym_count].value = 0;
		syms[sym_count].section = text_sec;
		syms[sym_count].type = 0;
		syms[sym_count].storage_class = IMAGE_SYM_CLASS_STATIC;
		sym_count++;
	}
	if (has_data) {
		if (sym_count >= sym_cap) { sym_cap *= 2; syms = realloc(syms, sym_cap * sizeof(SymEntry)); }
		syms[sym_count].name = strdup(".data");
		syms[sym_count].value = 0;
		syms[sym_count].section = data_sec;
		syms[sym_count].type = 0;
		syms[sym_count].storage_class = IMAGE_SYM_CLASS_STATIC;
		sym_count++;
	}
	if (has_bss) {
		if (sym_count >= sym_cap) { sym_cap *= 2; syms = realloc(syms, sym_cap * sizeof(SymEntry)); }
		syms[sym_count].name = strdup(".bss");
		syms[sym_count].value = 0;
		syms[sym_count].section = bss_sec;
		syms[sym_count].type = 0;
		syms[sym_count].storage_class = IMAGE_SYM_CLASS_STATIC;
		sym_count++;
	}

	/* 定义的符??(from EncodeResult) */
	for (int i = 0; i < enc->sym_count; i++) {
		if (sym_count >= sym_cap) { sym_cap *= 2; syms = realloc(syms, sym_cap * sizeof(SymEntry)); }
		int sec = enc->symbols[i].section;
		int coff_sec = 0;
		if (sec == 1) coff_sec = text_sec;
		else if (sec == 2) coff_sec = data_sec;
		else if (sec == 3) coff_sec = bss_sec;

		/* 妫€鏌ユ槸鍚︽槸鍏ㄥ眬绗﹀�?*/
		bool is_global = false;
		for (int g = 0; g < ir->global_count; g++) {
			if (strcmp(ir->globals[g], enc->symbols[i].name) == 0) {
				is_global = true;
				break;
			}
		}

		syms[sym_count].name = strdup(enc->symbols[i].name);
		syms[sym_count].value = enc->symbols[i].offset;
		syms[sym_count].section = coff_sec;
		syms[sym_count].type = (sec == 1) ? IMAGE_SYM_DTYPE_FUNCTION : 0;
		syms[sym_count].storage_class = is_global ? IMAGE_SYM_CLASS_EXTERNAL : IMAGE_SYM_CLASS_STATIC;
		sym_count++;
	}

	/* 鍏ㄥ眬绗﹀彿浣嗘病鏈夊畾涔夌殑锛坓lobal 澹版槑浣嗗彲鑳介€氳�?label_named 瀹氫箟浜嗭級 */
	for (int g = 0; g < ir->global_count; g++) {
		bool found = false;
		for (int i = 0; i < sym_count; i++) {
			if (strcmp(syms[i].name, ir->globals[g]) == 0) { found = true; break; }
		}
		if (!found) {
			if (sym_count >= sym_cap) { sym_cap *= 2; syms = realloc(syms, sym_cap * sizeof(SymEntry)); }
			syms[sym_count].name = strdup(ir->globals[g]);
			syms[sym_count].value = 0;
			syms[sym_count].section = text_sec;
			syms[sym_count].type = IMAGE_SYM_DTYPE_FUNCTION;
			syms[sym_count].storage_class = IMAGE_SYM_CLASS_EXTERNAL;
			sym_count++;
		}
	}

	/* 澶栭儴绗﹀�?(undefined) �?dead-strip: 只输出被 relocation 引用�?extern */
	for (int i = 0; i < ir->extern_count; i++) {
		bool found = false;
		for (int j = 0; j < sym_count; j++) {
			if (strcmp(syms[j].name, ir->externs[i]) == 0) { found = true; break; }
		}
		if (!found) {
			/* Static Reference is semantic, not an optimization level: an
			 * undefined symbol with no relocation cannot affect the program. */
			bool referenced = false;
			for (int r = 0; r < enc->reloc_count; r++) {
				if (enc->relocs[r].sym_name &&
				    strcmp(enc->relocs[r].sym_name, ir->externs[i]) == 0) {
					referenced = true;
					break;
				}
			}
			if (!referenced) continue;

			if (sym_count >= sym_cap) { sym_cap *= 2; syms = realloc(syms, sym_cap * sizeof(SymEntry)); }
			syms[sym_count].name = strdup(ir->externs[i]);
			syms[sym_count].value = 0;
			syms[sym_count].section = 0; /* UNDEFINED */
			syms[sym_count].type = IMAGE_SYM_DTYPE_FUNCTION;
			syms[sym_count].storage_class = IMAGE_SYM_CLASS_EXTERNAL;
			sym_count++;
		}
	}

	/* 鏋勫缓閲嶅畾浣嶏紝灏嗙鍙峰悕鏄犲皠鍒扮鍙疯〃绱㈠紩 */
	CoffRelocation *text_relocs = NULL;
	int text_reloc_count = 0;
	CoffRelocation *data_relocs = NULL;
	int data_reloc_count = 0;

	for (int i = 0; i < enc->reloc_count; i++) {
		IrReloc *r = &enc->relocs[i];
		/* 鏌ユ壘绗﹀彿绱㈠紩 */
		int sym_idx = -1;
		for (int j = 0; j < sym_count; j++) {
			if (r->sym_name && strcmp(syms[j].name, r->sym_name) == 0) {
				sym_idx = j;
				break;
			}
		}
		if (sym_idx < 0 && r->sym_name) {
			/* Automatically add referenced but undefined symbols to the symbol table */
			if (sym_count >= sym_cap) { sym_cap *= 2; syms = realloc(syms, sym_cap * sizeof(SymEntry)); }
			syms[sym_count].name = strdup(r->sym_name);
			syms[sym_count].value = 0;
			syms[sym_count].section = 0; /* UNDEFINED */
			syms[sym_count].type = IMAGE_SYM_DTYPE_FUNCTION;
			syms[sym_count].storage_class = IMAGE_SYM_CLASS_EXTERNAL;
			sym_idx = sym_count;
			sym_count++;
		}

		CoffRelocation cr;
		cr.VirtualAddress = r->offset;
		cr.SymbolTableIndex = sym_idx;
		cr.Type = r->type;

		if (r->is_rip_data && r->type == IMAGE_REL_AMD64_ADDR64) {
			/* 鏁版嵁娈靛唴鐨勯噸瀹氫�?*/
			data_reloc_count++;
			data_relocs = realloc(data_relocs, data_reloc_count * sizeof(CoffRelocation));
			data_relocs[data_reloc_count - 1] = cr;
		} else {
			/* 鏂囨湰娈靛唴鐨勯噸瀹氫�?*/
			text_reloc_count++;
			text_relocs = realloc(text_relocs, text_reloc_count * sizeof(CoffRelocation));
			text_relocs[text_reloc_count - 1] = cr;
		}
	}

	/* 璁＄畻瀛楃涓茶�?*/
	int strtab_size = 4; /* 璧峰�?4 瀛楄妭涓哄ぇ�?*/
	for (int i = 0; i < sym_count; i++) {
		if (strlen(syms[i].name) > 8)
			strtab_size += (int)strlen(syms[i].name) + 1;
	}

	/* === 璁＄畻鏂囦欢甯冨�?=== */
	uint32_t header_size = sizeof(CoffHeader) + num_sections * sizeof(CoffSectionHeader);
	uint32_t cur_offset = header_size;

	uint32_t text_data_off = 0, text_reloc_off = 0;
	uint32_t data_data_off = 0, data_reloc_off = 0;

	if (has_text) {
		text_data_off = cur_offset;
		cur_offset += enc->text_len;
		text_reloc_off = cur_offset;
		cur_offset += text_reloc_count * sizeof(CoffRelocation);
	}
	if (has_data) {
		data_data_off = cur_offset;
		cur_offset += enc->data_len;
		data_reloc_off = cur_offset;
		cur_offset += data_reloc_count * sizeof(CoffRelocation);
	}

	uint32_t sym_table_off = cur_offset;

	/* === 鍐欏�?COFF �?=== */
	CoffHeader hdr = {0};
	hdr.Machine = IMAGE_FILE_MACHINE_AMD64;
	hdr.NumberOfSections = num_sections;
	hdr.TimeDateStamp = (uint32_t)time(NULL);
	hdr.PointerToSymbolTable = sym_table_off;
	hdr.NumberOfSymbols = sym_count;
	hdr.SizeOfOptionalHeader = 0;
	hdr.Characteristics = 0;
	write_buf(f, &hdr, sizeof(hdr));

	/* === 鍐欏�?Section Headers === */
	if (has_text) {
		CoffSectionHeader sh = {0};
		memcpy(sh.Name, ".text\0\0\0", 8);
		sh.VirtualSize = 0;
		sh.VirtualAddress = 0;
		sh.SizeOfRawData = enc->text_len;
		sh.PointerToRawData = text_data_off;
		sh.PointerToRelocations = text_reloc_count ? text_reloc_off : 0;
		sh.NumberOfRelocations = text_reloc_count;
		sh.Characteristics = IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ | IMAGE_SCN_ALIGN_16BYTES;
		write_buf(f, &sh, sizeof(sh));
	}

	if (has_data) {
		CoffSectionHeader sh = {0};
		memcpy(sh.Name, ".data\0\0\0", 8);
		sh.SizeOfRawData = enc->data_len;
		sh.PointerToRawData = data_data_off;
		sh.PointerToRelocations = data_reloc_count ? data_reloc_off : 0;
		sh.NumberOfRelocations = data_reloc_count;
		sh.Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE;
		write_buf(f, &sh, sizeof(sh));
	}

	if (has_bss) {
		CoffSectionHeader sh = {0};
		memcpy(sh.Name, ".bss\0\0\0\0", 8);
		sh.VirtualSize = enc->bss_size;
		sh.SizeOfRawData = 0;
		sh.PointerToRawData = 0;
		sh.Characteristics = IMAGE_SCN_CNT_UNINITIALIZED_DATA | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE;
		write_buf(f, &sh, sizeof(sh));
	}

	/* === 鍐欏叆娈垫暟�?=== */
	if (has_text) {
		write_buf(f, enc->text_code, enc->text_len);
		for (int i = 0; i < text_reloc_count; i++)
			write_buf(f, &text_relocs[i], sizeof(CoffRelocation));
	}

	if (has_data) {
		write_buf(f, enc->data_buf, enc->data_len);
		for (int i = 0; i < data_reloc_count; i++)
			write_buf(f, &data_relocs[i], sizeof(CoffRelocation));
	}

	/* === 鍐欏叆绗﹀彿琛?=== */
	uint32_t str_offset = 4;
	for (int i = 0; i < sym_count; i++) {
		CoffSymbol cs = {0};
		size_t nlen = strlen(syms[i].name);
		if (nlen <= 8) {
			memcpy(cs.Name.ShortName, syms[i].name, nlen);
		} else {
			cs.Name.LongName.Zeroes = 0;
			cs.Name.LongName.Offset = str_offset;
			str_offset += (uint32_t)nlen + 1;
		}
		cs.Value = syms[i].value;
		cs.SectionNumber = syms[i].section;
		cs.Type = syms[i].type;
		cs.StorageClass = syms[i].storage_class;
		cs.NumberOfAuxSymbols = 0;
		write_buf(f, &cs, sizeof(cs));
	}

	/* === 鍐欏叆瀛楃涓茶�?=== */
	write_u32(f, strtab_size);
	for (int i = 0; i < sym_count; i++) {
		if (strlen(syms[i].name) > 8) {
			fwrite(syms[i].name, 1, strlen(syms[i].name) + 1, f);
		}
	}

	fclose(f);

	/* 娓呯�?*/
	free(text_relocs);
	free(data_relocs);
	for (int i = 0; i < sym_count; i++) free(syms[i].name);
	free(syms);

	return 0;
}

/* 鍐欏叆鍐呭瓨缂撳�?(鐢ㄤ�?REPL �? */
int coff_write_mem(EncodeResult *enc, IrBuffer *ir, uint8_t **out_buf, int *out_len) {
	/* 鍏堝啓鍒颁复鏃舵枃浠讹紝鍐嶈鍥炲唴�?*/
	const char *tmp = "_ir_tmp.obj";
	int ret = coff_write_obj(enc, ir, tmp);
	if (ret != 0) return ret;
	FILE *f = fopen(tmp, "rb");
	if (!f) return 1;
	fseek(f, 0, SEEK_END);
	*out_len = (int)ftell(f);
	fseek(f, 0, SEEK_SET);
	*out_buf = (uint8_t *)malloc(*out_len);
	fread(*out_buf, 1, *out_len, f);
	fclose(f);
	remove(tmp);
	return 0;
}
