#include <assert.h>
#include <string.h>

#include "codegen/ir_ssa.h"

static SsaModule module_with_instruction(SsaInst *inst) {
    static SsaBasicBlock block;
    static SsaBasicBlock *blocks[1];
    static SsaFunction function;
    static SsaFunction *functions[1];
    SsaModule module;

    memset(&block, 0, sizeof(block));
    memset(&function, 0, sizeof(function));
    memset(&module, 0, sizeof(module));
    block.inst_head = block.inst_tail = inst;
    blocks[0] = &block;
    function.blocks = blocks;
    function.block_count = 1;
    functions[0] = &function;
    module.functions = functions;
    module.func_count = 1;
    return module;
}

int main(void) {
    SsaInst integer_inst = {0};
    integer_inst.IrNode = SSA_OP_ADD;
    integer_inst.type = SSA_TYPE_INT;
    SsaModule integer_module = module_with_instruction(&integer_inst);
    assert(!ssa_module_has_float_ops(&integer_module));

    SsaInst float_inst = {0};
    float_inst.IrNode = SSA_OP_FADD;
    float_inst.type = SSA_TYPE_FLOAT;
    SsaModule float_module = module_with_instruction(&float_inst);
    assert(ssa_module_has_float_ops(&float_module));

    SsaInst conversion = {0};
    conversion.IrNode = SSA_OP_FPTOSI;
    conversion.type = SSA_TYPE_INT;
    SsaModule conversion_module = module_with_instruction(&conversion);
    assert(ssa_module_has_float_ops(&conversion_module));

    SsaModule empty = {0};
    assert(!ssa_module_has_float_ops(&empty));

    SsaInst ret = {0};
    ret.IrNode = SSA_OP_RET;
    SsaModule leaf_module = module_with_instruction(&ret);
    assert(ssa_function_needs_preinline_cleanup(leaf_module.functions[0]));

    SsaBasicBlock second_block = {0};
    SsaBasicBlock *two_blocks[] = {leaf_module.functions[0]->blocks[0], &second_block};
    leaf_module.functions[0]->blocks = two_blocks;
    leaf_module.functions[0]->block_count = 2;
    assert(!ssa_function_needs_preinline_cleanup(leaf_module.functions[0]));

    SsaInst call = {0};
    call.IrNode = SSA_OP_CALL;
    SsaModule caller_module = module_with_instruction(&call);
    assert(!ssa_function_needs_preinline_cleanup(caller_module.functions[0]));
    assert(!ssa_function_has_signed_div(caller_module.functions[0]));

    SsaInst signed_div = {0};
    signed_div.IrNode = SSA_OP_SDIV;
    SsaModule div_module = module_with_instruction(&signed_div);
    assert(ssa_function_has_signed_div(div_module.functions[0]));
    return 0;
}
