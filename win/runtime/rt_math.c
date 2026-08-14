/* rt_math.c - Math and random functions */
#include <stdlib.h>
#include <math.h>

long long mira_abs(long long x) { return x < 0 ? -x : x; }
long long mira_min(long long a, long long b) { return a < b ? a : b; }
long long mira_max(long long a, long long b) { return a > b ? a : b; }

long long mira_f_sqrt(long long bits) {
	union { long long ll; double d; } u;
	u.ll = bits; u.d = sqrt(u.d); return u.ll;
}

long long mira_f_pow(long long base_bits, long long exp_bits) {
	union { long long ll; double d; } u, v;
	u.ll = base_bits; v.ll = exp_bits;
	u.d = pow(u.d, v.d); return u.ll;
}

long long mira_f_floor(long long bits) {
	union { long long ll; double d; } u;
	u.ll = bits; u.d = floor(u.d); return u.ll;
}

long long mira_f_ceil(long long bits) {
	union { long long ll; double d; } u;
	u.ll = bits; u.d = ceil(u.d); return u.ll;
}

long long mira_random(void) { return (long long)rand(); }

long long mira_random_range(long long min, long long max) {
	if (min >= max) return min;
	return min + (long long)(rand() % (unsigned long long)(max - min));
}

void mira_random_seed(unsigned long seed) { srand((unsigned int)seed); }
