#include <windows.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
__attribute__((noinline))
static uint64_t work(uint64_t n) {
    uint64_t x = 1, sum = 0, i = 0;
    while (i < n) {
        x = (x * 6364136223846793005ULL) + 1ULL;
        sum = sum + x;
        i = i + 1;
    }
    return sum;
}
static int64_t clock_ns(void) {
    LARGE_INTEGER c, f; QueryPerformanceCounter(&c); QueryPerformanceFrequency(&f);
    return (c.QuadPart / f.QuadPart) * 1000000000LL + (c.QuadPart % f.QuadPart) * 1000000000LL / f.QuadPart;
}
int main(void) {
    volatile uint64_t n = 50000000ULL;
    int64_t s = clock_ns();
    uint64_t r = work(n);
    int64_t e = clock_ns();
    printf("result=\n%" PRIu64 "\nelapsed_ns=\n%" PRId64 "\n", r, e - s);
    return 0;
}
