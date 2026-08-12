/* abi_win64.c - Windows x64 ABI facts.
 *
 * Values lifted verbatim from the previous hard-coded lowering so the
 * Win64 backend is byte-for-byte equivalent to before the abstraction. */
#include "abi.h"

/* Win64 integer argument registers: RCX, RDX, R8, R9. */
static const IrReg win64_arg_regs[4] = { REG_RCX, REG_RDX, REG_R8, REG_R9 };

/*
 * Phys-reg slot layout (matches the original map_phys_reg switch in
 * ssa_lower.c exactly):
 *   0  RAX   return / dividend
 *   1  RCX   arg1
 *   2  RDX   arg2 / dividend-high
 *   3  R8    arg3
 *   4  R9    arg4
 *   5  R13   callee-saved (nonvolatile)
 *   6  R14
 *   7  R15
 *   8  RBX
 *   9  RDI   nonvolatile on Win64 - safe to reuse for fast vars
 *   10 RSI
 *   11 R12
 * R10/R11 stay reserved as lowering scratch.
 */
#define WIN64_PHYS_COUNT        12
#define WIN64_FIRST_NONVOLATILE 5

/* Fast-variable register pool.  Identical to the old fIR_reg_pool[]:
 * first 7 are nonvolatile (persist across calls without spilling),
 * last 2 are volatile (R8/R9 - spilt around calls).  On Win64 RDI/RSI
 * are nonvolatile so they may host fast vars safely. */
static const IrReg win64_fir_pool[9] = {
	REG_R13, REG_R14, REG_R15, REG_RBX, REG_RDI, REG_RSI, REG_R12,
	REG_R8, REG_R9
};
#define WIN64_FIR_VOLATILE_START 7

int mira_abi_int_arg_reg_count(void) { return 4; }

IrReg mira_abi_int_arg_reg(int idx) {
	if (idx < 0 || idx >= 4) return REG_NONE;
	return win64_arg_regs[idx];
}

int mira_abi_shadow_space(void) { return 32; }

bool mira_abi_param_in_reg(int idx) {
	/* Every Win64 parameter has a shadow home; the four register args
	 * are also at [rbp+16+idx*8] because callers spill them. */
	(void)idx;
	return true;
}

int mira_abi_stack_param_base(void) { return 16; }

const char *mira_abi_entry_symbol(void) { return "mainCRTStartup"; }
const char *mira_abi_exit_symbol(void)  { return "ExitProcess"; }

IrReg mira_abi_map_phys_reg(int phys_reg) {
	switch (phys_reg) {
	case 0:  return REG_RAX;
	case 1:  return REG_RCX;
	case 2:  return REG_RDX;
	case 3:  return REG_R8;
	case 4:  return REG_R9;
	case 5:  return REG_R13;
	case 6:  return REG_R14;
	case 7:  return REG_R15;
	case 8:  return REG_RBX;
	case 9:  return REG_RDI;
	case 10: return REG_RSI;
	case 11: return REG_R12;
	default: return REG_NONE;
	}
}

int mira_abi_phys_reg_count(void)        { return WIN64_PHYS_COUNT; }
int mira_abi_first_nonvolatile_phys(void){ return WIN64_FIRST_NONVOLATILE; }

const IrReg *mira_abi_fir_pool(int *count) {
	if (count) *count = 9;
	return win64_fir_pool;
}

int mira_abi_fir_volatile_start(void) { return WIN64_FIR_VOLATILE_START; }

IrReg mira_abi_scratch_reg(int idx) {
	return idx == 0 ? REG_R10 : REG_R11;
}

int mira_abi_call_stack_alignment(void) { return 16; }
