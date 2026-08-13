/* rt_print.c - Print function */
#include <stdlib.h>
#include <stdio.h>

extern double mira_float_tmp;

void mira_print(int type, long long val_lo) {
	switch (type) {
		case 0:
			printf("%lld\n", val_lo);
			break;
		case 1:
			if ((const char *)val_lo)
				printf("%s\n", (const char *)val_lo);
			break;
		case 2: {
			/* val_lo contains raw double bits, reinterpret via union */
			union { long long i; double d; } u;
			u.i = val_lo;
			printf("%g\n", u.d);
			break;
		}
		case 3:
			printf("%s\n", val_lo ? "true" : "false");
			break;
		default:
			printf("%lld\n", val_lo);
			break;
	}
	/* The generated PE enters through mainCRTStartup rather than the C runtime
	 * wrapper, so do not pass a FILE object obtained through another CRT ABI. */
	fflush(NULL);
}
