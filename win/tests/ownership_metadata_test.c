#include "../mira.h"

#include <stdio.h>

void parser_do_import(const char *path, const char *alias, int is_lib) {
    (void)path;
    (void)alias;
    (void)is_lib;
}

static Def *find_def(Program *program, const char *name) {
    size_t len = strlen(name);
    for (Def *def = program->defs; def; def = def->next)
        if (def->name_len == len && memcmp(def->name, name, len) == 0)
            return def;
    return NULL;
}

static IrNode *find_kind(IrNode *node, IrKind kind) {
    for (; node; node = node->next) {
        if (node->kind == kind) return node;
        IrNode *nested = NULL;
        switch (node->kind) {
        case IR_BLOCK: nested = find_kind(node->u.block, kind); break;
        case IR_IF:
            nested = find_kind(node->u.iff.cond, kind);
            if (!nested) nested = find_kind(node->u.iff.then_b, kind);
            if (!nested) nested = find_kind(node->u.iff.else_b, kind);
            break;
        default: break;
        }
        if (nested) return nested;
    }
    return NULL;
}

static IrNode *find_word(IrNode *node, const char *name) {
    size_t len = strlen(name);
    for (; node; node = node->next) {
        if (node->kind == IR_WORD && node->u.word.len == len &&
            memcmp(node->u.word.name, name, len) == 0)
            return node;
        IrNode *nested = NULL;
        switch (node->kind) {
        case IR_BLOCK: nested = find_word(node->u.block, name); break;
        case IR_IF:
            nested = find_word(node->u.iff.cond, name);
            if (!nested) nested = find_word(node->u.iff.then_b, name);
            if (!nested) nested = find_word(node->u.iff.else_b, name);
            break;
        default: break;
        }
        if (nested) return nested;
    }
    return NULL;
}

static void expect_ownership(const IrNode *node, MiraOwnership expected,
                             const char *label) {
    if (!node || !node->ownership_checked ||
        node->checked_ownership != expected) {
        fprintf(stderr, "%s ownership mismatch: checked=%u actual=%d expected=%d\n",
            label, node ? node->ownership_checked : 0,
            node ? (int)node->checked_ownership : -1, (int)expected);
        exit(1);
    }
}

int main(void) {
    char source[] =
        "extern fn hold(value: str) -> void;\n"
        "fn make() -> str { \"mi\" \"ra\" str-cat }\n"
        "fn choose(flag: bool) -> str { if (flag) { \"o\" \"k\" str-cat } else { \"borrowed\" } }\n"
        "fn observe(value: str) -> i64 { 1 }\n"
        "fn pass_unknown(value: str) -> void { hold(value); }\n"
        "fn pass_safe(value: str) -> i64 { observe(value) }\n"
        "fn return_param(value: str) -> str { value }\n"
        "fn forward() -> str { later() }\n"
        "fn later() -> str { \"la\" \"ter\" str-cat }\n"
        "fn main() -> void { print(choose(true)); }\n";

    Compiler compiler = {0};
    compiler.src = source;
    compiler.filename = "<ownership-metadata-test>";
    Program *program = parser_parse(&compiler);
    mira_typecheck_program(&compiler, program);

    Def *make = find_def(program, "make");
    Def *choose = find_def(program, "choose");
    Def *observe = find_def(program, "observe");
    Def *return_param = find_def(program, "return_param");
    Def *forward = find_def(program, "forward");
    Def *pass_unknown = find_def(program, "pass_unknown");
    Def *pass_safe = find_def(program, "pass_safe");
    if (!make || !choose || !observe || !return_param || !forward ||
        !pass_unknown || !pass_safe) return 2;

    IrNode *literal = find_kind(make->body, IR_STR);
    IrNode *owned_call = find_word(make->body, "str-cat");
    IrNode *mixed_if = find_kind(choose->body, IR_IF);

    expect_ownership(literal, MIRA_OWNERSHIP_BORROWED, "literal");
    expect_ownership(owned_call, MIRA_OWNERSHIP_OWNED, "allocating call");
    expect_ownership(mixed_if, MIRA_OWNERSHIP_MAYBE_OWNED, "mixed if");

    if (!make->ownership_checked ||
        make->return_ownership != MIRA_OWNERSHIP_OWNED ||
        !make->return_free_func_name)
        return 3;
    if (!observe->ownership_checked || !observe->param_may_escape ||
        observe->param_may_escape[0])
        return 4;
    if (!return_param->param_may_escape || !return_param->param_may_escape[0])
        return 5;
    if (forward->return_ownership != MIRA_OWNERSHIP_OWNED ||
        !forward->return_free_func_name)
        return 6;
    if (!pass_unknown->param_may_escape || !pass_unknown->param_may_escape[0])
        return 7;
    if (!pass_safe->param_may_escape || pass_safe->param_may_escape[0])
        return 8;

    puts("OWNERSHIP METADATA PASS");
    program_free(program);
    return 0;
}
