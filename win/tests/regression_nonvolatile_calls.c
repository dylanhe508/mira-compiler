#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#define MASK UINT64_C(0x7fffffff)

static uint64_t multi_return(int64_t x) {
    if (x < 0) return (uint64_t)(-x * 3 + 7);
    if (x == 0) return UINT64_C(19);
    return (uint64_t)(x * 5 - 11);
}

static uint64_t hot_step(uint64_t x, uint64_t salt) {
    uint64_t hot_mixed =
        (x * UINT64_C(6364136223846793005) + salt + UINT64_C(1)) & MASK;
    if ((salt & UINT64_C(1)) == 0)
        return (hot_mixed * UINT64_C(1442695040888963407) +
                UINT64_C(33)) & MASK;
    return (hot_mixed * UINT64_C(2862933555777941757) +
            UINT64_C(3037000493)) & MASK;
}

static uint64_t ssa_shape(uint64_t x) {
    return hot_step(x, 1) + hot_step(x, 2);
}

static uint64_t pressure_call(uint64_t x) {
    uint64_t a = x + 1;
    uint64_t b = x * 3 + 2;
    uint64_t c = x * 5 + 3;
    uint64_t d = x * 7 + 4;
    uint64_t e = x * 11 + 5;
    uint64_t f = x * 13 + 6;
    uint64_t g = x * 17 + 7;
    uint64_t h = x * 19 + 8;
    uint64_t i = x * 23 + 9;
    uint64_t j = x * 29 + 10;
    uint64_t k = x * 31 + 11;
    uint64_t l = x * 37 + 12;
    uint64_t called = hot_step(x, 99);
    return (called + a * 3 + b * 5 + c * 7 + d * 11 + e * 13 +
            f * 17 + g * 19 + h * 23 + i * 29 + j * 31 + k * 37 +
            l * 41) & MASK;
}

static uint64_t nested_outer(uint64_t x) {
    uint64_t keep = (x * 97 + 31) & MASK;
    uint64_t nested = ssa_shape(x + 5);
    uint64_t pressure = pressure_call(x + 9);
    uint64_t branches =
        multi_return(-5) + multi_return(0) + multi_return(9);
    return (keep + nested + pressure + branches) & MASK;
}

static uint64_t call_storm(uint64_t rounds) {
    uint64_t acc = 1;
    for (uint64_t i = 0; i < rounds; ++i)
        acc = hot_step(acc, i);
    return acc;
}

int main(void) {
    printf("%" PRIu64 "\n", nested_outer(12345));
    printf("%" PRIu64 "\n", pressure_call(24680));
    printf("%" PRIu64 "\n", call_storm(UINT64_C(4000000)));
    return 0;
}
