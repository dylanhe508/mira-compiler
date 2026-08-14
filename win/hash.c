/*
 * hash.c �?通用字符串哈希表实现
 * FNV-1a 哈希 + 开放寻址 + 线性探�?
 */
#include "hash.h"
#include <stdlib.h>
#include <string.h>

/* FNV-1a 哈希 */
static uint32_t fnv1a(const char *key, size_t len) {
	uint32_t h = 0x811c9dc5u;
	for (size_t i = 0; i < len; i++) {
		h ^= (uint8_t)key[i];
		h *= 0x01000193u;
	}
	return h;
}

static uint32_t fnv1a_str(const char *key) {
	uint32_t h = 0x811c9dc5u;
	for (; *key; key++) {
		h ^= (uint8_t)*key;
		h *= 0x01000193u;
	}
	return h;
}

/* 向上取到 2 的幂 */
static int next_pow2(int v) {
	if (v < 16) return 16;
	v--;
	v |= v >> 1; v |= v >> 2; v |= v >> 4;
	v |= v >> 8; v |= v >> 16;
	return v + 1;
}

void ht_init(HashTable *ht, int initial_cap) {
	ht->cap = next_pow2(initial_cap);
	ht->count = 0;
	ht->entries = calloc(ht->cap, sizeof(HT_Entry));
}

void ht_free(HashTable *ht) {
	for (int i = 0; i < ht->cap; i++) {
		free(ht->entries[i].key);
	}
	free(ht->entries);
	ht->entries = NULL;
	ht->count = 0;
	ht->cap = 0;
}

/* 内部查找：返�?slot 索引（找�?key 或第一个空槽） */
static int ht_find_slot(const HT_Entry *entries, int cap,
                         const char *key, size_t len, uint32_t h) {
	int mask = cap - 1;
	int idx = h & mask;
	for (;;) {
		HT_Entry *e = (HT_Entry *)&entries[idx];
		if (!e->key) return idx;
		if (e->hash == h && e->len == len &&
		    memcmp(e->key, key, len) == 0)
			return idx;
		idx = (idx + 1) & mask;
	}
}

/* 扩容 */
static void ht_grow(HashTable *ht) {
	int new_cap = ht->cap * 2;
	HT_Entry *new_entries = calloc(new_cap, sizeof(HT_Entry));
	for (int i = 0; i < ht->cap; i++) {
		HT_Entry *e = &ht->entries[i];
		if (!e->key) continue;
		int idx = ht_find_slot(new_entries, new_cap,
		                        e->key, e->len, e->hash);
		new_entries[idx] = *e;
	}
	free(ht->entries);
	ht->entries = new_entries;
	ht->cap = new_cap;
}

/* ─── 公开 API ─── */

void ht_setn(HashTable *ht, const char *key, size_t len, void *value) {
	if (ht->count * 4 >= ht->cap * 3) ht_grow(ht); /* 75% 负载 */

	uint32_t h = fnv1a(key, len);
	int idx = ht_find_slot(ht->entries, ht->cap, key, len, h);
	HT_Entry *e = &ht->entries[idx];

	if (e->key) {
		/* 已存在，更新 value */
		e->value = value;
	} else {
		/* 新插�?*/
		e->key = malloc(len + 1);
		memcpy(e->key, key, len);
		e->key[len] = '\0';
		e->len = len;
		e->hash = h;
		e->value = value;
		ht->count++;
	}
}

void ht_set(HashTable *ht, const char *key, void *value) {
	ht_setn(ht, key, strlen(key), value);
}

void *ht_getn(const HashTable *ht, const char *key, size_t len) {
	if (ht->count == 0) return NULL;
	uint32_t h = fnv1a(key, len);
	int mask = ht->cap - 1;
	int idx = h & mask;
	for (;;) {
		const HT_Entry *e = &ht->entries[idx];
		if (!e->key) return NULL;
		if (e->hash == h && e->len == len &&
		    memcmp(e->key, key, len) == 0)
			return e->value;
		idx = (idx + 1) & mask;
	}
}

void *ht_get(const HashTable *ht, const char *key) {
	return ht_getn(ht, key, strlen(key));
}

/* --- 全局字符串驻留池 --- */
static HashTable intern_pool;
static int intern_init_done = 0;

const char *intern_string(const char *str, size_t len) {
	if (!intern_init_done) {
		ht_init(&intern_pool, 1024);
		intern_init_done = 1;
	}

	uint32_t h = fnv1a(str, len);
	if (intern_pool.count > 0) {
		int mask = intern_pool.cap - 1;
		int idx = h & mask;
		for (;;) {
			const HT_Entry *e = &intern_pool.entries[idx];
			if (!e->key) break;
			if (e->hash == h && e->len == len && memcmp(e->key, str, len) == 0) {
				return e->key;
			}
			idx = (idx + 1) & mask;
		}
	}

	if (intern_pool.count * 4 >= intern_pool.cap * 3) {
		ht_grow(&intern_pool);
	}

	int mask = intern_pool.cap - 1;
	int idx = h & mask;
	for (;;) {
		HT_Entry *e = &intern_pool.entries[idx];
		if (!e->key) {
			e->key = malloc(len + 1);
			memcpy(e->key, str, len);
			e->key[len] = '\0';
			e->len = len;
			e->hash = h;
			e->value = NULL;
			intern_pool.count++;
			return e->key;
		}
		idx = (idx + 1) & mask;
	}
}
