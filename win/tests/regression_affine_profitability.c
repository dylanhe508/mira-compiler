#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

static int64_t wrap_add(int64_t a, int64_t b) {
    return (int64_t)((uint64_t)a + (uint64_t)b);
}
static int64_t wrap_mul(int64_t a, int64_t b) {
    return (int64_t)((uint64_t)a * (uint64_t)b);
}
int main(void) {
    static const int64_t factors[] = {3, 5, 7, 11, 13, 17, 19, 23, 23};
    int64_t total = 0;
    for (int64_t i = 3; i < 103; ++i)
        for (unsigned j = 0; j < sizeof(factors) / sizeof(factors[0]); ++j)
            total = wrap_add(total, wrap_mul(i, factors[j]));
    printf("%" PRId64 "\n", total);
    return 0;
}
