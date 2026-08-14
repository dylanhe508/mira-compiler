/* Linux 版 fib 基线:与 bench_fib_c.c 等价,仅计时改用 clock_gettime。
 * 用法:gcc -O2 -o fib_gcc bench_fib_linux.c && ./fib_gcc
 * 对比 Mira 编译器生成的 ELF 与 GCC -O2 生成的代码质量。 */
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

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
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

int main(void) {
    volatile uint64_t n = 100000000ULL;
    int64_t start = clock_ns();
    uint64_t result = fib(n);
    int64_t end = clock_ns();
    printf("result=%" PRIu64 " elapsed_ns=%" PRId64 "\n", result, end - start);
    return 0;
}
