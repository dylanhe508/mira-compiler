#include "target.h"
#include <string.h>

#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
#include <cpuid.h>

static unsigned long long read_xcr0(void) {
    unsigned eax, edx;
    __asm__ volatile ("xgetbv" : "=a"(eax), "=d"(edx) : "c"(0));
    return ((unsigned long long)edx << 32) | eax;
}
#endif

TargetFeatures mira_target_features;

void target_set_baseline(TargetFeatures *out) {
    memset(out, 0, sizeof(*out));
}

bool target_detect_native(TargetFeatures *out) {
    target_set_baseline(out);
#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
    unsigned eax, ebx, ecx, edx;
    if (!__get_cpuid(1, &eax, &ebx, &ecx, &edx)) return false;
    out->sse42 = (ecx & bit_SSE4_2) != 0;
    out->popcnt = (ecx & bit_POPCNT) != 0;
    bool cpu_avx = (ecx & bit_AVX) != 0;
    bool osxsave = (ecx & bit_OSXSAVE) != 0;
    bool ymm_os = osxsave && ((read_xcr0() & 0x6) == 0x6);
    out->avx = cpu_avx && ymm_os;
    out->fma3 = out->avx && ((ecx & bit_FMA) != 0);

    if (__get_cpuid_max(0, NULL) >= 7 && __get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
        out->avx2 = out->avx && ((ebx & bit_AVX2) != 0);
        out->bmi1 = (ebx & bit_BMI) != 0;
        out->bmi2 = (ebx & bit_BMI2) != 0;
    }
    unsigned max_ext = __get_cpuid_max(0x80000000u, NULL);
    if (max_ext >= 0x80000001u && __get_cpuid(0x80000001u, &eax, &ebx, &ecx, &edx))
        out->lzcnt = (ecx & bit_LZCNT) != 0;
    return true;
#else
    return false;
#endif
}

bool target_apply_march(TargetFeatures *out, const char *name) {
    target_set_baseline(out);
    if (strcmp(name, "x86-64") == 0) return true;
    if (strcmp(name, "native") == 0) return target_detect_native(out);
    if (strcmp(name, "sandybridge") == 0) {
        out->sse42 = true; out->avx = true; out->popcnt = true;
        return true;
    }
    if (strcmp(name, "alderlake") == 0) {
        out->sse42 = true; out->avx = true; out->avx2 = true; out->fma3 = true;
        out->bmi1 = true; out->bmi2 = true; out->lzcnt = true; out->popcnt = true;
        return true;
    }
    return false;
}
