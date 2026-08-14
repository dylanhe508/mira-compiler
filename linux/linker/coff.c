/*
 * coff.c �?COFF 鏍煎紡瑙ｆ�?
 */
#include "linker.h"

const char *coff_sym_name(const ObjFile *obj, const CoffSymbol *sym) {
	if (sym->Name.LongName.Zeroes != 0) {
		static char buf[9];
		memcpy(buf, sym->Name.ShortName, 8);
		buf[8] = '\0';
		return buf;
	}
	if (sym->Name.LongName.Offset < obj->strtab_size) {
		return obj->strtab + sym->Name.LongName.Offset;
	}
	return "???";
}

int load_obj(ObjFile *obj, const char *path) {
	FILE *f = fopen(path, "rb");
	if (!f) {
		fprintf(stderr, "error: cannot open object file '%s'\n", path);
		return 0;
	}
	fseek(f, 0, SEEK_END);
	obj->size = (size_t)ftell(f);
	fseek(f, 0, SEEK_SET);
	obj->data = malloc(obj->size);
	if (!obj->data) { fclose(f); return 0; }
	fread(obj->data, 1, obj->size, f);
	fclose(f);

	obj->hdr = (CoffHeader *)obj->data;
	if (obj->hdr->Machine != 0x8664) {
		fprintf(stderr, "error: '%s' is not x86_64 COFF (machine=0x%04x)\n",
				path, obj->hdr->Machine);
		return 0;
	}
	obj->sections = (CoffSectionHeader *)(obj->data + sizeof(CoffHeader));
	obj->symbols = (CoffSymbol *)(obj->data + obj->hdr->PointerToSymbolTable);
	uint8_t *strtab_ptr = (uint8_t *)obj->symbols +
	                       obj->hdr->NumberOfSymbols * sizeof(CoffSymbol);
	if (strtab_ptr + 4 <= obj->data + obj->size) {
		obj->strtab_size = *(uint32_t *)strtab_ptr;
		obj->strtab = (char *)strtab_ptr;
	} else {
		obj->strtab_size = 0;
		obj->strtab = NULL;
	}
	return 1;
}

const char *section_name(const ObjFile *obj, const CoffSectionHeader *sec) {
	if (sec->Name[0] == '/') {
		uint32_t off = (uint32_t)atoi(sec->Name + 1);
		if (obj->strtab && off < obj->strtab_size)
			return obj->strtab + off;
	}
	static char buf[9];
	memcpy(buf, sec->Name, 8);
	buf[8] = '\0';
	return buf;
}
