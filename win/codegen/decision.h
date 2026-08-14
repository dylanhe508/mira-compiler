#ifndef MIRA_DECISION_H
#define MIRA_DECISION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DECISION_SCALE 1024u
#define DECISION_SCORE_MAX INT32_MAX
#define DECISION_SCORE_MIN INT32_MIN

typedef enum {
    DECISION_SITE_BRANCH = 0,
    DECISION_SITE_INLINE,
    DECISION_SITE_UNROLL,
    DECISION_SITE_VECTORIZE,
    DECISION_SITE_MEMORY,
    DECISION_SITE_REGISTER,
    DECISION_SITE_ARITHMETIC
} DecisionSite;

typedef enum {
    DECISION_KEEP = 0,
    DECISION_BRANCH,
    DECISION_BRANCHLESS,
    DECISION_INLINE_FULL,
    DECISION_INLINE_PARTIAL,
    DECISION_UNROLL,
    DECISION_VECTORIZE,
    DECISION_MEMORY_TRANSFORM,
    DECISION_AFFINE_COLLAPSE
} DecisionKind;

typedef enum {
    DECISION_REG_SCALAR = 0,
    DECISION_REG_FLOAT,
    DECISION_REG_VECTOR
} DecisionRegisterClass;

typedef struct {
    int32_t start;
    int32_t end;
    DecisionRegisterClass reg_class;
} DecisionLiveRange;

typedef struct {
    uint32_t scalar_peak;
    uint32_t float_peak;
    uint32_t vector_peak;
} DecisionPressure;

typedef struct {
    uint32_t reads;
    uint32_t writes;
    bool accesses_known;
    bool may_alias;
    bool unknown_call;
    bool ownership_transfer;
    bool reorder_safe;
} DecisionMemoryFacts;

typedef struct {
    bool allow_vectorize;
    bool allow_unroll;
    bool allow_schedule;
    bool allow_if_conversion;
    bool allow_float_optimization;
    bool allow_scalar_loop_optimization;
    bool allow_affine_recurrence;
    bool allow_affine_collapse;
    bool allow_magic_division;
    bool allow_loop_rotation;
    bool allow_memory_optimization;
    bool require_runtime_alias_checks;
    uint32_t code_growth_budget;
} DecisionPipelinePlan;

typedef enum {
    DECISION_REF_VETO_NONE = 0,
    DECISION_REF_VETO_MEMORY_REORDER = 1u << 0,
    DECISION_REF_VETO_EFFECT_REMOVAL = 1u << 1,
    DECISION_REF_VETO_OWNERSHIP_MOVE = 1u << 2
} DecisionReferenceVeto;

/* Produced by static reference. Decision consumes these facts but cannot
 * weaken a veto: legality remains owned by the reference/ownership layer. */
typedef struct {
    uint32_t value_count;
    uint32_t known_value_count;
    uint32_t unique_object_count;
    uint32_t owned_object_count;
    bool reads_memory;
    bool writes_memory;
    bool reads_global;
    bool writes_global;
    bool allocates;
    bool has_unknown_effect;
} DecisionReferenceFacts;

typedef struct {
    DecisionPipelinePlan pipeline;
    uint32_t reference_vetoes;
    uint32_t reference_analysis_budget;
    uint32_t generation;
    bool allow_inline;
    bool request_deep_reference_analysis;
    bool prefer_global_graph_coloring;
} DecisionFunctionPlan;

typedef struct {
    DecisionKind preferred;
    int32_t parameter;
    uint32_t reference_vetoes;
    uint32_t body_instructions;
    uint32_t backedge_count;
    bool allow_unroll;
    bool allow_vectorize;
    bool require_runtime_alias_check;
} DecisionLoopPlan;

typedef enum {
    DECISION_REJECT_NONE = 0,
    DECISION_REJECT_ILLEGAL = 1u << 0,
    DECISION_REJECT_BUDGET = 1u << 1,
    DECISION_REJECT_UNCERTAIN = 1u << 2,
    DECISION_REJECT_NOT_PROFITABLE = 1u << 3
} DecisionRejectReason;

typedef struct {
    DecisionSite site;
    uint32_t hotness;          /* 0..DECISION_SCALE */
    uint32_t sample_count;
    uint32_t remaining_budget;
    uint32_t code_size_weight;
    uint32_t register_pressure;
    uint32_t memory_pressure;
} DecisionContext;

typedef struct {
    DecisionKind kind;
    int32_t parameter;
    int32_t estimated_benefit;
    int32_t estimated_cost;
    int32_t code_size_cost;
    int32_t register_cost;
    int32_t memory_cost;
    uint32_t confidence;       /* 0..DECISION_SCALE */
    uint32_t budget_cost;
    bool legal;
} DecisionCandidate;

typedef struct {
    size_t index;
    DecisionKind kind;
    int32_t parameter;
    int32_t score;
    uint32_t rejected;
    bool used_fallback;
} DecisionResult;

void decision_context_init(DecisionContext *ctx, DecisionSite site);
DecisionResult decision_choose(const DecisionContext *ctx,
                               const DecisionCandidate *candidates,
                               size_t candidate_count);

/* Returns DECISION_BRANCH or DECISION_BRANCHLESS.  The caller remains
 * responsible for structural and semantic legality of if-conversion. */
DecisionKind decision_choose_branch(uint64_t taken, uint64_t not_taken,
                                    int branchless_legal,
                                    DecisionResult *detail);
DecisionKind decision_choose_inline(uint32_t instruction_count, int hot_callsite,
                                    int zero_growth, uint32_t register_pressure,
                                    uint32_t memory_cost,
                                    uint32_t remaining_budget,
                                    DecisionResult *detail);
DecisionKind decision_choose_loop(uint64_t trip_count, uint32_t body_instructions,
                                  int dependence_free, int avx2_available,
                                  uint32_t max_unroll_factor,
                                  uint32_t register_pressure,
                                  uint32_t remaining_budget,
                                  DecisionResult *detail);
DecisionKind decision_choose_affine(uint32_t old_insts, uint32_t new_insts,
                                    uint32_t old_bytes, uint32_t new_bytes,
                                    uint32_t old_pressure, uint32_t new_pressure,
                                    uint32_t confidence,
                                    DecisionResult *detail);

const char *decision_kind_name(DecisionKind kind);
DecisionPressure decision_measure_pressure(const DecisionLiveRange *ranges,
                                           size_t range_count);
DecisionMemoryFacts decision_memory_facts(uint32_t reads, uint32_t writes,
                                          int accesses_known, int may_alias,
                                          int unknown_call,
                                          int ownership_transfer);
DecisionPipelinePlan decision_pipeline_plan(uint32_t ir_instruction_count,
                                            int optimization_level,
                                            int avx2_available,
                                            uint32_t register_pressure,
                                            uint32_t float_register_pressure,
                                            int memory_unknown);
DecisionFunctionPlan decision_function_plan(uint32_t ir_instruction_count,
                                            int optimization_level,
                                            int avx2_available,
                                            uint32_t register_pressure,
                                            uint32_t float_register_pressure,
                                            const DecisionReferenceFacts *reference,
                                            uint32_t generation);
DecisionLoopPlan decision_loop_plan(uint64_t trip_count,
                                    uint32_t body_instructions,
                                    uint32_t backedge_count,
                                    uint32_t register_pressure,
                                    int avx2_available,
                                    const DecisionMemoryFacts *memory,
                                    uint32_t remaining_budget);
void decision_pipeline_disable(DecisionPipelinePlan *plan, const char *categories);

#endif
