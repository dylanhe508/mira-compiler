/*
 * linker.c — Mira 链接器入口。
 *
 * 根据 ObjFormat 分发到 COFF/PE(Windows)或 ELF(Linux)后端:
 *   COFF 路径: load_obj        → collect_symbols    → resolve_imports    → write_pe
 *   ELF  路径: load_obj_elf    → collect_symbols_elf→ resolve_imports_elf→ write_elf
 *
 * 平台判断:用编译时宏 _WIN32 决定,Windows 上编译进 COFF 路径,
 *           非 Windows 上编译进 ELF 路径。--target 跨编译只影响代码
 *           生成(ABI/obj 格式),链接器后端始终匹配宿主平台。
 */
#include "linker.h"

/* === 段合并辅助函数(平台无关,PE/ELF 两个后端共用) ===
 * get_base_section:取段名主干,去掉 '$' 折叠后缀(COFF)或第二个点
 * 之后的部分(GCC .text.startup 等);返回主段名。
 * find_merged:在已合并段表中按主段名查找(前 8 字符比较,与
 * MergedSection.name 的定长存储一致)。 */

void get_base_section(const char *full_name, char *out, size_t out_size) {
	memset(out, 0, out_size);
	const char *dollar = strchr(full_name, '$');
	if (dollar) {
		size_t len = (size_t)(dollar - full_name);
		if (len >= out_size) len = out_size - 1;
		memcpy(out, full_name, len);
		return;
	}
	if (full_name[0] == '.') {
		const char *second_dot = strchr(full_name + 1, '.');
		if (second_dot) {
			size_t len = (size_t)(second_dot - full_name);
			if (len >= out_size) len = out_size - 1;
			memcpy(out, full_name, len);
			return;
		}
	}
	strncpy(out, full_name, out_size - 1);
}

MergedSection *find_merged(MergedSection *secs, int count, const char *name) {
	for (int i = 0; i < count; i++) {
		if (strncmp(secs[i].name, name, 8) == 0) return &secs[i];
	}
	return NULL;
}

int linker_run(const char **obj_paths, int obj_count, const char *out_path) {
	LinkerCtx ctx;
	ctx_init(&ctx);

	ctx.objs = calloc(obj_count, sizeof(ObjFile));
	ctx.obj_count = obj_count;

#ifdef _WIN32
	/* ====== Windows / COFF / PE 路径 ====== */
	for (int i = 0; i < obj_count; i++) {
		if (!load_obj(&ctx.objs[i], obj_paths[i])) return 1;
	}
	for (int i = 0; i < obj_count; i++) {
		collect_symbols(&ctx, i);
	}
	if (!resolve_imports(&ctx)) {
		return 1;
	}
	if (!write_pe(&ctx, out_path)) {
		return 1;
	}
#else
	/* ====== Linux / ELF 路径 ====== */
	for (int i = 0; i < obj_count; i++) {
		if (!load_obj_elf(&ctx.objs[i], obj_paths[i])) return 1;
	}
	for (int i = 0; i < obj_count; i++) {
		collect_symbols_elf(&ctx, i);
	}
	if (!resolve_imports_elf(&ctx)) {
		return 1;
	}
	if (!write_elf(&ctx, out_path)) {
		return 1;
	}
#endif

	/* Cleanup(两种格式共用:data 指向整个文件的 malloc 块) */
	for (int i = 0; i < obj_count; i++) free(ctx.objs[i].data);
	free(ctx.objs);
	for (int i = 0; i < ctx.sym_count; i++) free(ctx.symbols[i].name);
	free(ctx.symbols);
	for (int i = 0; i < ctx.import_count; i++) {
		free(ctx.imports[i].name);
		free(ctx.imports[i].dll_name);
	}
	free(ctx.imports);

	return 0;
}
