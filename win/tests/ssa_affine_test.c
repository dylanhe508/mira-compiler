#include "../codegen/ir_ssa.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static SsaOperand vreg(VReg value) {
    SsaOperand operand = {0};
    operand.kind = SSA_OPND_VREG;
    operand.u.vreg = value;
    return operand;
}

static SsaOperand imm(int64_t value) {
    SsaOperand operand = {0};
    operand.kind = SSA_OPND_IMM;
    operand.u.imm = value;
    return operand;
}

static void define(SsaFunction *func, SsaInst *insts, size_t index,
                   SsaOpcode opcode, VReg dst, SsaOperand left,
                   SsaOperand right) {
    SsaInst *inst = &insts[index];
    memset(inst, 0, sizeof(*inst));
    inst->IrNode = opcode;
    inst->type = SSA_TYPE_INT;
    inst->dst = dst;
    inst->op1 = left;
    inst->op2 = right;
    func->vreg_defs[dst] = inst;
}

static void test_composes_affine_expression(void) {
    SsaFunction func = {0};
    SsaInst insts[13];
    SsaInst *defs[16] = {0};
    SsaAffineFact facts[16];
    func.next_vreg = 14;
    func.vreg_defs = defs;
    func.vreg_defs_cap = 16;

    define(&func, insts, 0, SSA_OP_LOAD_PARAM, 1, imm(0), imm(0));
    define(&func, insts, 1, SSA_OP_COPY, 2, vreg(1), imm(0));
    define(&func, insts, 2, SSA_OP_IMM, 3, imm(3), imm(0));
    define(&func, insts, 3, SSA_OP_MUL, 4, vreg(2), vreg(3));
    define(&func, insts, 4, SSA_OP_IMM, 5, imm(2), imm(0));
    define(&func, insts, 5, SSA_OP_ADD, 6, vreg(4), vreg(5));
    define(&func, insts, 6, SSA_OP_IMM, 7, imm(5), imm(0));
    define(&func, insts, 7, SSA_OP_MUL, 8, vreg(6), vreg(7));
    define(&func, insts, 8, SSA_OP_IMM, 9, imm(7), imm(0));
    define(&func, insts, 9, SSA_OP_MUL, 10, vreg(1), vreg(9));
    define(&func, insts, 10, SSA_OP_IMM, 11, imm(4), imm(0));
    define(&func, insts, 11, SSA_OP_ADD, 12, vreg(10), vreg(11));
    define(&func, insts, 12, SSA_OP_ADD, 13, vreg(8), vreg(12));

    assert(ssa_affine_analyze(&func, facts, 16));
    assert(facts[13].proven);
    assert(facts[13].base == 1);
    assert(facts[13].coefficient == 22);
    assert(facts[13].constant == 14);
}

static void test_rejects_unsupported_forms(void) {
    SsaFunction func = {0};
    SsaInst insts[5];
    SsaInst *defs[8] = {0};
    SsaAffineFact facts[8];
    func.next_vreg = 6;
    func.vreg_defs = defs;
    func.vreg_defs_cap = 8;

    define(&func, insts, 0, SSA_OP_LOAD_PARAM, 1, imm(0), imm(0));
    define(&func, insts, 1, SSA_OP_LOAD_PARAM, 2, imm(1), imm(0));
    define(&func, insts, 2, SSA_OP_ADD, 3, vreg(1), vreg(2));
    define(&func, insts, 3, SSA_OP_MUL, 4, vreg(1), vreg(2));
    define(&func, insts, 4, SSA_OP_CALL, 5, imm(0), imm(0));

    assert(ssa_affine_analyze(&func, facts, 8));
    assert(!facts[3].proven);
    assert(!facts[4].proven);
    assert(!facts[5].proven);
}

static void test_rejects_short_fact_array(void) {
    SsaFunction func = {0};
    SsaInst inst = {0};
    SsaInst *defs[8] = {0};
    SsaAffineFact facts[4];
    func.next_vreg = 6;
    func.vreg_defs = defs;
    func.vreg_defs_cap = 8;
    define(&func, &inst, 0, SSA_OP_LOAD_PARAM, 5, imm(0), imm(0));
    assert(!ssa_affine_analyze(&func, facts, 4));
}

static void test_load_var_is_an_opaque_affine_base(void) {
    SsaFunction func = {0};
    SsaInst insts[2];
    SsaInst *defs[4] = {0};
    SsaAffineFact facts[4];
    func.next_vreg = 3;
    func.vreg_defs = defs;
    func.vreg_defs_cap = 4;
    define(&func, insts, 0, SSA_OP_LOAD_VAR, 1, imm(7), imm(0));
    define(&func, insts, 1, SSA_OP_ADD, 2, vreg(1), imm(9));
    assert(ssa_affine_analyze(&func, facts, 4));
    assert(facts[2].proven);
    assert(facts[2].base == 1);
    assert(facts[2].coefficient == 1);
    assert(facts[2].constant == 9);
}

static SsaInst *append_inst(SsaBasicBlock *block, SsaInst **defs,
                            SsaOpcode opcode, VReg dst, SsaOperand left,
                            SsaOperand right) {
    SsaInst *inst = calloc(1, sizeof(*inst));
    assert(inst);
    inst->IrNode = opcode;
    inst->type = SSA_TYPE_INT;
    inst->dst = dst;
    inst->op1 = left;
    inst->op2 = right;
    inst->operand_count = 2;
    inst->parent = block;
    inst->prev = block->inst_tail;
    if (block->inst_tail) block->inst_tail->next = inst;
    else block->inst_head = inst;
    block->inst_tail = inst;
    if (dst) defs[dst] = inst;
    return inst;
}

static void test_follows_instruction_order_after_inlining(void) {
    SsaFunction func = {0};
    SsaBasicBlock block = {0};
    SsaBasicBlock *blocks[] = {&block};
    SsaInst *defs[4] = {0};
    SsaAffineFact facts[4];
    func.blocks = blocks;
    func.block_count = 1;
    func.vreg_defs = defs;
    func.vreg_defs_cap = 4;
    func.next_vreg = 4;
    append_inst(&block, defs, SSA_OP_LOAD_PARAM, 3, imm(0), imm(0));
    append_inst(&block, defs, SSA_OP_ADD, 1, vreg(3), imm(9));
    assert(ssa_affine_analyze(&func, facts, 4));
    assert(facts[1].proven);
    assert(facts[1].base == 3);
    assert(facts[1].constant == 9);
    SsaInst *inst = block.inst_head;
    while (inst) {
        SsaInst *next = inst->next;
        free(inst);
        inst = next;
    }
}

static uint64_t ir_digest(const SsaFunction *func) {
    uint64_t hash = UINT64_C(1469598103934665603);
    for (int bi = 0; bi < func->block_count; ++bi) {
        for (const SsaInst *inst = func->blocks[bi]->inst_head; inst;
             inst = inst->next) {
            const uint64_t words[] = {
                inst->IrNode, inst->dst, inst->op1.kind,
                inst->op1.kind == SSA_OPND_VREG ? inst->op1.u.vreg
                                                : (uint64_t)inst->op1.u.imm,
                inst->op2.kind,
                inst->op2.kind == SSA_OPND_VREG ? inst->op2.u.vreg
                                                : (uint64_t)inst->op2.u.imm
            };
            for (size_t i = 0; i < sizeof(words) / sizeof(words[0]); ++i) {
                hash ^= words[i];
                hash *= UINT64_C(1099511628211);
            }
        }
    }
    return hash;
}

static int count_opcode(const SsaFunction *func, SsaOpcode opcode) {
    int count = 0;
    for (int bi = 0; bi < func->block_count; ++bi)
        for (const SsaInst *inst = func->blocks[bi]->inst_head; inst;
             inst = inst->next)
            if (inst->IrNode == opcode) count++;
    return count;
}

static void destroy_fixture(SsaFunction *func) {
    SsaBasicBlock *block = func->blocks[0];
    SsaInst *inst = block->inst_head;
    while (inst) {
        SsaInst *next = inst->next;
        free(inst->operands);
        free(inst);
        inst = next;
    }
    free(func->blocks);
    free(func->vreg_defs);
    free(block);
}

enum FixtureVariant {
    FIXTURE_COLLAPSE,
    FIXTURE_DIFFERENT_MASK,
    FIXTURE_SHIFT,
    FIXTURE_TWO_BASES,
    FIXTURE_CALL,
    FIXTURE_NOT_PROFITABLE
};

static SsaFunction make_rewrite_fixture(enum FixtureVariant variant) {
    const int cap = 32;
    const int64_t mask = INT64_MAX;
    SsaFunction func = {0};
    SsaBasicBlock *block = calloc(1, sizeof(*block));
    SsaInst **defs = calloc((size_t)cap, sizeof(*defs));
    func.blocks = calloc(1, sizeof(*func.blocks));
    assert(block && defs && func.blocks);
    func.blocks[0] = block;
    func.block_count = 1;
    func.block_cap = 1;
    func.entry_block = block;
    func.vreg_defs = defs;
    func.vreg_defs_cap = cap;
    func.next_vreg = 1;
    func.estimated_scalar_pressure = 12;
    block->parent = &func;
    block->id = 0;

    VReg x = func.next_vreg++;
    append_inst(block, defs, SSA_OP_LOAD_PARAM, x, imm(0), imm(0));
    if (variant == FIXTURE_NOT_PROFITABLE) {
        VReg add = func.next_vreg++;
        VReg root = func.next_vreg++;
        append_inst(block, defs, SSA_OP_ADD, add, vreg(x), imm(1));
        append_inst(block, defs, SSA_OP_STORE_VAR, 0, vreg(add), imm(9));
        append_inst(block, defs, SSA_OP_AND, root, vreg(add), imm(mask));
        return func;
    }

    VReg shifted = func.next_vreg++;
    VReg left_mul = func.next_vreg++;
    VReg left_add = func.next_vreg++;
    VReg inner = func.next_vreg++;
    append_inst(block, defs, SSA_OP_ADD, shifted, vreg(x), imm(10));
    append_inst(block, defs, SSA_OP_MUL, left_mul, vreg(shifted), imm(3));
    append_inst(block, defs, SSA_OP_ADD, left_add, vreg(left_mul), imm(2));
    append_inst(block, defs, SSA_OP_STORE_VAR, 0, vreg(left_add), imm(10));
    append_inst(block, defs, SSA_OP_AND, inner, vreg(left_add),
                imm(variant == FIXTURE_DIFFERENT_MASK ? 255 : mask));

    VReg left_value = inner;
    if (variant == FIXTURE_SHIFT) {
        left_value = func.next_vreg++;
        append_inst(block, defs, SSA_OP_SHL, left_value, vreg(inner), imm(1));
    } else if (variant == FIXTURE_CALL) {
        left_value = func.next_vreg++;
        append_inst(block, defs, SSA_OP_CALL, left_value, vreg(inner), imm(0));
    }

    VReg scaled = func.next_vreg++;
    append_inst(block, defs, SSA_OP_MUL, scaled, vreg(left_value), imm(5));
    VReg right_base = shifted;
    if (variant == FIXTURE_TWO_BASES) {
        right_base = func.next_vreg++;
        append_inst(block, defs, SSA_OP_LOAD_PARAM, right_base, imm(1), imm(0));
    }
    VReg right_mul = func.next_vreg++;
    VReg right_add = func.next_vreg++;
    VReg sum = func.next_vreg++;
    VReg root = func.next_vreg++;
    append_inst(block, defs, SSA_OP_MUL, right_mul, vreg(right_base), imm(7));
    append_inst(block, defs, SSA_OP_ADD, right_add, vreg(right_mul), imm(4));
    append_inst(block, defs, SSA_OP_STORE_VAR, 0, vreg(right_add), imm(11));
    append_inst(block, defs, SSA_OP_ADD, sum, vreg(scaled), vreg(right_add));
    append_inst(block, defs, SSA_OP_AND, root, vreg(sum), imm(mask));
    return func;
}

static void test_collapses_identically_masked_region(void) {
    SsaFunction func = make_rewrite_fixture(FIXTURE_COLLAPSE);
    assert(ssa_opt_affine_collapse(&func));
    assert(count_opcode(&func, SSA_OP_MUL) == 1);
    assert(count_opcode(&func, SSA_OP_ADD) <= 2);
    assert(count_opcode(&func, SSA_OP_AND) == 1);
    destroy_fixture(&func);
}

static void test_rewrite_rejections_are_transactional(void) {
    const enum FixtureVariant variants[] = {
        FIXTURE_DIFFERENT_MASK, FIXTURE_SHIFT, FIXTURE_TWO_BASES,
        FIXTURE_CALL, FIXTURE_NOT_PROFITABLE
    };
    for (size_t i = 0; i < sizeof(variants) / sizeof(variants[0]); ++i) {
        SsaFunction func = make_rewrite_fixture(variants[i]);
        uint64_t before = ir_digest(&func);
        assert(!ssa_opt_affine_collapse(&func));
        assert(ir_digest(&func) == before);
        destroy_fixture(&func);
    }
}

static void test_rejects_loop_member_block(void) {
    SsaFunction func = make_rewrite_fixture(FIXTURE_COLLAPSE);
    bool members[] = {true};
    SsaLoopInfo loop = {0};
    loop.members = members;
    loop.member_count = 1;
    func.loops = &loop;
    func.loop_count = 1;
    uint64_t before = ir_digest(&func);
    assert(!ssa_opt_affine_collapse(&func));
    assert(ir_digest(&func) == before);
    destroy_fixture(&func);
}

int main(void) {
    test_composes_affine_expression();
    test_rejects_unsupported_forms();
    test_rejects_short_fact_array();
    test_load_var_is_an_opaque_affine_base();
    test_follows_instruction_order_after_inlining();
    test_collapses_identically_masked_region();
    test_rewrite_rejections_are_transactional();
    test_rejects_loop_member_block();
    return 0;
}
