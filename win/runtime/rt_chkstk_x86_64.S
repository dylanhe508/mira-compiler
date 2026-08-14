/* MinGW/MS x64 stack probe helper.
 *
 * RAX contains the requested stack allocation size. Probe every crossed
 * guard page without changing the caller's stack pointer or the value in RAX;
 * the caller's generated prologue performs the actual allocation. */

    .text
    .globl ___chkstk_ms
    .def ___chkstk_ms; .scl 2; .type 32; .endef
___chkstk_ms:
    pushq %rcx
    pushq %rax
    cmpq $4096, %rax
    leaq 24(%rsp), %rcx
    jb 2f
1:
    subq $4096, %rcx
    orq $0, (%rcx)
    subq $4096, %rax
    cmpq $4096, %rax
    ja 1b
2:
    subq %rax, %rcx
    orq $0, (%rcx)
    popq %rax
    popq %rcx
    ret
