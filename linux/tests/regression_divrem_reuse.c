#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

static int64_t wrap_add(int64_t left, int64_t right) {
    return (int64_t)((uint64_t)left + (uint64_t)right);
}

static int64_t wrap_mul(int64_t left, int64_t right) {
    return (int64_t)((uint64_t)left * (uint64_t)right);
}

static int64_t paired(int64_t value, int64_t divisor) {
    int64_t quotient = value / divisor;
    int64_t spacer = wrap_add(wrap_mul(quotient, 31), value);
    int64_t remainder = value % divisor;
    return wrap_add(wrap_add(wrap_mul(quotient, 131),
                             wrap_mul(remainder, 17)),
                    spacer);
}

static int64_t add_case(int64_t checksum, int64_t value, int64_t divisor) {
    return wrap_add(wrap_mul(checksum, 257), paired(value, divisor));
}

static int64_t unrelated_div(int64_t value, int64_t divisor) {
    return value / divisor;
}

int main(void) {
    int64_t checksum = 0;
    checksum = add_case(checksum, 0, 7);
    checksum = add_case(checksum, 0, -5);
    checksum = add_case(checksum, 17, 3);
    checksum = add_case(checksum, 17, -3);
    checksum = add_case(checksum, -17, 3);
    checksum = add_case(checksum, -17, -3);
    checksum = add_case(checksum, INT64_MAX - 1, 97);
    checksum = add_case(checksum, INT64_MIN + 1, -97);
    checksum = wrap_add(wrap_mul(checksum, 257), unrelated_div(100, 7));
    printf("%" PRId64 "\n", checksum);
    return 0;
}
