#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

__attribute__((noinline))
static int64_t add_sub_imm(int64_t n) {
    int64_t sum = 0, x = 1;
    for (int64_t i = 0; i < n; ++i) {
        x = (int64_t)((uint64_t)x * UINT64_C(6364136223846793005) + 1);
        if (x < 0) sum += 7; else sum -= 11;
    }
    return sum;
}

__attribute__((noinline))
static int64_t add_different(int64_t n) {
    int64_t sum = 0, x = 1;
    for (int64_t i = 0; i < n; ++i) {
        x = (int64_t)((uint64_t)x * UINT64_C(6364136223846793005) + 1);
        if (x < 0) sum += i; else sum += x;
    }
    return sum;
}

__attribute__((noinline))
static int64_t select_values(int64_t n) {
    int64_t sum = 0, x = 1;
    for (int64_t i = 0; i < n; ++i) {
        x = (int64_t)((uint64_t)x * UINT64_C(6364136223846793005) + 1);
        int64_t chosen = x < 0 ? i : x;
        sum += chosen;
    }
    return sum;
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
    int64_t start = clock_ns();
    int64_t a = add_sub_imm(100000000);
    int64_t b = add_different(100000000);
    int64_t c = select_values(100000000);
    int64_t end = clock_ns();
    printf("result=\n%" PRId64 "\n%" PRId64 "\n%" PRId64 "\n"
           "elapsed_ns=\n%" PRId64 "\n", a, b, c, end - start);
    return 0;
}
