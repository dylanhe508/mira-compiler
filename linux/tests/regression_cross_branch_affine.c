#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

static int64_t add_wrap(int64_t a, int64_t b) {
    return (int64_t)((uint64_t)a + (uint64_t)b);
}

static int64_t sub_wrap(int64_t a, int64_t b) {
    return (int64_t)((uint64_t)a - (uint64_t)b);
}

static int64_t mul_wrap(int64_t a, int64_t b) {
    return (int64_t)((uint64_t)a * (uint64_t)b);
}

int main(void) {
    int64_t total = 0;
    for (int64_t i = -37; i < 1963; ++i) {
        if ((i & 3) == 0)
            total = add_wrap(total, add_wrap(mul_wrap(i, 1103515245), 12345));
        else if ((i & 3) == 1)
            total = sub_wrap(total, sub_wrap(mul_wrap(i, 1103515245), 17));
        else
            total = add_wrap(total, add_wrap(mul_wrap(i, 17), 9));
    }
    printf("%" PRId64 "\n", total);
    return 0;
}
