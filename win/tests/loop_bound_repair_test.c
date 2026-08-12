#include "../codegen/ir.h"
#include <stdio.h>

extern void ir_opt_repair_loop_bounds(IrBuffer *ir);
int mira_target_avx2 = 1;

int main(void) {
    IrBuffer ir; ir_init(&ir);
    ir_label_named(&ir, "f");
    ir_mov_reg_mem(&ir, REG_RDX, REG_RBP, 16);
    ir_label(&ir, 7);
    ir_cmp_reg_reg(&ir, REG_R13, REG_RDX);
    ir_jcc(&ir, IR_JG, 9);
    ir_label(&ir, 9); /* outlined exit is laid out before the real backedge */
    ir_ret(&ir);
    ir_label(&ir, 8);
    ir_mov_reg_imm(&ir, REG_RDX, 1);
    ir_add_reg_imm(&ir, REG_R13, 1);
    ir_jmp(&ir, 7);
    int before = ir.text_count;
    ir_opt_repair_loop_bounds(&ir);
    int reload = 0;
    for (int i = 0; i < ir.text_count; ++i)
        if (ir.text[i].IrNode == IR_MOV_REG_MEM && ir.text[i].dst == REG_RDX &&
            ir.text[i].src == REG_RBP && ir.text[i].imm == 16 && i > 1) reload++;
    ir_free(&ir);
    if (reload != 1) {
        fprintf(stderr, "before=%d reload=%d\n", before, reload);
        return 1;
    }
    return 0;
}
