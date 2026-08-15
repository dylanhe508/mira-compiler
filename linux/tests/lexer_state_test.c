#include "../mira.h"

int main(void) {
    char source[] = "parent next";
    Compiler compiler = {0};
    compiler.src = source;
    compiler.filename = "<lexer-state-parent>";
    lexer_init(&compiler);

    const char *parent_line_start = compiler.line_start;
    if (!lexer_push_file("lexer_state_child.mira", NULL)) return 1;
    if (compiler.line_start != compiler.src) return 2;

    lexer_pop_file();
    if (compiler.line_start != parent_line_start) return 3;
    return 0;
}
