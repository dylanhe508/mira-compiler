#include "cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_ARGS = 80, COMMAND_SIZE = 512 };

static int split_command(const char *command, char storage[COMMAND_SIZE], char *argv[MAX_ARGS]) {
    size_t length = strlen(command);
    if (length >= COMMAND_SIZE) {
        fprintf(stderr, "command too long: %s\n", command);
        exit(1);
    }
    memcpy(storage, command, length + 1);
    int argc = 0;
    char *cursor = storage;
    while (*cursor) {
        while (*cursor == ' ') cursor++;
        if (!*cursor) break;
        if (argc == MAX_ARGS) {
            fprintf(stderr, "too many arguments: %s\n", command);
            exit(1);
        }
        argv[argc++] = cursor;
        while (*cursor && *cursor != ' ') cursor++;
        if (*cursor) *cursor++ = '\0';
    }
    return argc;
}

static MiraCliOptions parse_success(const char *command, char storage[COMMAND_SIZE]) {
    char *argv[MAX_ARGS];
    char error[256] = {0};
    int argc = split_command(command, storage, argv);
    MiraCliOptions options;
    if (!mira_cli_parse(argc, argv, &options, error, sizeof(error))) {
        fprintf(stderr, "expected success for '%s', got: %s\n", command, error);
        exit(1);
    }
    return options;
}

static void expect_compile(const char *command, MiraEmitKind emit,
                           const char *input, const char *output, int opt_level) {
    char storage[COMMAND_SIZE];
    MiraCliOptions options = parse_success(command, storage);
    if (options.command != MIRA_COMMAND_COMPILE || options.emit != emit ||
        !options.input || strcmp(options.input, input) != 0 ||
        ((output == NULL) != (options.output == NULL)) ||
        (output && strcmp(options.output, output) != 0) ||
        options.opt_level != opt_level) {
        fprintf(stderr, "parsed values differ for '%s'\n", command);
        exit(1);
    }
}

static void expect_error(const char *command, const char *expected) {
    char storage[COMMAND_SIZE];
    char *argv[MAX_ARGS];
    char error[256] = {0};
    int argc = split_command(command, storage, argv);
    MiraCliOptions options;
    if (mira_cli_parse(argc, argv, &options, error, sizeof(error))) {
        fprintf(stderr, "expected failure for '%s'\n", command);
        exit(1);
    }
    if (strcmp(error, expected) != 0) {
        fprintf(stderr, "wrong error for '%s': expected '%s', got '%s'\n",
                command, expected, error);
        exit(1);
    }
}

int main(void) {
    expect_compile("mira -S a.mira", MIRA_EMIT_ASM, "a.mira", NULL, 2);
    expect_compile("mira --emit=asm a.mira", MIRA_EMIT_ASM, "a.mira", NULL, 2);
    expect_compile("mira --emit=ir a.mira -o a.ir -O3", MIRA_EMIT_IR,
                   "a.mira", "a.ir", 3);
    expect_compile("mira -c -O0 a.mira", MIRA_EMIT_OBJ, "a.mira", NULL, 0);
    expect_compile("mira --emit=obj a.mira -o a.obj", MIRA_EMIT_OBJ,
                   "a.mira", "a.obj", 2);
    expect_compile("mira a.mira -o app.exe", MIRA_EMIT_EXE,
                   "a.mira", "app.exe", 2);

    char help_storage[COMMAND_SIZE], version_storage[COMMAND_SIZE];
    char project_storage[COMMAND_SIZE], link_storage[COMMAND_SIZE];
    MiraCliOptions help = parse_success("mira --help", help_storage);
    MiraCliOptions version = parse_success("mira -O3 --version", version_storage);
    MiraCliOptions project = parse_success("mira --new demo", project_storage);
    MiraCliOptions link = parse_success("mira -l a.obj b.obj -o app.exe", link_storage);
    if (help.command != MIRA_COMMAND_HELP || version.command != MIRA_COMMAND_VERSION ||
        version.opt_level != 3 || project.command != MIRA_COMMAND_NEW ||
        strcmp(project.project_name, "demo") != 0 ||
        link.command != MIRA_COMMAND_LINK || link.link_input_count != 2 ||
        strcmp(link.output, "app.exe") != 0) {
        fprintf(stderr, "non-compile command parsing mismatch\n");
        return 1;
    }

    expect_error("mira --emit=wat a.mira", "unknown emit mode 'wat'");
    expect_error("mira -S -c a.mira", "conflicting emit modes");
    expect_error("mira -o", "option '-o' requires a value");
    expect_error("mira a.mira b.mira", "multiple input files");
    expect_error("mira --mystery a.mira", "unknown option '--mystery'");
    expect_error("mira", "no input file");

    puts("CLI PARSE PASS");
    return 0;
}
