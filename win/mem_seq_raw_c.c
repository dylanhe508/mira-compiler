#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static int64_t ns(void) {
    LARGE_INTEGER c, f;
    QueryPerformanceCounter(&c);
    QueryPerformanceFrequency(&f);
    return (c.QuadPart / f.QuadPart) * 1000000000LL +
           (c.QuadPart % f.QuadPart) * 1000000000LL / f.QuadPart;
}

int main(void) {
    const int64_t n = 16777216;
    volatile int64_t *p = (volatile int64_t *)malloc((size_t)n * sizeof(*p));
    if (!p) return 2;
    int64_t t0 = ns();
    for (int r = 0; r < 4; ++r)
        for (int64_t i = 0; i < n; ++i) p[i] = i;
    int64_t t1 = ns();
    int64_t sum = 0;
    int64_t t2 = ns();
    for (int r = 0; r < 4; ++r)
        for (int64_t i = 0; i < n; ++i) sum += p[i];
    int64_t t3 = ns();
    printf("%lld\n%lld\n%lld\n", (long long)sum,
           (long long)(t1 - t0), (long long)(t3 - t2));
    free((void *)p);
    return 0;
}
