/*
 * hash.h �?通用字符串哈希表
 * 支持 const char* �?void* 映射，开放寻址 + 线性探�?
 */
#ifndef HASH_H
#define HASH_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
	char   *key;       /* 拥有的副本（strdup�?*/
	size_t len;
	void   *value;
	uint32_t hash;     /* 缓存的哈希�?*/
} HT_Entry;

typedef struct {
	HT_Entry *entries;
	int count;
	int cap;           /* 必须�?2 的幂 */
} HashTable;

/* 初始化（cap 会向上取�?2 的幂，最�?16�?*/
void     ht_init(HashTable *ht, int initial_cap);
void     ht_free(HashTable *ht);

/* 设置/获取。key 会被复制。value �?NULL 表示删除 */
void     ht_set(HashTable *ht, const char *key, void *value);
void    *ht_get(const HashTable *ht, const char *key);

/* 字符串驻留（String Interning�?*/
const char *intern_string(const char *str, size_t len);

/* 带长度的版本（key 不需�?null 结尾�?*/
void     ht_setn(HashTable *ht, const char *key, size_t len, void *value);
void    *ht_getn(const HashTable *ht, const char *key, size_t len);

/* 便捷：存�?intptr_t �?*/
static inline void ht_set_int(HashTable *ht, const char *key, intptr_t val) {
	ht_set(ht, key, (void *)val);
}
static inline intptr_t ht_get_int(const HashTable *ht, const char *key) {
	return (intptr_t)ht_get(ht, key);
}

#endif /* HASH_H */
