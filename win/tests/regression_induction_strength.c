#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

/* Keep the C oracle defined for every 64-bit bit pattern Mira can produce. */
static int64_t wrap_add(int64_t left, int64_t right) {
    return (int64_t)((uint64_t)left + (uint64_t)right);
}

static int64_t wrap_mul(int64_t left, int64_t right) {
    return (int64_t)((uint64_t)left * (uint64_t)right);
}

static int64_t affine(int64_t i, int64_t multiplier, int64_t offset) {
    return wrap_add(wrap_mul(i, multiplier), offset);
}

static int64_t positive_step(void) {
    int64_t total = 0;
    for (int64_t i = 3; i < 53; ++i)
        total = wrap_add(total, affine(i, 17, 11));
    return total;
}

static int64_t negative_step(void) {
    int64_t total = 0;
    for (int64_t i = 61; i > -19; --i)
        total = wrap_add(total, affine(i, -19, 23));
    return total;
}

static int64_t nonunit_step(void) {
    int64_t total = 0;
    for (int64_t i = -29; i < 79; i += 5)
        total = wrap_add(total, affine(i, 37, -101));
    return total;
}

static int64_t wraparound(void) {
    int64_t total = 0;
    int64_t i = INT64_MAX - 4;
    for (int64_t rounds = 0; rounds < 8; ++rounds) {
        total = wrap_add(total, affine(i, -7, 13));
        i = wrap_add(i, 1);
    }
    return total;
}

static int64_t nested_induction(void) {
    int64_t total = 0;
    for (int64_t i = 2; i < 23; i += 3) {
        total = wrap_add(total, affine(i, 13, 7));
        for (int64_t j = 31; j > 0; j -= 2)
            total = wrap_add(total, affine(j, -5, 19));
    }
    return total;
}

int main(void) {
    int64_t checksum = 0;
    checksum = wrap_add(wrap_mul(checksum, 131), positive_step());
    checksum = wrap_add(wrap_mul(checksum, 131), negative_step());
    checksum = wrap_add(wrap_mul(checksum, 131), nonunit_step());
    checksum = wrap_add(wrap_mul(checksum, 131), wraparound());
    checksum = wrap_add(wrap_mul(checksum, 131), nested_induction());
    printf("%" PRId64 "\n", checksum);
    return 0;
}
