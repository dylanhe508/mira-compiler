#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

__attribute__((noinline))
static void add_arrays(int64_t *dst, const int64_t *left, const int64_t *right, int64_t n) {
    for (int64_t i = 0; i < n; ++i) {
        int64_t offset = i;
        dst[offset] = left[offset] + right[offset];
    }
}

static int64_t ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

int main(void) {
    int64_t n = 1048576;
    int64_t *left = malloc(n * 8), *right = malloc(n * 8), *dst = malloc(n * 8);
    for (int64_t i = 0; i < n; ++i) { left[i] = i*3+1; right[i] = i*5+7; }
    int64_t start = ns();
    for (int r = 0; r < 64; ++r) add_arrays(dst, left, right, n);
    int64_t checksum = 0;
    for (int64_t i = 0; i < n; ++i) checksum += dst[i];
    printf("%lld\n%lld\n", (long long)checksum, (long long)(ns() - start));
    return 0;
}
