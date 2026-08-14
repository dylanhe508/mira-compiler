#include <stdint.h>
#include <stdio.h>
static int64_t f0(int64_t x) {
    int64_t a = x * 1 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f1(int64_t x) {
    int64_t a = x * 2 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f2(int64_t x) {
    int64_t a = x * 3 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f3(int64_t x) {
    int64_t a = x * 4 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f4(int64_t x) {
    int64_t a = x * 5 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f5(int64_t x) {
    int64_t a = x * 6 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f6(int64_t x) {
    int64_t a = x * 7 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f7(int64_t x) {
    int64_t a = x * 8 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f8(int64_t x) {
    int64_t a = x * 9 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f9(int64_t x) {
    int64_t a = x * 10 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f10(int64_t x) {
    int64_t a = x * 11 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f11(int64_t x) {
    int64_t a = x * 12 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f12(int64_t x) {
    int64_t a = x * 13 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f13(int64_t x) {
    int64_t a = x * 14 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f14(int64_t x) {
    int64_t a = x * 15 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f15(int64_t x) {
    int64_t a = x * 16 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f16(int64_t x) {
    int64_t a = x * 17 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f17(int64_t x) {
    int64_t a = x * 18 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f18(int64_t x) {
    int64_t a = x * 19 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f19(int64_t x) {
    int64_t a = x * 20 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f20(int64_t x) {
    int64_t a = x * 21 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f21(int64_t x) {
    int64_t a = x * 22 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f22(int64_t x) {
    int64_t a = x * 23 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f23(int64_t x) {
    int64_t a = x * 24 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f24(int64_t x) {
    int64_t a = x * 25 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f25(int64_t x) {
    int64_t a = x * 26 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f26(int64_t x) {
    int64_t a = x * 27 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f27(int64_t x) {
    int64_t a = x * 28 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f28(int64_t x) {
    int64_t a = x * 29 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f29(int64_t x) {
    int64_t a = x * 30 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f30(int64_t x) {
    int64_t a = x * 31 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f31(int64_t x) {
    int64_t a = x * 32 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f32(int64_t x) {
    int64_t a = x * 33 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f33(int64_t x) {
    int64_t a = x * 34 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f34(int64_t x) {
    int64_t a = x * 35 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f35(int64_t x) {
    int64_t a = x * 36 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f36(int64_t x) {
    int64_t a = x * 37 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f37(int64_t x) {
    int64_t a = x * 38 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f38(int64_t x) {
    int64_t a = x * 39 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f39(int64_t x) {
    int64_t a = x * 40 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f40(int64_t x) {
    int64_t a = x * 41 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f41(int64_t x) {
    int64_t a = x * 42 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f42(int64_t x) {
    int64_t a = x * 43 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f43(int64_t x) {
    int64_t a = x * 44 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f44(int64_t x) {
    int64_t a = x * 45 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f45(int64_t x) {
    int64_t a = x * 46 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f46(int64_t x) {
    int64_t a = x * 47 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f47(int64_t x) {
    int64_t a = x * 48 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f48(int64_t x) {
    int64_t a = x * 49 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f49(int64_t x) {
    int64_t a = x * 50 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f50(int64_t x) {
    int64_t a = x * 51 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f51(int64_t x) {
    int64_t a = x * 52 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f52(int64_t x) {
    int64_t a = x * 53 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f53(int64_t x) {
    int64_t a = x * 54 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f54(int64_t x) {
    int64_t a = x * 55 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f55(int64_t x) {
    int64_t a = x * 56 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f56(int64_t x) {
    int64_t a = x * 57 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f57(int64_t x) {
    int64_t a = x * 58 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f58(int64_t x) {
    int64_t a = x * 59 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f59(int64_t x) {
    int64_t a = x * 60 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f60(int64_t x) {
    int64_t a = x * 61 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f61(int64_t x) {
    int64_t a = x * 62 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f62(int64_t x) {
    int64_t a = x * 63 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f63(int64_t x) {
    int64_t a = x * 64 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f64(int64_t x) {
    int64_t a = x * 65 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f65(int64_t x) {
    int64_t a = x * 66 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f66(int64_t x) {
    int64_t a = x * 67 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f67(int64_t x) {
    int64_t a = x * 68 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f68(int64_t x) {
    int64_t a = x * 69 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f69(int64_t x) {
    int64_t a = x * 70 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f70(int64_t x) {
    int64_t a = x * 71 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f71(int64_t x) {
    int64_t a = x * 72 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f72(int64_t x) {
    int64_t a = x * 73 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f73(int64_t x) {
    int64_t a = x * 74 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f74(int64_t x) {
    int64_t a = x * 75 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f75(int64_t x) {
    int64_t a = x * 76 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f76(int64_t x) {
    int64_t a = x * 77 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f77(int64_t x) {
    int64_t a = x * 78 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f78(int64_t x) {
    int64_t a = x * 79 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f79(int64_t x) {
    int64_t a = x * 80 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f80(int64_t x) {
    int64_t a = x * 81 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f81(int64_t x) {
    int64_t a = x * 82 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f82(int64_t x) {
    int64_t a = x * 83 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f83(int64_t x) {
    int64_t a = x * 84 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f84(int64_t x) {
    int64_t a = x * 85 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f85(int64_t x) {
    int64_t a = x * 86 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f86(int64_t x) {
    int64_t a = x * 87 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f87(int64_t x) {
    int64_t a = x * 88 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f88(int64_t x) {
    int64_t a = x * 89 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f89(int64_t x) {
    int64_t a = x * 90 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f90(int64_t x) {
    int64_t a = x * 91 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f91(int64_t x) {
    int64_t a = x * 92 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f92(int64_t x) {
    int64_t a = x * 93 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f93(int64_t x) {
    int64_t a = x * 94 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f94(int64_t x) {
    int64_t a = x * 95 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f95(int64_t x) {
    int64_t a = x * 96 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f96(int64_t x) {
    int64_t a = x * 97 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f97(int64_t x) {
    int64_t a = x * 98 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f98(int64_t x) {
    int64_t a = x * 99 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f99(int64_t x) {
    int64_t a = x * 100 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f100(int64_t x) {
    int64_t a = x * 101 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f101(int64_t x) {
    int64_t a = x * 102 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f102(int64_t x) {
    int64_t a = x * 103 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f103(int64_t x) {
    int64_t a = x * 104 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f104(int64_t x) {
    int64_t a = x * 105 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f105(int64_t x) {
    int64_t a = x * 106 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f106(int64_t x) {
    int64_t a = x * 107 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f107(int64_t x) {
    int64_t a = x * 108 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f108(int64_t x) {
    int64_t a = x * 109 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f109(int64_t x) {
    int64_t a = x * 110 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f110(int64_t x) {
    int64_t a = x * 111 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f111(int64_t x) {
    int64_t a = x * 112 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f112(int64_t x) {
    int64_t a = x * 113 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f113(int64_t x) {
    int64_t a = x * 114 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f114(int64_t x) {
    int64_t a = x * 115 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f115(int64_t x) {
    int64_t a = x * 116 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f116(int64_t x) {
    int64_t a = x * 117 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f117(int64_t x) {
    int64_t a = x * 118 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f118(int64_t x) {
    int64_t a = x * 119 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f119(int64_t x) {
    int64_t a = x * 120 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f120(int64_t x) {
    int64_t a = x * 121 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f121(int64_t x) {
    int64_t a = x * 122 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f122(int64_t x) {
    int64_t a = x * 123 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f123(int64_t x) {
    int64_t a = x * 124 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f124(int64_t x) {
    int64_t a = x * 125 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f125(int64_t x) {
    int64_t a = x * 126 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f126(int64_t x) {
    int64_t a = x * 127 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f127(int64_t x) {
    int64_t a = x * 128 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f128(int64_t x) {
    int64_t a = x * 129 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f129(int64_t x) {
    int64_t a = x * 130 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f130(int64_t x) {
    int64_t a = x * 131 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f131(int64_t x) {
    int64_t a = x * 132 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f132(int64_t x) {
    int64_t a = x * 133 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f133(int64_t x) {
    int64_t a = x * 134 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f134(int64_t x) {
    int64_t a = x * 135 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f135(int64_t x) {
    int64_t a = x * 136 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f136(int64_t x) {
    int64_t a = x * 137 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f137(int64_t x) {
    int64_t a = x * 138 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f138(int64_t x) {
    int64_t a = x * 139 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f139(int64_t x) {
    int64_t a = x * 140 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f140(int64_t x) {
    int64_t a = x * 141 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f141(int64_t x) {
    int64_t a = x * 142 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f142(int64_t x) {
    int64_t a = x * 143 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f143(int64_t x) {
    int64_t a = x * 144 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f144(int64_t x) {
    int64_t a = x * 145 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f145(int64_t x) {
    int64_t a = x * 146 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f146(int64_t x) {
    int64_t a = x * 147 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f147(int64_t x) {
    int64_t a = x * 148 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f148(int64_t x) {
    int64_t a = x * 149 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f149(int64_t x) {
    int64_t a = x * 150 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f150(int64_t x) {
    int64_t a = x * 151 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f151(int64_t x) {
    int64_t a = x * 152 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f152(int64_t x) {
    int64_t a = x * 153 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f153(int64_t x) {
    int64_t a = x * 154 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f154(int64_t x) {
    int64_t a = x * 155 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f155(int64_t x) {
    int64_t a = x * 156 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f156(int64_t x) {
    int64_t a = x * 157 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f157(int64_t x) {
    int64_t a = x * 158 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f158(int64_t x) {
    int64_t a = x * 159 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f159(int64_t x) {
    int64_t a = x * 160 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f160(int64_t x) {
    int64_t a = x * 161 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f161(int64_t x) {
    int64_t a = x * 162 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f162(int64_t x) {
    int64_t a = x * 163 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f163(int64_t x) {
    int64_t a = x * 164 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f164(int64_t x) {
    int64_t a = x * 165 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f165(int64_t x) {
    int64_t a = x * 166 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f166(int64_t x) {
    int64_t a = x * 167 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f167(int64_t x) {
    int64_t a = x * 168 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f168(int64_t x) {
    int64_t a = x * 169 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f169(int64_t x) {
    int64_t a = x * 170 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f170(int64_t x) {
    int64_t a = x * 171 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f171(int64_t x) {
    int64_t a = x * 172 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f172(int64_t x) {
    int64_t a = x * 173 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f173(int64_t x) {
    int64_t a = x * 174 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f174(int64_t x) {
    int64_t a = x * 175 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f175(int64_t x) {
    int64_t a = x * 176 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f176(int64_t x) {
    int64_t a = x * 177 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f177(int64_t x) {
    int64_t a = x * 178 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f178(int64_t x) {
    int64_t a = x * 179 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f179(int64_t x) {
    int64_t a = x * 180 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f180(int64_t x) {
    int64_t a = x * 181 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f181(int64_t x) {
    int64_t a = x * 182 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f182(int64_t x) {
    int64_t a = x * 183 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f183(int64_t x) {
    int64_t a = x * 184 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f184(int64_t x) {
    int64_t a = x * 185 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f185(int64_t x) {
    int64_t a = x * 186 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f186(int64_t x) {
    int64_t a = x * 187 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f187(int64_t x) {
    int64_t a = x * 188 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f188(int64_t x) {
    int64_t a = x * 189 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f189(int64_t x) {
    int64_t a = x * 190 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f190(int64_t x) {
    int64_t a = x * 191 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f191(int64_t x) {
    int64_t a = x * 192 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f192(int64_t x) {
    int64_t a = x * 193 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f193(int64_t x) {
    int64_t a = x * 194 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f194(int64_t x) {
    int64_t a = x * 195 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f195(int64_t x) {
    int64_t a = x * 196 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f196(int64_t x) {
    int64_t a = x * 197 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f197(int64_t x) {
    int64_t a = x * 198 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f198(int64_t x) {
    int64_t a = x * 199 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f199(int64_t x) {
    int64_t a = x * 200 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f200(int64_t x) {
    int64_t a = x * 201 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f201(int64_t x) {
    int64_t a = x * 202 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f202(int64_t x) {
    int64_t a = x * 203 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f203(int64_t x) {
    int64_t a = x * 204 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f204(int64_t x) {
    int64_t a = x * 205 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f205(int64_t x) {
    int64_t a = x * 206 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f206(int64_t x) {
    int64_t a = x * 207 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f207(int64_t x) {
    int64_t a = x * 208 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f208(int64_t x) {
    int64_t a = x * 209 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f209(int64_t x) {
    int64_t a = x * 210 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f210(int64_t x) {
    int64_t a = x * 211 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f211(int64_t x) {
    int64_t a = x * 212 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f212(int64_t x) {
    int64_t a = x * 213 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f213(int64_t x) {
    int64_t a = x * 214 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f214(int64_t x) {
    int64_t a = x * 215 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f215(int64_t x) {
    int64_t a = x * 216 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f216(int64_t x) {
    int64_t a = x * 217 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f217(int64_t x) {
    int64_t a = x * 218 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f218(int64_t x) {
    int64_t a = x * 219 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f219(int64_t x) {
    int64_t a = x * 220 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f220(int64_t x) {
    int64_t a = x * 221 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f221(int64_t x) {
    int64_t a = x * 222 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f222(int64_t x) {
    int64_t a = x * 223 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f223(int64_t x) {
    int64_t a = x * 224 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f224(int64_t x) {
    int64_t a = x * 225 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f225(int64_t x) {
    int64_t a = x * 226 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f226(int64_t x) {
    int64_t a = x * 227 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f227(int64_t x) {
    int64_t a = x * 228 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f228(int64_t x) {
    int64_t a = x * 229 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f229(int64_t x) {
    int64_t a = x * 230 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f230(int64_t x) {
    int64_t a = x * 231 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f231(int64_t x) {
    int64_t a = x * 232 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f232(int64_t x) {
    int64_t a = x * 233 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f233(int64_t x) {
    int64_t a = x * 234 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f234(int64_t x) {
    int64_t a = x * 235 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f235(int64_t x) {
    int64_t a = x * 236 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f236(int64_t x) {
    int64_t a = x * 237 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f237(int64_t x) {
    int64_t a = x * 238 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f238(int64_t x) {
    int64_t a = x * 239 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f239(int64_t x) {
    int64_t a = x * 240 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f240(int64_t x) {
    int64_t a = x * 241 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f241(int64_t x) {
    int64_t a = x * 242 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f242(int64_t x) {
    int64_t a = x * 243 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f243(int64_t x) {
    int64_t a = x * 244 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f244(int64_t x) {
    int64_t a = x * 245 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f245(int64_t x) {
    int64_t a = x * 246 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f246(int64_t x) {
    int64_t a = x * 247 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f247(int64_t x) {
    int64_t a = x * 248 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f248(int64_t x) {
    int64_t a = x * 249 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f249(int64_t x) {
    int64_t a = x * 250 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f250(int64_t x) {
    int64_t a = x * 251 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f251(int64_t x) {
    int64_t a = x * 252 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f252(int64_t x) {
    int64_t a = x * 253 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f253(int64_t x) {
    int64_t a = x * 254 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f254(int64_t x) {
    int64_t a = x * 255 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f255(int64_t x) {
    int64_t a = x * 256 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f256(int64_t x) {
    int64_t a = x * 257 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f257(int64_t x) {
    int64_t a = x * 258 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f258(int64_t x) {
    int64_t a = x * 259 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f259(int64_t x) {
    int64_t a = x * 260 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f260(int64_t x) {
    int64_t a = x * 261 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f261(int64_t x) {
    int64_t a = x * 262 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f262(int64_t x) {
    int64_t a = x * 263 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f263(int64_t x) {
    int64_t a = x * 264 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f264(int64_t x) {
    int64_t a = x * 265 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f265(int64_t x) {
    int64_t a = x * 266 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f266(int64_t x) {
    int64_t a = x * 267 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f267(int64_t x) {
    int64_t a = x * 268 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f268(int64_t x) {
    int64_t a = x * 269 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f269(int64_t x) {
    int64_t a = x * 270 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f270(int64_t x) {
    int64_t a = x * 271 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f271(int64_t x) {
    int64_t a = x * 272 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f272(int64_t x) {
    int64_t a = x * 273 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f273(int64_t x) {
    int64_t a = x * 274 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f274(int64_t x) {
    int64_t a = x * 275 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f275(int64_t x) {
    int64_t a = x * 276 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f276(int64_t x) {
    int64_t a = x * 277 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f277(int64_t x) {
    int64_t a = x * 278 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f278(int64_t x) {
    int64_t a = x * 279 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f279(int64_t x) {
    int64_t a = x * 280 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f280(int64_t x) {
    int64_t a = x * 281 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f281(int64_t x) {
    int64_t a = x * 282 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f282(int64_t x) {
    int64_t a = x * 283 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f283(int64_t x) {
    int64_t a = x * 284 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f284(int64_t x) {
    int64_t a = x * 285 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f285(int64_t x) {
    int64_t a = x * 286 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f286(int64_t x) {
    int64_t a = x * 287 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f287(int64_t x) {
    int64_t a = x * 288 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f288(int64_t x) {
    int64_t a = x * 289 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f289(int64_t x) {
    int64_t a = x * 290 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f290(int64_t x) {
    int64_t a = x * 291 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f291(int64_t x) {
    int64_t a = x * 292 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f292(int64_t x) {
    int64_t a = x * 293 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f293(int64_t x) {
    int64_t a = x * 294 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f294(int64_t x) {
    int64_t a = x * 295 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f295(int64_t x) {
    int64_t a = x * 296 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f296(int64_t x) {
    int64_t a = x * 297 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f297(int64_t x) {
    int64_t a = x * 298 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f298(int64_t x) {
    int64_t a = x * 299 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f299(int64_t x) {
    int64_t a = x * 300 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f300(int64_t x) {
    int64_t a = x * 301 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f301(int64_t x) {
    int64_t a = x * 302 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f302(int64_t x) {
    int64_t a = x * 303 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f303(int64_t x) {
    int64_t a = x * 304 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f304(int64_t x) {
    int64_t a = x * 305 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f305(int64_t x) {
    int64_t a = x * 306 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f306(int64_t x) {
    int64_t a = x * 307 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f307(int64_t x) {
    int64_t a = x * 308 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f308(int64_t x) {
    int64_t a = x * 309 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f309(int64_t x) {
    int64_t a = x * 310 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f310(int64_t x) {
    int64_t a = x * 311 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f311(int64_t x) {
    int64_t a = x * 312 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f312(int64_t x) {
    int64_t a = x * 313 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f313(int64_t x) {
    int64_t a = x * 314 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f314(int64_t x) {
    int64_t a = x * 315 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f315(int64_t x) {
    int64_t a = x * 316 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f316(int64_t x) {
    int64_t a = x * 317 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f317(int64_t x) {
    int64_t a = x * 318 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f318(int64_t x) {
    int64_t a = x * 319 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f319(int64_t x) {
    int64_t a = x * 320 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f320(int64_t x) {
    int64_t a = x * 321 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f321(int64_t x) {
    int64_t a = x * 322 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f322(int64_t x) {
    int64_t a = x * 323 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f323(int64_t x) {
    int64_t a = x * 324 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f324(int64_t x) {
    int64_t a = x * 325 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f325(int64_t x) {
    int64_t a = x * 326 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f326(int64_t x) {
    int64_t a = x * 327 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f327(int64_t x) {
    int64_t a = x * 328 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f328(int64_t x) {
    int64_t a = x * 329 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f329(int64_t x) {
    int64_t a = x * 330 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f330(int64_t x) {
    int64_t a = x * 331 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f331(int64_t x) {
    int64_t a = x * 332 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f332(int64_t x) {
    int64_t a = x * 333 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f333(int64_t x) {
    int64_t a = x * 334 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f334(int64_t x) {
    int64_t a = x * 335 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f335(int64_t x) {
    int64_t a = x * 336 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f336(int64_t x) {
    int64_t a = x * 337 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f337(int64_t x) {
    int64_t a = x * 338 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f338(int64_t x) {
    int64_t a = x * 339 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f339(int64_t x) {
    int64_t a = x * 340 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f340(int64_t x) {
    int64_t a = x * 341 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f341(int64_t x) {
    int64_t a = x * 342 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f342(int64_t x) {
    int64_t a = x * 343 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f343(int64_t x) {
    int64_t a = x * 344 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f344(int64_t x) {
    int64_t a = x * 345 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f345(int64_t x) {
    int64_t a = x * 346 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f346(int64_t x) {
    int64_t a = x * 347 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f347(int64_t x) {
    int64_t a = x * 348 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f348(int64_t x) {
    int64_t a = x * 349 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f349(int64_t x) {
    int64_t a = x * 350 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f350(int64_t x) {
    int64_t a = x * 351 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f351(int64_t x) {
    int64_t a = x * 352 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f352(int64_t x) {
    int64_t a = x * 353 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f353(int64_t x) {
    int64_t a = x * 354 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f354(int64_t x) {
    int64_t a = x * 355 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f355(int64_t x) {
    int64_t a = x * 356 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f356(int64_t x) {
    int64_t a = x * 357 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f357(int64_t x) {
    int64_t a = x * 358 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f358(int64_t x) {
    int64_t a = x * 359 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f359(int64_t x) {
    int64_t a = x * 360 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f360(int64_t x) {
    int64_t a = x * 361 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f361(int64_t x) {
    int64_t a = x * 362 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f362(int64_t x) {
    int64_t a = x * 363 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f363(int64_t x) {
    int64_t a = x * 364 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f364(int64_t x) {
    int64_t a = x * 365 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f365(int64_t x) {
    int64_t a = x * 366 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f366(int64_t x) {
    int64_t a = x * 367 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f367(int64_t x) {
    int64_t a = x * 368 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f368(int64_t x) {
    int64_t a = x * 369 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f369(int64_t x) {
    int64_t a = x * 370 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f370(int64_t x) {
    int64_t a = x * 371 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f371(int64_t x) {
    int64_t a = x * 372 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f372(int64_t x) {
    int64_t a = x * 373 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f373(int64_t x) {
    int64_t a = x * 374 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f374(int64_t x) {
    int64_t a = x * 375 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f375(int64_t x) {
    int64_t a = x * 376 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f376(int64_t x) {
    int64_t a = x * 377 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f377(int64_t x) {
    int64_t a = x * 378 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f378(int64_t x) {
    int64_t a = x * 379 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f379(int64_t x) {
    int64_t a = x * 380 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f380(int64_t x) {
    int64_t a = x * 381 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f381(int64_t x) {
    int64_t a = x * 382 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f382(int64_t x) {
    int64_t a = x * 383 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f383(int64_t x) {
    int64_t a = x * 384 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f384(int64_t x) {
    int64_t a = x * 385 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f385(int64_t x) {
    int64_t a = x * 386 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f386(int64_t x) {
    int64_t a = x * 387 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f387(int64_t x) {
    int64_t a = x * 388 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f388(int64_t x) {
    int64_t a = x * 389 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f389(int64_t x) {
    int64_t a = x * 390 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f390(int64_t x) {
    int64_t a = x * 391 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f391(int64_t x) {
    int64_t a = x * 392 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f392(int64_t x) {
    int64_t a = x * 393 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f393(int64_t x) {
    int64_t a = x * 394 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f394(int64_t x) {
    int64_t a = x * 395 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f395(int64_t x) {
    int64_t a = x * 396 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f396(int64_t x) {
    int64_t a = x * 397 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f397(int64_t x) {
    int64_t a = x * 398 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f398(int64_t x) {
    int64_t a = x * 399 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f399(int64_t x) {
    int64_t a = x * 400 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f400(int64_t x) {
    int64_t a = x * 401 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f401(int64_t x) {
    int64_t a = x * 402 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f402(int64_t x) {
    int64_t a = x * 403 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f403(int64_t x) {
    int64_t a = x * 404 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f404(int64_t x) {
    int64_t a = x * 405 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f405(int64_t x) {
    int64_t a = x * 406 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f406(int64_t x) {
    int64_t a = x * 407 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f407(int64_t x) {
    int64_t a = x * 408 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f408(int64_t x) {
    int64_t a = x * 409 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f409(int64_t x) {
    int64_t a = x * 410 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f410(int64_t x) {
    int64_t a = x * 411 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f411(int64_t x) {
    int64_t a = x * 412 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f412(int64_t x) {
    int64_t a = x * 413 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f413(int64_t x) {
    int64_t a = x * 414 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f414(int64_t x) {
    int64_t a = x * 415 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f415(int64_t x) {
    int64_t a = x * 416 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f416(int64_t x) {
    int64_t a = x * 417 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f417(int64_t x) {
    int64_t a = x * 418 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f418(int64_t x) {
    int64_t a = x * 419 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f419(int64_t x) {
    int64_t a = x * 420 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f420(int64_t x) {
    int64_t a = x * 421 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f421(int64_t x) {
    int64_t a = x * 422 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f422(int64_t x) {
    int64_t a = x * 423 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f423(int64_t x) {
    int64_t a = x * 424 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f424(int64_t x) {
    int64_t a = x * 425 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f425(int64_t x) {
    int64_t a = x * 426 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f426(int64_t x) {
    int64_t a = x * 427 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f427(int64_t x) {
    int64_t a = x * 428 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f428(int64_t x) {
    int64_t a = x * 429 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f429(int64_t x) {
    int64_t a = x * 430 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f430(int64_t x) {
    int64_t a = x * 431 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f431(int64_t x) {
    int64_t a = x * 432 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f432(int64_t x) {
    int64_t a = x * 433 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f433(int64_t x) {
    int64_t a = x * 434 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f434(int64_t x) {
    int64_t a = x * 435 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f435(int64_t x) {
    int64_t a = x * 436 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f436(int64_t x) {
    int64_t a = x * 437 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f437(int64_t x) {
    int64_t a = x * 438 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f438(int64_t x) {
    int64_t a = x * 439 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f439(int64_t x) {
    int64_t a = x * 440 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f440(int64_t x) {
    int64_t a = x * 441 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f441(int64_t x) {
    int64_t a = x * 442 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f442(int64_t x) {
    int64_t a = x * 443 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f443(int64_t x) {
    int64_t a = x * 444 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f444(int64_t x) {
    int64_t a = x * 445 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f445(int64_t x) {
    int64_t a = x * 446 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f446(int64_t x) {
    int64_t a = x * 447 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f447(int64_t x) {
    int64_t a = x * 448 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f448(int64_t x) {
    int64_t a = x * 449 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f449(int64_t x) {
    int64_t a = x * 450 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f450(int64_t x) {
    int64_t a = x * 451 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f451(int64_t x) {
    int64_t a = x * 452 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f452(int64_t x) {
    int64_t a = x * 453 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f453(int64_t x) {
    int64_t a = x * 454 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f454(int64_t x) {
    int64_t a = x * 455 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f455(int64_t x) {
    int64_t a = x * 456 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f456(int64_t x) {
    int64_t a = x * 457 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f457(int64_t x) {
    int64_t a = x * 458 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f458(int64_t x) {
    int64_t a = x * 459 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f459(int64_t x) {
    int64_t a = x * 460 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f460(int64_t x) {
    int64_t a = x * 461 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f461(int64_t x) {
    int64_t a = x * 462 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f462(int64_t x) {
    int64_t a = x * 463 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f463(int64_t x) {
    int64_t a = x * 464 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f464(int64_t x) {
    int64_t a = x * 465 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f465(int64_t x) {
    int64_t a = x * 466 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f466(int64_t x) {
    int64_t a = x * 467 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f467(int64_t x) {
    int64_t a = x * 468 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f468(int64_t x) {
    int64_t a = x * 469 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f469(int64_t x) {
    int64_t a = x * 470 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f470(int64_t x) {
    int64_t a = x * 471 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f471(int64_t x) {
    int64_t a = x * 472 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f472(int64_t x) {
    int64_t a = x * 473 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f473(int64_t x) {
    int64_t a = x * 474 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f474(int64_t x) {
    int64_t a = x * 475 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f475(int64_t x) {
    int64_t a = x * 476 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f476(int64_t x) {
    int64_t a = x * 477 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f477(int64_t x) {
    int64_t a = x * 478 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f478(int64_t x) {
    int64_t a = x * 479 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f479(int64_t x) {
    int64_t a = x * 480 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f480(int64_t x) {
    int64_t a = x * 481 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f481(int64_t x) {
    int64_t a = x * 482 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f482(int64_t x) {
    int64_t a = x * 483 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f483(int64_t x) {
    int64_t a = x * 484 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f484(int64_t x) {
    int64_t a = x * 485 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f485(int64_t x) {
    int64_t a = x * 486 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f486(int64_t x) {
    int64_t a = x * 487 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f487(int64_t x) {
    int64_t a = x * 488 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f488(int64_t x) {
    int64_t a = x * 489 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f489(int64_t x) {
    int64_t a = x * 490 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f490(int64_t x) {
    int64_t a = x * 491 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f491(int64_t x) {
    int64_t a = x * 492 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f492(int64_t x) {
    int64_t a = x * 493 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f493(int64_t x) {
    int64_t a = x * 494 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f494(int64_t x) {
    int64_t a = x * 495 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f495(int64_t x) {
    int64_t a = x * 496 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f496(int64_t x) {
    int64_t a = x * 497 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f497(int64_t x) {
    int64_t a = x * 498 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f498(int64_t x) {
    int64_t a = x * 499 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f499(int64_t x) {
    int64_t a = x * 500 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f500(int64_t x) {
    int64_t a = x * 501 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f501(int64_t x) {
    int64_t a = x * 502 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f502(int64_t x) {
    int64_t a = x * 503 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f503(int64_t x) {
    int64_t a = x * 504 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f504(int64_t x) {
    int64_t a = x * 505 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f505(int64_t x) {
    int64_t a = x * 506 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f506(int64_t x) {
    int64_t a = x * 507 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f507(int64_t x) {
    int64_t a = x * 508 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f508(int64_t x) {
    int64_t a = x * 509 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f509(int64_t x) {
    int64_t a = x * 510 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f510(int64_t x) {
    int64_t a = x * 511 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f511(int64_t x) {
    int64_t a = x * 512 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f512(int64_t x) {
    int64_t a = x * 513 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f513(int64_t x) {
    int64_t a = x * 514 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f514(int64_t x) {
    int64_t a = x * 515 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f515(int64_t x) {
    int64_t a = x * 516 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f516(int64_t x) {
    int64_t a = x * 517 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f517(int64_t x) {
    int64_t a = x * 518 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f518(int64_t x) {
    int64_t a = x * 519 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f519(int64_t x) {
    int64_t a = x * 520 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f520(int64_t x) {
    int64_t a = x * 521 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f521(int64_t x) {
    int64_t a = x * 522 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f522(int64_t x) {
    int64_t a = x * 523 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f523(int64_t x) {
    int64_t a = x * 524 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f524(int64_t x) {
    int64_t a = x * 525 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f525(int64_t x) {
    int64_t a = x * 526 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f526(int64_t x) {
    int64_t a = x * 527 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f527(int64_t x) {
    int64_t a = x * 528 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f528(int64_t x) {
    int64_t a = x * 529 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f529(int64_t x) {
    int64_t a = x * 530 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f530(int64_t x) {
    int64_t a = x * 531 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f531(int64_t x) {
    int64_t a = x * 532 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f532(int64_t x) {
    int64_t a = x * 533 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f533(int64_t x) {
    int64_t a = x * 534 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f534(int64_t x) {
    int64_t a = x * 535 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f535(int64_t x) {
    int64_t a = x * 536 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f536(int64_t x) {
    int64_t a = x * 537 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f537(int64_t x) {
    int64_t a = x * 538 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f538(int64_t x) {
    int64_t a = x * 539 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f539(int64_t x) {
    int64_t a = x * 540 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f540(int64_t x) {
    int64_t a = x * 541 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f541(int64_t x) {
    int64_t a = x * 542 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f542(int64_t x) {
    int64_t a = x * 543 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f543(int64_t x) {
    int64_t a = x * 544 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f544(int64_t x) {
    int64_t a = x * 545 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f545(int64_t x) {
    int64_t a = x * 546 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f546(int64_t x) {
    int64_t a = x * 547 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f547(int64_t x) {
    int64_t a = x * 548 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f548(int64_t x) {
    int64_t a = x * 549 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f549(int64_t x) {
    int64_t a = x * 550 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f550(int64_t x) {
    int64_t a = x * 551 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f551(int64_t x) {
    int64_t a = x * 552 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f552(int64_t x) {
    int64_t a = x * 553 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f553(int64_t x) {
    int64_t a = x * 554 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f554(int64_t x) {
    int64_t a = x * 555 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f555(int64_t x) {
    int64_t a = x * 556 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f556(int64_t x) {
    int64_t a = x * 557 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f557(int64_t x) {
    int64_t a = x * 558 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f558(int64_t x) {
    int64_t a = x * 559 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f559(int64_t x) {
    int64_t a = x * 560 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f560(int64_t x) {
    int64_t a = x * 561 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f561(int64_t x) {
    int64_t a = x * 562 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f562(int64_t x) {
    int64_t a = x * 563 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f563(int64_t x) {
    int64_t a = x * 564 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f564(int64_t x) {
    int64_t a = x * 565 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f565(int64_t x) {
    int64_t a = x * 566 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f566(int64_t x) {
    int64_t a = x * 567 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f567(int64_t x) {
    int64_t a = x * 568 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f568(int64_t x) {
    int64_t a = x * 569 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f569(int64_t x) {
    int64_t a = x * 570 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f570(int64_t x) {
    int64_t a = x * 571 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f571(int64_t x) {
    int64_t a = x * 572 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f572(int64_t x) {
    int64_t a = x * 573 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f573(int64_t x) {
    int64_t a = x * 574 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f574(int64_t x) {
    int64_t a = x * 575 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f575(int64_t x) {
    int64_t a = x * 576 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f576(int64_t x) {
    int64_t a = x * 577 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f577(int64_t x) {
    int64_t a = x * 578 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f578(int64_t x) {
    int64_t a = x * 579 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f579(int64_t x) {
    int64_t a = x * 580 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f580(int64_t x) {
    int64_t a = x * 581 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f581(int64_t x) {
    int64_t a = x * 582 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f582(int64_t x) {
    int64_t a = x * 583 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f583(int64_t x) {
    int64_t a = x * 584 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f584(int64_t x) {
    int64_t a = x * 585 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f585(int64_t x) {
    int64_t a = x * 586 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f586(int64_t x) {
    int64_t a = x * 587 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f587(int64_t x) {
    int64_t a = x * 588 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f588(int64_t x) {
    int64_t a = x * 589 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f589(int64_t x) {
    int64_t a = x * 590 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f590(int64_t x) {
    int64_t a = x * 591 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f591(int64_t x) {
    int64_t a = x * 592 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f592(int64_t x) {
    int64_t a = x * 593 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f593(int64_t x) {
    int64_t a = x * 594 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f594(int64_t x) {
    int64_t a = x * 595 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f595(int64_t x) {
    int64_t a = x * 596 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f596(int64_t x) {
    int64_t a = x * 597 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f597(int64_t x) {
    int64_t a = x * 598 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f598(int64_t x) {
    int64_t a = x * 599 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f599(int64_t x) {
    int64_t a = x * 600 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f600(int64_t x) {
    int64_t a = x * 601 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f601(int64_t x) {
    int64_t a = x * 602 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f602(int64_t x) {
    int64_t a = x * 603 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f603(int64_t x) {
    int64_t a = x * 604 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f604(int64_t x) {
    int64_t a = x * 605 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f605(int64_t x) {
    int64_t a = x * 606 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f606(int64_t x) {
    int64_t a = x * 607 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f607(int64_t x) {
    int64_t a = x * 608 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f608(int64_t x) {
    int64_t a = x * 609 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f609(int64_t x) {
    int64_t a = x * 610 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f610(int64_t x) {
    int64_t a = x * 611 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f611(int64_t x) {
    int64_t a = x * 612 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f612(int64_t x) {
    int64_t a = x * 613 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f613(int64_t x) {
    int64_t a = x * 614 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f614(int64_t x) {
    int64_t a = x * 615 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f615(int64_t x) {
    int64_t a = x * 616 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f616(int64_t x) {
    int64_t a = x * 617 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f617(int64_t x) {
    int64_t a = x * 618 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f618(int64_t x) {
    int64_t a = x * 619 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f619(int64_t x) {
    int64_t a = x * 620 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f620(int64_t x) {
    int64_t a = x * 621 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f621(int64_t x) {
    int64_t a = x * 622 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f622(int64_t x) {
    int64_t a = x * 623 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f623(int64_t x) {
    int64_t a = x * 624 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f624(int64_t x) {
    int64_t a = x * 625 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f625(int64_t x) {
    int64_t a = x * 626 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f626(int64_t x) {
    int64_t a = x * 627 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f627(int64_t x) {
    int64_t a = x * 628 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f628(int64_t x) {
    int64_t a = x * 629 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f629(int64_t x) {
    int64_t a = x * 630 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f630(int64_t x) {
    int64_t a = x * 631 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f631(int64_t x) {
    int64_t a = x * 632 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f632(int64_t x) {
    int64_t a = x * 633 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f633(int64_t x) {
    int64_t a = x * 634 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f634(int64_t x) {
    int64_t a = x * 635 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f635(int64_t x) {
    int64_t a = x * 636 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f636(int64_t x) {
    int64_t a = x * 637 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f637(int64_t x) {
    int64_t a = x * 638 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f638(int64_t x) {
    int64_t a = x * 639 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f639(int64_t x) {
    int64_t a = x * 640 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f640(int64_t x) {
    int64_t a = x * 641 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f641(int64_t x) {
    int64_t a = x * 642 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f642(int64_t x) {
    int64_t a = x * 643 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f643(int64_t x) {
    int64_t a = x * 644 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f644(int64_t x) {
    int64_t a = x * 645 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f645(int64_t x) {
    int64_t a = x * 646 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f646(int64_t x) {
    int64_t a = x * 647 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f647(int64_t x) {
    int64_t a = x * 648 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f648(int64_t x) {
    int64_t a = x * 649 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f649(int64_t x) {
    int64_t a = x * 650 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f650(int64_t x) {
    int64_t a = x * 651 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f651(int64_t x) {
    int64_t a = x * 652 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f652(int64_t x) {
    int64_t a = x * 653 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f653(int64_t x) {
    int64_t a = x * 654 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f654(int64_t x) {
    int64_t a = x * 655 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f655(int64_t x) {
    int64_t a = x * 656 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f656(int64_t x) {
    int64_t a = x * 657 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f657(int64_t x) {
    int64_t a = x * 658 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f658(int64_t x) {
    int64_t a = x * 659 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f659(int64_t x) {
    int64_t a = x * 660 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f660(int64_t x) {
    int64_t a = x * 661 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f661(int64_t x) {
    int64_t a = x * 662 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f662(int64_t x) {
    int64_t a = x * 663 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f663(int64_t x) {
    int64_t a = x * 664 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f664(int64_t x) {
    int64_t a = x * 665 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f665(int64_t x) {
    int64_t a = x * 666 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f666(int64_t x) {
    int64_t a = x * 667 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f667(int64_t x) {
    int64_t a = x * 668 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f668(int64_t x) {
    int64_t a = x * 669 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f669(int64_t x) {
    int64_t a = x * 670 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f670(int64_t x) {
    int64_t a = x * 671 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f671(int64_t x) {
    int64_t a = x * 672 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f672(int64_t x) {
    int64_t a = x * 673 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f673(int64_t x) {
    int64_t a = x * 674 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f674(int64_t x) {
    int64_t a = x * 675 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f675(int64_t x) {
    int64_t a = x * 676 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f676(int64_t x) {
    int64_t a = x * 677 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f677(int64_t x) {
    int64_t a = x * 678 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f678(int64_t x) {
    int64_t a = x * 679 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f679(int64_t x) {
    int64_t a = x * 680 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f680(int64_t x) {
    int64_t a = x * 681 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f681(int64_t x) {
    int64_t a = x * 682 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f682(int64_t x) {
    int64_t a = x * 683 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f683(int64_t x) {
    int64_t a = x * 684 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f684(int64_t x) {
    int64_t a = x * 685 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f685(int64_t x) {
    int64_t a = x * 686 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f686(int64_t x) {
    int64_t a = x * 687 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f687(int64_t x) {
    int64_t a = x * 688 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f688(int64_t x) {
    int64_t a = x * 689 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f689(int64_t x) {
    int64_t a = x * 690 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f690(int64_t x) {
    int64_t a = x * 691 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f691(int64_t x) {
    int64_t a = x * 692 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f692(int64_t x) {
    int64_t a = x * 693 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f693(int64_t x) {
    int64_t a = x * 694 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f694(int64_t x) {
    int64_t a = x * 695 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f695(int64_t x) {
    int64_t a = x * 696 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f696(int64_t x) {
    int64_t a = x * 697 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f697(int64_t x) {
    int64_t a = x * 698 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f698(int64_t x) {
    int64_t a = x * 699 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f699(int64_t x) {
    int64_t a = x * 700 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f700(int64_t x) {
    int64_t a = x * 701 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f701(int64_t x) {
    int64_t a = x * 702 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f702(int64_t x) {
    int64_t a = x * 703 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f703(int64_t x) {
    int64_t a = x * 704 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f704(int64_t x) {
    int64_t a = x * 705 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f705(int64_t x) {
    int64_t a = x * 706 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f706(int64_t x) {
    int64_t a = x * 707 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f707(int64_t x) {
    int64_t a = x * 708 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f708(int64_t x) {
    int64_t a = x * 709 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f709(int64_t x) {
    int64_t a = x * 710 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f710(int64_t x) {
    int64_t a = x * 711 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f711(int64_t x) {
    int64_t a = x * 712 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f712(int64_t x) {
    int64_t a = x * 713 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f713(int64_t x) {
    int64_t a = x * 714 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f714(int64_t x) {
    int64_t a = x * 715 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f715(int64_t x) {
    int64_t a = x * 716 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f716(int64_t x) {
    int64_t a = x * 717 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f717(int64_t x) {
    int64_t a = x * 718 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f718(int64_t x) {
    int64_t a = x * 719 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f719(int64_t x) {
    int64_t a = x * 720 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f720(int64_t x) {
    int64_t a = x * 721 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f721(int64_t x) {
    int64_t a = x * 722 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f722(int64_t x) {
    int64_t a = x * 723 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f723(int64_t x) {
    int64_t a = x * 724 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f724(int64_t x) {
    int64_t a = x * 725 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f725(int64_t x) {
    int64_t a = x * 726 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f726(int64_t x) {
    int64_t a = x * 727 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f727(int64_t x) {
    int64_t a = x * 728 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f728(int64_t x) {
    int64_t a = x * 729 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f729(int64_t x) {
    int64_t a = x * 730 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f730(int64_t x) {
    int64_t a = x * 731 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f731(int64_t x) {
    int64_t a = x * 732 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f732(int64_t x) {
    int64_t a = x * 733 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f733(int64_t x) {
    int64_t a = x * 734 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f734(int64_t x) {
    int64_t a = x * 735 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f735(int64_t x) {
    int64_t a = x * 736 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f736(int64_t x) {
    int64_t a = x * 737 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f737(int64_t x) {
    int64_t a = x * 738 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f738(int64_t x) {
    int64_t a = x * 739 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f739(int64_t x) {
    int64_t a = x * 740 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f740(int64_t x) {
    int64_t a = x * 741 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f741(int64_t x) {
    int64_t a = x * 742 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f742(int64_t x) {
    int64_t a = x * 743 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f743(int64_t x) {
    int64_t a = x * 744 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f744(int64_t x) {
    int64_t a = x * 745 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f745(int64_t x) {
    int64_t a = x * 746 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f746(int64_t x) {
    int64_t a = x * 747 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f747(int64_t x) {
    int64_t a = x * 748 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f748(int64_t x) {
    int64_t a = x * 749 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f749(int64_t x) {
    int64_t a = x * 750 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f750(int64_t x) {
    int64_t a = x * 751 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f751(int64_t x) {
    int64_t a = x * 752 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f752(int64_t x) {
    int64_t a = x * 753 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f753(int64_t x) {
    int64_t a = x * 754 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f754(int64_t x) {
    int64_t a = x * 755 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f755(int64_t x) {
    int64_t a = x * 756 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f756(int64_t x) {
    int64_t a = x * 757 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f757(int64_t x) {
    int64_t a = x * 758 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f758(int64_t x) {
    int64_t a = x * 759 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f759(int64_t x) {
    int64_t a = x * 760 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f760(int64_t x) {
    int64_t a = x * 761 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f761(int64_t x) {
    int64_t a = x * 762 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f762(int64_t x) {
    int64_t a = x * 763 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f763(int64_t x) {
    int64_t a = x * 764 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f764(int64_t x) {
    int64_t a = x * 765 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f765(int64_t x) {
    int64_t a = x * 766 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f766(int64_t x) {
    int64_t a = x * 767 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f767(int64_t x) {
    int64_t a = x * 768 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f768(int64_t x) {
    int64_t a = x * 769 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f769(int64_t x) {
    int64_t a = x * 770 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f770(int64_t x) {
    int64_t a = x * 771 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f771(int64_t x) {
    int64_t a = x * 772 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f772(int64_t x) {
    int64_t a = x * 773 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f773(int64_t x) {
    int64_t a = x * 774 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f774(int64_t x) {
    int64_t a = x * 775 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f775(int64_t x) {
    int64_t a = x * 776 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f776(int64_t x) {
    int64_t a = x * 777 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f777(int64_t x) {
    int64_t a = x * 778 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f778(int64_t x) {
    int64_t a = x * 779 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f779(int64_t x) {
    int64_t a = x * 780 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f780(int64_t x) {
    int64_t a = x * 781 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f781(int64_t x) {
    int64_t a = x * 782 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f782(int64_t x) {
    int64_t a = x * 783 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f783(int64_t x) {
    int64_t a = x * 784 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f784(int64_t x) {
    int64_t a = x * 785 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f785(int64_t x) {
    int64_t a = x * 786 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f786(int64_t x) {
    int64_t a = x * 787 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f787(int64_t x) {
    int64_t a = x * 788 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f788(int64_t x) {
    int64_t a = x * 789 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f789(int64_t x) {
    int64_t a = x * 790 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f790(int64_t x) {
    int64_t a = x * 791 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f791(int64_t x) {
    int64_t a = x * 792 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f792(int64_t x) {
    int64_t a = x * 793 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f793(int64_t x) {
    int64_t a = x * 794 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f794(int64_t x) {
    int64_t a = x * 795 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f795(int64_t x) {
    int64_t a = x * 796 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f796(int64_t x) {
    int64_t a = x * 797 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f797(int64_t x) {
    int64_t a = x * 798 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f798(int64_t x) {
    int64_t a = x * 799 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f799(int64_t x) {
    int64_t a = x * 800 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f800(int64_t x) {
    int64_t a = x * 801 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f801(int64_t x) {
    int64_t a = x * 802 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f802(int64_t x) {
    int64_t a = x * 803 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f803(int64_t x) {
    int64_t a = x * 804 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f804(int64_t x) {
    int64_t a = x * 805 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f805(int64_t x) {
    int64_t a = x * 806 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f806(int64_t x) {
    int64_t a = x * 807 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f807(int64_t x) {
    int64_t a = x * 808 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f808(int64_t x) {
    int64_t a = x * 809 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f809(int64_t x) {
    int64_t a = x * 810 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f810(int64_t x) {
    int64_t a = x * 811 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f811(int64_t x) {
    int64_t a = x * 812 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f812(int64_t x) {
    int64_t a = x * 813 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f813(int64_t x) {
    int64_t a = x * 814 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f814(int64_t x) {
    int64_t a = x * 815 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f815(int64_t x) {
    int64_t a = x * 816 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f816(int64_t x) {
    int64_t a = x * 817 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f817(int64_t x) {
    int64_t a = x * 818 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f818(int64_t x) {
    int64_t a = x * 819 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f819(int64_t x) {
    int64_t a = x * 820 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f820(int64_t x) {
    int64_t a = x * 821 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f821(int64_t x) {
    int64_t a = x * 822 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f822(int64_t x) {
    int64_t a = x * 823 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f823(int64_t x) {
    int64_t a = x * 824 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f824(int64_t x) {
    int64_t a = x * 825 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f825(int64_t x) {
    int64_t a = x * 826 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f826(int64_t x) {
    int64_t a = x * 827 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f827(int64_t x) {
    int64_t a = x * 828 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f828(int64_t x) {
    int64_t a = x * 829 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f829(int64_t x) {
    int64_t a = x * 830 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f830(int64_t x) {
    int64_t a = x * 831 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f831(int64_t x) {
    int64_t a = x * 832 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f832(int64_t x) {
    int64_t a = x * 833 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f833(int64_t x) {
    int64_t a = x * 834 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f834(int64_t x) {
    int64_t a = x * 835 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f835(int64_t x) {
    int64_t a = x * 836 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f836(int64_t x) {
    int64_t a = x * 837 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f837(int64_t x) {
    int64_t a = x * 838 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f838(int64_t x) {
    int64_t a = x * 839 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f839(int64_t x) {
    int64_t a = x * 840 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f840(int64_t x) {
    int64_t a = x * 841 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f841(int64_t x) {
    int64_t a = x * 842 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f842(int64_t x) {
    int64_t a = x * 843 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f843(int64_t x) {
    int64_t a = x * 844 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f844(int64_t x) {
    int64_t a = x * 845 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f845(int64_t x) {
    int64_t a = x * 846 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f846(int64_t x) {
    int64_t a = x * 847 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f847(int64_t x) {
    int64_t a = x * 848 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f848(int64_t x) {
    int64_t a = x * 849 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f849(int64_t x) {
    int64_t a = x * 850 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f850(int64_t x) {
    int64_t a = x * 851 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f851(int64_t x) {
    int64_t a = x * 852 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f852(int64_t x) {
    int64_t a = x * 853 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f853(int64_t x) {
    int64_t a = x * 854 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f854(int64_t x) {
    int64_t a = x * 855 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f855(int64_t x) {
    int64_t a = x * 856 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f856(int64_t x) {
    int64_t a = x * 857 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f857(int64_t x) {
    int64_t a = x * 858 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f858(int64_t x) {
    int64_t a = x * 859 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f859(int64_t x) {
    int64_t a = x * 860 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f860(int64_t x) {
    int64_t a = x * 861 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f861(int64_t x) {
    int64_t a = x * 862 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f862(int64_t x) {
    int64_t a = x * 863 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f863(int64_t x) {
    int64_t a = x * 864 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f864(int64_t x) {
    int64_t a = x * 865 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f865(int64_t x) {
    int64_t a = x * 866 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f866(int64_t x) {
    int64_t a = x * 867 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f867(int64_t x) {
    int64_t a = x * 868 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f868(int64_t x) {
    int64_t a = x * 869 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f869(int64_t x) {
    int64_t a = x * 870 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f870(int64_t x) {
    int64_t a = x * 871 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f871(int64_t x) {
    int64_t a = x * 872 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f872(int64_t x) {
    int64_t a = x * 873 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f873(int64_t x) {
    int64_t a = x * 874 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f874(int64_t x) {
    int64_t a = x * 875 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f875(int64_t x) {
    int64_t a = x * 876 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f876(int64_t x) {
    int64_t a = x * 877 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f877(int64_t x) {
    int64_t a = x * 878 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f878(int64_t x) {
    int64_t a = x * 879 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f879(int64_t x) {
    int64_t a = x * 880 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f880(int64_t x) {
    int64_t a = x * 881 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f881(int64_t x) {
    int64_t a = x * 882 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f882(int64_t x) {
    int64_t a = x * 883 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f883(int64_t x) {
    int64_t a = x * 884 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f884(int64_t x) {
    int64_t a = x * 885 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f885(int64_t x) {
    int64_t a = x * 886 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f886(int64_t x) {
    int64_t a = x * 887 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f887(int64_t x) {
    int64_t a = x * 888 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f888(int64_t x) {
    int64_t a = x * 889 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f889(int64_t x) {
    int64_t a = x * 890 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f890(int64_t x) {
    int64_t a = x * 891 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f891(int64_t x) {
    int64_t a = x * 892 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f892(int64_t x) {
    int64_t a = x * 893 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f893(int64_t x) {
    int64_t a = x * 894 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f894(int64_t x) {
    int64_t a = x * 895 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f895(int64_t x) {
    int64_t a = x * 896 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f896(int64_t x) {
    int64_t a = x * 897 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f897(int64_t x) {
    int64_t a = x * 898 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f898(int64_t x) {
    int64_t a = x * 899 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f899(int64_t x) {
    int64_t a = x * 900 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f900(int64_t x) {
    int64_t a = x * 901 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f901(int64_t x) {
    int64_t a = x * 902 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f902(int64_t x) {
    int64_t a = x * 903 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f903(int64_t x) {
    int64_t a = x * 904 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f904(int64_t x) {
    int64_t a = x * 905 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f905(int64_t x) {
    int64_t a = x * 906 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f906(int64_t x) {
    int64_t a = x * 907 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f907(int64_t x) {
    int64_t a = x * 908 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f908(int64_t x) {
    int64_t a = x * 909 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f909(int64_t x) {
    int64_t a = x * 910 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f910(int64_t x) {
    int64_t a = x * 911 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f911(int64_t x) {
    int64_t a = x * 912 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f912(int64_t x) {
    int64_t a = x * 913 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f913(int64_t x) {
    int64_t a = x * 914 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f914(int64_t x) {
    int64_t a = x * 915 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f915(int64_t x) {
    int64_t a = x * 916 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f916(int64_t x) {
    int64_t a = x * 917 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f917(int64_t x) {
    int64_t a = x * 918 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f918(int64_t x) {
    int64_t a = x * 919 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f919(int64_t x) {
    int64_t a = x * 920 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f920(int64_t x) {
    int64_t a = x * 921 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f921(int64_t x) {
    int64_t a = x * 922 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f922(int64_t x) {
    int64_t a = x * 923 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f923(int64_t x) {
    int64_t a = x * 924 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f924(int64_t x) {
    int64_t a = x * 925 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f925(int64_t x) {
    int64_t a = x * 926 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f926(int64_t x) {
    int64_t a = x * 927 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f927(int64_t x) {
    int64_t a = x * 928 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f928(int64_t x) {
    int64_t a = x * 929 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f929(int64_t x) {
    int64_t a = x * 930 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f930(int64_t x) {
    int64_t a = x * 931 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f931(int64_t x) {
    int64_t a = x * 932 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f932(int64_t x) {
    int64_t a = x * 933 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f933(int64_t x) {
    int64_t a = x * 934 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f934(int64_t x) {
    int64_t a = x * 935 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f935(int64_t x) {
    int64_t a = x * 936 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f936(int64_t x) {
    int64_t a = x * 937 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f937(int64_t x) {
    int64_t a = x * 938 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f938(int64_t x) {
    int64_t a = x * 939 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f939(int64_t x) {
    int64_t a = x * 940 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f940(int64_t x) {
    int64_t a = x * 941 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f941(int64_t x) {
    int64_t a = x * 942 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f942(int64_t x) {
    int64_t a = x * 943 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f943(int64_t x) {
    int64_t a = x * 944 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f944(int64_t x) {
    int64_t a = x * 945 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f945(int64_t x) {
    int64_t a = x * 946 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f946(int64_t x) {
    int64_t a = x * 947 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f947(int64_t x) {
    int64_t a = x * 948 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f948(int64_t x) {
    int64_t a = x * 949 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f949(int64_t x) {
    int64_t a = x * 950 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f950(int64_t x) {
    int64_t a = x * 951 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f951(int64_t x) {
    int64_t a = x * 952 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f952(int64_t x) {
    int64_t a = x * 953 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f953(int64_t x) {
    int64_t a = x * 954 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f954(int64_t x) {
    int64_t a = x * 955 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f955(int64_t x) {
    int64_t a = x * 956 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f956(int64_t x) {
    int64_t a = x * 957 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f957(int64_t x) {
    int64_t a = x * 958 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f958(int64_t x) {
    int64_t a = x * 959 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f959(int64_t x) {
    int64_t a = x * 960 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f960(int64_t x) {
    int64_t a = x * 961 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f961(int64_t x) {
    int64_t a = x * 962 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f962(int64_t x) {
    int64_t a = x * 963 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f963(int64_t x) {
    int64_t a = x * 964 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f964(int64_t x) {
    int64_t a = x * 965 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f965(int64_t x) {
    int64_t a = x * 966 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f966(int64_t x) {
    int64_t a = x * 967 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f967(int64_t x) {
    int64_t a = x * 968 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f968(int64_t x) {
    int64_t a = x * 969 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f969(int64_t x) {
    int64_t a = x * 970 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f970(int64_t x) {
    int64_t a = x * 971 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f971(int64_t x) {
    int64_t a = x * 972 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f972(int64_t x) {
    int64_t a = x * 973 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f973(int64_t x) {
    int64_t a = x * 974 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f974(int64_t x) {
    int64_t a = x * 975 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f975(int64_t x) {
    int64_t a = x * 976 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f976(int64_t x) {
    int64_t a = x * 977 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f977(int64_t x) {
    int64_t a = x * 978 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f978(int64_t x) {
    int64_t a = x * 979 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f979(int64_t x) {
    int64_t a = x * 980 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f980(int64_t x) {
    int64_t a = x * 981 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f981(int64_t x) {
    int64_t a = x * 982 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f982(int64_t x) {
    int64_t a = x * 983 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f983(int64_t x) {
    int64_t a = x * 984 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f984(int64_t x) {
    int64_t a = x * 985 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f985(int64_t x) {
    int64_t a = x * 986 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f986(int64_t x) {
    int64_t a = x * 987 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f987(int64_t x) {
    int64_t a = x * 988 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f988(int64_t x) {
    int64_t a = x * 989 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f989(int64_t x) {
    int64_t a = x * 990 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f990(int64_t x) {
    int64_t a = x * 991 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f991(int64_t x) {
    int64_t a = x * 992 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f992(int64_t x) {
    int64_t a = x * 993 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f993(int64_t x) {
    int64_t a = x * 994 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f994(int64_t x) {
    int64_t a = x * 995 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f995(int64_t x) {
    int64_t a = x * 996 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f996(int64_t x) {
    int64_t a = x * 997 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f997(int64_t x) {
    int64_t a = x * 998 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f998(int64_t x) {
    int64_t a = x * 999 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
static int64_t f999(int64_t x) {
    int64_t a = x * 1000 + 7;
    int64_t b = a * 3 + x;
    int64_t c = 0;
    for (int64_t i = 0; i < 100; ++i) c = c + b;
    return c & INT64_MAX;
}
int main(void) {
    int64_t total = 0;
    total = total + f0(0) + f5(1);
    total = total + f10(10) + f15(11);
    total = total + f20(20) + f25(21);
    total = total + f30(30) + f35(31);
    total = total + f40(40) + f45(41);
    total = total + f50(50) + f55(51);
    total = total + f60(60) + f65(61);
    total = total + f70(70) + f75(71);
    total = total + f80(80) + f85(81);
    total = total + f90(90) + f95(91);
    total = total + f100(100) + f105(101);
    total = total + f110(110) + f115(111);
    total = total + f120(120) + f125(121);
    total = total + f130(130) + f135(131);
    total = total + f140(140) + f145(141);
    total = total + f150(150) + f155(151);
    total = total + f160(160) + f165(161);
    total = total + f170(170) + f175(171);
    total = total + f180(180) + f185(181);
    total = total + f190(190) + f195(191);
    total = total + f200(200) + f205(201);
    total = total + f210(210) + f215(211);
    total = total + f220(220) + f225(221);
    total = total + f230(230) + f235(231);
    total = total + f240(240) + f245(241);
    total = total + f250(250) + f255(251);
    total = total + f260(260) + f265(261);
    total = total + f270(270) + f275(271);
    total = total + f280(280) + f285(281);
    total = total + f290(290) + f295(291);
    total = total + f300(300) + f305(301);
    total = total + f310(310) + f315(311);
    total = total + f320(320) + f325(321);
    total = total + f330(330) + f335(331);
    total = total + f340(340) + f345(341);
    total = total + f350(350) + f355(351);
    total = total + f360(360) + f365(361);
    total = total + f370(370) + f375(371);
    total = total + f380(380) + f385(381);
    total = total + f390(390) + f395(391);
    total = total + f400(400) + f405(401);
    total = total + f410(410) + f415(411);
    total = total + f420(420) + f425(421);
    total = total + f430(430) + f435(431);
    total = total + f440(440) + f445(441);
    total = total + f450(450) + f455(451);
    total = total + f460(460) + f465(461);
    total = total + f470(470) + f475(471);
    total = total + f480(480) + f485(481);
    total = total + f490(490) + f495(491);
    total = total + f500(500) + f505(501);
    total = total + f510(510) + f515(511);
    total = total + f520(520) + f525(521);
    total = total + f530(530) + f535(531);
    total = total + f540(540) + f545(541);
    total = total + f550(550) + f555(551);
    total = total + f560(560) + f565(561);
    total = total + f570(570) + f575(571);
    total = total + f580(580) + f585(581);
    total = total + f590(590) + f595(591);
    total = total + f600(600) + f605(601);
    total = total + f610(610) + f615(611);
    total = total + f620(620) + f625(621);
    total = total + f630(630) + f635(631);
    total = total + f640(640) + f645(641);
    total = total + f650(650) + f655(651);
    total = total + f660(660) + f665(661);
    total = total + f670(670) + f675(671);
    total = total + f680(680) + f685(681);
    total = total + f690(690) + f695(691);
    total = total + f700(700) + f705(701);
    total = total + f710(710) + f715(711);
    total = total + f720(720) + f725(721);
    total = total + f730(730) + f735(731);
    total = total + f740(740) + f745(741);
    total = total + f750(750) + f755(751);
    total = total + f760(760) + f765(761);
    total = total + f770(770) + f775(771);
    total = total + f780(780) + f785(781);
    total = total + f790(790) + f795(791);
    total = total + f800(800) + f805(801);
    total = total + f810(810) + f815(811);
    total = total + f820(820) + f825(821);
    total = total + f830(830) + f835(831);
    total = total + f840(840) + f845(841);
    total = total + f850(850) + f855(851);
    total = total + f860(860) + f865(861);
    total = total + f870(870) + f875(871);
    total = total + f880(880) + f885(881);
    total = total + f890(890) + f895(891);
    total = total + f900(900) + f905(901);
    total = total + f910(910) + f915(911);
    total = total + f920(920) + f925(921);
    total = total + f930(930) + f935(931);
    total = total + f940(940) + f945(941);
    total = total + f950(950) + f955(951);
    total = total + f960(960) + f965(961);
    total = total + f970(970) + f975(971);
    total = total + f980(980) + f985(981);
    total = total + f990(990) + f995(991);
    printf("%lld\n", (long long)total);
    return 0;
}
