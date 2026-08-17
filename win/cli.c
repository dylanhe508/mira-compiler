#include "cli.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static bool cli_error(char *error, size_t error_size, const char *format, ...) {
    if (error && error_size) {
        va_list args;
        va_start(args, format);
        vsnprintf(error, error_size, format, args);
        va_end(args);
    }
    return false;
}

static bool set_emit(MiraCliOptions *out, MiraEmitKind emit, bool *emit_seen,
                     char *error, size_t error_size) {
    if (*emit_seen && out->emit != emit)
        return cli_error(error, error_size, "conflicting emit modes");
    out->emit = emit;
    *emit_seen = true;
    return true;
}

static bool set_command(MiraCliOptions *out, MiraCommand command,
                        bool *command_seen, char *error, size_t error_size) {
    if (*command_seen && out->command != command)
        return cli_error(error, error_size, "conflicting commands");
    out->command = command;
    *command_seen = true;
    return true;
}

bool mira_cli_parse(int argc, char **argv, MiraCliOptions *out,
                    char *error, size_t error_size) {
    if (!out) return cli_error(error, error_size, "internal CLI error");
    *out = (MiraCliOptions){
        .command = MIRA_COMMAND_COMPILE,
        .emit = MIRA_EMIT_EXE,
        .opt_level = 2,
        .avx2_override = -1
    };
    if (error && error_size) error[0] = '\0';

    bool emit_seen = false;
    bool command_seen = false;
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];

        if (strcmp(arg, "-S") == 0) {
            if (!set_emit(out, MIRA_EMIT_ASM, &emit_seen, error, error_size)) return false;
        } else if (strcmp(arg, "-c") == 0) {
            if (!set_emit(out, MIRA_EMIT_OBJ, &emit_seen, error, error_size)) return false;
        } else if (strncmp(arg, "--emit=", 7) == 0) {
            const char *mode = arg + 7;
            MiraEmitKind emit;
            if (strcmp(mode, "asm") == 0) emit = MIRA_EMIT_ASM;
            else if (strcmp(mode, "ir") == 0) emit = MIRA_EMIT_IR;
            else if (strcmp(mode, "obj") == 0) emit = MIRA_EMIT_OBJ;
            else return cli_error(error, error_size, "unknown emit mode '%s'", mode);
            if (!set_emit(out, emit, &emit_seen, error, error_size)) return false;
        } else if (strcmp(arg, "-o") == 0) {
            if (++i >= argc)
                return cli_error(error, error_size, "option '-o' requires a value");
            if (out->output)
                return cli_error(error, error_size, "option '-o' specified more than once");
            out->output = argv[i];
        } else if (arg[0] == '-' && arg[1] == 'O' &&
                   arg[2] >= '0' && arg[2] <= '3' && arg[3] == '\0') {
            out->opt_level = arg[2] - '0';
        } else if (strncmp(arg, "-march=", 7) == 0) {
            if (!arg[7]) return cli_error(error, error_size, "option '-march' requires a value");
            out->march = arg + 7;
        } else if (strcmp(arg, "-mavx2") == 0) {
            out->avx2_override = 1;
        } else if (strcmp(arg, "-mno-avx2") == 0) {
            out->avx2_override = 0;
        } else if (strncmp(arg, "--target=", 9) == 0) {
            if (!arg[9]) return cli_error(error, error_size, "option '--target' requires a value");
            out->target = arg + 9;
        } else if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0 ||
                   strcmp(arg, "-help") == 0) {
            if (!set_command(out, MIRA_COMMAND_HELP, &command_seen, error, error_size)) return false;
        } else if (strcmp(arg, "-v") == 0 || strcmp(arg, "--version") == 0 ||
                   strcmp(arg, "-version") == 0) {
            if (!set_command(out, MIRA_COMMAND_VERSION, &command_seen, error, error_size)) return false;
        } else if (strcmp(arg, "-n") == 0 || strcmp(arg, "--new") == 0) {
            if (!set_command(out, MIRA_COMMAND_NEW, &command_seen, error, error_size)) return false;
            if (++i >= argc)
                return cli_error(error, error_size, "option '%s' requires a value", arg);
            out->project_name = argv[i];
        } else if (strcmp(arg, "-l") == 0) {
            if (!set_command(out, MIRA_COMMAND_LINK, &command_seen, error, error_size)) return false;
        } else if (strcmp(arg, "-i") == 0 || strcmp(arg, "--dump-asm") == 0) {
            return cli_error(error, error_size, "unsupported option '%s'", arg);
        } else if (arg[0] == '-') {
            return cli_error(error, error_size, "unknown option '%s'", arg);
        } else if (out->command == MIRA_COMMAND_LINK) {
            if (out->link_input_count == 64)
                return cli_error(error, error_size, "too many link input files");
            out->link_inputs[out->link_input_count++] = arg;
        } else if (out->command == MIRA_COMMAND_NEW) {
            return cli_error(error, error_size, "unexpected argument '%s'", arg);
        } else if (!out->input) {
            out->input = arg;
        } else {
            return cli_error(error, error_size, "multiple input files");
        }
    }

    if (out->command == MIRA_COMMAND_HELP || out->command == MIRA_COMMAND_VERSION)
        return true;
    if (out->command == MIRA_COMMAND_NEW)
        return out->project_name != NULL;
    if (out->command == MIRA_COMMAND_LINK) {
        if (!out->link_input_count)
            return cli_error(error, error_size, "no link input files");
        return true;
    }
    if (!out->input) return cli_error(error, error_size, "no input file");
    return true;
}
