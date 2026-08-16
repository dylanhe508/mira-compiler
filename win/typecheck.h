#ifndef MIRA_TYPECHECK_H
#define MIRA_TYPECHECK_H

#include <stdbool.h>
#include <stddef.h>

typedef enum MiraType {
	MIRA_TYPE_UNKNOWN = 0,
	MIRA_TYPE_I64,
	MIRA_TYPE_F64,
	MIRA_TYPE_BOOL,
	MIRA_TYPE_STR,
	MIRA_TYPE_PTR,
	MIRA_TYPE_VOID
} MiraType;

typedef enum MiraOwnership {
	MIRA_OWNERSHIP_UNKNOWN = 0,
	MIRA_OWNERSHIP_BORROWED,
	MIRA_OWNERSHIP_OWNED,
	MIRA_OWNERSHIP_MAYBE_OWNED,
	MIRA_OWNERSHIP_ESCAPED
} MiraOwnership;

static inline bool mira_type_is_known(MiraType type) {
	return type != MIRA_TYPE_UNKNOWN;
}

struct Compiler;
struct Program;
struct Def;

bool mira_type_from_name(const char *name, size_t len, MiraType *out);
const char *mira_type_name(MiraType type);
struct Def *mira_find_signature(struct Program *program,
	const char *name, size_t name_len);
void mira_typecheck_program(struct Compiler *compiler, struct Program *program);

#endif
