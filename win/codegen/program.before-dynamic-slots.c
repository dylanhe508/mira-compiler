/* program.c ????? codegen ??? (IR) */
#include "codegen.h"
#include "ir_ssa.h"
#include "decision.h"
#include "../dll_ext.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void gen_def(Def *d) {
	cg->current_def = d;
	cg->stack_depth = 0;
	cg->type_depth = 0;
	cg->lIR_var_slot = -1;

	/* ????????*/
	char sym[256];
	size_t slen = d->name_len < 255 ? d->name_len : 255;
	for (size_t i = 0; i < slen; i++) sym[i] = (d->name[i] == '-') ? '_' : d->name[i];
	sym[slen] = '\0';

	ir_label_named(&cg->ir, sym);
	ir_push(&cg->ir, REG_RBP);
	ir_mov_reg_reg(&cg->ir, REG_RBP, REG_RSP);

	/* ?????? */
	if (d->param_count > 0) ir_mov_mem_reg(&cg->ir, REG_RBP, 16, REG_RCX);
	if (d->param_count > 1) ir_mov_mem_reg(&cg->ir, REG_RBP, 24, REG_RDX);
	if (d->param_count > 2) ir_mov_mem_reg(&cg->ir, REG_RBP, 32, REG_R8);
	if (d->param_count > 3) ir_mov_mem_reg(&cg->ir, REG_RBP, 40, REG_R9);

	gen_ops(d->body);

	ir_pop(&cg->ir, REG_RBP);
	ir_ret(&cg->ir);

	cg->current_def = NULL;
}

/* ????????? */
static const char *externs[] = {
	"mira_float_tmp",
	"mira_print", "mira_read_int", "mira_input", "ExitProcess", "mira_cr",
	"mem_alloc", "mem_free", "mem_move", "mem_erase", "mira_mem_dump",
	"mira_dump_data_stack", "mira_dump_var_slot", "mira_dump_vars",
	"mira_stats", "mira_debug_break", "mira_debug_step", "mira_debug_next",
	"mira_debug_continue", "mira_dump_return_stack", "mira_dump_type_stack",
	"mira_backtrace", "mira_where", "mira_watch_not_supported",
	"mira_list_new", "mira_list_len", "mira_list_get", "mira_list_set",
	"mira_list_free", "mira_list_push", "mira_list_pop",
	"mira_dict_new", "mira_dict_set", "mira_dict_get", "mira_dict_has",
	"mira_dict_free", "mira_dict_keys", "mira_dict_count",
	"mira_str_len", "mira_str_concat", "mira_str_eq",
	"mira_int_to_str", "mira_to_str", "mira_str_to_int",
	"mira_int_to_float", "mira_float_to_int",
	"mira_try_call", "mira_throw", "mira_get_error",
	"mira_try_begin", "mira_try_end", "_setjmp",
	"mira_struct_new", "mira_struct_free",
	"mira_random", "mira_random_range", "mira_random_seed",
	"mira_abs", "mira_min", "mira_max",
	"mira_f_sqrt", "mira_f_pow",
	"mira_time_now", "mira_time_ms",
	"mira_str_contains", "mira_str_trim", "mira_str_substr",
	"mira_f_floor", "mira_f_ceil",
	"mira_async_start", "mira_async_yield",
	"mira_parallel_start", "mira_parallel_join",
	"mira_go_start0", "mira_go_start_fast0",
	"mira_go_join", "mira_go_yield", "mira_go_wait_all",
	"mira_channel_new_value", "mira_channel_send_value",
	"mira_channel_recv_value", "mira_channel_close_value",
	"mira_channel_free_value",
	"mira_win_msgbox", "mira_win_sleep", "mira_win_shell",
	"mira_win_env", "mira_win_env_set",
	"mira_win_clip_get", "mira_win_clip_set",
	"mira_win_beep", "mira_win_beep_freq",
	"mira_win_set_title", "mira_win_color_set", "mira_win_cursor_move",
	"mira_win_screen_width", "mira_win_screen_height",
	"mira_win_pid", "mira_win_tick", "mira_win_tick_ns",
	"mira_print", "mira_win_sleep", "mira_win_shell", "mira_win_env", "mira_win_clip_get",
	"mira_win_pid", "mira_win_tick", "mira_win_tick_ns", "mira_async_start", "mira_async_yield",
	"mira_win_msgbox", "mira_f_sqrt", "mira_f_pow", "mira_random", "mira_random_seed",
	"mira_str_to_int", "mira_int_to_float", "mira_str_concat", "mira_str_eq",
	"mira_str_substr", "mira_str_contains", "mira_str_at",
	"mira_file_read", "mira_file_write", "mira_file_append",
	"mira_file_exists", "mira_file_delete",
	/* lwmgl_* ???????????? import-ext ????????????? */
	NULL
};

void codegen(Compiler *c, Program *prog) {
	codegen_state_init(c, prog);
	c->label_id = 0;

	/* ????????? */
	for (int i = 0; externs[i]; i++)
		ir_extern(&cg->ir, externs[i]);

	/* ??? dll-map JSON ?????????????? */
	dll_ext_register_to_ir(&cg->ir);

	/* ???????????*/
	extern void dll_map_register(const char *, const char *);
	for (int i = 0; i < g_dll_ext_count; i++)
		dll_map_register(g_dll_ext_entries[i].c_name, g_dll_ext_entries[i].dll_name);

	/* ?????? */
	ir_global(&cg->ir, "mira_main");
	ir_global(&cg->ir, "mira_var_count");
	ir_global(&cg->ir, "mira_var_names");
	ir_global(&cg->ir, "mira_vars");

	/* .data ??*/
	/* ??? doubles */
	for (int i = 0; i < prog->const_count; i++) {
		if (prog->const_kinds[i] == CONST_DOUBLE) {
			char lbl[64]; snprintf(lbl, sizeof(lbl), "const_dbl.%d", i);
			ir_data_label(&cg->ir, lbl);
			ir_data_qword_dbl(&cg->ir, prog->const_doubles[i]);
		} else if (prog->const_kinds[i] == CONST_STR) {
			char lbl[64]; snprintf(lbl, sizeof(lbl), "const_str.%d", i);
			ir_data_label(&cg->ir, lbl);
			const char *s = prog->const_strs[i];
			size_t slen = prog->const_str_lens[i];
			uint8_t *bytes = (uint8_t *)malloc(slen + 1);
			memcpy(bytes, s, slen);
			bytes[slen] = 0;
			ir_data_bytes(&cg->ir, bytes, (int)(slen + 1));
			free(bytes);
		}
	}

	/* ????????*/
	ir_data_label(&cg->ir, "mira_var_count");
	ir_data_qword(&cg->ir, prog->var_count);

	if (prog->var_count > 0) {
		for (int i = 0; i < prog->var_count; i++) {
			char vlbl[64]; snprintf(vlbl, sizeof(vlbl), "mira_var_name_%d", i);
			ir_data_label(&cg->ir, vlbl);
			const char *vn = prog->var_names[i];
			size_t vlen = prog->var_lens[i];
			uint8_t *bytes = (uint8_t *)malloc(vlen + 1);
			memcpy(bytes, vn, vlen);
			bytes[vlen] = 0;
			ir_data_bytes(&cg->ir, bytes, (int)(vlen + 1));
			free(bytes);
		}
		ir_data_label(&cg->ir, "mira_var_names");
		for (int i = 0; i < prog->var_count; i++) {
			char vlbl[64]; snprintf(vlbl, sizeof(vlbl), "mira_var_name_%d", i);
			ir_data_qword_sym(&cg->ir, vlbl);
		}
	} else {
		ir_data_label(&cg->ir, "mira_var_names");
		ir_data_qword(&cg->ir, 0);
	}

	/* .bss ??*/
	ir_bss_label(&cg->ir, "mira_stack");
	ir_bss_resq(&cg->ir, 65536);
	ir_bss_label(&cg->ir, "mira_vars");
	ir_bss_resq(&cg->ir, (prog->var_count > 0 ? prog->var_count : 1) + 16);


	/* .text ????????????? */
	for (Def *d = prog->defs; d; d = d->next) {
		if (d->is_extern) {
			char sym[256];
			size_t slen = d->name_len < 255 ? d->name_len : 255;
			for (size_t i = 0; i < slen; i++) sym[i] = (d->name[i] == '-') ? '_' : d->name[i];
			sym[slen] = '\0';
			ir_extern(&cg->ir, sym);
		}
	}

	/* -------------------------------------------------------------
	 * ???????SSA ????????(The New SSA Pipeline)????
	 * ------------------------------------------------------------- 
	 * 1. ?????SSA Module
	 * 2. ??? IR ????????????????????????????????(Builder)
	 * 3. ???????????????? Phi ??? (DomTree + mem2reg)
	 * 4. ?????????????????? (Linear Scan)
	 * 5. ??SSA ????????? x86-64 ??? IR ?????(Lowering)
	 */
	SsaModule ssa_mod;
	ssa_init_module(&ssa_mod);

	// ??????IR -> SSA TAC (?????Alloca)
	extern void ssa_build_program(Program *prog, SsaModule *out_mod);
	ssa_build_program(prog, &ssa_mod);

	/* ???:?? CFG??????PHI,??? mem2reg? */
	extern void ssa_build(SsaModule *mod);
	ssa_build(&ssa_mod);

	/* Build ownership/alias/effect facts on the finalized SSA closure. */
	ssa_ref_analyze_module(&ssa_mod);

	/* ???:?? SSA ???????????,????????
	 * ?????????? */
	extern void ssa_optimize_module(SsaModule *mod);
	ssa_optimize_module(&ssa_mod);

	/* PHIs remain first-class SSA through module inlining and optimization.
	 * Lower them exactly once at the explicit pre-allocation phase boundary. */
	ssa_destroy_phis_module(&ssa_mod);

	/* ???:??????? LOAD_VAR ????????,????
	 * ??????????????? */
	extern void ssa_compute_var_reg_maps(SsaModule *mod, int var_count);
	ssa_compute_var_reg_maps(&ssa_mod, prog->var_count);

	/* ???:? SSA VReg ?????? x86-64/YMM ???????? */
	extern void ssa_allocate_registers(SsaModule *mod);
	ssa_allocate_registers(&ssa_mod);
	extern int mira_opt_level;
	extern int mira_target_avx2;
	/* Allocation supplies final pressure/spill facts. Refresh the per-function
	 * 2.1 plans before lowering and machine-level selection. */
	ssa_decision_refresh_plans(&ssa_mod, mira_opt_level, mira_target_avx2, 4);

	/* ???:?????????? IR? */
	extern void ssa_lower_module(SsaModule *mod, IrBuffer *ir);
	ssa_lower_module(&ssa_mod, &cg->ir);

	// ssa_lower_module(&ssa_mod, &cg->ir); /* Already converted */

	/* ????????????????????????????mov rax, rax ??add reg, 0 ???????????*/
	ir_opt_peephole(&cg->ir);
	extern void ir_opt_remove_dead_stack_stores(IrBuffer *ir);
	ir_opt_remove_dead_stack_stores(&cg->ir);

	DecisionPipelinePlan decision_plan = {0};
	for (int dfi = 0; dfi < ssa_mod.func_count; ++dfi) {
		SsaFunction *decision_func = ssa_mod.functions[dfi];
		DecisionPipelinePlan *local = &decision_func->decision_plan.pipeline;
		decision_plan.allow_vectorize |= local->allow_vectorize;
		decision_plan.allow_unroll |= local->allow_unroll;
		decision_plan.allow_schedule |= local->allow_schedule;
		decision_plan.allow_if_conversion |= local->allow_if_conversion;
		decision_plan.allow_float_optimization |= local->allow_float_optimization;
		decision_plan.allow_scalar_loop_optimization |= local->allow_scalar_loop_optimization;
		decision_plan.allow_affine_recurrence |= local->allow_affine_recurrence;
		decision_plan.allow_magic_division |= local->allow_magic_division;
		decision_plan.allow_loop_rotation |= local->allow_loop_rotation;
		decision_plan.allow_memory_optimization |= local->allow_memory_optimization;
		decision_plan.require_runtime_alias_checks |= local->require_runtime_alias_checks;
		uint64_t combined_budget = (uint64_t)decision_plan.code_growth_budget +
			local->code_growth_budget;
		decision_plan.code_growth_budget = combined_budget > UINT32_MAX
			? UINT32_MAX : (uint32_t)combined_budget;
	}
	decision_pipeline_disable(&decision_plan, getenv("MIRA_DECISION_DISABLE"));

	/* O3:???????????/????????? */
	if (decision_plan.allow_unroll || decision_plan.allow_vectorize) {
		if (decision_plan.allow_vectorize)
		ir_opt_auto_vectorize(&cg->ir);
		if (decision_plan.allow_unroll) {
		ir_opt_countdown_loops(&cg->ir);
		ir_opt_unroll4_remainder(&cg->ir);
		ir_opt_register_rotation(&cg->ir);
		}
	}
	/* Physical-register reuse can clobber a loop bound at every SSA-enabled
	 * optimization level, including O2.  Repair after all optional loop
	 * rewrites so nested-loop bounds remain resident in distinct registers. */
	if (mira_opt_level >= 2)
	{
		ir_opt_repair_loop_bounds(&cg->ir);
	}
	if (decision_plan.allow_schedule)
		ir_opt_ilp_schedule(&cg->ir);

	/* ??????? SSA ????????? */
	ssa_free_module(&ssa_mod);

	/* -------------------------------------------------------------
	 * ????????? mainCRTStartup????????? OS ??????????????
	 * ------------------------------------------------------------- */
	if (prog->main_block) {
		ir_global(&cg->ir, "mainCRTStartup");
		ir_label_named(&cg->ir, "mainCRTStartup");
		
		/* 1. ?????Mira ?????(R12) - ??????R12 ??????7???????????? */
		/* ?????????mira_main ?????????????? */
		
		/* 2. Windows x64 ABI: ??? 32 ??? shadow space????????(16B alignment) */
		ir_sub_reg_imm(&cg->ir, REG_RSP, 40);
		
		/* 3. ??? mira_main */
		ir_call_extern(&cg->ir, "mira_main");
		
		/* A PE entry point is not an ordinary caller-owned function.  Returning
		 * from it leaves process termination dependent on undocumented loader
		 * state and has been observed to print the result but keep the process
		 * alive.  ExitProcess is already a declared kernel32 import; preserve
		 * the allocated shadow space and terminate with a deterministic zero
		 * status in RCX, as required by the Win64 ABI. */
		ir_mov_reg_imm(&cg->ir, REG_RCX, 0);
		ir_call_extern(&cg->ir, "ExitProcess");
	}

	/* Numeric SSA block ids are function-local; the encoder patch table is
	 * module-wide, so make them unique only after every function is emitted. */
	ir_opt_uniquify_labels(&cg->ir);
	extern void ir_opt_remove_fallthrough_branches(IrBuffer *ir);
	ir_opt_remove_fallthrough_branches(&cg->ir);
	if (decision_plan.allow_float_optimization) {
		extern void ir_opt_hoist_loop_scratch_constants(IrBuffer *ir);
		ir_opt_hoist_loop_scratch_constants(&cg->ir);
		ir_opt_hoist_loop_scratch_constants(&cg->ir);
		extern void ir_opt_scalar_fp_chains(IrBuffer *ir);
		ir_opt_scalar_fp_chains(&cg->ir);
		extern void ir_opt_promote_scalar_fp_loop_state(IrBuffer *ir);
		ir_opt_promote_scalar_fp_loop_state(&cg->ir);
		extern void ir_opt_hoist_scalar_fp_constants(IrBuffer *ir);
		ir_opt_hoist_scalar_fp_constants(&cg->ir);
	}
	if (decision_plan.allow_scalar_loop_optimization) {
		/* Integer recurrences also materialize wide constants in scratch
		 * registers.  Hoist those loads before recurrence coalescing so the
		 * state-copy/multiply/update chain is adjacent and can be proved
		 * locally. */
		extern void ir_opt_hoist_loop_scratch_constants(IrBuffer *ir);
		ir_opt_hoist_loop_scratch_constants(&cg->ir);
		if (decision_plan.allow_affine_recurrence) {
		extern void ir_opt_coalesce_affine_recurrences(IrBuffer *ir);
		ir_opt_coalesce_affine_recurrences(&cg->ir);
		}
		if (decision_plan.allow_magic_division) {
		extern void ir_opt_hoist_repeated_wide_magic(IrBuffer *ir);
		ir_opt_hoist_repeated_wide_magic(&cg->ir);
		extern void ir_opt_unsigned_magic_from_constant_loop(IrBuffer *ir);
		ir_opt_unsigned_magic_from_constant_loop(&cg->ir);
		extern void ir_opt_schedule_dual_magic_loop(IrBuffer *ir);
		ir_opt_schedule_dual_magic_loop(&cg->ir);
		}
		if (decision_plan.allow_loop_rotation) {
		extern void ir_opt_rotate_canonical_loop_test(IrBuffer *ir);
		ir_opt_rotate_canonical_loop_test(&cg->ir);
		}
	}
	if (decision_plan.allow_memory_optimization) {
		extern void ir_opt_sink_cold_range_reset(IrBuffer *ir);
		ir_opt_sink_cold_range_reset(&cg->ir);
	}
	if (decision_plan.allow_vectorize || decision_plan.allow_if_conversion) {
		if (decision_plan.allow_vectorize) {
		extern void ir_opt_vectorize_i64_reductions(IrBuffer *ir);
		ir_opt_vectorize_i64_reductions(&cg->ir);
		}
		if (decision_plan.allow_if_conversion) {
		extern void ir_opt_if_convert_small_diamonds(IrBuffer *ir);
		ir_opt_if_convert_small_diamonds(&cg->ir);
		}
		extern void ir_opt_forward_sign_flags(IrBuffer *ir);
		ir_opt_forward_sign_flags(&cg->ir);
	}
	if (mira_opt_level >= 3)
		ir_opt_align_loop_headers(&cg->ir);
}

