#include "codegen/ir.h"

#include <stdio.h>

int main(int argc, char **argv) {
	if (argc != 2) return 2;
	IrInst bad = {0};
	bad.IrNode = IR_OPCODE_COUNT;
	IrBuffer ir = {0};
	ir.text = &bad;
	ir.text_count = 1;
	IrOpcode unsupported = 0;
	FILE *sink = fopen(argv[1], "wb+");
	if (!sink) return 2;
	bool ok = ir_dump(&ir, sink, &unsupported);
	fclose(sink);
	if (ok || unsupported != IR_OPCODE_COUNT) {
		fprintf(stderr, "unknown opcode was not rejected: ok=%d opcode=%d\n",
		        ok ? 1 : 0, (int)unsupported);
		return 1;
	}
	puts("IR DUMP UNKNOWN OPCODE PASS");
	return 0;
}
