/* ir_ssa.h 鈥?Mira SSA IR (Static Single Assignment Intermediate Representation)
 *
 * 杩欐槸涓€绉嶆洿楂樼骇鐨勫舰寮忥紝姣忎釜铏氭嫙瀵勫瓨鍣ㄥ彧琚祴鍊间竴娆°€?
 * 鏀寔寮哄ぇ鐨勪紭鍖栵紝濡?GVN銆丼CCP銆丏CE 绛夈€?
 */
#ifndef IR_SSA_H
#define IR_SSA_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include "decision.h"

/* ========== 绫诲瀷绯荤粺 (SSA Type System) ========== */
typedef enum {
	SSA_TYPE_INT,     /* 64-bit integer */
	SSA_TYPE_FLOAT,   /* 64-bit float (double) */
	SSA_TYPE_PTR,     /* 64-bit pointer */
	SSA_TYPE_V4I64,   /* four packed 64-bit integer lanes */
	SSA_TYPE_VOID     /* void / no value */
} SsaType;

/* ========== 铏氭嫙瀵勫瓨鍣?(Virtual Register) ========== */
/* 铏氭嫙瀵勫瓨鍣ㄤ粠 1 寮€濮嬬紪鍙枫€? 琛ㄧず鏃犳晥/鏃犲瘎瀛樺櫒銆?*/
typedef uint32_t VReg;

typedef struct {
	VReg base;
	uint64_t coefficient;
	uint64_t constant;
	uint32_t instruction_count;
	bool proven;
} SsaAffineFact;

typedef enum {
	SSA_BRANCH_UNKNOWN = 0,
	SSA_BRANCH_PREFER_JUMP,
	SSA_BRANCH_PREFER_BRANCHLESS
} SsaBranchPolicy;

/* ========== SSA 鎿嶄綔鐮?(SSA Opcodes) ========== */
typedef enum {
	/* -- 绠楁湳涓庨€昏緫杩愮畻 (Arithmetic & Logic) -- */
	SSA_OP_ADD,       /* dst = src1 + src2 */
	SSA_OP_SUB,       /* dst = src1 - src2 */
	SSA_OP_MUL,       /* dst = src1 * src2 */
	SSA_OP_SDIV,      /* dst = src1 / src2 (signed) */
	SSA_OP_SREM,      /* dst = src1 % src2 (signed) */
	SSA_OP_NEG,       /* dst = -src1 */

	SSA_OP_AND,       /* dst = src1 & src2 */
	SSA_OP_OR,        /* dst = src1 | src2 */
	SSA_OP_XOR,       /* dst = src1 ^ src2 */
	SSA_OP_SHL,       /* dst = src1 << src2 */
	SSA_OP_ASHR,      /* dst = src1 >> src2 (arithmetic) */
	SSA_OP_LSHR,      /* dst = src1 >> src2 (logical) */
	SSA_OP_NOT,       /* dst = ~src1 */

	/* -- 姣旇緝杩愮畻 (Comparisons) -- */
	SSA_OP_CMP_EQ,    /* dst = (src1 == src2) */
	SSA_OP_CMP_NE,    /* dst = (src1 != src2) */
	SSA_OP_CMP_LT,    /* dst = (src1 < src2) */
	SSA_OP_CMP_LE,    /* dst = (src1 <= src2) */
	SSA_OP_CMP_GT,    /* dst = (src1 > src2) */
	SSA_OP_CMP_GE,    /* dst = (src1 >= src2) */

	/* -- 娴偣杩愮畻 (Floating Point) -- */
	SSA_OP_FADD,      /* dst = src1 + src2 */
	SSA_OP_FSUB,      /* dst = src1 - src2 */
	SSA_OP_FMUL,      /* dst = src1 * src2 */
	SSA_OP_FDIV,      /* dst = src1 / src2 */
	SSA_OP_FCMP_EQ,
	SSA_OP_FCMP_NE,
	SSA_OP_FCMP_LT,
	SSA_OP_FCMP_LE,
	SSA_OP_FCMP_GT,
	SSA_OP_FCMP_GE,
	SSA_OP_SITOFP,    /* Signed Int To FP: dst = (double)src1 */
	SSA_OP_FPTOSI,    /* FP To Signed Int: dst = (int64_t)src1 */

	/* -- 鍐呭瓨鎿嶄綔 (Memory Operations) -- */
	SSA_OP_ALLOCA,    /* dst = alloc(size) - 鍦ㄦ爤涓婂垎閰嶇┖闂?*/
	SSA_OP_LOAD,      /* dst = load(ptr) */
	SSA_OP_STORE,     /* store(val, ptr) */
	SSA_OP_LOAD8,     /* dst = load8(ptr) */
	SSA_OP_STORE8,    /* store8(val, ptr) */
	SSA_OP_GETELEMENTPTR, /* dst = ptr + offset (鎸囬拡绠楁湳) */

	/* -- 鎺у埗娴佷笌鍑芥暟璋冪敤 (Control Flow & Call) -- */
	SSA_OP_JMP,       /* 鏃犳潯浠惰烦杞? jmp block */
	SSA_OP_BR,        /* 鏈夋潯浠惰烦杞? br cond, true_block, false_block */
	SSA_OP_RET,       /* 杩斿洖: ret [val] */
	SSA_OP_CALL,      /* dst = call func(args...) */

	/* -- SSA 专属结构 (SSA Specific) -- */
	SSA_OP_PHI,       /* dst = phi( [val1, block1], [val2, block2], ... ) */
	SSA_OP_IMM,       /* dst = immediate_value - 甯搁噺璧嬪€?*/
	SSA_OP_COPY,      /* dst = src (浠呯敤浜庝粠 SSA 閫€鍖栬嚦鐗╃悊鏈哄櫒鐮佹椂锛屾垨鍚堝苟鍧? */
	SSA_OP_VEC_LOAD,
	SSA_OP_VEC_STORE,
	SSA_OP_VEC_ADD,
	SSA_OP_VEC_SUB,
	SSA_OP_VEC_XOR,
	SSA_OP_VEC_AND,
	SSA_OP_VEC_OR,
	SSA_OP_VEC_MULLD,
	SSA_OP_VEC_CMPEQ,

	/* -- Mira 鍙橀噺妲芥搷浣?(鐩存帴璇诲啓 mira_vars[slot]锛屼笉璧?alloca/mem2reg) -- */
	SSA_OP_LOAD_VAR,  /* dst = mira_vars[slot]  op1.u.imm=slot */
	SSA_OP_STORE_VAR, /* mira_vars[slot] = val   op1=val_vreg, op2.u.imm=slot */
	SSA_OP_LOAD_PARAM,/* dst = param[index]    op1.u.imm=index */

	/* -- Lambdas -- */
	SSA_OP_LEA_FUNC,  /* dst = AddressOfFunc(sym) */
	SSA_OP_ICALL      /* dst = call reg(args...) 鈥?indirect call via function pointer VReg */

} SsaOpcode;

/* ========== 鎿嶄綔鏁?(Operand) ========== */
typedef enum {
	SSA_OPND_NONE,
	SSA_OPND_VREG,    /* 铏氭嫙瀵勫瓨鍣?*/
	SSA_OPND_IMM,     /* 鏁存暟绔嬪嵆鏁?(64-bit) */
	SSA_OPND_FIMM,    /* 娴偣绔嬪嵆鏁?(double) */
	SSA_OPND_BLOCK,   /* 鍩烘湰鍧?(浣滀负璺宠浆鐩爣) */
	SSA_OPND_SYM,     /* 绗﹀彿鍚?(鐢ㄤ簬鍑芥暟璋冪敤鎴栧叏灞€鍙橀噺) */
	SSA_OPND_STRING   /* 瀛楃涓插瓧闈㈤噺 (鍙兘闇€瑕佷笓闂ㄧ殑鏁版嵁娈? */
} SsaOperandKind;

struct SsaBasicBlock;
struct SsaLoopInfo;
typedef struct SsaFunctionIndex SsaFunctionIndex;

typedef enum { SSA_REF_ORIGIN_UNKNOWN, SSA_REF_ORIGIN_PARAM, SSA_REF_ORIGIN_HEAP,
	SSA_REF_ORIGIN_GLOBAL, SSA_REF_ORIGIN_STACK, SSA_REF_ORIGIN_CONSTANT } SsaRefOriginKind;
typedef enum { SSA_ALIAS_MAY, SSA_ALIAS_NONE, SSA_ALIAS_MUST } SsaAliasResult;
enum { SSA_REF_UNIQUE=1u<<0, SSA_REF_SHARED=1u<<1, SSA_REF_ESCAPED=1u<<2,
	SSA_REF_CAPTURED=1u<<3, SSA_REF_NEEDS_FREE=1u<<4 };
typedef struct SsaRefFact {
	SsaRefOriginKind origin_kind; uint32_t root_id; int64_t offset_min, offset_max;
	size_t access_width; uint32_t flags; const char *free_func_name; int return_alias_param;
} SsaRefFact;
typedef struct SsaFunctionEffect {
	uint64_t param_reads, param_writes, param_full_overwrites, param_reads_old_value;
	uint64_t param_captures, param_frees; bool reads_memory, writes_memory;
	bool reads_global, writes_global, allocates, has_unknown_effect;
	bool has_concurrency_effect, may_suspend; int return_alias_param;
} SsaFunctionEffect;
typedef enum { SSA_REF_VM_BRANCH, SSA_REF_VM_CALL_TARGET, SSA_REF_VM_MEMORY_READ,
	SSA_REF_VM_MEMORY_WRITE, SSA_REF_VM_ESCAPE, SSA_REF_VM_FREE,
	SSA_REF_VM_OBSERVABLE_EFFECT } SsaRefVmEventKind;
typedef struct SsaRefVmEvent {
	SsaRefVmEventKind kind; uint32_t node_id, object_id; int64_t value, offset; size_t width;
} SsaRefVmEvent;
typedef struct SsaRefVmTrace {
	SsaRefVmEvent *events; size_t event_count, event_cap, event_budget, guard_count;
	bool compile_time_proven, runtime_dependent, guards_complete, fallback_complete, overflowed;
} SsaRefVmTrace;

typedef struct {
	SsaOperandKind kind;
	union {
		VReg vreg;
		int64_t imm;
		double fimm;
		struct SsaBasicBlock *block;
		char *sym;
		struct { char *str; size_t len; } string;
	} u;
} SsaOperand;

/* ========== SSA 鎸囦护 (Instruction) ========== */
typedef struct SsaInst {
	SsaOpcode IrNode;
	SsaType type;     /* 鎸囦护缁撴灉鐨勭被鍨?*/
	uint8_t vector_lanes; /* zero for scalar, four for V4I64 */
	VReg dst;         /* 鐩爣瀵勫瓨鍣?(0 琛ㄧず娌℃湁缁撴灉) */

	/* 鐢变簬澶ч儴鍒嗘寚浠ゆ槸 2 鎴?3 涓搷浣滄暟锛屽鏋滄槸 CALL 鎴栨槸 PHI锛屽叾鎿嶄綔鏁颁釜鏁板彲鍙?*/
	SsaOperand *operands;
	int operand_count;
	int operand_cap;

	/* 方便常用指令直接存储前两个操作数，减少内存分配开销 */
	SsaOperand op1;
	SsaOperand op2;

	struct SsaInst *prev;
	struct SsaInst *next;
	struct SsaBasicBlock *parent; /* 鎵€灞炲熀鏈嫙 */

	/* === 静态引用所有权分析 (Static Reference Ownership) === */
	int needs_free;               /* 1 = 此指令产生的 VReg 持有堆内存，生命周期结束时需自动释放 */
	const char *free_func_name;   /* 释放函数的符号名 (如 "mem_free", "mira_list_free") */
	bool ref_observable;
	bool ref_analyzed;
	/* Bounded compile-time VM evidence.  Policy is advisory only: static
	 * legality proofs still decide whether a transform may be performed. */
	uint64_t vm_taken;
	uint64_t vm_not_taken;
	uint8_t branch_policy; /* SsaBranchPolicy */
} SsaInst;

/* ========== 鍩烘湰鍧?(Basic Block) ========== */
typedef struct SsaBasicBlock {
	int id;           /* 鍧楃殑鍞竴 ID */
	char *name;       /* 鍙€夊悕瀛楋紝鐢ㄤ簬璋冨紡 (濡?"entry", "loop_body") */

	/* 鎸囦护鍙岄摼琛?*/
	SsaInst *inst_head;
	SsaInst *inst_tail;

	/* 鎺у埗娴佽竟 (CFG Edges) */
	struct SsaBasicBlock **preds; /* 鍓嶉┍鍧楁暟缁?*/
	int pred_count;
	int pred_cap;

	struct SsaBasicBlock **succs; /* 鍚庣户鍧楁暟缁?*/
	int succ_count;
	int succ_cap;

	/* 鍒嗘瀽鐢?(Dominator Tree 绛? */
	struct SsaBasicBlock *idom;   /* 立即必经结点 (Immediate Dominator) */
	struct SsaBasicBlock **dom_children; 
	int dom_child_count;
	int dom_child_cap;

	struct SsaBasicBlock **df;    /* 鏀厤杈圭晫 (Dominance Frontier) */
	int df_count;
	int df_cap;

	struct SsaFunction *parent;
	bool inline_hot; /* inherited by blocks split from a hot-loop call site */
} SsaBasicBlock;

/* ========== 鍑芥暟 (Function) ========== */
typedef struct SsaFunction {
	char *name;
	SsaType return_type;
	
	/* 褰㈠弬铏氭嫙瀵勫瓨鍣ㄥ垪琛?*/
	VReg *params;
	SsaType *param_types;
	int param_count;

	/* 鍩烘湰鍧楀垪琛?*/
	SsaBasicBlock **blocks;
	int block_count;
	int block_cap;

	SsaBasicBlock *entry_block;

	/* 铏氭嫙瀵勫瓨鍣ㄥ垎閰嶅櫒璁℃暟鍣?*/
	VReg next_vreg;

	/* 鍚勪釜铏氭嫙瀵勫瓨鍣ㄥ搴旂殑瀹氫箟鎸囦护锛屾柟渚垮揩閫熸煡鎵惧叾璧嬪€煎湴鐐?*/
	SsaInst **vreg_defs; 
	int vreg_defs_cap;

	/* 瀵勫瓨鍣ㄥ垎閰嶉樁娈电粨鏉熷悗锛屼繚瀛?VReg 鍒?x86 鐗╃悊瀵勫瓨鍣↖D鐨勬槧灏?(-1琛ㄧず鏍堝唴) */
	int *vreg_phys_map;
	int *vreg_float_phys_map; /* independent XMM3-XMM5 allocation, -1 = GPR/spill */
	int *vreg_vec_phys_map; /* independent YMM0-YMM15 allocation */
	int *vreg_spill_map;    /* stack slot for scalar spills, -1 when register-resident */
	int spill_size;         /* byte size of the 16-byte-aligned scalar spill area */
	uint32_t estimated_scalar_pressure;
	uint32_t estimated_float_pressure;
	uint32_t estimated_vector_pressure;
	uint32_t actual_spill_count;
	struct SsaLoopInfo *loops;
	int loop_count;
	SsaRefFact *ref_facts;
	size_t ref_fact_count;
	SsaFunctionEffect *ref_effect;
	DecisionReferenceFacts decision_ref_facts;
	DecisionFunctionPlan decision_plan;

	/* 动态变量寄存器映射: var_reg_map[slot] = IrReg, 或 -1 表示走内存 */
	int *var_reg_map;
	int var_count;
	int *var_remat_param;
	int64_t *var_remat_mul;
	int64_t *var_remat_add;
} SsaFunction;

typedef struct SsaLoopInfo {
	SsaBasicBlock *header;
	SsaBasicBlock *latch;
	SsaBasicBlock *exit;
	bool *members;
	size_t member_count;
	int backedge_count;
	int exit_count;
	int induction_slot;
	int64_t step;
	int memory_reads;
	int memory_writes;
	bool memory_accesses_known;
	bool memory_may_alias;
	bool memory_reorder_safe;
	bool has_unknown_call;
	bool has_ownership_transfer;
	DecisionLoopPlan decision_plan;
} SsaLoopInfo;

/* ========== 妯″潡 (Module) ========== */
typedef struct SsaModule {
	SsaFunction **functions;
	int func_count;
	int func_cap;
	SsaFunctionIndex *function_index;
	uint64_t function_epoch;
	/* Exclusive high-water mark for source and optimizer-internal var slots. */
	int var_slot_count;

	/* 鍏ㄥ眬鍙橀噺鍜屽鍏ヤ篃鍙湪杩欓噷璁板綍 */
} SsaModule;

bool ssa_module_has_float_ops(const SsaModule *mod);
bool ssa_function_needs_preinline_cleanup(const SsaFunction *func);
bool ssa_function_has_signed_div(const SsaFunction *func);
bool ssa_function_index_rebuild(SsaModule *mod);
void ssa_function_index_invalidate(SsaModule *mod);
void ssa_function_index_free(SsaModule *mod);
SsaFunction *ssa_function_index_find(const SsaModule *mod, const char *name);
int ssa_function_index_ordinal(const SsaModule *mod, const SsaFunction *func);
uint64_t ssa_function_index_name_comparisons(const SsaModule *mod);
bool ssa_function_index_rebuild_call_facts(SsaModule *mod);
void ssa_function_index_invalidate_call_facts(SsaModule *mod);
uint32_t ssa_function_index_direct_calls(const SsaModule *mod,
                                         const SsaFunction *func);
bool ssa_function_index_is_referenced(const SsaModule *mod,
                                      const SsaFunction *func);
bool ssa_function_index_is_leaf(const SsaModule *mod,
                                const SsaFunction *func);

/* CFG rewrites and PHI destruction share this conservative PHI-to-predecessor
 * bijection check, so malformed edge state is never reinterpreted. */
static inline bool ssa_phi_prefix_is_valid(const SsaFunction *func,
                                           const SsaBasicBlock *block) {
	if (!func || !block || block->pred_count < 0) return false;
	for (int pi = 0; pi < block->pred_count; ++pi) {
		if (!block->preds || !block->preds[pi]) return false;
		for (int prior = 0; prior < pi; ++prior)
			if (block->preds[prior] == block->preds[pi]) return false;
	}
	for (const SsaInst *phi = block->inst_head;
		 phi && phi->IrNode == SSA_OP_PHI; phi = phi->next) {
		if (!phi->operands || phi->operand_count < 0 ||
			phi->operand_cap < 0 || phi->operand_cap < phi->operand_count ||
			(phi->operand_count & 1) != 0 ||
			phi->operand_count / 2 != block->pred_count)
			return false;
		for (int oi = 0; oi < phi->operand_count; oi += 2) {
			if (phi->operands[oi].kind != SSA_OPND_VREG ||
				phi->operands[oi].u.vreg == 0 ||
				phi->operands[oi].u.vreg >= func->next_vreg ||
				phi->operands[oi + 1].kind != SSA_OPND_BLOCK)
				return false;
			int predecessor_matches = 0;
			for (int pi = 0; pi < block->pred_count; ++pi)
				if (phi->operands[oi + 1].u.block == block->preds[pi])
					predecessor_matches++;
			if (predecessor_matches != 1) return false;
			for (int prior = 1; prior < oi; prior += 2)
				if (phi->operands[prior].u.block ==
					phi->operands[oi + 1].u.block)
					return false;
		}
	}
	return true;
}

/* ========== API (鏋勫缓涓庝紭鍖? ========== */

void ssa_init_module(SsaModule *mod);
void ssa_free_module(SsaModule *mod);
void ssa_destroy_phis_module(SsaModule *mod);

void ssa_ref_analyze_module(SsaModule *mod);
void ssa_ref_free_module(SsaModule *mod);
SsaAliasResult ssa_ref_alias(const SsaRefFact *a, int64_t a_offset, size_t a_width,
	const SsaRefFact *b, int64_t b_offset, size_t b_width);
const SsaFunctionEffect *ssa_ref_effect(const SsaFunction *func);
DecisionReferenceFacts ssa_ref_decision_facts(const SsaFunction *func);
void ssa_decision_refresh_plans(SsaModule *mod, int optimization_level,
	int avx2_available, uint32_t generation);
void ssa_estimate_register_pressure(SsaFunction *func);
bool ssa_affine_analyze(const SsaFunction *func, SsaAffineFact *facts,
						size_t fact_count);
bool ssa_opt_affine_collapse(SsaFunction *func);
bool ssa_ref_inst_observable(const SsaInst *inst);
void ssa_ref_vm_trace_init(SsaRefVmTrace *trace, size_t event_budget);
void ssa_ref_vm_trace_free(SsaRefVmTrace *trace);
bool ssa_ref_vm_trace_event(SsaRefVmTrace *trace, SsaRefVmEvent event);
bool ssa_ref_apply_vm_trace(SsaModule *mod, const SsaRefVmTrace *trace);

SsaFunction *ssa_create_function(SsaModule *mod, const char *name, SsaType ret_type);
SsaBasicBlock *ssa_create_block(SsaFunction *func, const char *name);

VReg ssa_new_vreg(SsaFunction *func, SsaType type);
void ssa_add_edge(SsaBasicBlock *from, SsaBasicBlock *to);
void ssa_compute_dom_info(SsaFunction *func);
void ssa_analyze_loops(SsaFunction *func);

/* 鎸囦护鍙戝皠鍑芥暟... */

#endif /* IR_SSA_H */

