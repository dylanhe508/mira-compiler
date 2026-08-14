/* rt_common.h - Shared declarations for Mira runtime modules */
#ifndef RT_COMMON_H
#define RT_COMMON_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

/* Symbols exported by the Mira-generated .obj */
extern long long mira_var_count;
extern long long mira_vars[];
extern const char *mira_var_names[];

/* Float temp buffer (used by print) */
extern double mira_float_tmp;

/* Memory alloc tracking (shared between rt_core and rt_mem) */
extern long long g_alloc_count;
extern long long g_alloc_bytes;

#endif /* RT_COMMON_H */
