#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#define MASK UINT64_C(0x7fffffff)

static uint64_t multi_return(int64_t x) {
    if (x < 0) return (uint64_t)(-x * 3 + 7);
    if (x == 0) return UINT64_C(19);
    return (uint64_t)(x * 5 - 11);
}

static uint64_t recursive_keep(int64_t n, uint64_t seed) {
    if (n <= 0) return seed & MASK;
    uint64_t keep = (seed * 131 + (uint64_t)n * 17 + 9) & MASK;
    uint64_t child =
        recursive_keep(n - 1, (seed + (uint64_t)n * 19 + 3) & MASK);
    if ((n & 1) == 0)
        return (keep + child + multi_return(n - 7)) & MASK;
    return (keep * 3 + child + multi_return(7 - n)) & MASK;
}

int main(void) {
    printf("%" PRIu64 "\n", recursive_keep(18, UINT64_C(987654321)));
    return 0;
}
