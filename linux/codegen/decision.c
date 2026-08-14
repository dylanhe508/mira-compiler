#include "decision.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int64_t position;
    int delta;
    DecisionRegisterClass reg_class;
} PressureEvent;

static int compare_pressure_event(const void *left, const void *right) {
    const PressureEvent *a = (const PressureEvent *)left;
    const PressureEvent *b = (const PressureEvent *)right;
    if (a->position < b->position) return -1;
    if (a->position > b->position) return 1;
    return (int)a->reg_class - (int)b->reg_class;
}

static int32_t clamp_score(int64_t value) {
    if (value > DECISION_SCORE_MAX) return DECISION_SCORE_MAX;
    if (value < DECISION_SCORE_MIN) return DECISION_SCORE_MIN;
    return (int32_t)value;
}

void decision_context_init(DecisionContext *ctx, DecisionSite site) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
    ctx->site = site;
    ctx->hotness = DECISION_SCALE;
    ctx->remaining_budget = UINT32_MAX;
    ctx->code_size_weight = 1;
}

static int32_t score_candidate(const DecisionContext *ctx,
                               const DecisionCandidate *candidate) {
    int64_t gross = candidate->estimated_benefit;
    int64_t costs = candidate->estimated_cost;
    costs += (int64_t)candidate->code_size_cost * ctx->code_size_weight;
    costs += (int64_t)candidate->register_cost * (DECISION_SCALE + ctx->register_pressure)
             / DECISION_SCALE;
    costs += (int64_t)candidate->memory_cost * (DECISION_SCALE + ctx->memory_pressure)
             / DECISION_SCALE;

    gross = gross * ctx->hotness / DECISION_SCALE;
    gross = gross * candidate->confidence / DECISION_SCALE;
    return clamp_score(gross - costs);
}

DecisionResult decision_choose(const DecisionContext *ctx,
                               const DecisionCandidate *candidates,
                               size_t candidate_count) {
    DecisionResult result;
    memset(&result, 0, sizeof(result));
    result.kind = DECISION_KEEP;
    result.score = DECISION_SCORE_MIN;
    result.used_fallback = true;
    if (!ctx || !candidates || candidate_count == 0) return result;

    for (size_t i = 0; i < candidate_count; ++i) {
        const DecisionCandidate *candidate = &candidates[i];
        uint32_t rejected = DECISION_REJECT_NONE;
        if (!candidate->legal) rejected |= DECISION_REJECT_ILLEGAL;
        if (candidate->budget_cost > ctx->remaining_budget)
            rejected |= DECISION_REJECT_BUDGET;
        if (candidate->kind != DECISION_KEEP && candidate->confidence == 0)
            rejected |= DECISION_REJECT_UNCERTAIN;
        if (rejected) {
            result.rejected |= rejected;
            continue;
        }

        int32_t score = score_candidate(ctx, candidate);
        if (candidate->kind != DECISION_KEEP && score <= 0) {
            result.rejected |= DECISION_REJECT_NOT_PROFITABLE;
            continue;
        }
        if (result.score == DECISION_SCORE_MIN || score > result.score) {
            result.index = i;
            result.kind = candidate->kind;
            result.parameter = candidate->parameter;
            result.score = score;
            result.used_fallback = candidate->kind == DECISION_KEEP;
        }
    }
    if (result.score == DECISION_SCORE_MIN) {
        result.index = 0;
        result.kind = DECISION_KEEP;
        result.score = 0;
        result.used_fallback = true;
    }
    return result;
}

DecisionKind decision_choose_branch(uint64_t taken, uint64_t not_taken,
                                    int branchless_legal,
                                    DecisionResult *detail) {
    uint64_t total = taken + not_taken;
    uint64_t hot = taken > not_taken ? taken : not_taken;
    DecisionContext ctx;
    DecisionCandidate candidates[3];
    decision_context_init(&ctx, DECISION_SITE_BRANCH);
    ctx.sample_count = total > UINT32_MAX ? UINT32_MAX : (uint32_t)total;

    uint32_t confidence = 0;
    if (total >= 16) {
        uint64_t capped = total > 256 ? 256 : total;
        confidence = (uint32_t)(capped * DECISION_SCALE / 256);
        if (confidence < 256) confidence = 256;
    }
    uint32_t predictability = total ? (uint32_t)(hot * DECISION_SCALE / total)
                                    : DECISION_SCALE / 2;

    memset(candidates, 0, sizeof(candidates));
    candidates[0].kind = DECISION_KEEP;
    candidates[0].legal = true;
    candidates[0].confidence = DECISION_SCALE;

    candidates[1].kind = DECISION_BRANCH;
    candidates[1].legal = true;
    candidates[1].confidence = confidence;
    /* A predictable branch earns up to 96 units; random branches earn none. */
    candidates[1].estimated_benefit = predictability > DECISION_SCALE / 2
        ? (int32_t)((predictability - DECISION_SCALE / 2) * 192 / DECISION_SCALE)
        : 0;
    candidates[1].estimated_cost = 4;

    candidates[2].kind = DECISION_BRANCHLESS;
    candidates[2].legal = branchless_legal != 0;
    candidates[2].confidence = confidence;
    /* Branchless form is valuable in inverse proportion to predictability. */
    candidates[2].estimated_benefit = (int32_t)((DECISION_SCALE - predictability) * 160
                                                / (DECISION_SCALE / 2));
    candidates[2].estimated_cost = 12;
    candidates[2].register_cost = 2;

    DecisionResult result = decision_choose(&ctx, candidates, 3);
    if (detail) *detail = result;
    return result.kind;
}

DecisionKind decision_choose_inline(uint32_t instruction_count, int hot_callsite,
                                    int zero_growth, uint32_t register_pressure,
                                    uint32_t memory_cost,
                                    uint32_t remaining_budget,
                                    DecisionResult *detail) {
    DecisionContext ctx;
    DecisionCandidate candidates[2];
    decision_context_init(&ctx, DECISION_SITE_INLINE);
    ctx.hotness = hot_callsite ? DECISION_SCALE : DECISION_SCALE / 4;
    ctx.register_pressure = register_pressure * 96;
    ctx.memory_pressure = memory_cost * 8;
    ctx.remaining_budget = remaining_budget;

    memset(candidates, 0, sizeof(candidates));
    candidates[0].kind = DECISION_KEEP;
    candidates[0].legal = true;
    candidates[0].confidence = DECISION_SCALE;

    candidates[1].kind = DECISION_INLINE_FULL;
    candidates[1].legal = instruction_count != 0 && register_pressure < 12;
    candidates[1].confidence = DECISION_SCALE;
    candidates[1].budget_cost = instruction_count;
    candidates[1].estimated_benefit = hot_callsite ? 160 : 72;
    candidates[1].estimated_cost = (int32_t)(instruction_count / 3);
    candidates[1].code_size_cost = zero_growth ? 0 : (int32_t)instruction_count;
    candidates[1].register_cost = (int32_t)(register_pressure / 2);
    candidates[1].memory_cost = memory_cost > INT32_MAX ? INT32_MAX : (int32_t)memory_cost;

    DecisionResult result = decision_choose(&ctx, candidates, 2);
    if (detail) *detail = result;
    return result.kind;
}

DecisionKind decision_choose_loop(uint64_t trip_count, uint32_t body_instructions,
                                  int dependence_free, int avx2_available,
                                  uint32_t max_unroll_factor,
                                  uint32_t register_pressure,
                                  uint32_t remaining_budget,
                                  DecisionResult *detail) {
    DecisionContext ctx;
    DecisionCandidate candidates[3];
    decision_context_init(&ctx, DECISION_SITE_UNROLL);
    ctx.hotness = trip_count >= 64 ? DECISION_SCALE
        : (uint32_t)(trip_count * DECISION_SCALE / 64);
    ctx.register_pressure = register_pressure * 96;
    ctx.remaining_budget = remaining_budget;

    memset(candidates, 0, sizeof(candidates));
    candidates[0].kind = DECISION_KEEP;
    candidates[0].legal = true;
    candidates[0].confidence = DECISION_SCALE;

    candidates[1].kind = DECISION_UNROLL;
    candidates[1].parameter = max_unroll_factor <= 2 ? 2
        : (body_instructions <= 12 ? 4 : 2);
    candidates[1].legal = dependence_free && max_unroll_factor >= 2 &&
        trip_count >= (uint64_t)candidates[1].parameter;
    candidates[1].confidence = DECISION_SCALE;
    candidates[1].budget_cost = body_instructions * (uint32_t)(candidates[1].parameter - 1);
    candidates[1].estimated_benefit = candidates[1].parameter == 4 ? 150 : 80;
    candidates[1].estimated_cost = (int32_t)body_instructions * (candidates[1].parameter - 1);
    candidates[1].code_size_cost = (int32_t)body_instructions * (candidates[1].parameter - 1);
    candidates[1].register_cost = (int32_t)(register_pressure / 2);

    candidates[2].kind = DECISION_VECTORIZE;
    candidates[2].parameter = 4;
    candidates[2].legal = dependence_free && avx2_available && trip_count >= 4;
    candidates[2].confidence = DECISION_SCALE;
    candidates[2].budget_cost = body_instructions + 12;
    candidates[2].estimated_benefit = 280;
    candidates[2].estimated_cost = 24 + (int32_t)(body_instructions / 2);
    candidates[2].code_size_cost = 12;
    candidates[2].register_cost = 3;

    /* High pressure plus a large body is intentionally left scalar. */
    if (register_pressure >= 12 && body_instructions >= 24) {
        candidates[1].legal = false;
        candidates[2].legal = false;
    }

    DecisionResult result = decision_choose(&ctx, candidates, 3);
    if (detail) *detail = result;
    return result.kind;
}

DecisionKind decision_choose_affine(uint32_t old_insts, uint32_t new_insts,
                                    uint32_t old_bytes, uint32_t new_bytes,
                                    uint32_t old_pressure, uint32_t new_pressure,
                                    uint32_t confidence,
                                    DecisionResult *detail) {
    DecisionContext ctx;
    DecisionCandidate candidates[2];
    decision_context_init(&ctx, DECISION_SITE_ARITHMETIC);

    memset(candidates, 0, sizeof(candidates));
    candidates[0].kind = DECISION_KEEP;
    candidates[0].legal = true;
    candidates[0].confidence = DECISION_SCALE;

    candidates[1].kind = DECISION_AFFINE_COLLAPSE;
    candidates[1].legal = new_insts < old_insts && new_bytes <= old_bytes &&
                          new_pressure <= old_pressure;
    candidates[1].confidence = confidence;
    candidates[1].estimated_benefit =
        old_insts - new_insts > INT32_MAX / 16u
            ? INT32_MAX
            : (int32_t)((old_insts - new_insts) * 16u);
    candidates[1].estimated_cost =
        new_insts > INT32_MAX / 2u ? INT32_MAX : (int32_t)(new_insts * 2u);
    candidates[1].code_size_cost =
        new_bytes > INT32_MAX ? INT32_MAX : (int32_t)new_bytes;
    candidates[1].register_cost =
        new_pressure > INT32_MAX ? INT32_MAX : (int32_t)new_pressure;

    DecisionResult result = decision_choose(&ctx, candidates, 2);
    if (detail) *detail = result;
    return result.kind;
}

const char *decision_kind_name(DecisionKind kind) {
    switch (kind) {
    case DECISION_KEEP: return "keep";
    case DECISION_BRANCH: return "branch";
    case DECISION_BRANCHLESS: return "branchless";
    case DECISION_INLINE_FULL: return "inline-full";
    case DECISION_INLINE_PARTIAL: return "inline-partial";
    case DECISION_UNROLL: return "unroll";
    case DECISION_VECTORIZE: return "vectorize";
    case DECISION_MEMORY_TRANSFORM: return "memory";
    case DECISION_AFFINE_COLLAPSE: return "affine-collapse";
    }
    return "unknown";
}

DecisionPressure decision_measure_pressure(const DecisionLiveRange *ranges,
                                           size_t range_count) {
    DecisionPressure result = {0, 0, 0};
    if (!ranges || range_count == 0 || range_count > SIZE_MAX / (2 * sizeof(PressureEvent)))
        return result;
    PressureEvent *events = malloc(range_count * 2 * sizeof(*events));
    if (!events) return result;
    size_t event_count = 0;
    for (size_t i = 0; i < range_count; ++i) {
        if (ranges[i].start <= 0 || ranges[i].end < ranges[i].start ||
            ranges[i].reg_class > DECISION_REG_VECTOR) continue;
        events[event_count++] = (PressureEvent){ ranges[i].start, 1, ranges[i].reg_class };
        events[event_count++] = (PressureEvent){ (int64_t)ranges[i].end + 1, -1,
                                                ranges[i].reg_class };
    }
    qsort(events, event_count, sizeof(*events), compare_pressure_event);
    uint32_t live[3] = {0, 0, 0};
    for (size_t i = 0; i < event_count; ) {
        int64_t position = events[i].position;
        while (i < event_count && events[i].position == position) {
            unsigned cls = (unsigned)events[i].reg_class;
            if (events[i].delta > 0) live[cls]++;
            else if (live[cls] != 0) live[cls]--;
            i++;
        }
        if (live[0] > result.scalar_peak) result.scalar_peak = live[0];
        if (live[1] > result.float_peak) result.float_peak = live[1];
        if (live[2] > result.vector_peak) result.vector_peak = live[2];
    }
    free(events);
    return result;
}

DecisionMemoryFacts decision_memory_facts(uint32_t reads, uint32_t writes,
                                          int accesses_known, int may_alias,
                                          int unknown_call,
                                          int ownership_transfer) {
    DecisionMemoryFacts result;
    result.reads = reads;
    result.writes = writes;
    result.accesses_known = accesses_known != 0;
    result.may_alias = may_alias != 0;
    result.unknown_call = unknown_call != 0;
    result.ownership_transfer = ownership_transfer != 0;
    result.reorder_safe = result.accesses_known && !result.unknown_call &&
        !result.ownership_transfer && (writes == 0 || !result.may_alias);
    return result;
}

DecisionPipelinePlan decision_pipeline_plan(uint32_t ir_instruction_count,
                                            int optimization_level,
                                            int avx2_available,
                                            uint32_t register_pressure,
                                            uint32_t float_register_pressure,
                                            int memory_unknown) {
    DecisionPipelinePlan result = {0};
    if (optimization_level < 2 || ir_instruction_count == 0) return result;
    result.allow_memory_optimization = ir_instruction_count < 200000;
    result.allow_magic_division = register_pressure < 14 &&
        ir_instruction_count < 100000;
    if (optimization_level < 3) return result;
    result.allow_schedule = true;
    result.allow_if_conversion = ir_instruction_count < 200000;
    result.allow_unroll = register_pressure < 12 && ir_instruction_count < 100000;
    result.require_runtime_alias_checks = memory_unknown != 0;
    /* The raw-pointer vectorizer emits overlap guards and retains the scalar
     * loop as its fallback. Read-only reductions need no alias guard. */
    result.allow_vectorize = result.allow_unroll && avx2_available;
    result.allow_float_optimization = float_register_pressure < 10 &&
        ir_instruction_count < 100000;
    result.allow_scalar_loop_optimization = register_pressure < 14 &&
        ir_instruction_count < 100000;
    result.allow_affine_recurrence = result.allow_scalar_loop_optimization;
    result.allow_affine_collapse = true;
    result.allow_magic_division = result.allow_scalar_loop_optimization;
    result.allow_loop_rotation = result.allow_scalar_loop_optimization;
    uint32_t proportional = ir_instruction_count / 4;
    if (proportional < 64) proportional = 64;
    if (proportional > 4096) proportional = 4096;
    result.code_growth_budget = proportional;
    return result;
}

DecisionFunctionPlan decision_function_plan(uint32_t ir_instruction_count,
                                            int optimization_level,
                                            int avx2_available,
                                            uint32_t register_pressure,
                                            uint32_t float_register_pressure,
                                            const DecisionReferenceFacts *reference,
                                            uint32_t generation) {
    DecisionFunctionPlan result;
    memset(&result, 0, sizeof(result));
    result.generation = generation;

    bool memory_unknown = reference && reference->has_unknown_effect;
    result.pipeline = decision_pipeline_plan(ir_instruction_count,
        optimization_level, avx2_available, register_pressure,
        float_register_pressure, memory_unknown);

    uint64_t raw_budget = (uint64_t)ir_instruction_count * 2u + 32u;
    uint32_t budget = raw_budget > UINT32_MAX ? UINT32_MAX : (uint32_t)raw_budget;
    if (budget < 64u) budget = 64u;
    if (budget > 4096u) budget = 4096u;
    result.reference_analysis_budget = budget;
    result.allow_inline = optimization_level >= 3 &&
        ir_instruction_count != 0 && ir_instruction_count < 100000u;

    if (reference) {
        if (reference->has_unknown_effect) {
            result.reference_vetoes |= DECISION_REF_VETO_MEMORY_REORDER;
            result.reference_vetoes |= DECISION_REF_VETO_EFFECT_REMOVAL;
            result.request_deep_reference_analysis = optimization_level >= 3;
        }
        if (reference->owned_object_count != 0)
            result.reference_vetoes |= DECISION_REF_VETO_OWNERSHIP_MOVE;
        /* Deterministic DCE remains enabled: the veto only forbids removing
         * operations whose observability has not been proven by reference. */
        if (reference->writes_global)
            result.reference_vetoes |= DECISION_REF_VETO_EFFECT_REMOVAL;
    }
    return result;
}

DecisionLoopPlan decision_loop_plan(uint64_t trip_count,
                                    uint32_t body_instructions,
                                    uint32_t backedge_count,
                                    uint32_t register_pressure,
                                    int avx2_available,
                                    const DecisionMemoryFacts *memory,
                                    uint32_t remaining_budget) {
    DecisionLoopPlan result;
    memset(&result, 0, sizeof(result));
    result.preferred = DECISION_KEEP;
    result.body_instructions = body_instructions;
    result.backedge_count = backedge_count;
    if (body_instructions == 0 || backedge_count != 1) return result;

    bool memory_legal = !memory || memory->reorder_safe;
    bool guarded_memory = memory && !memory->reorder_safe &&
        !memory->unknown_call && !memory->ownership_transfer;
    result.require_runtime_alias_check = guarded_memory;
    if (memory && memory->unknown_call)
        result.reference_vetoes |= DECISION_REF_VETO_MEMORY_REORDER;
    if (memory && memory->ownership_transfer)
        result.reference_vetoes |= DECISION_REF_VETO_OWNERSHIP_MOVE;

    DecisionResult detail;
    /* The current SIMD loop backend needs a vectorizable memory stream.
     * Pure scalar recurrences may still be unrolled, but advertising AVX2
     * here would promise a transform the executor cannot yet emit. */
    bool vector_memory_candidate = memory && memory->reads != 0;
    DecisionKind choice = decision_choose_loop(
        trip_count ? trip_count : 64u, body_instructions,
        memory_legal || guarded_memory,
        avx2_available && vector_memory_candidate, 4,
        register_pressure, remaining_budget, &detail);
    result.parameter = detail.parameter;
    result.allow_unroll = choice == DECISION_UNROLL;
    result.allow_vectorize = choice == DECISION_VECTORIZE;
    result.preferred = choice;
    if (result.reference_vetoes != 0) {
        result.allow_unroll = false;
        result.allow_vectorize = false;
        result.preferred = DECISION_KEEP;
        result.parameter = 0;
    }
    return result;
}

static bool category_equal(const char *begin, size_t length, const char *name) {
    return strlen(name) == length && memcmp(begin, name, length) == 0;
}

void decision_pipeline_disable(DecisionPipelinePlan *plan, const char *categories) {
    if (!plan || !categories) return;
    const char *cursor = categories;
    while (*cursor) {
        while (*cursor == ',' || *cursor == ' ' || *cursor == '\t') cursor++;
        const char *begin = cursor;
        while (*cursor && *cursor != ',' && *cursor != ' ' && *cursor != '\t') cursor++;
        size_t length = (size_t)(cursor - begin);
        if (category_equal(begin, length, "vectorize")) plan->allow_vectorize = false;
        else if (category_equal(begin, length, "unroll")) plan->allow_unroll = false;
        else if (category_equal(begin, length, "schedule")) plan->allow_schedule = false;
        else if (category_equal(begin, length, "ifconvert")) plan->allow_if_conversion = false;
        else if (category_equal(begin, length, "float")) plan->allow_float_optimization = false;
        else if (category_equal(begin, length, "scalar-loop")) {
            plan->allow_scalar_loop_optimization = false;
            plan->allow_affine_recurrence = false;
            plan->allow_magic_division = false;
            plan->allow_loop_rotation = false;
        }
        else if (category_equal(begin, length, "affine")) plan->allow_affine_recurrence = false;
        else if (category_equal(begin, length, "affine-collapse"))
            plan->allow_affine_collapse = false;
        else if (category_equal(begin, length, "magic")) plan->allow_magic_division = false;
        else if (category_equal(begin, length, "rotate")) plan->allow_loop_rotation = false;
        else if (category_equal(begin, length, "memory")) plan->allow_memory_optimization = false;
    }
}
