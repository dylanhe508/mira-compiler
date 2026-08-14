/* Mira 閸愬懎鐡ㄧ粻锛勬倞閿涙ST ??Program 闁插﹥鏂?*/
#include "mira.h"
#include <stdlib.h>
#include <string.h>

/* --- Arena Allocator --- */
#define ARENA_BLOCK_SIZE (1024 * 1024) /* 1MB blocks */

void *arena_alloc(Arena *a, size_t size) {
	/* Align size to 8 bytes */
	size = (size + 7) & ~7;
	if (!a->head || a->head->used + size > a->head->size) {
		size_t alloc_size = size > ARENA_BLOCK_SIZE ? size : ARENA_BLOCK_SIZE;
		ArenaBlock *nb = malloc(sizeof(ArenaBlock) + alloc_size);
		if (!nb) { fprintf(stderr, "arena_alloc: out of memory\n"); exit(1); }
		nb->size = alloc_size;
		nb->used = size;
		nb->next = a->head;
		a->head = nb;
		return nb->data;
	}
	void *ptr = a->head->data + a->head->used;
	a->head->used += size;
	return ptr;
}

void arena_free(Arena *a) {
	ArenaBlock *curr = a->head;
	while (curr) {
		ArenaBlock *next = curr->next;
		free(curr);
		curr = next;
	}
	a->head = NULL;
}

/* 闂侇偅甯掔紞濠囨煂婵犲啯�?IrNode 闂佺偓宕橀�?*/
/* IR nodes are now managed by Arena Allocator, so these are no-ops. */
void IR_free(IrNode *list) { }
static void def_free(Def *list) { }
static void pragma_free(Pragma *list) { }

void program_free(Program *prog) {
	if (!prog) return;
	
	arena_free(&prog->ir_arena);

	if (prog->var_names) free(prog->var_names);
	if (prog->var_lens) free(prog->var_lens);
	
	if (prog->const_names) free(prog->const_names);
	if (prog->const_lens) free(prog->const_lens);
	if (prog->const_ints) free(prog->const_ints);
	if (prog->const_doubles) free(prog->const_doubles);
	if (prog->const_strs) free(prog->const_strs);
	if (prog->const_str_lens) free(prog->const_str_lens);
	if (prog->const_kinds) free(prog->const_kinds);
	
	if (prog->structs) free(prog->structs);
	
	free(prog);
}
