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
    int64_t total = 0;
    for (int64_t i = 3; i < 103; ++i) {
        total = wrap_add(total, wrap_mul(i, 3));
        total = wrap_add(total, wrap_mul(i, 5));
        total = wrap_add(total, wrap_mul(i, 7));
        total = wrap_add(total, wrap_mul(i, 5));
    }
    printf("%" PRId64 "\n", total);
    return 0;
}
