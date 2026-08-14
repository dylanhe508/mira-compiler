#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

static int64_t wrap_add(int64_t a, int64_t b) {
    return (int64_t)((uint64_t)a + (uint64_t)b);
}
static int64_t wrap_mul(int64_t a, int64_t b) {
    return (int64_t)((uint64_t)a * (uint64_t)b);
}

static int64_t slot_0(void) {
    int64_t total = 0;
    for (int64_t i = 1; i < 41; ++i)
        total = wrap_add(total, wrap_add(wrap_mul(i, 17), 11));
    return total;
}

static int64_t slot_1(void) {
    int64_t total = 0;
    for (int64_t i = 2; i < 42; ++i)
        total = wrap_add(total, wrap_add(wrap_mul(i, 19), 12));
    return total;
}

static int64_t slot_2(void) {
    int64_t total = 0;
    for (int64_t i = 3; i < 43; ++i)
        total = wrap_add(total, wrap_add(wrap_mul(i, 21), 13));
    return total;
}

static int64_t slot_3(void) {
    int64_t total = 0;
    for (int64_t i = 4; i < 44; ++i)
        total = wrap_add(total, wrap_add(wrap_mul(i, 23), 14));
    return total;
}

static int64_t slot_4(void) {
    int64_t total = 0;
    for (int64_t i = 5; i < 45; ++i)
        total = wrap_add(total, wrap_add(wrap_mul(i, 25), 15));
    return total;
}

static int64_t slot_5(void) {
    int64_t total = 0;
    for (int64_t i = 6; i < 46; ++i)
        total = wrap_add(total, wrap_add(wrap_mul(i, 27), 16));
    return total;
}

static int64_t slot_6(void) {
    int64_t total = 0;
    for (int64_t i = 7; i < 47; ++i)
        total = wrap_add(total, wrap_add(wrap_mul(i, 29), 17));
    return total;
}

static int64_t slot_7(void) {
    int64_t total = 0;
    for (int64_t i = 8; i < 48; ++i)
        total = wrap_add(total, wrap_add(wrap_mul(i, 31), 18));
    return total;
}

static int64_t slot_8(void) {
    int64_t total = 0;
    for (int64_t i = 9; i < 49; ++i)
        total = wrap_add(total, wrap_add(wrap_mul(i, 33), 19));
    return total;
}

static int64_t slot_9(void) {
    int64_t total = 0;
    for (int64_t i = 10; i < 50; ++i)
        total = wrap_add(total, wrap_add(wrap_mul(i, 35), 20));
    return total;
}

static int64_t slot_10(void) {
    int64_t total = 0;
    for (int64_t i = 11; i < 51; ++i)
        total = wrap_add(total, wrap_add(wrap_mul(i, 37), 21));
    return total;
}

static int64_t slot_11(void) {
    int64_t total = 0;
    for (int64_t i = 12; i < 52; ++i)
        total = wrap_add(total, wrap_add(wrap_mul(i, 39), 22));
    return total;
}

static int64_t slot_12(void) {
    int64_t total = 0;
    for (int64_t i = 13; i < 53; ++i)
        total = wrap_add(total, wrap_add(wrap_mul(i, 41), 23));
    return total;
}

static int64_t slot_13(void) {
    int64_t total = 0;
    for (int64_t i = 14; i < 54; ++i)
        total = wrap_add(total, wrap_add(wrap_mul(i, 43), 24));
    return total;
}

static int64_t slot_14(void) {
    int64_t total = 0;
    for (int64_t i = 15; i < 55; ++i)
        total = wrap_add(total, wrap_add(wrap_mul(i, 45), 25));
    return total;
}

static int64_t slot_15(void) {
    int64_t total = 0;
    for (int64_t i = 16; i < 56; ++i)
        total = wrap_add(total, wrap_add(wrap_mul(i, 47), 26));
    return total;
}

static int64_t slot_16(void) {
    int64_t total = 0;
    for (int64_t i = 17; i < 57; ++i)
        total = wrap_add(total, wrap_add(wrap_mul(i, 49), 27));
    return total;
}

static int64_t slot_17(void) {
    int64_t total = 0;
    for (int64_t i = 18; i < 58; ++i)
        total = wrap_add(total, wrap_add(wrap_mul(i, 51), 28));
    return total;
}

static int64_t slot_18(void) {
    int64_t total = 0;
    for (int64_t i = 19; i < 59; ++i)
        total = wrap_add(total, wrap_add(wrap_mul(i, 53), 29));
    return total;
}

static int64_t slot_19(void) {
    int64_t total = 0;
    for (int64_t i = 20; i < 60; ++i)
        total = wrap_add(total, wrap_add(wrap_mul(i, 55), 30));
    return total;
}

int main(void) {
    int64_t checksum = 0;
    checksum = wrap_add(wrap_mul(checksum, 131), slot_0());
    checksum = wrap_add(wrap_mul(checksum, 131), slot_1());
    checksum = wrap_add(wrap_mul(checksum, 131), slot_2());
    checksum = wrap_add(wrap_mul(checksum, 131), slot_3());
    checksum = wrap_add(wrap_mul(checksum, 131), slot_4());
    checksum = wrap_add(wrap_mul(checksum, 131), slot_5());
    checksum = wrap_add(wrap_mul(checksum, 131), slot_6());
    checksum = wrap_add(wrap_mul(checksum, 131), slot_7());
    checksum = wrap_add(wrap_mul(checksum, 131), slot_8());
    checksum = wrap_add(wrap_mul(checksum, 131), slot_9());
    checksum = wrap_add(wrap_mul(checksum, 131), slot_10());
    checksum = wrap_add(wrap_mul(checksum, 131), slot_11());
    checksum = wrap_add(wrap_mul(checksum, 131), slot_12());
    checksum = wrap_add(wrap_mul(checksum, 131), slot_13());
    checksum = wrap_add(wrap_mul(checksum, 131), slot_14());
    checksum = wrap_add(wrap_mul(checksum, 131), slot_15());
    checksum = wrap_add(wrap_mul(checksum, 131), slot_16());
    checksum = wrap_add(wrap_mul(checksum, 131), slot_17());
    checksum = wrap_add(wrap_mul(checksum, 131), slot_18());
    checksum = wrap_add(wrap_mul(checksum, 131), slot_19());
    printf("%" PRId64 "\n", checksum);
    return 0;
}
