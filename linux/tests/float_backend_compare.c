#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>

static int64_t clock_ns(void) {
    LARGE_INTEGER counter, frequency;
    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&frequency);
    return (counter.QuadPart / frequency.QuadPart) * 1000000000LL +
           (counter.QuadPart % frequency.QuadPart) * 1000000000LL / frequency.QuadPart;
}

__attribute__((noinline)) static double float_muladd(int64_t n) {
    double x = 1.000001;
    for (int64_t i = 0; i < n; ++i)
        x = x * 1.00000001 + 0.00000003;
    return x;
}

__attribute__((noinline)) static double float_div(int64_t n) {
    double x = 1.000001;
    for (int64_t i = 0; i < n; ++i)
        x = x / 1.00000001 + 0.00000003;
    return x;
}

__attribute__((noinline)) static int64_t int_div_runtime(int64_t n) {
    int64_t sum = 0;
    for (int64_t i = 1; i <= n; ++i) {
        int64_t d = (i & 1023) + 3;
        sum += i / d;
    }
    return sum;
}

int main(void) {
    int64_t t0 = clock_ns();
    double a = float_muladd(20000000);
    int64_t t1 = clock_ns();
    printf("%.5f\n%lld\n", a, (long long)(t1 - t0));

    t0 = clock_ns();
    double b = float_div(20000000);
    t1 = clock_ns();
    printf("%.5f\n%lld\n", b, (long long)(t1 - t0));

    t0 = clock_ns();
    int64_t c = int_div_runtime(20000000);
    t1 = clock_ns();
    printf("%lld\n%lld\n", (long long)c, (long long)(t1 - t0));
    return 0;
}
