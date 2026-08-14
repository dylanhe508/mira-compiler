#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

static int64_t cell(int64_t r, int64_t c, int64_t seed) {
    return (r * 73856093) ^ (c * 19349663) ^ seed;
}

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

int main(void) {
    printf("%" PRId64 "\n", stencil(193, 257, 350));
    return 0;
}
