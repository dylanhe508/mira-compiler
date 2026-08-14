#include "ir_ssa.h"
#include <stdlib.h>
#include <string.h>

void ssa_ref_vm_trace_init(SsaRefVmTrace *trace, size_t event_budget) {
	if (!trace) return;
	memset(trace, 0, sizeof(*trace));
	trace->event_budget = event_budget ? event_budget : 4096;
}

void ssa_ref_vm_trace_free(SsaRefVmTrace *trace) {
	if (!trace) return;
	free(trace->events);
	memset(trace, 0, sizeof(*trace));
}

bool ssa_ref_vm_trace_event(SsaRefVmTrace *trace, SsaRefVmEvent event) {
	if (!trace || trace->overflowed) return false;
	if (trace->event_count >= trace->event_budget) {
		trace->overflowed = true;
		return false;
	}
	if (trace->event_count >= trace->event_cap) {
		size_t next_cap = trace->event_cap ? trace->event_cap * 2 : 64;
		if (next_cap > trace->event_budget) next_cap = trace->event_budget;
		SsaRefVmEvent *next = realloc(trace->events, next_cap * sizeof(*next));
		if (!next) { trace->overflowed = true; return false; }
		trace->events = next;
		trace->event_cap = next_cap;
	}
	trace->events[trace->event_count++] = event;
	return true;
}

bool ssa_ref_apply_vm_trace(SsaModule *mod, const SsaRefVmTrace *trace) {
	(void)mod;
	if (!trace || trace->overflowed) return false;
	if (trace->compile_time_proven && !trace->runtime_dependent) return true;
	if (!trace->runtime_dependent) return false;
	/* Runtime observations are never proofs by themselves.  Static Reference
	 * may consume them only when every assumption has a guard and the original
	 * high-level program remains available as a complete fallback. */
	return trace->guard_count > 0 && trace->guards_complete && trace->fallback_complete;
}
