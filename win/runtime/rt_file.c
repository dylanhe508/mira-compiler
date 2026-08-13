/* rt_file.c - File operations */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

char *mira_file_read(const char *path) {
	if (!path) return NULL;
	FILE *f = fopen(path, "rb");
	if (!f) return NULL;
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (sz < 0) { fclose(f); return NULL; }
	char *buf = malloc((size_t)sz + 1);
	if (!buf) { fclose(f); return NULL; }
	size_t n = fread(buf, 1, (size_t)sz, f);
	buf[n] = '\0';
	fclose(f);
	return buf;
}

long long mira_file_write(const char *path, const char *content) {
	if (!path) return -1;
	if (!content) content = "";
	FILE *f = fopen(path, "wb");
	if (!f) return -1;
	size_t len = strlen(content);
	if (len > 0 && fwrite(content, 1, len, f) != len) { fclose(f); return -1; }
	fclose(f);
	return 0;
}

long long mira_file_append(const char *path, const char *content) {
	if (!path) return -1;
	if (!content) content = "";
	FILE *f = fopen(path, "ab");
	if (!f) return -1;
	size_t len = strlen(content);
	if (fwrite(content, 1, len, f) != len) { fclose(f); return -1; }
	fclose(f);
	return 0;
}

long long mira_file_exists(const char *path) {
	if (!path) return 0;
	FILE *f = fopen(path, "rb");
	if (!f) return 0;
	fclose(f);
	return 1;
}

long long mira_file_delete(const char *path) {
	if (!path) return -1;
	return remove(path) == 0 ? 0 : -1;
}
