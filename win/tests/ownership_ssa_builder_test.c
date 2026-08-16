#include "../mira.h"
#include "../codegen/ir_ssa.h"

#include <stdio.h>

void parser_do_import(const char *path, const char *alias, int is_lib) {
    (void)path;
    (void)alias;
    (void)is_lib;
}

extern void ssa_build_program(Program *program, SsaModule *module);

static SsaFunction *find_function(SsaModule *module, const char *name) {
    for (int i = 0; i < module->func_count; ++i)
        if (module->functions[i]->name &&
            strcmp(module->functions[i]->name, name) == 0)
            return module->functions[i];
    return NULL;
}

static SsaInst *find_call(SsaFunction *function, const char *symbol) {
    for (int b = 0; b < function->block_count; ++b)
        for (SsaInst *inst = function->blocks[b]->inst_head; inst;
             inst = inst->next)
            if (inst->IrNode == SSA_OP_CALL && inst->operands &&
                inst->operand_count > 0 &&
                inst->operands[0].kind == SSA_OPND_SYM &&
                strcmp(inst->operands[0].u.sym, symbol) == 0)
                return inst;
    return NULL;
}

static SsaInst *find_opcode(SsaFunction *function, SsaOpcode opcode) {
    for (int b = 0; b < function->block_count; ++b)
        for (SsaInst *inst = function->blocks[b]->inst_head; inst;
             inst = inst->next)
            if (inst->IrNode == opcode) return inst;
    return NULL;
}

int main(void) {
    char source[] =
        "fn make() -> str { \"mi\" \"ra\" str-cat }\n"
        "fn choose(flag: bool) -> str { if (flag) { \"o\" \"k\" str-cat } else { \"borrowed\" } }\n"
        "fn observe(value: str) -> i64 { 1 }\n"
        "fn retain(value: str) -> str { value }\n"
        "fn main() -> void { print(observe(make())); print(retain(make())); }\n";
    Compiler compiler = {0};
    compiler.src = source;
    compiler.filename = "<ownership-ssa-metadata-test>";
    Program *program = parser_parse(&compiler);
    mira_typecheck_program(&compiler, program);

    SsaModule module = {0};
    ssa_init_module(&module);
    ssa_build_program(program, &module);

    SsaFunction *make = find_function(&module, "make");
    SsaFunction *choose = find_function(&module, "choose");
    SsaInst *owned_call = make ? find_call(make, "mira_str_concat") : NULL;
    SsaInst *mixed_phi = choose ? find_opcode(choose, SSA_OP_PHI) : NULL;
    SsaFunction *main_function = find_function(&module, "mira_main");
    SsaInst *safe_call = main_function ? find_call(main_function, "observe") : NULL;
    SsaInst *escape_call = main_function ? find_call(main_function, "retain") : NULL;
    if (!owned_call || owned_call->ownership != SSA_OWNERSHIP_OWNED)
        return 1;
    if (!mixed_phi || mixed_phi->ownership != SSA_OWNERSHIP_MAYBE_OWNED ||
        !mixed_phi->free_func_name)
        return 2;
    if (!safe_call || !safe_call->escape_summary_known ||
        safe_call->param_escape_mask != 0) {
        fprintf(stderr, "safe call summary: call=%p known=%d mask=%llu\n",
            (void *)safe_call, safe_call ? safe_call->escape_summary_known : -1,
            (unsigned long long)(safe_call ? safe_call->param_escape_mask : 0));
        return 3;
    }
    if (!escape_call || !escape_call->escape_summary_known ||
        escape_call->param_escape_mask != 1)
        return 4;

    ssa_destroy_phis_module(&module);
    if (!mixed_phi->owner_token ||
        ssa_phi_owner_token_for_value(choose, mixed_phi->dst) !=
            mixed_phi->owner_token)
        return 5;

    puts("OWNERSHIP SSA BUILDER PASS");
    ssa_free_module(&module);
    program_free(program);
    return 0;
}
