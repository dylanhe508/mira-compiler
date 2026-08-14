#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

static int64_t wrap_add(int64_t a, int64_t b) {
    return (int64_t)((uint64_t)a + (uint64_t)b);
}
static int64_t wrap_mul(int64_t a, int64_t b) {
    return (int64_t)((uint64_t)a * (uint64_t)b);
}
static int64_t constant_muls(int64_t x) {
    static const int64_t factors[] = {
        0, 1, -1, 2, -2, 8, -8, INT64_C(-4611686018427387904), 3
    };
    int64_t result = 0;
    for (unsigned i = 0; i < sizeof(factors) / sizeof(factors[0]); ++i)
        result = wrap_add(result, wrap_mul(x, factors[i]));
    return result;
}
int main(void) {
    printf("%" PRId64 "\n", constant_muls(123456789));
    printf("%" PRId64 "\n", constant_muls(-987654321));
    return 0;
}
