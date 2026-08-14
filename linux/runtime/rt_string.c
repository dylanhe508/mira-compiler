/* rt_string.c - String operations */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h> /* intptr_t:Windows 由 windows.h 间接提供,POSIX 需显式包含 */
#ifdef _WIN32
#include <windows.h>
#endif

long long mira_str_len(const char *s) {
	return s ? (long long)strlen(s) : 0;
}

char *mira_str_concat(const char *a, const char *b) {
	if (!a) a = "";
	if (!b) b = "";
	size_t la = strlen(a), lb = strlen(b);
	char *r = malloc(la + lb + 1);
	if (!r) return NULL;
	memcpy(r, a, la + 1);
	memcpy(r + la, b, lb + 1);
	return r;
}

char *mira_str_copy(const char *s) {
	if (!s) return NULL;
	size_t n = strlen(s) + 1;
	char *r = malloc(n);
	if (!r) return NULL;
	memcpy(r, s, n);
	return r;
}

long long mira_str_eq(const char *a, const char *b) {
	if (!a && !b) return 1;
	if (!a || !b) return 0;
	return strcmp(a, b) == 0 ? 1 : 0;
}

long long mira_str_contains(const char *s, const char *sub) {
	if (!s || !sub) return 0;
	return strstr(s, sub) != NULL ? 1 : 0;
}

long long mira_str_find(const char *s, const char *sub) {
	if (!s || !sub) return -1;
	const char *found = strstr(s, sub);
	if (!found) return -1;
	return (long long)(found - s);
}

char *mira_str_trim(const char *s) {
	if (!s) return NULL;
	while (*s && isspace((unsigned char)*s)) s++;
	size_t len = strlen(s);
	while (len > 0 && isspace((unsigned char)s[len - 1])) len--;
	char *r = malloc(len + 1);
	if (!r) return NULL;
	memcpy(r, s, len);
	r[len] = '\0';
	return r;
}

long long mira_str_at(const char *s, long long index) {
	if (!s || index < 0) return -1;
	size_t len = strlen(s);
	if ((size_t)index >= len) return -1;
	return (unsigned char)s[index];
}

char *mira_str_substr(const char *s, long long start, long long n) {
	if (!s || start < 0 || n <= 0) return NULL;
	size_t len = strlen(s);
	if ((size_t)start >= len) return mira_str_copy("");
	if ((size_t)(start + n) > len) n = (long long)(len - start);
	char *r = malloc((size_t)n + 1);
	if (!r) return NULL;
	memcpy(r, s + start, (size_t)n);
	r[n] = '\0';
	return r;
}

/* Type conversion */
char *mira_int_to_str(long long x) {
	char buf[32];
	snprintf(buf, sizeof(buf), "%lld", x);
	return mira_str_copy(buf);
}

char *mira_to_str(long long x) {
	if (x > 0x10000 || x < -0x7FFFFFFF) {
		const char *p = (const char *)(intptr_t)x;
#ifdef _WIN32
		if (!IsBadReadPtr(p, 1)) {
			int ok = 1;
			for (int i = 0; i < 4 && p[i]; i++) {
				unsigned char c = (unsigned char)p[i];
				if (c != 0 && c < 0x20 && c != '\t' && c != '\n' && c != '\r') { ok = 0; break; }
			}
			if (ok) return mira_str_copy(p);
		}
#else
		return mira_str_copy(p);
#endif
	}
	char buf[32];
	snprintf(buf, sizeof(buf), "%lld", x);
	return mira_str_copy(buf);
}

long long mira_str_to_int(const char *s) {
	if (!s) return 0;
	return (long long)strtoll(s, NULL, 10);
}

long long mira_int_to_float(long long i) {
	union { long long ll; double d; } u;
	u.d = (double)i;
	return u.ll;
}

long long mira_float_to_int(long long bits) {
	union { long long ll; double d; } u;
	u.ll = bits;
	return (long long)u.d;
}
