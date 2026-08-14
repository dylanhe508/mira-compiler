#include "../codegen/ir.h"
#include "../mdisasm/mdisasm.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    static const unsigned char expected[] = {
        0x48, 0x0f, 0x44, 0xc1, /* cmove rax, rcx */
        0x4d, 0x0f, 0x4c, 0xc1, /* cmovl r8, r9 */
        0x79, 0x00,             /* jns .L1 (forward relaxed) */
        0x78, 0xfe              /* js .L1 */
    };
    IrBuffer ir;
    EncodeResult encoded;
    ir_init(&ir);
    ir_cmovcc(&ir, IR_CMOVE, REG_RAX, REG_RCX);
    ir_cmovcc(&ir, IR_CMOVL, REG_R8, REG_R9);
    ir_jcc(&ir, IR_JNS, 1);
    ir_label(&ir, 1);
    ir_jcc(&ir, IR_JS, 1);
    if (ir_encode(&ir, &encoded) != 0) return 2;
    if (encoded.text_len != (int)sizeof(expected) ||
        memcmp(encoded.text_code, expected, sizeof(expected)) != 0) {
        fprintf(stderr, "unexpected cmov encoding, len=%d\n", encoded.text_len);
        return 1;
    }
    MdInst decoded;
    if (mdisasm_decode(encoded.text_code + 4, 4, 0, &decoded) != 4 ||
        strcmp(decoded.mnemonic, "cmovl") != 0 ||
        strcmp(decoded.operands, "r8, r9") != 0) return 3;
    encode_result_free(&encoded);
    ir_free(&ir);
    return 0;
}
