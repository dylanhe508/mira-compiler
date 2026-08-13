/* rt_fiber_x86_64.S - SysV 对称协程切换核心
 *
 * 上下文布局(与 rt_fiber_posix.c 的 struct mira_fiber_ctx 严格对应):
 *   0x00 sp          保存的 RSP(未保存过为 0)
 *   0x08 entry       首次切入时的入口函数
 *   0x10 arg         入口参数
 *   0x18 stack_base
 *   0x20 stack_size
 *
 * 保存/恢复集合:callee-saved(RBP/RBX/R12-R15)+ RSP + RIP(ret 链)。
 * 与 Windows SwitchToFiber 一致:浮点状态(含 MXCSR)不切换。
 * volatile 寄存器(RAX/RCX/RDX/RSI/RDI/R8-R11)不保证跨 switch 保持。
 *
 * 栈对齐约定:进入 mira_fiber_switch 时 call 前 rsp%16==0。
 * 首次切入新 fiber 时,恢复流程 ret 落到 trampoline 时 rsp%16==0,
 * trampoline 内 call entry 前无需再调整(与 create 的伪帧布置配套)。
 */

    .text

/* void mira_fiber_switch(mira_fiber_ctx *current,   rdi */
/*                        mira_fiber_ctx *target     rsi) */
/* 保存 current 现场(6 个 callee-saved + rsp),恢复 target 现场。 */
    .globl  mira_fiber_switch
    .type   mira_fiber_switch, @function
mira_fiber_switch:
    pushq   %rbp
    pushq   %rbx
    pushq   %r12
    pushq   %r13
    pushq   %r14
    pushq   %r15
    movq    %rsp, 0(%rdi)      /* current->sp = rsp */
    movq    0(%rsi), %rsp      /* rsp = target->sp */
    popq    %r15
    popq    %r14
    popq    %r13
    popq    %r12
    popq    %rbx
    popq    %rbp
    ret
    .size   mira_fiber_switch, .-mira_fiber_switch

/* 首次切入新 fiber 的跳板。进入时 rsi 仍等于 target(ctx 指针,
 * switch 不破坏 rsi),rsp 已 16 对齐。
 *   rdi = ctx->arg
 *   call ctx->entry
 * task_fiber_entry 是死循环,正常永不返回;若返回则 abort。 */
    .globl  mira_fiber_trampoline
    .type   mira_fiber_trampoline, @function
mira_fiber_trampoline:
    movq    16(%rsi), %rdi     /* arg → 第一参数 */
    call    *8(%rsi)           /* entry(arg) */
    call    abort
    ud2
    .size   mira_fiber_trampoline, .-mira_fiber_trampoline

    .section .note.GNU-stack,"",@progbits
