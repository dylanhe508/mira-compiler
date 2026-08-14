/* rt_struct.c - Struct allocation */
#include <stdlib.h>
#include <stdio.h>

void *mira_struct_new(long long size) {
	void *p = calloc(1, (size_t)size);
	if (!p) { fprintf(stderr, "mira: struct alloc failed (%lld bytes)\n", size); exit(1); }
	return p;
}

void mira_struct_free(void *p) {
	if (p) free(p);
}
