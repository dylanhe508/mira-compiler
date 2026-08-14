/*
 * pe.c �?PE 鍙�墽琛屾枃浠剁敓鎴?
 */
#include "linker.h"

#define IMAGE_BASE       0x00400000
#define SECTION_ALIGN    0x1000
#define FILE_ALIGN       0x200

uint32_t align_up(uint32_t val, uint32_t align) {
	return (val + align - 1) & ~(align - 1);
}

int write_pe(LinkerCtx *ctx, const char *out_path) {
	/* 1. 鍚堝苟鍚屽悕 sections */
	MergedSection merged[16];
	int merged_count = 0;
	memset(merged, 0, sizeof(merged));

	for (int oi = 0; oi < ctx->obj_count; oi++) {
		ObjFile *obj = &ctx->objs[oi];
		for (int si = 0; si < obj->hdr->NumberOfSections; si++) {
			CoffSectionHeader *sh = &obj->sections[si];
			const char *sname = section_name(obj, sh);

			char base_name[16] = {0};
			get_base_section(sname, base_name, sizeof(base_name));

			if (strcmp(base_name, ".reloc") == 0) continue;
			if (strncmp(base_name, ".debug", 6) == 0) continue;
			if (strcmp(base_name, ".pdata") == 0) continue;
			if (strcmp(base_name, ".xdata") == 0) continue;

			MergedSection *ms = find_merged(merged, merged_count, base_name);
			if (!ms) {
				if (merged_count >= 16) {
					mira_error_simple(2, "linker: too many sections (limit 16)");
					return 0;
				}
				ms = &merged[merged_count++];
				strncpy(ms->name, base_name, 8);
				ms->characteristics = sh->Characteristics;
				ms->piece_cap = 32;
				ms->pieces = calloc(ms->piece_cap, sizeof(*ms->pieces));
			}
			ms->characteristics |= sh->Characteristics;

			uint32_t sec_size = sh->SizeOfRawData;
			if (sh->VirtualSize > sec_size) sec_size = sh->VirtualSize;
			if (sec_size == 0) continue;

			if (ms->piece_count >= ms->piece_cap) {
				ms->piece_cap *= 2;
				ms->pieces = realloc(ms->pieces, ms->piece_cap * sizeof(*ms->pieces));
			}
			int pi = ms->piece_count++;
			ms->pieces[pi].obj_idx = oi;
			ms->pieces[pi].sec_idx = si + 1;
			ms->pieces[pi].offset = ms->size;
			ms->pieces[pi].size = sec_size;
			ms->size += sec_size;
			ms->size = align_up(ms->size, 16);
		}
	}

	/* 鍒嗛厤鏁版嵁 */
	for (int i = 0; i < merged_count; i++) {
		merged[i].data = calloc(1, merged[i].size);
		for (int j = 0; j < merged[i].piece_count; j++) {
			int oi = merged[i].pieces[j].obj_idx;
			int si = merged[i].pieces[j].sec_idx - 1;
			ObjFile *obj = &ctx->objs[oi];
			CoffSectionHeader *sh = &obj->sections[si];
			if (sh->SizeOfRawData > 0 && sh->PointerToRawData > 0) {
				memcpy(merged[i].data + merged[i].pieces[j].offset,
				       obj->data + sh->PointerToRawData,
				       sh->SizeOfRawData);
			}
		}
	}

	/* 1b. 鐢熸垚瀛樻牴鍜?thunks锛堣拷鍔犲埌 .text�?*/
	MergedSection *text_for_stub = find_merged(merged, merged_count, ".text");

	typedef struct { char name[128]; uint32_t text_offset; } ThunkInfo;
	ThunkInfo *thunk_list = calloc(ctx->sym_count > 0 ? ctx->sym_count : 1, sizeof(ThunkInfo));
	int thunk_count = 0;

	if (text_for_stub) {
		uint32_t cur = text_for_stub->size;

		/* __main 瀛樻�?(ret) */
		GlobalSymbol *gs_main = ctx_find_sym(ctx, "__main");
		if (gs_main && !gs_main->is_defined) {
			text_for_stub->data = realloc(text_for_stub->data, cur + 16);
			memset(text_for_stub->data + cur, 0xCC, 16);
			text_for_stub->data[cur] = 0xC3;
			gs_main->obj_idx = -1;
			gs_main->value = text_for_stub->rva + cur;
			gs_main->is_defined = 1;
			cur = align_up(cur + 1, 16);
		}

		/* _setjmpex / __intrinsic_setjmpex (xor rax,rax; ret) */
		GlobalSymbol *gs_setjmp = ctx_find_sym(ctx, "_setjmpex");
		GlobalSymbol *gs_setjmp2 = ctx_find_sym(ctx, "__intrinsic_setjmpex");
		if ((gs_setjmp && !gs_setjmp->is_defined) || (gs_setjmp2 && !gs_setjmp2->is_defined)) {
			text_for_stub->data = realloc(text_for_stub->data, cur + 16);
			memset(text_for_stub->data + cur, 0xCC, 16);
			text_for_stub->data[cur] = 0x48;
			text_for_stub->data[cur+1] = 0x31;
			text_for_stub->data[cur+2] = 0xC0; /* xor rax,rax */
			text_for_stub->data[cur+3] = 0xC3; /* ret */
			if (gs_setjmp) {
				gs_setjmp->is_defined = 1;
				gs_setjmp->obj_idx = -1;
				gs_setjmp->value = text_for_stub->rva + cur;
			}
			if (gs_setjmp2) {
				gs_setjmp2->is_defined = 1;
				gs_setjmp2->obj_idx = -1;
				gs_setjmp2->value = text_for_stub->rva + cur;
			}
			cur = align_up(cur + 16, 16);
			text_for_stub->size = cur;
		}

		/* 涓烘瘡涓�潪 __imp_ �?DLL 鍙�В鏋愮�鍙风敓�?jmp thunk */
		for (int i = 0; i < ctx->sym_count; i++) {
			GlobalSymbol *s = &ctx->symbols[i];
			if (s->is_defined) continue;
			if (strncmp(s->name, "__imp_", 6) == 0) continue;
			if (strncmp(s->name, "__intrinsic_", 12) == 0) continue;
			const char *dll = find_dll_for(s->name);
			if (!dll) continue;

			text_for_stub->data = realloc(text_for_stub->data, cur + 16);
			memset(text_for_stub->data + cur, 0xCC, 16);
			text_for_stub->data[cur + 0] = 0xFF;
			text_for_stub->data[cur + 1] = 0x25;
			memset(text_for_stub->data + cur + 2, 0, 4);

			strncpy(thunk_list[thunk_count].name, s->name, 127);
			thunk_list[thunk_count].text_offset = cur;
			thunk_count++;

			s->obj_idx = -1;
			s->value = cur;
			s->is_defined = 1;
			cur = align_up(cur + 6, 8);
		}
		text_for_stub->size = cur;
	}

	/* 2. 鏋勫�?import table */
	char **dll_names = calloc(ctx->import_count > 0 ? ctx->import_count : 1, sizeof(char *));
	int dll_count = 0;
	for (int i = 0; i < ctx->import_count; i++) {
		int found = 0;
		for (int d = 0; d < dll_count; d++) {
			if (strcmp(dll_names[d], ctx->imports[i].dll_name) == 0) { found = 1; break; }
		}
		if (!found) {
			dll_names[dll_count++] = ctx->imports[i].dll_name;
		}
	}

	uint32_t idt_size = (uint32_t)(dll_count + 1) * 20;
	uint32_t iat_entries = 0;
	for (int d = 0; d < dll_count; d++) {
		for (int i = 0; i < ctx->import_count; i++) {
			if (strcmp(ctx->imports[i].dll_name, dll_names[d]) == 0)
				iat_entries++;
		}
		iat_entries++;
	}
	uint32_t ilt_size = iat_entries * 8;
	uint32_t iat_size = ilt_size;
	uint32_t hnt_size = 0;
	for (int i = 0; i < ctx->import_count; i++) {
		hnt_size += 2;
		hnt_size += (uint32_t)strlen(ctx->imports[i].name) + 1;
		if (hnt_size & 1) hnt_size++;
	}
	uint32_t dllname_size = 0;
	for (int d = 0; d < dll_count; d++) {
		dllname_size += (uint32_t)strlen(dll_names[d]) + 1;
	}

	uint32_t idata_size = idt_size + ilt_size + iat_size + hnt_size + dllname_size;
	idata_size = align_up(idata_size, 16);

	/* 3. 璁＄�?section 甯冨�?*/
	uint32_t pe_sections = (uint32_t)merged_count + 1;
	uint32_t headers_size = align_up(
		64 + 4 + 20 + 240 + pe_sections * 40,
		FILE_ALIGN
	);

	uint32_t current_rva = align_up(headers_size, SECTION_ALIGN);
	uint32_t current_file = headers_size;

	MergedSection *text_sec = find_merged(merged, merged_count, ".text");
	MergedSection *data_sec = find_merged(merged, merged_count, ".data");
	MergedSection *bss_sec  = find_merged(merged, merged_count, ".bss");
	MergedSection *rdata_sec = find_merged(merged, merged_count, ".rdata");

	for (int i = 0; i < merged_count; i++) {
		merged[i].rva = current_rva;
		merged[i].file_offset = current_file;
		uint32_t vsize = align_up(merged[i].size, SECTION_ALIGN);
		if (strcmp(merged[i].name, ".bss") == 0) {
			merged[i].raw_size = 0;
			merged[i].file_offset = 0;
		} else {
			merged[i].raw_size = align_up(merged[i].size, FILE_ALIGN);
			current_file += merged[i].raw_size;
		}
		current_rva += vsize;
	}

	uint32_t idata_rva = current_rva;
	uint32_t idata_file = current_file;
	uint32_t idata_raw = align_up(idata_size, FILE_ALIGN);
	current_rva += align_up(idata_size, SECTION_ALIGN);
	current_file += idata_raw;

	uint32_t image_size = current_rva;

	/* 4. 璁＄畻绗﹀�?RVA */
	for (int i = 0; i < ctx->sym_count; i++) {
		GlobalSymbol *s = &ctx->symbols[i];
		if (!s->is_defined) continue;
		if (s->obj_idx == -1) {
			MergedSection *tms = find_merged(merged, merged_count, ".text");
			if (tms) s->rva = tms->rva + s->value;
			continue;
		}
		ObjFile *obj = &ctx->objs[s->obj_idx];
		int coff_sec = s->sec_idx;
		if (coff_sec <= 0 || coff_sec > obj->hdr->NumberOfSections) continue;

		const char *sname = section_name(obj, &obj->sections[coff_sec - 1]);
		char base_name[16] = {0};
		get_base_section(sname, base_name, sizeof(base_name));

		MergedSection *ms = find_merged(merged, merged_count, base_name);
		if (!ms) continue;

		for (int j = 0; j < ms->piece_count; j++) {
			if (ms->pieces[j].obj_idx == s->obj_idx &&
			    ms->pieces[j].sec_idx == coff_sec) {
				s->rva = ms->rva + ms->pieces[j].offset + s->value;
				break;
			}
		}
	}

	/* 5. 鏋勫�?.idata 鏁版�?*/
	uint8_t *idata = calloc(1, idata_size);

	uint32_t ilt_offset = idt_size;
	uint32_t iat_offset = ilt_offset + ilt_size;
	uint32_t hnt_offset = iat_offset + iat_size;
	uint32_t dname_offset = hnt_offset + hnt_size;

	uint32_t cur_ilt = ilt_offset;
	uint32_t cur_iat = iat_offset;
	uint32_t cur_hnt = hnt_offset;
	uint32_t cur_dname = dname_offset;

	for (int d = 0; d < dll_count; d++) {
		uint8_t *idt_entry = idata + d * 20;
		*(uint32_t *)(idt_entry + 0) = idata_rva + cur_ilt;
		*(uint32_t *)(idt_entry + 4) = 0;
		*(uint32_t *)(idt_entry + 8) = 0;
		*(uint32_t *)(idt_entry + 12) = idata_rva + cur_dname;
		*(uint32_t *)(idt_entry + 16) = idata_rva + cur_iat;

		size_t dlen = strlen(dll_names[d]);
		memcpy(idata + cur_dname, dll_names[d], dlen + 1);
		cur_dname += (uint32_t)dlen + 1;

		for (int i = 0; i < ctx->import_count; i++) {
			if (strcmp(ctx->imports[i].dll_name, dll_names[d]) != 0) continue;

			uint32_t hnt_rva = idata_rva + cur_hnt;
			*(uint16_t *)(idata + cur_hnt) = 0;
			cur_hnt += 2;
			size_t nlen = strlen(ctx->imports[i].name);
			memcpy(idata + cur_hnt, ctx->imports[i].name, nlen + 1);
			cur_hnt += (uint32_t)nlen + 1;
			if (cur_hnt & 1) cur_hnt++;

			*(uint64_t *)(idata + cur_ilt) = hnt_rva;
			*(uint64_t *)(idata + cur_iat) = hnt_rva;

			ctx->imports[i].iat_rva = idata_rva + cur_iat;

			cur_ilt += 8;
			cur_iat += 8;
		}
		cur_ilt += 8; /* zero terminator for this DLL's ILT/IAT arrays */
		cur_iat += 8;
	}

	/* 6. �?__imp_ 绗﹀彿璁剧疆 RVA 鎸囧�?IAT */
	for (int i = 0; i < ctx->sym_count; i++) {
		GlobalSymbol *s = &ctx->symbols[i];
		if (s->is_defined) continue;
		const char *func = import_func_name(s->name);
		const char *lookup = func;
		if (strcmp(s->name, "__intrinsic_setjmpex") == 0) lookup = "_setjmpex";

		for (int j = 0; j < ctx->import_count; j++) {
			if (strcmp(ctx->imports[j].name, lookup) == 0) {
				s->rva = ctx->imports[j].iat_rva;
				s->is_defined = 1;
				break;
			}
		}
	}

	/* 6b. 淇�ˉ thunk �?jmp disp32 */
	if (text_for_stub) {
		for (int t = 0; t < thunk_count; t++) {
			const char *func = rename_import(thunk_list[t].name);
			uint32_t iat_rva_target = 0;
			for (int j = 0; j < ctx->import_count; j++) {
				if (strcmp(ctx->imports[j].name, func) == 0) {
					iat_rva_target = ctx->imports[j].iat_rva;
					break;
				}
			}
			if (iat_rva_target == 0) continue;
			uint32_t thunk_rva = text_for_stub->rva + thunk_list[t].text_offset;
			int32_t disp = (int32_t)(iat_rva_target - (thunk_rva + 6));
			memcpy(text_for_stub->data + thunk_list[t].text_offset + 2, &disp, 4);
		}
	}

	/* 7. 搴旂敤閲嶅畾�?*/
	int unresolved_errs = 0;
	for (int oi = 0; oi < ctx->obj_count; oi++) {
		ObjFile *obj = &ctx->objs[oi];
		for (int si = 0; si < obj->hdr->NumberOfSections; si++) {
			CoffSectionHeader *sh = &obj->sections[si];
			if (sh->NumberOfRelocations == 0) continue;

			const char *sname = section_name(obj, sh);
			char base_name[16] = {0};
			get_base_section(sname, base_name, sizeof(base_name));

			if (strcmp(base_name, ".reloc") == 0 || strcmp(base_name, ".pdata") == 0 ||
			    strcmp(base_name, ".xdata") == 0 || strncmp(base_name, ".debug", 6) == 0)
				continue;

			MergedSection *ms = find_merged(merged, merged_count, base_name);
			if (!ms) continue;

			uint32_t piece_offset = 0;
			int found_piece = 0;
			for (int j = 0; j < ms->piece_count; j++) {
				if (ms->pieces[j].obj_idx == oi && ms->pieces[j].sec_idx == si + 1) {
					piece_offset = ms->pieces[j].offset;
					found_piece = 1;
					break;
				}
			}
			if (!found_piece) continue;

			CoffRelocation *relocs = (CoffRelocation *)(obj->data + sh->PointerToRelocations);
			for (int r = 0; r < sh->NumberOfRelocations; r++) {
				CoffRelocation *rel = &relocs[r];
				uint32_t patch_off = piece_offset + rel->VirtualAddress;
				if (patch_off + 4 > ms->size) continue;

				CoffSymbol *tsym = &obj->symbols[rel->SymbolTableIndex];
				const char *tname = coff_sym_name(obj, tsym);

				uint32_t target_rva = 0;

				if (tsym->StorageClass == 3 && tsym->SectionNumber > 0) {
					const char *tsname = section_name(obj, &obj->sections[tsym->SectionNumber - 1]);
					char tbase[16] = {0};
					get_base_section(tsname, tbase, sizeof(tbase));
					MergedSection *tms = find_merged(merged, merged_count, tbase);
					if (tms) {
						for (int j = 0; j < tms->piece_count; j++) {
							if (tms->pieces[j].obj_idx == oi &&
							    tms->pieces[j].sec_idx == tsym->SectionNumber) {
								target_rva = tms->rva + tms->pieces[j].offset + tsym->Value;
								break;
							}
						}
					}
				} else {
					GlobalSymbol *gs = ctx_find_sym(ctx, tname);
					if (gs && gs->is_defined) {
						target_rva = gs->rva;
					} else {
						if (strncmp(tname, ".", 1) == 0) {
							MergedSection *tms = find_merged(merged, merged_count, tname);
							if (tms) target_rva = tms->rva;
						}
						if (target_rva == 0) {
						fprintf(stderr, "error: linker: unresolved symbol '%s'\n"
						        "  This symbol is used but never defined.\n"
						        "  If it is a variable, declare it with 'var'.\n"
						        "  If it is a DLL function, use 'import-ext'.\n\n", tname);
							unresolved_errs++;
							continue;
						}
					}
				}

				uint32_t patch_rva = ms->rva + patch_off;
				int32_t *patch_ptr = (int32_t *)(ms->data + patch_off);
				int32_t addend = *patch_ptr;

				switch (rel->Type) {
				case IMAGE_REL_AMD64_REL32:
					*patch_ptr = (int32_t)(target_rva - (patch_rva + 4) + addend);
					break;
				case IMAGE_REL_AMD64_REL32_1:
					*patch_ptr = (int32_t)(target_rva - (patch_rva + 5) + addend);
					break;
				case IMAGE_REL_AMD64_REL32_2:
					*patch_ptr = (int32_t)(target_rva - (patch_rva + 6) + addend);
					break;
				case IMAGE_REL_AMD64_REL32_3:
					*patch_ptr = (int32_t)(target_rva - (patch_rva + 7) + addend);
					break;
				case IMAGE_REL_AMD64_REL32_4:
					*patch_ptr = (int32_t)(target_rva - (patch_rva + 8) + addend);
					break;
				case IMAGE_REL_AMD64_REL32_5:
					*patch_ptr = (int32_t)(target_rva - (patch_rva + 9) + addend);
					break;
				case IMAGE_REL_AMD64_ADDR32NB:
					*patch_ptr = (int32_t)(target_rva + addend);
					break;
				case IMAGE_REL_AMD64_ADDR64: {
					int64_t *p64 = (int64_t *)(ms->data + patch_off);
					*p64 = (int64_t)(IMAGE_BASE + target_rva) + *p64;
					break;
				}
				case IMAGE_REL_AMD64_ADDR32:
					*patch_ptr = (int32_t)(IMAGE_BASE + target_rva + addend);
					break;
				case IMAGE_REL_AMD64_SECREL:
					*patch_ptr = (int32_t)(target_rva - ms->rva + addend);
					break;
				default:
					fprintf(stderr, "warning: unsupported relocation type 0x%x\n", rel->Type);
					break;
				}
			}
		}
	}

	if (unresolved_errs > 0) return 0;

	/* 8. 鍐欏�?PE */
	FILE *out = fopen(out_path, "wb");
	if (!out) {
		fprintf(stderr, "error: cannot create output file '%s'\n", out_path);
		free(thunk_list);
		free(dll_names);
		return 0;
	}

	/* DOS header */
	uint8_t dos[64];
	memset(dos, 0, sizeof(dos));
	dos[0] = 'M'; dos[1] = 'Z';
	*(uint32_t *)(dos + 60) = 64;
	fwrite(dos, 1, 64, out);

	/* PE signature */
	uint32_t pe_sig = 0x00004550;
	fwrite(&pe_sig, 4, 1, out);

	/* COFF header */
	CoffHeader pe_coff = {0};
	pe_coff.Machine = 0x8664;
	pe_coff.NumberOfSections = (uint16_t)pe_sections;
	pe_coff.SizeOfOptionalHeader = 240;
	pe_coff.Characteristics = 0x0022;
	fwrite(&pe_coff, sizeof(pe_coff), 1, out);

	/* Entry point */
	uint32_t entry_rva = 0;
	GlobalSymbol *main_sym = ctx_find_sym(ctx, "main");
	if (main_sym && main_sym->is_defined) {
		entry_rva = main_sym->rva;
	} else {
		GlobalSymbol *mira_main = ctx_find_sym(ctx, "mira_main");
		if (mira_main && mira_main->is_defined) entry_rva = mira_main->rva;
	}

	/* Optional header (PE32+) */
	uint8_t opt_buf[240];
	memset(opt_buf, 0, sizeof(opt_buf));
	PE64OptionalHeader *opt = (PE64OptionalHeader *)opt_buf;
	opt->Magic = 0x020b;
	opt->MajorLinkerVersion = 1;
	opt->SizeOfCode = text_sec ? align_up(text_sec->size, FILE_ALIGN) : 0;
	opt->SizeOfInitializedData = (data_sec ? align_up(data_sec->size, FILE_ALIGN) : 0) +
	                             (rdata_sec ? align_up(rdata_sec->size, FILE_ALIGN) : 0) +
	                             idata_raw;
	opt->SizeOfUninitializedData = bss_sec ? align_up(bss_sec->size, SECTION_ALIGN) : 0;
	opt->AddressOfEntryPoint = entry_rva;
	opt->BaseOfCode = text_sec ? text_sec->rva : SECTION_ALIGN;
	opt->ImageBase = IMAGE_BASE;
	opt->SectionAlignment = SECTION_ALIGN;
	opt->FileAlignment = FILE_ALIGN;
	opt->MajorOperatingSystemVersion = 6;
	opt->MinorOperatingSystemVersion = 0;
	opt->MajorSubsystemVersion = 6;
	opt->SizeOfImage = image_size;
	opt->SizeOfHeaders = headers_size;
	opt->Subsystem = 3;
	opt->DllCharacteristics = 0x8120;
	opt->SizeOfStackReserve = 0x100000;
	opt->SizeOfStackCommit = 0x1000;
	opt->SizeOfHeapReserve = 0x100000;
	opt->SizeOfHeapCommit = 0x1000;
	opt->NumberOfRvaAndSizes = 16;

	DataDirectory *dirs = (DataDirectory *)(opt_buf + 112);
	dirs[1].VirtualAddress = idata_rva;
	dirs[1].Size = idt_size;
	dirs[12].VirtualAddress = idata_rva + iat_offset;
	dirs[12].Size = iat_size;

	fwrite(opt_buf, 1, 240, out);

	/* Section headers */
	for (int i = 0; i < merged_count; i++) {
		PeSectionHeader sh = {0};
		memcpy(sh.Name, merged[i].name, 8);
		sh.VirtualSize = merged[i].size;
		sh.VirtualAddress = merged[i].rva;
		sh.SizeOfRawData = merged[i].raw_size;
		sh.PointerToRawData = merged[i].file_offset;
		sh.Characteristics = merged[i].characteristics;
		if (strcmp(merged[i].name, ".text") == 0)
			sh.Characteristics |= 0x60000020;
		else if (strcmp(merged[i].name, ".data") == 0 || strcmp(merged[i].name, ".bss") == 0)
			sh.Characteristics |= 0xC0000040;
		else if (strcmp(merged[i].name, ".rdata") == 0)
			sh.Characteristics |= 0x40000040;
		fwrite(&sh, sizeof(sh), 1, out);
	}

	/* .idata section header */
	{
		PeSectionHeader sh = {0};
		memcpy(sh.Name, ".idata", 6);
		sh.VirtualSize = idata_size;
		sh.VirtualAddress = idata_rva;
		sh.SizeOfRawData = idata_raw;
		sh.PointerToRawData = idata_file;
		sh.Characteristics = 0xC0000040;
		fwrite(&sh, sizeof(sh), 1, out);
	}

	/* Pad headers */
	long pos = ftell(out);
	while (pos < (long)headers_size) {
		fputc(0, out);
		pos++;
	}

	/* Write section data */
	for (int i = 0; i < merged_count; i++) {
		if (merged[i].raw_size == 0) continue;
		fseek(out, merged[i].file_offset, SEEK_SET);
		fwrite(merged[i].data, 1, merged[i].size, out);
		for (uint32_t p = merged[i].size; p < merged[i].raw_size; p++)
			fputc(0, out);
	}

	/* Write .idata */
	fseek(out, idata_file, SEEK_SET);
	fwrite(idata, 1, idata_size, out);
	for (uint32_t p = idata_size; p < idata_raw; p++)
		fputc(0, out);

	fclose(out);

	/* Cleanup */
	for (int i = 0; i < merged_count; i++) {
		free(merged[i].data);
		free(merged[i].pieces);
	}
	free(idata);
	free(thunk_list);
	free(dll_names);

	return 1;
}

