#include <time.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

__attribute__((noinline))
static int64_t cell(int64_t r, int64_t c, int64_t seed) {
    return (r * 73856093) ^ (c * 19349663) ^ seed;
}

__attribute__((noinline))
static int64_t stencil(int64_t rows, int64_t cols, int64_t rounds) {
    uint64_t total = 0;
    for (int64_t round = 0; round < rounds; ++round) {
        for (int64_t r = 1; r < rows - 1; ++r) {
            uint64_t row = 0;
            for (int64_t c = 1; c < cols - 1; ++c) {
                int64_t value = cell(r, c, round + 17) * 4;
                value += cell(r - 1, c, round + 17);
                value += cell(r + 1, c, round + 17);
                value += cell(r, c - 1, round + 17);
                value += cell(r, c + 1, round + 17);
                value /= 8;
                if ((value & 7) == 0) row -= (uint64_t)value;
                else row += (uint64_t)value;
            }
            total = total * 17 + row;
        }
    }
    return (int64_t)total;
}

static int64_t clock_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

int main(void) {
    int64_t start = clock_ns();
    int64_t result = stencil(193, 257, 350);
    int64_t end = clock_ns();
    printf("result=\n%" PRId64 "\nelapsed_ns=\n%" PRId64 "\n",
           result, end - start);
    return 0;
}
