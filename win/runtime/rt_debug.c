/* rt_debug.c - Debug/dump functions */
#include <stdlib.h>
#include <stdio.h>

extern long long mira_var_count;
extern long long mira_vars[];
extern const char *mira_var_names[];
extern long long g_alloc_count;
extern long long g_alloc_bytes;

void mira_dump_data_stack(void *base, void *sp) {
	long long depth = 0;
	if (sp && base && sp >= base) depth = ((char *)sp - (char *)base) / 8;
	printf("=== data stack (depth=%lld) ===\n", depth);
	long long *v = (long long *)base;
	for (long long i = 0; i < depth; i++)
		printf("[%3lld] %lld (0x%llx)\n", i, v[i], (unsigned long long)v[i]);
}

void mira_dump_var_slot(int slot) {
	if (slot < 0 || (long long)slot >= mira_var_count) {
		printf("var[%d]: <invalid slot>\n", slot); return;
	}
	long long *vars = mira_vars;
	long long value = vars[slot];
	const char *name = mira_var_names ? mira_var_names[slot] : "?";
	printf("var %s [slot %d] @%p = %lld (0x%llx)\n",
	       name, slot, (void *)&vars[slot], value, (unsigned long long)value);
}

void mira_dump_vars(void) {
	printf("=== vars (count=%lld) ===\n", mira_var_count);
	for (int i = 0; (long long)i < mira_var_count; i++) mira_dump_var_slot(i);
}

void mira_stats(void) {
	printf("=== .stats ===\n");
	printf("alloc_count  = %lld\n", g_alloc_count);
	printf("alloc_bytes  = %lld\n", g_alloc_bytes);
	printf("var_count    = %lld\n", mira_var_count);
}

void mira_debug_break(void)    { printf("[break] breakpoint hit (non-blocking)\n"); }
void mira_debug_step(void)     { printf("[step] reached step point (non-blocking)\n"); }
void mira_debug_next(void)     { printf("[next] reached next point (non-blocking)\n"); }
void mira_debug_continue(void) { printf("[continue]\n"); }
void mira_dump_return_stack(void) { printf("[.r] direct CPU call stack, no separate return stack.\n"); }
void mira_dump_type_stack(void) { printf("[.t] type info erased at runtime.\n"); }
void mira_backtrace(void)      { printf("[.backtrace] not implemented, use debugger.\n"); }
void mira_where(void)          { printf("[.where] no source location info.\n"); }
void mira_watch_not_supported(void) { printf("[watch] not implemented.\n"); }
