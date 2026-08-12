/* rt_collection.c - List and Dictionary */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

extern void *mem_alloc(long long size);
extern void mem_free(void *ptr);

/* --- List --- */
void *mira_list_new(long long size) {
	if (size <= 0 || size > 0x7fffffff) return NULL;
	long long nbytes = 8 + size * 8;
	void *p = mem_alloc(nbytes);
	if (!p) return NULL;
	*(long long *)p = size;
	return p;
}

long long mira_list_len(void *ptr) {
	if (!ptr) return 0;
	return *(long long *)ptr;
}

long long mira_list_get(void *ptr, long long index) {
	if (!ptr) return 0;
	long long len = *(long long *)ptr;
	if (index < 0 || index >= len) return 0;
	return *(long long *)((char *)ptr + 8 + index * 8);
}

void mira_list_set(void *ptr, long long index, long long value) {
	if (!ptr) return;
	long long len = *(long long *)ptr;
	if (index < 0 || index >= len) return;
	*(long long *)((char *)ptr + 8 + index * 8) = value;
}

void mira_list_free(void *ptr) {
	mem_free(ptr);
}

void *mira_list_push(void *ptr, long long value) {
	if (!ptr) return NULL;
	long long *len = (long long *)ptr;
	long long n = *len;
	long long newcap = n + 1;
	void *p = realloc(ptr, 8 + newcap * 8);
	if (!p) return ptr;
	*(long long *)p = newcap;
	*(long long *)((char *)p + 8 + n * 8) = value;
	return p;
}

long long mira_list_pop(void *ptr) {
	if (!ptr) return 0;
	long long *len = (long long *)ptr;
	if (*len <= 0) return 0;
	(*len)--;
	return *(long long *)((char *)ptr + 8 + (*len) * 8);
}

/* --- Dictionary --- */
void *mira_dict_new(long long cap) {
	if (cap <= 0 || cap > 0x7fffffff) return NULL;
	long long nbytes = 16 + cap * 16;
	void *p = mem_alloc(nbytes);
	if (!p) return NULL;
	*(long long *)p = 0;
	*(long long *)((char *)p + 8) = cap;
	return p;
}

void mira_dict_set(void *ptr, long long key, long long value) {
	if (!ptr) return;
	long long *count = (long long *)ptr;
	long long *cap = (long long *)ptr + 1;
	long long *pairs = (long long *)ptr + 2;
	for (long long i = 0; i < *count; i++) {
		if (pairs[i * 2] == key) {
			pairs[i * 2 + 1] = value;
			return;
		}
	}
	if (*count < *cap) {
		pairs[*count * 2] = key;
		pairs[*count * 2 + 1] = value;
		(*count)++;
	}
}

long long mira_dict_get(void *ptr, long long key) {
	if (!ptr) return 0;
	long long count = *(long long *)ptr;
	long long *pairs = (long long *)ptr + 2;
	for (long long i = 0; i < count; i++) {
		if (pairs[i * 2] == key) return pairs[i * 2 + 1];
	}
	return 0;
}

long long mira_dict_has(void *ptr, long long key) {
	if (!ptr) return 0;
	long long count = *(long long *)ptr;
	long long *pairs = (long long *)ptr + 2;
	for (long long i = 0; i < count; i++) {
		if (pairs[i * 2] == key) return 1;
	}
	return 0;
}

void mira_dict_free(void *ptr) {
	mem_free(ptr);
}

void *mira_dict_keys(void *ptr) {
	if (!ptr) return NULL;
	long long count = *(long long *)ptr;
	if (count <= 0) return mira_list_new(0);
	void *keys = mira_list_new(count);
	if (!keys) return NULL;
	long long *pairs = (long long *)ptr + 2;
	for (long long i = 0; i < count; i++)
		mira_list_set(keys, (int)i, pairs[i * 2]);
	return keys;
}

long long mira_dict_count(void *ptr) {
	return ptr ? *(long long *)ptr : 0;
}
