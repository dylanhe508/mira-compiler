#ifndef MIRA_TARGET_H
#define MIRA_TARGET_H

#include <stdbool.h>

typedef struct {
    bool sse42;
    bool avx;
    bool avx2;
    bool fma3;
    bool bmi1;
    bool bmi2;
    bool lzcnt;
    bool popcnt;
} TargetFeatures;

extern TargetFeatures mira_target_features;

void target_set_baseline(TargetFeatures *out);
bool target_detect_native(TargetFeatures *out);
bool target_apply_march(TargetFeatures *out, const char *name);

#endif
