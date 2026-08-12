/* rt_fiber.h - 对称协程切换抽象(Windows Fiber 语义的 POSIX 实现)
 *
 * 接口对齐 Windows Fiber 的四个动作,供 rt_sched_posix.c 使用:
 *   ConvertThreadToFiber(NULL) → mira_fiber_convert_thread()
 *   CreateFiber(size, fn, arg) → mira_fiber_create(size, fn, arg)
 *   SwitchToFiber(target)      → mira_fiber_switch(current, target)
 *   DeleteFiber(ctx)           → mira_fiber_destroy(ctx)
 *
 * switch 显式携带 current:POSIX 没有线程级"当前 fiber"概念,
 * 调用方总是知道自己当前所在的上下文。实现(x86-64 汇编,
 * rt_fiber_x86_64.S):保存 callee-saved + RSP + RIP,恢复目标同集合。
 * 与 Windows SwitchToFiber 一致:浮点状态(含 MXCSR)不切换。
 *
 * 注意:ConvertFiberToThread 在 POSIX 无对应物(线程上下文随
 * pthread 退出自然消失),故接口不提供。
 */
#ifndef MIRA_RT_FIBER_H
#define MIRA_RT_FIBER_H

#include <stddef.h>

typedef struct mira_fiber_ctx mira_fiber_ctx;

/* 把当前线程转换为可切换上下文(等价 ConvertThreadToFiber(NULL))。
 * 返回的 ctx 首次作为 switch 的 current 时,保存线程现场。 */
mira_fiber_ctx *mira_fiber_convert_thread(void);

/* 创建新 fiber(等价 CreateFiber)。stack_size 小于 4KB 时按 4KB 处理。
 * 首次切换到该 fiber 时,执行 entry(arg);entry 正常返回则 abort。 */
mira_fiber_ctx *mira_fiber_create(size_t stack_size,
                                  void (*entry)(void *), void *arg);

/* 保存 current 现场,恢复 target 现场(等价 SwitchToFiber(target))。
 * current 与 target 均不得为 NULL。 */
void mira_fiber_switch(mira_fiber_ctx *current, mira_fiber_ctx *target);

/* 销毁 fiber 并释放其栈(等价 DeleteFiber)。不得在 fiber 自身
 * 上下文内销毁自己(与 Windows 限制一致)。 */
void mira_fiber_destroy(mira_fiber_ctx *ctx);

/* 调试:打印 ctx 状态(MIRA_FIBER_TRACE 环境变量开启)。 */
void mira_fiber_dump(const char *tag, mira_fiber_ctx *ctx);

#endif /* MIRA_RT_FIBER_H */
