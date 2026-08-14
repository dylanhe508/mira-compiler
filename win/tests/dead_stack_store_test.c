#include "../codegen/ir.h"
#include <stdio.h>

extern void ir_opt_remove_dead_stack_stores(IrBuffer *ir);

int main(void) {
    IrBuffer ir;
    ir_init(&ir);
    ir_label_named(&ir, "f");
    ir_mov_mem_reg(&ir, REG_RBP, -8, REG_R10);  /* dead */
    ir_mov_mem_reg(&ir, REG_RBP, -16, REG_R11); /* live */
    ir_mov_reg_mem(&ir, REG_RAX, REG_RBP, -16);
    ir_ret(&ir);
    ir_opt_remove_dead_stack_stores(&ir);
    int dead = 0, live_store = 0, live_load = 0;
    for (int i = 0; i < ir.text_count; ++i) {
        IrInst *x = &ir.text[i];
        if (x->IrNode == IR_MOV_MEM_REG && x->dst == REG_RBP && x->imm == -8) dead++;
        if (x->IrNode == IR_MOV_MEM_REG && x->dst == REG_RBP && x->imm == -16) live_store++;
        if (x->IrNode == IR_MOV_REG_MEM && x->src == REG_RBP && x->imm == -16) live_load++;
    }
    ir_free(&ir);
    if (dead || live_store != 1 || live_load != 1) {
        fprintf(stderr, "dead=%d live_store=%d live_load=%d\n", dead, live_store, live_load);
        return 1;
    }
    return 0;
}
