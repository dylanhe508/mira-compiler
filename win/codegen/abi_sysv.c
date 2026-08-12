/* abi_sysv.c - System V x86-64 ABI facts (Linux).
 *
 * Key differences from Win64 handled here:
 *   - 6 integer arg registers (RDI,RSI,RDX,RCX,R8,R9) instead of 4.
 *   - No shadow space: the caller does NOT reserve 32 bytes, so the
 *     lowering cannot use the "spill args to shadow then reload" trick
 *     that the Win64 path relies on.  Argument setup must go straight
 *     to the argument registers, with R10/R11 as scratch when an arg
 *     register is also a live fast-variable.
 *   - RDI/RSI are volatile (argument) registers, so they CANNOT be part
 *     of the callee-saved fast-variable pool.  The fIR pool is reduced
 *     to the true SysV callee-saved set: RBX,R12,R13,R14,R15.
 *   - Stack parameters begin at [rbp+16] for idx>=6; the first six
 *     parameters arrive in registers and are saved by the callee
 *     prologue into local stack slots.
 */
#include "abi.h"

/* SysV integer argument registers, in order. */
static const IrReg sysv_arg_regs[6] = { REG_RDI, REG_RSI, REG_RDX, REG_RCX, REG_R8, REG_R9 };

/*
 * Phys-reg slot layout for the SysV backend.
 *
 * The register allocator makes ONE hardware-specific assumption about
 * slot numbers: slots 0 and 2 are RAX and RDX (the IDIV dividend /
 * divisor-high pair), so that the "crosses_div avoids 0 and 2" rule
 * in ssa_regalloc.c works without modification.  Beyond that, the only
 * structural invariant is that every slot in [0, FIRST_NONVOLATILE) is
 * caller-saved (volatile) and every slot in [FIRST_NONVOLATILE, COUNT)
 * is callee-saved.
 *
 *   0  RAX   return / dividend          \
 *   1  RCX   arg4 (volatile)             |
 *   2  RDX   arg3 / divisor-high         | volatile front
 *   3  RDI   arg1                        |
 *   4  RSI   arg2                        |
 *   5  R8    arg5                        |
 *   6  R9    arg6                       /
 *   7  RBX   callee-saved (nonvolatile) \
 *   8  R12                                 | callee-saved tail
 *   9  R13                                 |
 *   10 R14                                 |
 *   11 R15                                /
 *
 * R10/R11 stay reserved as lowering scratch.  RBP is the frame pointer.
 * first_nonvolatile_phys = 7, giving 7 volatile + 5 callee-saved slots.
 */
#define SYSV_PHYS_COUNT        12
#define SYSV_FIRST_NONVOLATILE 7

/* Fast-variable register pool - SysV callee-saved integers only.
 * Unlike Win64 this EXCLUDES RDI/RSI (they are argument/volatile here),
 * leaving five genuinely nonvolatile registers.  R8/R9 remain at the
 * tail as the volatile pair, spilt around calls exactly as on Win64. */
static const IrReg sysv_fir_pool[7] = {
	REG_RBX, REG_R12, REG_R13, REG_R14, REG_R15,
	REG_R8, REG_R9
};
#define SYSV_FIR_VOLATILE_START 5

int mira_abi_int_arg_reg_count(void) { return 6; }

IrReg mira_abi_int_arg_reg(int idx) {
	if (idx < 0 || idx >= 6) return REG_NONE;
	return sysv_arg_regs[idx];
}

int mira_abi_shadow_space(void) { return 0; }

bool mira_abi_param_in_reg(int idx) {
	return idx >= 0 && idx < 6;
}

int mira_abi_stack_param_base(void) { return 16; }

const char *mira_abi_entry_symbol(void) { return "_start"; }
const char *mira_abi_exit_symbol(void)  { return "exit"; }

IrReg mira_abi_map_phys_reg(int phys_reg) {
	switch (phys_reg) {
	case 0:  return REG_RAX;
	case 1:  return REG_RCX;
	case 2:  return REG_RDX;
	case 3:  return REG_RDI;
	case 4:  return REG_RSI;
	case 5:  return REG_R8;
	case 6:  return REG_R9;
	case 7:  return REG_RBX;
	case 8:  return REG_R12;
	case 9:  return REG_R13;
	case 10: return REG_R14;
	case 11: return REG_R15;
	default: return REG_NONE;
	}
}

int mira_abi_phys_reg_count(void)        { return SYSV_PHYS_COUNT; }
int mira_abi_first_nonvolatile_phys(void){ return SYSV_FIRST_NONVOLATILE; }

const IrReg *mira_abi_fir_pool(int *count) {
	if (count) *count = 7;
	return sysv_fir_pool;
}

int mira_abi_fir_volatile_start(void) { return SYSV_FIR_VOLATILE_START; }

IrReg mira_abi_scratch_reg(int idx) {
	return idx == 0 ? REG_R10 : REG_R11;
}

int mira_abi_call_stack_alignment(void) { return 16; }
