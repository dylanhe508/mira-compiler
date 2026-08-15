#include "../mira.h"

#include <stdlib.h>

static void *expected_frees[5];
static unsigned char freed[5];

static void tracked_free(void *ptr) {
	for (int i = 0; i < 5; ++i)
		if (ptr == expected_frees[i]) freed[i] = 1;
	free(ptr);
}

#define free tracked_free
#include "../memory.c"
#undef free

int main(void) {
	Program *program = calloc(1, sizeof(*program));
	program->var_types = malloc(sizeof(*program->var_types));
	program->var_type_explicit = malloc(sizeof(*program->var_type_explicit));
	program->const_types = malloc(sizeof(*program->const_types));
	program->const_type_explicit = malloc(sizeof(*program->const_type_explicit));
	program->const_origins = malloc(sizeof(*program->const_origins));
	expected_frees[0] = program->var_types;
	expected_frees[1] = program->var_type_explicit;
	expected_frees[2] = program->const_types;
	expected_frees[3] = program->const_type_explicit;
	expected_frees[4] = program->const_origins;

	program_free(program);
	for (int i = 0; i < 5; ++i)
		if (!freed[i]) return i + 1;
	return 0;
}
