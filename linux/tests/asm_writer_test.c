#include "codegen/asm_writer.h"

#include <stdio.h>

int main(void) {
	for (int op = 0; op < IR_OPCODE_COUNT; ++op) {
		if (!ir_asm_supports_opcode((IrOpcode)op)) {
			fprintf(stderr, "unsupported opcode %d\n", op);
			return 1;
		}
	}
	puts("ASM WRITER OPCODE COVERAGE PASS");
	return 0;
}
