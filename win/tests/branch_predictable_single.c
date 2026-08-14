#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <windows.h>

static int64_t clock_ns(void) {
    static LARGE_INTEGER f;
    LARGE_INTEGER n;
    if (!f.QuadPart) QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&n);
    return (int64_t)(n.QuadPart * INT64_C(1000000000) / f.QuadPart);
}

static int64_t work(int64_t n) {
    uint64_t sum = 0, x = 1;
    for (int64_t i = 0; i < n; ++i) {
        x = x * UINT64_C(6364136223846793005) + 1;
        if (i < n - 1) sum += x;
        else sum -= x;
    }
    return (int64_t)sum;
}

int main(void) {
    int64_t begin = clock_ns();
    int64_t result = work(INT64_C(50000000));
    int64_t end = clock_ns();
    printf("%" PRId64 "\n%" PRId64 "\n", result, end - begin);
    return 0;
}
