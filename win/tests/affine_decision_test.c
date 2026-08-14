#include "../codegen/decision.h"

#include <assert.h>

int main(void) {
    DecisionResult detail;

    assert(decision_choose_affine(24, 3, 96, 12, 12, 2,
                                  DECISION_SCALE, &detail) ==
           DECISION_AFFINE_COLLAPSE);
    assert(decision_choose_affine(3, 3, 12, 12, 2, 2,
                                  DECISION_SCALE, &detail) == DECISION_KEEP);
    assert(decision_choose_affine(24, 3, 12, 16, 12, 2,
                                  DECISION_SCALE, &detail) == DECISION_KEEP);
    assert(decision_choose_affine(24, 3, 96, 12, 2, 3,
                                  DECISION_SCALE, &detail) == DECISION_KEEP);
    assert(decision_choose_affine(24, 3, 96, 12, 12, 2,
                                  0, &detail) == DECISION_KEEP);
    return 0;
}
