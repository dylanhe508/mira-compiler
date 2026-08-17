#ifndef MIRA_CLI_H
#define MIRA_CLI_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    MIRA_COMMAND_COMPILE,
    MIRA_COMMAND_LINK,
    MIRA_COMMAND_NEW,
    MIRA_COMMAND_HELP,
    MIRA_COMMAND_VERSION
} MiraCommand;

typedef enum {
    MIRA_EMIT_EXE,
    MIRA_EMIT_ASM,
    MIRA_EMIT_IR,
    MIRA_EMIT_OBJ
} MiraEmitKind;

typedef struct {
    MiraCommand command;
    MiraEmitKind emit;
    const char *input;
    const char *output;
    const char *project_name;
    const char *march;
    const char *target;
    const char *link_inputs[64];
    int link_input_count;
    int opt_level;
    int avx2_override;
} MiraCliOptions;

bool mira_cli_parse(int argc, char **argv, MiraCliOptions *out,
                    char *error, size_t error_size);

#endif
