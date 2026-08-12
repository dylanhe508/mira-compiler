#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

static int64_t mix_a(int64_t x, int64_t i) {
    int64_t d = (i & 31) + 3;
    int64_t q = x / d;
    int64_t r = x % d;
    return (int64_t)(((uint64_t)q * 17 + (uint64_t)r * 13) ^
                     ((uint64_t)i * 65537));
}

static int64_t mix_b(int64_t x, int64_t i) {
    int64_t d = (i & 15) + 5;
    if ((x & 7) < 3)
        return (int64_t)(((uint64_t)x * 5 + (uint64_t)i * 11) ^
                         (uint64_t)(x / d));
    return (int64_t)((uint64_t)x * 9 - (uint64_t)i * 7 +
                     (uint64_t)(x % d));
}

static int64_t run(int64_t seed, int64_t n) {
    int64_t x = seed;
    uint64_t a = 1, b = 3, c = 7, d = 11;
    for (int64_t i = 1; i <= n; ++i) {
        x = (int64_t)((uint64_t)x * UINT64_C(2862933555777941757) +
                      UINT64_C(3037000493));
        x = mix_b(mix_a(x, i), i);
        if ((x & 15) == 0) a += (uint64_t)x;
        else if ((x & 15) < 5) b -= (uint64_t)x;
        else if ((x & 15) < 11) c ^= (uint64_t)x;
        else d += (uint64_t)x * (uint64_t)i;
        if ((i & 63) == 0) {
            a = (a ^ d) + b;
            b = b * 33 ^ c;
            c += d * 17;
            d ^= a;
        }
    }
    return (int64_t)((uint64_t)x + a * 3 + b * 5 + c * 7 + d * 11);
}

int main(void) {
    printf("%" PRId64 "\n", run(INT64_C(88172645463325252), 1));
    printf("%" PRId64 "\n", run(-INT64_C(9918273), 31));
    printf("%" PRId64 "\n", run(INT64_C(123456789), 257));
    printf("%" PRId64 "\n", run(-INT64_C(700000000000000001), 4099));
    printf("%" PRId64 "\n", run(INT64_C(17), 20003));
}
