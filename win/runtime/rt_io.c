/* rt_io.c - Console input functions */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

long long mira_read_int(void) {
	long long x;
	if (scanf("%lld", &x) != 1) return 0;
	return x;
}

char *mira_input(void) {
	static char buf[4096];
	if (!fgets(buf, sizeof(buf), stdin)) return NULL;
	size_t len = strlen(buf);
	if (len > 0 && buf[len - 1] == '\n') { buf[len - 1] = '\0'; len--; }
	char *s = malloc(len + 1);
	if (!s) return NULL;
	memcpy(s, buf, len + 1);
	return s;
}
