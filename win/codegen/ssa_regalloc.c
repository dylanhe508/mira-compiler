/* ssa_regalloc.c — 智能寄存器分配 (Smart Register Allocation)
 *
 * 战役三升级版:
 *   1. 基于循环深度的优先级加权评级 (Priority Heuristics)
 *      - 排序时以 priority = use_count * (1 + loop_depth * 10) 为准
 *      - 循环热点变量优先抢占物理寄存器
 *   2. 智能生命周期切分 (Live Range Splitting)
 *      - 当一个低频 VReg 生前跨度很长但只在少量位置被使用，
 *        选择 Spill 它而非驱逐高频变量
 *   3. 超大规模活跃变量降级 (>1024 强行 Spill)
 *      - 当同时活跃变量超过 ACTIVE_CAP 时，不再做优先级计算，
 *        直接将所有新到达的 VReg 无条件 Spill 到栈上
 *      - 保证算法退化为 O(N)，绝不会因排序开销炸掉编译时间
 */
#include "ir_ssa.h"
#include "ir.h"
#include "decision.h"
#include "abi.h"
#include <stdlib.h>
#include <string.h>

extern int mira_opt_level;

/* 寄存器分配是 ABI 无关的,它只依赖三条不变式:
 *   1. 槽位 [0, FIRST_NONVOLATILE) 是调用者保存(易失),
 *      [FIRST_NONVOLATILE, COUNT) 是被调用者保存(非易失);
 *   2. 槽位 0 恒为 RAX、槽位 2 恒为 RDX(IDIV 的被除数/高 32 位),
 *      这样下方 "crosses_div 避开 0 和 2" 的规则才能成立;
 *   3. 任何活跃区间都能在这套槽位方案下着色。
 * 槽位总数与切分点通过 abi.h 查询,使同一份分配器同时服务 Win64
 * 与 SysV,无需重复代码。
 *
 * MIRA_PHYS_REG_MAX 是编译期上界,足以容纳所有受支持 ABI 的槽位数
 * (当前 Win64 与 SysV 均为 12),用于给着色/扫描循环中的定长局部数组
 * 取尺寸。 */
#define MIRA_PHYS_REG_MAX 16

/* 超大规模变量熔断阈值 */
#define ACTIVE_CAP 1024

typedef struct {
	VReg vreg;
	int start_pos;
	int end_pos;
	int use_count;    /* 原始使用次数 */
	int loop_depth;   /* 所在最大循环深度 */
	int priority;     /* 加权优先级 = use_count * (1 + loop_depth * 10) */
	int phys_reg;     /* 分配到的物理寄存器，-1 表示溢出到栈 */
	int vec_phys_reg; /* independent YMM register, -1 for non-vector/spill */
	int float_phys_reg; /* independent volatile XMM register */
	int is_vector;
	int is_float;
	int crosses_call; /* live across CALL/ICALL; volatile scalar pool is unsafe */
	int crosses_div;  /* live across SDIV/SREM; RAX/RDX are unsafe */
	int crosses_block; /* lowering scratch registers are only safe block-locally */
	int stack_slot;
	/* === 静态引用所有权追踪 === */
	int needs_free;               /* 1 = 此 VReg 持有堆内存 */
	const char *free_func_name;   /* 释放函数符号名 */
} LiveInterval;

typedef struct {
	SsaFunction *func;
	LiveInterval *intervals;
	int interval_count;
	int interval_cap;
	
	int *active_list; /* 正活跃的 VReg 编号列表 */
	int active_count;
	int spill_overflow; /* 是否触发了超大规模降级 */
	int *call_positions;
	int call_count;
	int call_cap;
	int *div_positions;
	int div_count;
	int div_cap;
	VReg *affinity_sources; /* dst -> dying source preferred for two-address reuse */
	int affinity_edge_count;
} RegAllocCtx;

/* 估算循环深度：若块有 back-edge 进来（后继 id < 自身 id），认为块在循环内 */
static int estimate_loop_depth(SsaBasicBlock *b, SsaFunction *func) {
	(void)func;
	int depth = 0;
	for (int s = 0; s < b->succ_count; s++) {
		if (b->succs[s]->id <= b->id) depth++; /* back-edge = loop */
	}
	for (int p = 0; p < b->pred_count; p++) {
		if (b->preds[p]->id > b->id) depth++; /* entry from later block = loop body */
	}
	return depth;
}

static int block_for_position(const int *starts, const int *ends,
	                          int block_count, int position) {
	int low = 0;
	int high = block_count - 1;
	while (low <= high) {
		int mid = low + (high - low) / 2;
		if (position < starts[mid]) high = mid - 1;
		else if (position > ends[mid]) low = mid + 1;
		else return mid;
	}
	return -1;
}

static void compute_live_intervals(RegAllocCtx *ctx) {
	int pos = 0;
	int block_count = ctx->func->block_count;
	int *block_start = calloc((size_t)block_count, sizeof(int));
	int *block_end = calloc((size_t)block_count, sizeof(int));
	for (int i = 0; i < ctx->func->block_count; i++) {
		SsaBasicBlock *b = ctx->func->blocks[i];
		int loop_d = estimate_loop_depth(b, ctx->func);
		block_start[i] = pos + 1;

		for (SsaInst *inst = b->inst_head; inst; inst = inst->next) {
			pos++;
			if (inst->IrNode == SSA_OP_CALL || inst->IrNode == SSA_OP_ICALL) {
				if (ctx->call_count >= ctx->call_cap) {
					ctx->call_cap = ctx->call_cap ? ctx->call_cap * 2 : 16;
					ctx->call_positions = realloc(ctx->call_positions,
					                              (size_t)ctx->call_cap * sizeof(int));
				}
				ctx->call_positions[ctx->call_count++] = pos;
			}
			if (inst->IrNode == SSA_OP_SDIV || inst->IrNode == SSA_OP_SREM) {
				if (ctx->div_count >= ctx->div_cap) {
					ctx->div_cap = ctx->div_cap ? ctx->div_cap * 2 : 16;
					ctx->div_positions = realloc(ctx->div_positions,
					                              (size_t)ctx->div_cap * sizeof(int));
				}
				ctx->div_positions[ctx->div_count++] = pos;
			}
			
			/* 定义: 设置 start_pos */
			if (inst->dst > 0 && (VReg)inst->dst < (VReg)ctx->interval_count) {
				VReg v = inst->dst;
				ctx->intervals[v].is_vector = (inst->type == SSA_TYPE_V4I64);
				ctx->intervals[v].is_float = (inst->type == SSA_TYPE_FLOAT);
				if (ctx->intervals[v].start_pos == 0) {
					ctx->intervals[v].start_pos = pos;
				}
				if (loop_d > ctx->intervals[v].loop_depth) {
					ctx->intervals[v].loop_depth = loop_d;
				}
				/* 静态引用所有权: 从 SsaInst 传播 needs_free 标记到 LiveInterval */
				if (inst->needs_free && !ctx->intervals[v].needs_free) {
					ctx->intervals[v].needs_free = 1;
					ctx->intervals[v].free_func_name = inst->free_func_name;
				}
			}
			
			/* 检查所有 Use，更新 end_pos 和 use_count */
#define UPDATE_USE(opnd) do { \
	if ((opnd).kind == SSA_OPND_VREG && (opnd).u.vreg > 0 && \
	    (opnd).u.vreg < (VReg)ctx->interval_count) { \
		VReg v = (opnd).u.vreg; \
		ctx->intervals[v].end_pos = pos; \
		ctx->intervals[v].use_count += 1 + loop_d * 10; /* loop uses worth 10x */ \
		if (loop_d > ctx->intervals[v].loop_depth) { \
			ctx->intervals[v].loop_depth = loop_d; \
		} \
	} \
} while(0)

			if (inst->operands) {
				for (int opIdx = 0; opIdx < inst->operand_count; opIdx++) {
					UPDATE_USE(inst->operands[opIdx]);
				}
			}
			UPDATE_USE(inst->op1);
			UPDATE_USE(inst->op2);
#undef UPDATE_USE

			/* === 逃逸分析 (Escape Analysis) ===
			 * 如果一个带有 needs_free 的 VReg 被存入全局变量 (STORE_VAR)
			 * 或者被作为参数传递给函数调用 (CALL)，则所有权已经逃逸，
			 * 不能再自动释放，否则会产生 Use-After-Free 悬垂指针。 */
#define ESCAPE_CHECK(opnd) do { \
	if ((opnd).kind == SSA_OPND_VREG && (opnd).u.vreg > 0 && \
	    (opnd).u.vreg < (VReg)ctx->interval_count) { \
		ctx->intervals[(opnd).u.vreg].needs_free = 0; \
	} \
} while(0)

			if (inst->IrNode == SSA_OP_STORE_VAR) {
				/* 值被存入全局变量，所有权转移 */
				ESCAPE_CHECK(inst->op1);
			}
			if (inst->IrNode == SSA_OP_CALL) {
				/* 值被传给函数调用，所有权可能转移 */
				if (inst->operands) {
					for (int opIdx = 1; opIdx < inst->operand_count; opIdx++) {
						ESCAPE_CHECK(inst->operands[opIdx]);
					}
				}
			}
			if (inst->IrNode == SSA_OP_STORE || inst->IrNode == SSA_OP_STORE8) {
				/* 值被写入内存地址，所有权转移 */
				ESCAPE_CHECK(inst->op1);
			}
			#undef ESCAPE_CHECK
		}
		block_end[i] = pos;
	}

	/* Linear instruction order alone ends a loop-invariant value at its use
	 * in the header.  The back edge executes the body before that next use,
	 * so keep every preheader-defined value that reaches the loop alive until
	 * the latch.  Otherwise the body may reuse and clobber its register. */
	for (int li = 0; li < block_count; li++) {
		SsaBasicBlock *latch = ctx->func->blocks[li];
		for (int si = 0; si < latch->succ_count; si++) {
			SsaBasicBlock *header = latch->succs[si];
			if (!header || header->id > latch->id) continue;
			int header_pos = block_start[header->id];
			int latch_pos = block_end[li];
			for (int v = 1; v < ctx->interval_count; v++) {
				LiveInterval *iv = &ctx->intervals[v];
				if (iv->start_pos > 0 && iv->start_pos < header_pos &&
				    iv->end_pos >= header_pos && iv->end_pos < latch_pos)
					iv->end_pos = latch_pos;
			}
		}
	}
	for (int v = 1; v < ctx->interval_count; ++v) {
		LiveInterval *iv = &ctx->intervals[v];
		int start_block = block_for_position(block_start, block_end,
		                                     block_count, iv->start_pos);
		int end_block = block_for_position(block_start, block_end,
		                                   block_count, iv->end_pos);
		iv->crosses_block = start_block >= 0 && end_block >= 0 &&
			start_block != end_block;
	}
	free(block_start);
	free(block_end);

	/* 计算加权优先级 */
	for (int i = 0; i < ctx->interval_count; i++) {
		LiveInterval *li = &ctx->intervals[i];
		li->priority = li->use_count * (1 + li->loop_depth * 10);
		for (int ci = 0; ci < ctx->call_count; ++ci) {
			int cp = ctx->call_positions[ci];
			if (li->start_pos < cp && cp < li->end_pos) {
				li->crosses_call = 1;
				break;
			}
		}
		for (int di = 0; di < ctx->div_count; ++di) {
			int dp = ctx->div_positions[di];
			if (li->start_pos < dp && dp < li->end_pos) {
				li->crosses_div = 1;
				break;
			}
		}
	}
}

/* 排序比较: 先按 start_pos 排，相同时高优先级的排前面 */
static int compare_interval_start(const void *a, const void *b) {
	LiveInterval *la = *(LiveInterval **)a;
	LiveInterval *lb = *(LiveInterval **)b;
	if (la->start_pos < lb->start_pos) return -1;
	if (la->start_pos > lb->start_pos) return 1;
	/* 若 start 相同，优先分配优先级高的 */
	return lb->priority - la->priority;
}

/* 找到 active 中最佳 Spill 候选:
 * 策略: 选 priority 最低的 (最不重要的变量)
 * 若 priority 相同，选 end_pos 最大的 (存活最久/最远)
 * 这样高频热循环变量几乎不会被驱逐 */
static int find_spill_candidate(RegAllocCtx *ctx) {
	int best_idx = -1;
	int best_priority = 0x7FFFFFFF;
	int best_end = -1;

	for (int j = 0; j < ctx->active_count; j++) {
		int av = ctx->active_list[j];
		LiveInterval *ali = &ctx->intervals[av];
		if (ali->priority < best_priority ||
		    (ali->priority == best_priority && ali->end_pos > best_end)) {
			best_priority = ali->priority;
			best_end = ali->end_pos;
			best_idx = j;
		}
	}
	return best_idx;
}

/* IrReg -> 分配器槽位号的反向映射,与 abi.h 的 mira_abi_map_phys_reg
 * 互为逆函数,但仅对快变量池(fIR pool)中的寄存器有定义 —— 因为
 * var_reg_map 只会被填入 fIR 池成员,这里也只标记这些寄存器占用的
 * 槽位为保留,与原 Win64 实现语义一致(RAX/RCX/RDX 等始终返回 -1)。
 * 两个 ABI 的槽位编号不同,因此不能硬编码,改为遍历当前 ABI 的槽位表
 * 反向查找并用 fIR 池做白名单过滤。 */
static int phys_for_ir_reg(IrReg reg) {
	int fir_count = 0;
	const IrReg *fir = mira_abi_fir_pool(&fir_count);
	/* 仅当 reg 属于 fIR 池才返回有效槽位 */
	bool in_pool = false;
	for (int i = 0; i < fir_count; ++i)
		if (fir[i] == reg) { in_pool = true; break; }
	if (!in_pool) return -1;
	int nphys = mira_abi_phys_reg_count();
	for (int p = 0; p < nphys; ++p)
		if (mira_abi_map_phys_reg(p) == reg)
			return p;
	return -1;
}

static void build_reserved_colors(const SsaFunction *func,
	                              int reserved[MIRA_PHYS_REG_MAX]) {
	int nphys = mira_abi_phys_reg_count();
	memset(reserved, 0, nphys * sizeof(*reserved));
	if (!func || !func->var_reg_map) return;
	for (int slot = 0; slot < func->var_count; ++slot) {
		int phys = phys_for_ir_reg((IrReg)func->var_reg_map[slot]);
		if (phys >= 0) reserved[phys] = 1;
	}
}

static int compare_interval_priority(const void *a, const void *b) {
	const LiveInterval *la = *(LiveInterval * const *)a;
	const LiveInterval *lb = *(LiveInterval * const *)b;
	if (la->priority != lb->priority)
		return la->priority < lb->priority ? 1 : -1;
	int la_span = la->end_pos - la->start_pos;
	int lb_span = lb->end_pos - lb->start_pos;
	if (la_span != lb_span) return la_span < lb_span ? -1 : 1;
	return la->vreg < lb->vreg ? -1 : la->vreg > lb->vreg ? 1 : 0;
}

static bool intervals_interfere(const LiveInterval *a,
	                            const LiveInterval *b) {
	return a->start_pos <= b->end_pos && b->start_pos <= a->end_pos;
}

static bool opcode_is_commutative_two_address(SsaOpcode op) {
	return op == SSA_OP_ADD || op == SSA_OP_MUL || op == SSA_OP_AND ||
		op == SSA_OP_OR || op == SSA_OP_XOR;
}

static bool vreg_uses_fixed_var_register(const RegAllocCtx *ctx, VReg value) {
	if (!ctx || !ctx->func || !ctx->func->vreg_defs || !value ||
	    value >= (VReg)ctx->func->vreg_defs_cap) return false;
	SsaInst *def = ctx->func->vreg_defs[value];
	if (!def || def->IrNode != SSA_OP_LOAD_VAR ||
	    def->op1.kind != SSA_OPND_IMM || !ctx->func->var_reg_map)
		return false;
	int slot = (int)def->op1.u.imm;
	return slot >= 0 && slot < ctx->func->var_count &&
		ctx->func->var_reg_map[slot] != REG_NONE;
}

static void collect_scalar_affinities(RegAllocCtx *ctx) {
	free(ctx->affinity_sources);
	ctx->affinity_sources = calloc((size_t)ctx->interval_count,
	                               sizeof(*ctx->affinity_sources));
	ctx->affinity_edge_count = 0;
	if (!ctx->affinity_sources) return;
	int pos = 0;
	for (int bi = 0; bi < ctx->func->block_count; ++bi) {
		for (SsaInst *inst = ctx->func->blocks[bi]->inst_head; inst;
		     inst = inst->next) {
			++pos;
			if (!inst->dst || inst->dst >= (VReg)ctx->interval_count ||
			    inst->type != SSA_TYPE_INT || inst->needs_free) continue;
			SsaOperand preferred = {0};
			if (inst->IrNode == SSA_OP_COPY) {
				preferred = inst->op1;
			} else if (opcode_is_commutative_two_address(inst->IrNode) ||
			           inst->IrNode == SSA_OP_SUB) {
				if (inst->op1.kind == SSA_OPND_VREG &&
				    inst->op1.u.vreg < (VReg)ctx->interval_count &&
				    ctx->intervals[inst->op1.u.vreg].end_pos == pos)
					preferred = inst->op1;
				else if (opcode_is_commutative_two_address(inst->IrNode) &&
				         inst->op2.kind == SSA_OPND_VREG &&
				         inst->op2.u.vreg < (VReg)ctx->interval_count &&
				         ctx->intervals[inst->op2.u.vreg].end_pos == pos)
					preferred = inst->op2;
			}
			if (preferred.kind != SSA_OPND_VREG || !preferred.u.vreg ||
			    preferred.u.vreg >= (VReg)ctx->interval_count) continue;
			if (vreg_uses_fixed_var_register(ctx, preferred.u.vreg)) continue;
			LiveInterval *source = &ctx->intervals[preferred.u.vreg];
			LiveInterval *dest = &ctx->intervals[inst->dst];
			if (source->is_float || source->is_vector || source->needs_free ||
			    source->end_pos != pos || dest->start_pos != pos) continue;
			ctx->affinity_sources[inst->dst] = preferred.u.vreg;
			ctx->affinity_edge_count++;
		}
	}
}

/* Bounded deterministic coloring.  Build the complete result in a side
 * array so every disabled/over-budget/allocation-failure path leaves the
 * linear-scan input untouched. */
static bool global_color(RegAllocCtx *ctx) {
	if (!ctx || !ctx->func ||
	    !ctx->func->decision_plan.prefer_global_graph_coloring)
		return false;

	LiveInterval **nodes = malloc(512u * sizeof(*nodes));
	if (!nodes) return false;
	int count = 0;
	for (int i = 1; i < ctx->interval_count; ++i) {
		LiveInterval *interval = &ctx->intervals[i];
		if (interval->start_pos <= 0 || interval->is_vector ||
		    interval->is_float)
			continue;
		if (count == 512) {
			free(nodes);
			return false;
		}
		nodes[count++] = interval;
	}
	if (count < 8) {
		free(nodes);
		return false;
	}

	qsort(nodes, (size_t)count, sizeof(*nodes), compare_interval_priority);
	int *colors = malloc((size_t)count * sizeof(*colors));
	if (!colors) {
		free(colors);
		free(nodes);
		return false;
	}
	int reserved[MIRA_PHYS_REG_MAX];
	build_reserved_colors(ctx->func, reserved);
	const int nphys = mira_abi_phys_reg_count();
	const int first_nv = mira_abi_first_nonvolatile_phys();

	for (int ni = 0; ni < count; ++ni) {
		bool unavailable[MIRA_PHYS_REG_MAX] = {false};
		VReg copy_source = ctx->affinity_sources ?
			ctx->affinity_sources[nodes[ni]->vreg] : 0;
		int preferred_color = -1;
		for (int color = 0; color < nphys; ++color)
			unavailable[color] = reserved[color] != 0;
		for (int pj = 0; pj < ni; ++pj) {
			bool dying_copy_pair = nodes[pj]->vreg == copy_source &&
				nodes[pj]->end_pos == nodes[ni]->start_pos;
			if (dying_copy_pair)
				preferred_color = colors[pj];
			if (colors[pj] >= 0 && !dying_copy_pair &&
			    intervals_interfere(nodes[ni], nodes[pj]))
				unavailable[colors[pj]] = true;
		}

		colors[ni] = -1;
		if (preferred_color >= 0 && !unavailable[preferred_color] &&
		    !((nodes[ni]->crosses_call || nodes[ni]->crosses_block) &&
		      preferred_color < first_nv) &&
		    !(nodes[ni]->crosses_div &&
		      (preferred_color == 0 || preferred_color == 2)))
			colors[ni] = preferred_color;
		for (int color = 0; color < nphys; ++color) {
			if (colors[ni] >= 0) break;
			if (unavailable[color]) continue;
			if ((nodes[ni]->crosses_call || nodes[ni]->crosses_block) &&
			    color < first_nv)
				continue;
			if (nodes[ni]->crosses_div &&
			    (color == 0 || color == 2))
				continue;
			colors[ni] = color;
			break;
		}
	}

	for (int ni = 0; ni < count; ++ni)
		nodes[ni]->phys_reg = colors[ni];
	free(colors);
	free(nodes);
	return true;
}

static void linear_scan(RegAllocCtx *ctx) {
	/* Sort by start_pos (ties broken by priority desc) */
	LiveInterval **sorted = malloc(ctx->interval_count * sizeof(LiveInterval*));
	int n_intervals = 0;
	for (int i = 0; i < ctx->interval_count; i++) {
		if (ctx->intervals[i].start_pos > 0 && !ctx->intervals[i].is_vector &&
		    !(mira_opt_level < 3 && ctx->intervals[i].is_float && !ctx->intervals[i].crosses_call)) {
			sorted[n_intervals++] = &ctx->intervals[i];
		}
	}
	qsort(sorted, n_intervals, sizeof(LiveInterval*), compare_interval_start);

	const int nphys = mira_abi_phys_reg_count();
	const int first_nv = mira_abi_first_nonvolatile_phys();
	int free_regs[MIRA_PHYS_REG_MAX];
	for (int i = 0; i < nphys; i++) free_regs[i] = 1;
	int reserved_regs[MIRA_PHYS_REG_MAX];
	/* 快变量分配器先于 SSA 分配运行。它对易失的 R8/R9 的占用与
	 * SSA 标量池必须共用同一份所有权映射,否则两个分配器会合法地
	 * 选中同一个硬件寄存器。 */
	build_reserved_colors(ctx->func, reserved_regs);
	for (int i = 0; i < nphys; ++i)
		if (reserved_regs[i]) free_regs[i] = 0;
	int next_volatile_reg = 0;
	int next_nonvolatile_reg = first_nv;

	for (int i = 0; i < n_intervals; i++) {
		LiveInterval *li = sorted[i];
		
		/* Expire old intervals */
		for (int j = 0; j < ctx->active_count; ) {
			int active_vreg = ctx->active_list[j];
			LiveInterval *old_li = &ctx->intervals[active_vreg];
			if (old_li->end_pos < li->start_pos) {
				if (old_li->phys_reg != -1 && !reserved_regs[old_li->phys_reg]) {
					free_regs[old_li->phys_reg] = 1;
				}
				ctx->active_list[j] = ctx->active_list[--ctx->active_count];
			} else {
				j++;
			}
		}

		/* ===== 超大规模降级熔断 ===== */
		if (ctx->active_count >= ACTIVE_CAP) {
			/* 超过 1024 个同时活跃变量，直接 Spill 所有新到达的 VReg */
			li->phys_reg = -1;
			ctx->spill_overflow = 1;
			continue;
		}
		VReg preferred_source = ctx->affinity_sources ?
			ctx->affinity_sources[li->vreg] : 0;
		int preferred_active = -1;
		if (preferred_source) {
			for (int j = 0; j < ctx->active_count; ++j)
				if (ctx->active_list[j] == (int)preferred_source) {
					preferred_active = j;
					break;
			}
		}
		if (preferred_active >= 0) {
			LiveInterval *source = &ctx->intervals[preferred_source];
			int preferred_reg = source->phys_reg;
			bool compatible = preferred_reg >= 0 &&
				!reserved_regs[preferred_reg] &&
				!((li->crosses_call || li->crosses_block) &&
				  preferred_reg < first_nv) &&
				!(li->crosses_div &&
				  (preferred_reg == 0 || preferred_reg == 2));
			if (compatible) {
				li->phys_reg = preferred_reg;
				ctx->active_list[preferred_active] = li->vreg;
				continue;
			}
		}
		/* 在空闲寄存器中轮转起点，减少相邻非重叠区间反复复用同一物理
		 * 寄存器造成的假 WAW/WAR；活跃区间和 ABI 约束保持不变。 */
		int reg = -1;
		if (!li->crosses_call && !li->crosses_block) {
			for (int k = 0; k < first_nv; k++) {
				int r = (next_volatile_reg + k) % first_nv;
				if (li->crosses_div && (r == 0 || r == 2)) continue;
				if (free_regs[r]) { reg = r; break; }
			}
		}
		if (reg == -1) {
			int nonvolatile_count = nphys - first_nv;
			for (int k = 0; k < nonvolatile_count; k++) {
				int r = first_nv +
					(next_nonvolatile_reg - first_nv + k) %
						nonvolatile_count;
				if (free_regs[r]) { reg = r; break; }
			}
		}
		if (reg == -1 &&
		    (li->crosses_call || li->crosses_div || li->crosses_block)) {
			li->phys_reg = -1;
			continue;
		}

		if (reg != -1) {
			li->phys_reg = reg;
			free_regs[reg] = 0;
			if (reg < first_nv)
				next_volatile_reg = (reg + 1) % first_nv;
			else
				next_nonvolatile_reg = first_nv +
					(reg - first_nv + 1) %
						(nphys - first_nv);
			ctx->active_list[ctx->active_count++] = li->vreg;
		} else {
			/* No free register: Smart Spill Decision
			 * Compare incoming VReg's priority against the lowest-priority
			 * active VReg. If the incoming one is hotter, evict the cold one.
			 * 
			 * Live Range Splitting heuristic:
			 * If the incoming VReg has a very short life span (end - start < 5)
			 * but very high priority, definitely evict a cold long-lived one. */
			int spill_idx = find_spill_candidate(ctx);
			if (spill_idx >= 0) {
				int spill_vreg = ctx->active_list[spill_idx];
				LiveInterval *spill_li = &ctx->intervals[spill_vreg];
				
				/* Evict if: incoming has higher priority, OR
				 * incoming has short life span (high locality) */
				int incoming_span = li->end_pos - li->start_pos;
				int candidate_span = spill_li->end_pos - spill_li->start_pos;
				
				bool should_evict = (li->priority > spill_li->priority);
				/* Short-lived hot value always wins over long-lived cold value */
				if (!should_evict && incoming_span < 5 && candidate_span > 20 &&
				    li->use_count > spill_li->use_count) {
					should_evict = true;
				}
				if ((li->crosses_call || li->crosses_block) &&
				    spill_li->phys_reg < first_nv)
					should_evict = false;
				if (li->crosses_div &&
				    (spill_li->phys_reg == 0 || spill_li->phys_reg == 2))
					should_evict = false;
				
				if (should_evict) {
					/* Steal the register from spilled interval */
					li->phys_reg = spill_li->phys_reg;
					spill_li->phys_reg = -1;
					ctx->active_list[spill_idx] = li->vreg;
				} else {
					/* Spill the new interval */
					li->phys_reg = -1;
				}
			} else {
				li->phys_reg = -1;
			}
		}
	}
	free(sorted);
}

static int scalar_spill_count(const RegAllocCtx *ctx) {
	int count = 0;
	for (int i = 1; i < ctx->interval_count; ++i) {
		const LiveInterval *interval = &ctx->intervals[i];
		if (interval->start_pos > 0 && !interval->is_vector &&
		    !interval->is_float && interval->phys_reg < 0)
			++count;
	}
	return count;
}

/* SSA COPY lowers to a real register move exactly when both values are
 * allocated and their colors differ.  Weight copies in loop blocks so an
 * equal-spill graph result is selected only for a concrete lowering win. */
static uint64_t scalar_copy_move_cost(const RegAllocCtx *ctx) {
	uint64_t cost = 0;
	for (int bi = 0; bi < ctx->func->block_count; ++bi) {
		SsaBasicBlock *block = ctx->func->blocks[bi];
		uint64_t weight = 1u +
			(uint64_t)estimate_loop_depth(block, ctx->func) * 10u;
		for (SsaInst *inst = block->inst_head; inst; inst = inst->next) {
			if (inst->IrNode != SSA_OP_COPY || inst->dst == 0 ||
			    inst->dst >= (VReg)ctx->interval_count ||
			    inst->op1.kind != SSA_OPND_VREG ||
			    inst->op1.u.vreg == 0 ||
			    inst->op1.u.vreg >= (VReg)ctx->interval_count)
				continue;
			const LiveInterval *dst = &ctx->intervals[inst->dst];
			const LiveInterval *src = &ctx->intervals[inst->op1.u.vreg];
			if (dst->is_float || dst->is_vector || src->is_float ||
			    src->is_vector || dst->phys_reg < 0 || src->phys_reg < 0)
				continue;
			if (dst->phys_reg != src->phys_reg) cost += weight;
		}
	}
	return cost;
}

/* Global coloring is deliberately speculative.  Keep the established linear
 * scan as a cheap quality floor: if coloring spills more scalar values, retain
 * the linear result.  With equal spills, coloring must also remove weighted
 * COPY moves; a tie retains the established allocator. */
static void allocate_scalars(RegAllocCtx *ctx) {
	if (!global_color(ctx)) {
		linear_scan(ctx);
		return;
	}

	int graph_spills = scalar_spill_count(ctx);
	uint64_t graph_moves = scalar_copy_move_cost(ctx);
	int *graph_colors = malloc((size_t)ctx->interval_count *
	                           sizeof(*graph_colors));
	if (!graph_colors)
		return;
	for (int i = 0; i < ctx->interval_count; ++i) {
		graph_colors[i] = ctx->intervals[i].phys_reg;
		ctx->intervals[i].phys_reg = -1;
	}

	ctx->active_count = 0;
	ctx->spill_overflow = 0;
	linear_scan(ctx);
	int linear_spills = scalar_spill_count(ctx);
	uint64_t linear_moves = scalar_copy_move_cost(ctx);
	bool use_graph = graph_spills < linear_spills ||
		(graph_spills == linear_spills && graph_moves < linear_moves);
	if (use_graph)
		for (int i = 0; i < ctx->interval_count; ++i)
			ctx->intervals[i].phys_reg = graph_colors[i];
	if (getenv("MIRA_DECISION_DEBUG"))
		fprintf(stderr,
		        "decision regalloc function=%s graph_spills=%d "
		        "linear_spills=%d graph_moves=%llu linear_moves=%llu "
		        "choice=%s\n",
		        ctx->func && ctx->func->name ? ctx->func->name : "?",
		        graph_spills, linear_spills,
		        (unsigned long long)graph_moves,
		        (unsigned long long)linear_moves,
		        use_graph ? "global-color" : "linear-scan");
	free(graph_colors);
}

static void update_estimated_pressure(RegAllocCtx *ctx) {
	SsaFunction *func = ctx->func;
	DecisionLiveRange *ranges = calloc((size_t)ctx->interval_count, sizeof(*ranges));
	if (ranges) {
		size_t count = 0;
		for (int i = 1; i < ctx->interval_count; ++i) {
			LiveInterval *interval = &ctx->intervals[i];
			if (interval->start_pos <= 0) continue;
			ranges[count].start = interval->start_pos;
			ranges[count].end = interval->end_pos >= interval->start_pos
				? interval->end_pos : interval->start_pos;
			ranges[count].reg_class = interval->is_vector ? DECISION_REG_VECTOR
				: (interval->is_float ? DECISION_REG_FLOAT : DECISION_REG_SCALAR);
			count++;
		}
		DecisionPressure pressure = decision_measure_pressure(ranges, count);
		func->estimated_scalar_pressure = pressure.scalar_peak;
		func->estimated_float_pressure = pressure.float_peak;
		func->estimated_vector_pressure = pressure.vector_peak;
		free(ranges);
	}
}

void ssa_estimate_register_pressure(SsaFunction *func) {
	if (!func || func->next_vreg <= 1) return;
	RegAllocCtx ctx = {0};
	ctx.func = func;
	ctx.interval_count = func->next_vreg;
	ctx.interval_cap = func->next_vreg;
	ctx.intervals = calloc((size_t)ctx.interval_cap, sizeof(*ctx.intervals));
	if (!ctx.intervals) return;
	for (int i = 0; i < ctx.interval_cap; ++i) {
		ctx.intervals[i].vreg = i;
		ctx.intervals[i].phys_reg = -1;
		ctx.intervals[i].vec_phys_reg = -1;
		ctx.intervals[i].float_phys_reg = -1;
		ctx.intervals[i].stack_slot = -1;
	}
	compute_live_intervals(&ctx);
	update_estimated_pressure(&ctx);
	free(ctx.call_positions);
	free(ctx.div_positions);
	free(ctx.intervals);
}

/* XMM0-XMM2 are lowering/peephole scratch registers.  Keep persistent scalar
 * float values in XMM3-XMM5. Values live across a call deliberately stay in
 * the scalar allocator because all six registers are volatile in Win64. */
static void linear_scan_floats(RegAllocCtx *ctx) {
	LiveInterval **sorted = malloc(ctx->interval_count * sizeof(*sorted));
	int count = 0;
	for (int i = 0; i < ctx->interval_count; ++i)
		if (ctx->intervals[i].start_pos > 0 && ctx->intervals[i].is_float &&
		    !ctx->intervals[i].crosses_call)
			sorted[count++] = &ctx->intervals[i];
	qsort(sorted, count, sizeof(*sorted), compare_interval_start);
	LiveInterval *active[3] = {0};
	int active_count = 0;
	int free_regs[3] = {1, 1, 1};
	for (int i = 0; i < count; ++i) {
		LiveInterval *li = sorted[i];
		for (int j = 0; j < active_count; ) {
			if (active[j]->end_pos < li->start_pos) {
				free_regs[active[j]->float_phys_reg] = 1;
				active[j] = active[--active_count];
			} else ++j;
		}
		int reg = -1;
		for (int r = 0; r < 3; ++r) if (free_regs[r]) { reg = r; break; }
		li->float_phys_reg = reg;
		if (reg >= 0) { free_regs[reg] = 0; active[active_count++] = li; }
	}
	free(sorted);
}

static void linear_scan_vectors(RegAllocCtx *ctx) {
	LiveInterval **sorted = malloc(ctx->interval_count * sizeof(*sorted));
	int count = 0;
	for (int i = 0; i < ctx->interval_count; ++i)
		if (ctx->intervals[i].start_pos > 0 && ctx->intervals[i].is_vector)
			sorted[count++] = &ctx->intervals[i];
	qsort(sorted, count, sizeof(*sorted), compare_interval_start);

	LiveInterval *active[16] = {0};
	int active_count = 0;
	int free_regs[16];
	for (int i = 0; i < 16; ++i) free_regs[i] = 1;
	for (int i = 0; i < count; ++i) {
		LiveInterval *li = sorted[i];
		for (int j = 0; j < active_count; ) {
			if (active[j]->end_pos < li->start_pos) {
				free_regs[active[j]->vec_phys_reg] = 1;
				active[j] = active[--active_count];
			} else ++j;
		}
		int reg = -1;
		for (int r = 0; r < 16; ++r) if (free_regs[r]) { reg = r; break; }
		li->vec_phys_reg = reg;
		if (reg >= 0) { free_regs[reg] = 0; active[active_count++] = li; }
	}
	free(sorted);
}

/* ======================================================================
 * 静态引用所有权: 自动内存释放插入 (Auto-Free Insertion)
 *
 * 遍历所有 SSA 指令，当某个标记了 needs_free 的 VReg 到达它的
 * 最后一次使用位置 (end_pos) 时，在该指令之后紧接插入一条
 * SSA_OP_CALL 调用对应的释放函数。
 *
 * 如果该 VReg 从未被使用（use_count==0），则其定义指令（allocate）
 * 也没有意义——但这种情况会被之前的 DCE pass 自动清除。
 * ====================================================================== */
static void ssa_insert_auto_free(RegAllocCtx *ctx) {
	int pos = 0;
	for (int bi = 0; bi < ctx->func->block_count; bi++) {
		SsaBasicBlock *b = ctx->func->blocks[bi];
		for (SsaInst *inst = b->inst_head; inst; inst = inst->next) {
			pos++;

			/* 检查所有操作数: 如果某个 VReg 在此处最后一次使用，插入 free */
#define CHECK_FREE(opnd) do { \
	if ((opnd).kind == SSA_OPND_VREG && (opnd).u.vreg > 0 && \
	    (opnd).u.vreg < (VReg)ctx->interval_count) { \
		VReg v = (opnd).u.vreg; \
		LiveInterval *li = &ctx->intervals[v]; \
		if (li->needs_free && li->end_pos == pos && li->free_func_name) { \
			/* 在当前指令之后插入: CALL free_func(vreg) */ \
			SsaInst *free_call = calloc(1, sizeof(SsaInst)); \
			free_call->IrNode = SSA_OP_CALL; \
			free_call->type = SSA_TYPE_VOID; \
			free_call->dst = 0; \
			free_call->parent = b; \
			free_call->operand_cap = 2; \
			free_call->operands = malloc(sizeof(SsaOperand) * 2); \
			free_call->operands[0].kind = SSA_OPND_SYM; \
			free_call->operands[0].u.sym = strdup(li->free_func_name); \
			free_call->operands[1].kind = SSA_OPND_VREG; \
			free_call->operands[1].u.vreg = v; \
			free_call->operand_count = 2; \
			/* 链表插入: inst -> free_call -> inst->next */ \
			free_call->prev = inst; \
			free_call->next = inst->next; \
			if (inst->next) inst->next->prev = free_call; \
			else b->inst_tail = free_call; \
			inst->next = free_call; \
			li->needs_free = 0; /* 已插入，防止重复 */ \
		} \
	} \
} while(0)

			if (inst->operands) {
				for (int opIdx = 0; opIdx < inst->operand_count; opIdx++) {
					CHECK_FREE(inst->operands[opIdx]);
				}
			}
			CHECK_FREE(inst->op1);
			CHECK_FREE(inst->op2);
#undef CHECK_FREE
		}
	}
}

static void reset_interval_analysis(RegAllocCtx *ctx) {
	free(ctx->call_positions);
	ctx->call_positions = NULL;
	ctx->call_count = 0;
	ctx->call_cap = 0;
	free(ctx->div_positions);
	ctx->div_positions = NULL;
	ctx->div_count = 0;
	ctx->div_cap = 0;
	for (int i = 0; i < ctx->interval_count; ++i) {
		VReg vreg = ctx->intervals[i].vreg;
		memset(&ctx->intervals[i], 0, sizeof(ctx->intervals[i]));
		ctx->intervals[i].vreg = vreg;
		ctx->intervals[i].phys_reg = -1;
		ctx->intervals[i].vec_phys_reg = -1;
		ctx->intervals[i].float_phys_reg = -1;
		ctx->intervals[i].stack_slot = -1;
	}
}

void ssa_allocate_registers(SsaModule *mod) {
	for (int f = 0; f < mod->func_count; f++) {
		SsaFunction *func = mod->functions[f];
		RegAllocCtx ctx = {0};
		ctx.func = func;
		ctx.interval_count = func->next_vreg;
		ctx.interval_cap = func->next_vreg;
		ctx.intervals = calloc(ctx.interval_cap, sizeof(LiveInterval));
		for (int i = 0; i < ctx.interval_cap; i++) {
			ctx.intervals[i].vreg = i;
			ctx.intervals[i].phys_reg = -1;
			ctx.intervals[i].vec_phys_reg = -1;
			ctx.intervals[i].float_phys_reg = -1;
			ctx.intervals[i].stack_slot = -1;
		}

		ctx.active_list = malloc(ctx.interval_cap * sizeof(int));
        ctx.active_count = 0;
        ctx.spill_overflow = 0;

		compute_live_intervals(&ctx);

		/* === 静态引用所有权: 在活跃区间末尾插入自动 free === */
		ssa_insert_auto_free(&ctx);
		/* Auto-free adds a real call. Values live across that new call must
		 * not remain in Win64 volatile registers, and all following positions
		 * have shifted. Rebuild intervals before physical allocation. */
		reset_interval_analysis(&ctx);
		compute_live_intervals(&ctx);
		collect_scalar_affinities(&ctx);

		/* Inlining and SSA rewrites can invalidate the early profitability
		 * estimate. Reuse the final allocation intervals so the backend
		 * decision plan receives current pressure without another scan. */
		update_estimated_pressure(&ctx);
		allocate_scalars(&ctx);
		/* O3's later loop-state promotion currently produces denser FMA/divide
		 * loops from the bit-pattern form. Keep that proven path until the
		 * machine pass consumes allocated XMM live ranges directly. */
		if (mira_opt_level < 3) linear_scan_floats(&ctx);
		linear_scan_vectors(&ctx);
		func->vreg_phys_map = malloc(func->next_vreg * sizeof(int));
		func->vreg_float_phys_map = malloc(func->next_vreg * sizeof(int));
		func->vreg_vec_phys_map = malloc(func->next_vreg * sizeof(int));
		func->vreg_spill_map = malloc(func->next_vreg * sizeof(int));
		int spill_count = 0;
		for (int i = 0; i < func->next_vreg; i++) {
			func->vreg_phys_map[i] = ctx.intervals[i].phys_reg;
			func->vreg_float_phys_map[i] = ctx.intervals[i].float_phys_reg;
			func->vreg_vec_phys_map[i] = ctx.intervals[i].vec_phys_reg;
			func->vreg_spill_map[i] = -1;
			if (i > 0 && ctx.intervals[i].start_pos > 0 &&
			    !ctx.intervals[i].is_vector && ctx.intervals[i].phys_reg == -1 &&
			    ctx.intervals[i].float_phys_reg == -1)
				func->vreg_spill_map[i] = spill_count++;
		}
		func->spill_size = (spill_count * 8 + 15) & ~15;
		func->actual_spill_count = (uint32_t)spill_count;
		if (getenv("MIRA_DECISION_DEBUG"))
			fprintf(stderr, "decision pressure function=%s scalar=%u float=%u vector=%u spills=%u affinity_edges=%d\n",
				func->name ? func->name : "?", func->estimated_scalar_pressure,
				func->estimated_float_pressure, func->estimated_vector_pressure,
				func->actual_spill_count, ctx.affinity_edge_count);

		free(ctx.active_list);
		free(ctx.call_positions);
		free(ctx.div_positions);
		free(ctx.affinity_sources);
		free(ctx.intervals);
	}
}

