#ifndef MIRA_ASM_WRITER_H
#define MIRA_ASM_WRITER_H

#include "ir.h"

bool ir_asm_supports_opcode(IrOpcode opcode);
bool ir_write_gas_intel(const IrBuffer *ir, FILE *out, IrOpcode *unsupported);

#endif
