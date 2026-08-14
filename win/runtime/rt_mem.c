/* rt_mem.c - Memory management */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

extern long long g_alloc_count;
extern long long g_alloc_bytes;

void *mem_alloc(long long size) {
	if (size <= 0 || size > 0x7fffffff) return NULL;
	void *p = malloc((size_t)size);
	if (p) {
		g_alloc_count++;
		g_alloc_bytes += size;
	}
	return p;
}

void mem_free(void *ptr) {
	if (!ptr) return;
	free(ptr);
}

void mem_move(void *dst, void *src, long long size) {
	if (!dst || !src || size <= 0) return;
	memmove(dst, src, (size_t)size);
}

void mem_erase(void *dst, long long size) {
	if (!dst || size <= 0) return;
	memset(dst, 0, (size_t)size);
}

void mira_mem_dump(void *addr, long long size) {
	unsigned char *p = (unsigned char *)addr;
	if (!p || size <= 0) {
		printf("[dump] invalid address or size (%p, %lld)\n", addr, size);
		return;
	}
	long long remaining = size;
	while (remaining > 0) {
		int line = (int)(remaining > 16 ? 16 : remaining);
		printf("%p: ", (void *)p);
		for (int i = 0; i < line; i++) printf("%02X ", p[i]);
		printf(" | ");
		for (int i = 0; i < line; i++) {
			unsigned char c = p[i];
			putchar(isprint(c) ? c : '.');
		}
		putchar('\n');
		p += line;
		remaining -= line;
	}
}
