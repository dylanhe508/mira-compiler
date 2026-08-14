#include "ir_ssa.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint64_t hash;
    const char *name;
    SsaFunction *function;
    int ordinal;
} SsaFunctionNameEntry;

typedef struct {
    const SsaFunction *function;
    int ordinal;
} SsaFunctionPointerEntry;

struct SsaFunctionIndex {
    SsaFunctionNameEntry *names;
    SsaFunctionPointerEntry *pointers;
    size_t capacity;
    uint64_t epoch;
    uint64_t name_comparisons;
    uint32_t *direct_calls;
    unsigned char *referenced;
    unsigned char *leaf;
    bool call_facts_valid;
};

static uint64_t hash_name(const char *name) {
    uint64_t hash = UINT64_C(1469598103934665603);
    if (!name) return hash;
    while (*name) {
        hash ^= (unsigned char)*name++;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static size_t hash_pointer(const void *pointer) {
    uintptr_t value = (uintptr_t)pointer;
    value ^= value >> 17;
    value *= (uintptr_t)0xed5ad4bbU;
    value ^= value >> 11;
    return (size_t)value;
}

static bool index_is_current(const SsaModule *mod) {
    return mod && mod->function_index &&
           mod->function_index->epoch == mod->function_epoch;
}

void ssa_function_index_free(SsaModule *mod) {
    if (!mod || !mod->function_index) return;
    free(mod->function_index->names);
    free(mod->function_index->pointers);
    free(mod->function_index->direct_calls);
    free(mod->function_index->referenced);
    free(mod->function_index->leaf);
    free(mod->function_index);
    mod->function_index = NULL;
}

void ssa_function_index_invalidate(SsaModule *mod) {
    if (!mod) return;
    mod->function_epoch++;
}

bool ssa_function_index_rebuild(SsaModule *mod) {
    if (!mod || mod->func_count < 0) return false;
    size_t capacity = 8;
    while (capacity < (size_t)mod->func_count * 2u) {
        if (capacity > SIZE_MAX / 2u) return false;
        capacity *= 2u;
    }
    SsaFunctionIndex *next = calloc(1, sizeof(*next));
    if (!next) return false;
    next->names = calloc(capacity, sizeof(*next->names));
    next->pointers = calloc(capacity, sizeof(*next->pointers));
    next->direct_calls = calloc(capacity, sizeof(*next->direct_calls));
    next->referenced = calloc(capacity, sizeof(*next->referenced));
    next->leaf = calloc(capacity, sizeof(*next->leaf));
    if (!next->names || !next->pointers || !next->direct_calls ||
        !next->referenced || !next->leaf) {
        free(next->names); free(next->pointers); free(next->direct_calls);
        free(next->referenced); free(next->leaf); free(next);
        return false;
    }
    next->capacity = capacity;
    next->epoch = mod->function_epoch;
    size_t mask = capacity - 1u;
    for (int i = 0; i < mod->func_count; ++i) {
        SsaFunction *function = mod->functions ? mod->functions[i] : NULL;
        if (!function) continue;
        uint64_t hash = hash_name(function->name);
        size_t slot = (size_t)hash & mask;
        while (next->names[slot].function) slot = (slot + 1u) & mask;
        next->names[slot] = (SsaFunctionNameEntry){hash, function->name, function, i};
        slot = hash_pointer(function) & mask;
        while (next->pointers[slot].function) slot = (slot + 1u) & mask;
        next->pointers[slot] = (SsaFunctionPointerEntry){function, i};
    }
    ssa_function_index_free(mod);
    mod->function_index = next;
    return true;
}

SsaFunction *ssa_function_index_find(const SsaModule *mod, const char *name) {
    if (!index_is_current(mod) || !name) return NULL;
    SsaFunctionIndex *index = mod->function_index;
    uint64_t hash = hash_name(name);
    size_t mask = index->capacity - 1u;
    size_t slot = (size_t)hash & mask;
    while (index->names[slot].function) {
        if (index->names[slot].hash == hash) {
            index->name_comparisons++;
            if (strcmp(index->names[slot].name, name) == 0)
                return index->names[slot].function;
        }
        slot = (slot + 1u) & mask;
    }
    return NULL;
}

int ssa_function_index_ordinal(const SsaModule *mod, const SsaFunction *function) {
    if (!index_is_current(mod) || !function) return -1;
    const SsaFunctionIndex *index = mod->function_index;
    size_t mask = index->capacity - 1u;
    size_t slot = hash_pointer(function) & mask;
    while (index->pointers[slot].function) {
        if (index->pointers[slot].function == function)
            return index->pointers[slot].ordinal;
        slot = (slot + 1u) & mask;
    }
    return -1;
}

uint64_t ssa_function_index_name_comparisons(const SsaModule *mod) {
    return mod && mod->function_index ? mod->function_index->name_comparisons : 0;
}

void ssa_function_index_invalidate_call_facts(SsaModule *mod) {
    if (mod && mod->function_index) mod->function_index->call_facts_valid = false;
}

static void mark_symbol_reference(SsaModule *mod, const SsaOperand *operand) {
    if (!operand || operand->kind != SSA_OPND_SYM || !operand->u.sym) return;
    SsaFunction *target = ssa_function_index_find(mod, operand->u.sym);
    int ordinal = ssa_function_index_ordinal(mod, target);
    if (ordinal >= 0) mod->function_index->referenced[ordinal] = 1;
}

bool ssa_function_index_rebuild_call_facts(SsaModule *mod) {
    if (!index_is_current(mod) && !ssa_function_index_rebuild(mod)) return false;
    SsaFunctionIndex *index = mod->function_index;
    memset(index->direct_calls, 0, index->capacity * sizeof(*index->direct_calls));
    memset(index->referenced, 0, index->capacity * sizeof(*index->referenced));
    memset(index->leaf, 1, index->capacity * sizeof(*index->leaf));
    for (int fi = 0; fi < mod->func_count; ++fi) {
        SsaFunction *func = mod->functions[fi];
        if (!func) continue;
        for (int bi = 0; bi < func->block_count; ++bi) {
            SsaBasicBlock *block = func->blocks[bi];
            if (!block) continue;
            for (SsaInst *inst = block->inst_head; inst; inst = inst->next) {
                if (inst->IrNode == SSA_OP_CALL || inst->IrNode == SSA_OP_ICALL)
                    index->leaf[fi] = 0;
                mark_symbol_reference(mod, &inst->op1);
                mark_symbol_reference(mod, &inst->op2);
                for (int oi = 0; inst->operands && oi < inst->operand_count; ++oi)
                    mark_symbol_reference(mod, &inst->operands[oi]);
                if (inst->IrNode == SSA_OP_CALL && inst->operands &&
                    inst->operand_count > 0 && inst->operands[0].kind == SSA_OPND_SYM) {
                    SsaFunction *target =
                        ssa_function_index_find(mod, inst->operands[0].u.sym);
                    int ordinal = ssa_function_index_ordinal(mod, target);
                    if (ordinal >= 0 && index->direct_calls[ordinal] != UINT32_MAX)
                        index->direct_calls[ordinal]++;
                }
            }
        }
    }
    index->call_facts_valid = true;
    return true;
}

uint32_t ssa_function_index_direct_calls(const SsaModule *mod,
                                         const SsaFunction *function) {
    if (!index_is_current(mod) || !mod->function_index->call_facts_valid) return 0;
    int ordinal = ssa_function_index_ordinal(mod, function);
    return ordinal >= 0 ? mod->function_index->direct_calls[ordinal] : 0;
}

bool ssa_function_index_is_referenced(const SsaModule *mod,
                                      const SsaFunction *function) {
    if (!index_is_current(mod) || !mod->function_index->call_facts_valid) return true;
    int ordinal = ssa_function_index_ordinal(mod, function);
    return ordinal < 0 || mod->function_index->referenced[ordinal] != 0;
}

bool ssa_function_index_is_leaf(const SsaModule *mod,
                                const SsaFunction *function) {
    if (!index_is_current(mod) || !mod->function_index->call_facts_valid) return false;
    int ordinal = ssa_function_index_ordinal(mod, function);
    return ordinal >= 0 && mod->function_index->leaf[ordinal] != 0;
}
