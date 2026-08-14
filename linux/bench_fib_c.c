#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

__attribute__((noinline))
static uint64_t fib(uint64_t n) {
    uint64_t a = 0;
    uint64_t b = 1;
    for (uint64_t i = 0; i < n; ++i) {
        uint64_t next = a + b;
        a = b;
        b = next;
    }
    return b;
}

static int64_t clock_ns(void) {
    LARGE_INTEGER counter, frequency;
    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&frequency);
    return (counter.QuadPart / frequency.QuadPart) * 1000000000LL +
           (counter.QuadPart % frequency.QuadPart) * 1000000000LL /
               frequency.QuadPart;
}

int main(void) {
    volatile uint64_t n = 100000000ULL;
    int64_t start = clock_ns();
    uint64_t result = fib(n);
    int64_t end = clock_ns();
    printf("result=\n%" PRIu64 "\nelapsed_ns=\n%" PRId64 "\n",
           result, end - start);
    return 0;
}
