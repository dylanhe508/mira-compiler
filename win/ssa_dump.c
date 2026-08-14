#include "mira.h"
#include "codegen/codegen.h"
void dump_ssa_ir(SsaModule *mod) {
  for (int i = 0; i < mod->func_count; i++) {
    SsaFunction *f = mod->functions[i];
    printf("FUNC %s:\n", f->name);
    for (int b = 0; b < f->block_count; b++) {
      SsaBasicBlock *bb = f->blocks[b];
      printf("  BLOCK %d:\n", bb->id);
      for (SsaInst *inst = bb->inst_head; inst; inst = inst->next) {
        printf("    IrNode=%d dst=%d op1=(%d,%lld,%d) op2=(%d,%lld,%d)\n", inst->IrNode, inst->dst, inst->op1.kind, inst->op1.u.imm, inst->op1.u.vreg, inst->op2.kind, inst->op2.u.imm, inst->op2.u.vreg);
      }
    }
  }
  fflush(stdout);
}
