#define WIN32_LEAN_AND_MEAN
#include <stdint.h>
#include <stdio.h>
#include <windows.h>

static uint64_t clock_ns(void) {
    LARGE_INTEGER now, frequency;
    QueryPerformanceCounter(&now);
    QueryPerformanceFrequency(&frequency);
    return (uint64_t)((now.QuadPart * UINT64_C(1000000000)) / (uint64_t)frequency.QuadPart);
}

static uint64_t analyze_orders(uint64_t n) {
    uint64_t revenue = 0, refunds = 0, pending = 0, score = 0;
    for (uint64_t i = 0; i < n; ++i) {
        if ((i & 3) == 0) {
            revenue += (((i * 13 + 7) & 16383) + 100) * ((i & 7) + 1);
            score += (((i * 13 + 7) & 16383) + 100) * ((i & 7) + 1) * (((i * 3 + 1) & 15) + 1) + ((i * 17 + 3) & 65535) + ((i * 29 + 5) & 4095);
        } else if ((i & 3) == 1) {
            refunds += (((i * 13 + 7) & 16383) + 100) * ((i & 7) + 1);
        } else {
            pending += 1;
        }
    }
    return revenue + refunds + pending + score;
}

int main(void) {
    uint64_t start = clock_ns();
    uint64_t checksum = analyze_orders(UINT64_C(5000000));
    uint64_t finish = clock_ns();
    puts("checksum=");
    printf("%lld\n", (long long)checksum);
    puts("elapsed_ns=");
    printf("%lld\n", (long long)(finish - start));
    return 0;
}
