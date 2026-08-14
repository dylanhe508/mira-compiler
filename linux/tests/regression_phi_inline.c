#include <stdio.h>

static long long branch_callee(long long value) {
    if (value < 0) return value + 10;
    return value + 20;
}

static long long caller(long long flag, long long value) {
    long long result = branch_callee(value);
    if (flag) return result + 100;
    return result + 200;
}

int main(void) {
    printf("%lld\n", caller(0, -4));
    return 0;
}
