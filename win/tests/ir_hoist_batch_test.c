#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "codegen/ir.h"

static void emit(IrBuffer *ir, IrOpcode op, IrReg dst, IrReg src,
                 int64_t imm, int label_id) {
    IrInst inst = {0};
    inst.IrNode = op;
    inst.dst = dst;
    inst.src = src;
    inst.imm = imm;
    inst.label_id = label_id;
    ir_emit_raw(ir, inst);
}

static void emit_function_loop(IrBuffer *ir, const char *name) {
    IrInst named = {0};
    named.IrNode = IR_LABEL_NAMED;
    size_t name_size = strlen(name) + 1;
    named.sym_name = malloc(name_size);
    assert(named.sym_name);
    memcpy(named.sym_name, name, name_size);
    ir_emit_raw(ir, named);
    emit(ir, IR_LABEL, REG_NONE, REG_NONE, 0, 1);
    emit(ir, IR_CMP_REG_IMM, REG_RCX, REG_NONE, 100, 0);
    emit(ir, IR_JGE, REG_NONE, REG_NONE, 0, 2);
    emit(ir, IR_MOV_REG_IMM, REG_R10, REG_NONE, 1234567, 0);
    emit(ir, IR_ADD_REG_REG, REG_RAX, REG_R10, 0, 0);
    emit(ir, IR_ADD_REG_IMM, REG_RCX, REG_NONE, 1, 0);
    emit(ir, IR_JMP, REG_NONE, REG_NONE, 0, 1);
    emit(ir, IR_LABEL, REG_NONE, REG_NONE, 0, 2);
    emit(ir, IR_RET, REG_NONE, REG_NONE, 0, 0);
}

int main(void) {
    IrBuffer ir;
    ir_init(&ir);
    emit_function_loop(&ir, "first");
    emit_function_loop(&ir, "second");

    uint64_t probes = ir_opt_hoist_loop_scratch_constants_counted(&ir);

    assert(ir.text[1].IrNode == IR_MOV_REG_IMM);
    assert(ir.text[11].IrNode == IR_MOV_REG_IMM);
    assert(probes < 200);
    ir_free(&ir);

    ir_init(&ir);
    for (int i = 0; i < 800; ++i) emit_function_loop(&ir, "f");
    probes = ir_opt_hoist_loop_scratch_constants_counted(&ir);
    assert(probes < 80000);
    ir_free(&ir);
    return 0;
}
