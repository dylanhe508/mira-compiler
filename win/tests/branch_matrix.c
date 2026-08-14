#include <stdint.h>
#include <stdio.h>

static int64_t add_sub_imm(int64_t n) {
    int64_t sum = 0, x = 1;
    for (int64_t i = 0; i < n; ++i) {
        x = (int64_t)((uint64_t)x * UINT64_C(6364136223846793005) + 1);
        if (x < 0) sum += 7; else sum -= 11;
    }
    return sum;
}

static int64_t add_different(int64_t n) {
    int64_t sum = 0, x = 1;
    for (int64_t i = 0; i < n; ++i) {
        x = (int64_t)((uint64_t)x * UINT64_C(6364136223846793005) + 1);
        if (x < 0) sum += i; else sum += x;
    }
    return sum;
}

static int64_t select_values(int64_t n) {
    int64_t sum = 0, x = 1;
    for (int64_t i = 0; i < n; ++i) {
        x = (int64_t)((uint64_t)x * UINT64_C(6364136223846793005) + 1);
        int64_t chosen = x < 0 ? i : x;
        sum += chosen;
    }
    return sum;
}

int main(void) {
    printf("%lld\n%lld\n%lld\n", (long long)add_sub_imm(1000000),
           (long long)add_different(1000000),
           (long long)select_values(1000000));
    return 0;
}
