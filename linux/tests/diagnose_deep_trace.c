#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

static int64_t s1(int64_t x, int64_t i) {
    return (x & 1) == 0 ? x / 3 + i * 7 : (int64_t)((uint64_t)x * 5 - i);
}
static int64_t s2(int64_t x, int64_t i) {
    return (x & 7) < 3 ? x + i * 11 : x - i * 13;
}
static int64_t s3(int64_t x, int64_t i) {
    int64_t d = (i & 127) + 3;
    return x / d + x % d + i;
}
static int64_t s4(int64_t x, int64_t i) {
    return (i & 255) == 0 ? x ^ (i * 65537)
                          : (int64_t)((uint64_t)x * 17 + 29);
}

int main(void) {
    int64_t x = -9918273;
    uint64_t a = 1, b = 2, c = 3, d = 4;
    for (int64_t i = 1; i <= 512; ++i) {
        x = s4(s3(s2(s1(x, i), i), i), i);
        if ((x & 15) == 0) a += (uint64_t)x;
        else if ((x & 15) < 5) b -= (uint64_t)x;
        else if ((x & 15) < 11) c ^= (uint64_t)x;
        else d += (uint64_t)x * (uint64_t)i;
        printf("%" PRId64 "\n%" PRId64 "\n%" PRId64 "\n%" PRId64
               "\n%" PRId64 "\n", x, (int64_t)a, (int64_t)b,
               (int64_t)c, (int64_t)d);
    }
}
