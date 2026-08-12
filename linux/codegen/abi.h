/* abi.h - Calling-convention abstraction for Mira codegen.
 *
 * All Windows-x64-specific ABI facts (argument registers, shadow space,
 * parameter stack offset, entry/exit symbols, phys-reg mapping, the
 * callee-saved fast-variable pool) live behind this header.  lowering
 * code queries these via functions so the same ssa_lower.c/program.c can
 * target either Win64 or System V x86-64 by switching mira_target_abi.
 *
 * Linux/x86-64 port keeps every exported runtime symbol name identical
 * (the static-reference effect tables in ssa_ref.c match by string), so
 * only the ABI facts below differ between backends.
 */
#ifndef MIRA_ABI_H
#define MIRA_ABI_H

#include "ir.h"

typedef enum {
	MIRA_ABI_WIN64 = 0,   /* Windows x64: RCX/RDX/R8/R9 + 32-byte shadow space */
	MIRA_ABI_SYSV  = 1    /* System V x86-64: RDI/RSI/RDX/RCX/R8/R9, no shadow space */
} MiraAbi;

/* Selected by main.c from --target / host platform.  Defaults to the
 * host ABI so existing Windows behaviour is unchanged when no target is
 * specified. */
extern MiraAbi mira_target_abi;

/* Number of integer argument registers (Win64=4, SysV=6). */
int mira_abi_int_arg_reg_count(void);

/* The idx-th integer argument register (idx < mira_abi_int_arg_reg_count).
 * Win64: 0..3 -> RCX,RDX,R8,R9
 * SysV:  0..5 -> RDI,RSI,RDX,RCX,R8,R9 */
IrReg mira_abi_int_arg_reg(int idx);

/* Caller-allocated home area for arguments (Win64=32 bytes, SysV=0). */
int mira_abi_shadow_space(void);

/* True if the idx-th parameter is passed in a register under the active ABI. */
bool mira_abi_param_in_reg(int idx);

/* Byte offset of the first stack-passed parameter relative to RBP after
 * the prologue (push rbp; the saved return address sits at [rbp+8]).
 * Win64: every parameter has a shadow home at [rbp+16+idx*8]; callers
 *        spill the four register args there, so LOAD_PARAM always reads +16.
 * SysV:  register params are saved by the callee prologue into local
 *        stack slots; stack params start at [rbp+16] for idx>=6.  Use
 *        mira_abi_param_in_reg() to distinguish the two populations. */
int mira_abi_stack_param_base(void);

/* Program entry symbol and the exit syscall/symbol used by the entry stub.
 * Win64: "mainCRTStartup" / "ExitProcess"
 * SysV:  "_start" / "exit"  (libc, non-PIE) */
const char *mira_abi_entry_symbol(void);
const char *mira_abi_exit_symbol(void);

/* Map a physical-register allocator slot number to a concrete IrReg.
 * Slot 0 is always RAX (return / division), the next N slots are the
 * integer argument registers (volatile), and the remainder are the
 * callee-saved pool used for values that must survive a call.
 * Win64: RAX,RCX,RDX,R8,R9, R13,R14,R15,RBX,RDI,RSI,R12
 * SysV:  RAX,RDI,RSI,RDX,RCX,R8,R9, RBX,R12,R13,R14,R15 */
IrReg mira_abi_map_phys_reg(int phys_reg);

/* Total number of physical slots the allocator may use. */
int mira_abi_phys_reg_count(void);

/* Index in the phys-reg sequence of the first callee-saved slot
 * (values that cross a call are forced into slots >= this value). */
int mira_abi_first_nonvolatile_phys(void);

/* The callee-saved fast-variable register pool (the fIR pool) and its
 * size.  These are the registers the lowering spills around calls.
 * Win64 keeps RDI/RSI here because they are nonvolatile on Windows;
 * SysV MUST NOT, since RDI/RSI are argument registers there. */
const IrReg *mira_abi_fir_pool(int *count);

/* Index within the fIR pool where volatile (caller-saved) registers
 * begin.  Registers before this index are callee-saved and only need
 * spilling when they are themselves argument registers under SysV. */
int mira_abi_fir_volatile_start(void);

/* Scratch registers reserved for lowering (never allocated).  Win64 and
 * SysV agree on R10/R11 as the two caller-saved scratch temporaries. */
IrReg mira_abi_scratch_reg(int idx);   /* idx 0 -> R10, idx 1 -> R11 */

/* Stack alignment a call must leave RSP at before the CALL instruction
 * (16 for both Win64 and SysV).  Lowering uses this to pad the frame. */
int mira_abi_call_stack_alignment(void);

#endif /* MIRA_ABI_H */
