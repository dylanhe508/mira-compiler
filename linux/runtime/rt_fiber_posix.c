/* rt_fiber_posix.c - fiber 创建/销毁(C 部分)
 *
 * 切换核心(mira_fiber_switch / mira_fiber_trampoline)在
 * rt_fiber_x86_64.S(纯汇编,不依赖任何 C 运行时)。
 *
 * 上下文布局(与汇编的字段偏移严格对应):
 *   0x00 sp          保存的 RSP(未保存过为 0)
 *   0x08 entry       首次切入时的入口函数
 *   0x10 arg         入口参数
 *   0x18 stack_base  malloc 的栈底(destroy 时释放)
 *   0x20 stack_size  栈大小(16 对齐)
 */
#include "rt_fiber.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct mira_fiber_ctx {
    void *sp;
    void (*entry)(void *);
    void *arg;
    void *stack_base;
    size_t stack_size;
};

/* 汇编定义(rt_fiber_x86_64.S) */
extern void mira_fiber_switch(mira_fiber_ctx *current, mira_fiber_ctx *target);
extern void mira_fiber_trampoline(void);

mira_fiber_ctx *mira_fiber_convert_thread(void) {
    /* sp = 0 表示"从未保存过";首次作为 switch 的 current 时,
     * 汇编直接把线程现场写入。与 ConvertThreadToFiber 语义一致。 */
    return (mira_fiber_ctx *)calloc(1, sizeof(mira_fiber_ctx));
}

mira_fiber_ctx *mira_fiber_create(size_t stack_size,
                                  void (*entry)(void *), void *arg) {
    if (!entry) return NULL;
    if (stack_size < 4096) stack_size = 4096;
    stack_size = (stack_size + 15) & ~(size_t)15;

    void *base = malloc(stack_size);
    if (!base) return NULL;
    mira_fiber_ctx *ctx = (mira_fiber_ctx *)calloc(1, sizeof(mira_fiber_ctx));
    if (!ctx) {
        free(base);
        return NULL;
    }

    void *top = (char *)base + stack_size;
    /* 伪帧布置(从高到低):
     *   [top-8]   trampoline 地址 —— switch 恢复流程 ret 至此
     *   [top-56..top-16] 48 字节"寄存器槽"(pop 到 r15..rbp,内容任意)
     * 恢复时 rsp=top-56 → pop×6 → ret,rsp 回到 top(16 对齐),
     * trampoline 内 call 前满足"call 前 rsp%16==0"。 */
    *(uintptr_t *)((char *)top - 8) = (uintptr_t)&mira_fiber_trampoline;
    ctx->sp = (char *)top - 8 - 48;
    ctx->entry = entry;
    ctx->arg = arg;
    ctx->stack_base = base;
    ctx->stack_size = stack_size;
    return ctx;
}

void mira_fiber_destroy(mira_fiber_ctx *ctx) {
    if (!ctx) return;
    free(ctx->stack_base);
    free(ctx);
}

/* 调试:打印 ctx 内部状态(MIRA_FIBER_TRACE=1 时启用)。
 * 用于定位 fiber switch 恢复目标损坏(rip=0 崩溃)。 */
void mira_fiber_dump(const char *tag, mira_fiber_ctx *ctx) {
    static int on = -1;
    if (on < 0) on = getenv("MIRA_FIBER_TRACE") != NULL;
    if (!on) return;
    if (!ctx) { fprintf(stderr, "[fiber] %s: NULL\n", tag); return; }
    fprintf(stderr, "[fiber] %s: ctx=%p sp=%p entry=%p arg=%p base=%p size=%zu\n",
            tag, (void *)ctx, ctx->sp, (void *)(uintptr_t)ctx->entry,
            ctx->arg, ctx->stack_base, ctx->stack_size);
}
