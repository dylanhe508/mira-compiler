#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

static int64_t branchy_recurrence(int64_t n) {
    int64_t checksum = 0, bucket0 = 0, bucket1 = 0, bucket2 = 0;
    for (int64_t i = 0; i < n; ++i) {
        int64_t sensor = ((i * 1103515245LL + 12345) >> 7) & 65535;
        int64_t adjusted = sensor * ((i & 31) + 1) + ((i >> 3) & 255);
        if ((adjusted & 7) < 3)
            bucket0 += adjusted;
        else if ((adjusted & 7) < 6)
            bucket1 += adjusted ^ (i * 17);
        else
            bucket2 += adjusted / ((i & 15) + 1);
        checksum += (bucket0 ^ bucket1) + bucket2 + adjusted;
    }
    return checksum + bucket0 + bucket1 + bucket2;
}

static int64_t branchy_repeated(int64_t n) {
    int64_t total = 0;
    for (int64_t i = 0; i < n; ++i) {
        if ((i & 1) == 0)
            total += i * 97;
        else
            total -= i * 97;
        total += i * 97;
    }
    return total;
}

static int64_t branchy_exclusive(int64_t n) {
    int64_t total = 0;
    for (int64_t i = 0; i < n; ++i) {
        if ((i & 1) == 0)
            total += i * 89;
        else
            total -= i * 89;
    }
    return total;
}

int main(void) {
    printf("%" PRId64 "\n", branchy_recurrence(20000));
    printf("%" PRId64 "\n", branchy_repeated(20000));
    printf("%" PRId64 "\n", branchy_exclusive(20000));
    return 0;
}
