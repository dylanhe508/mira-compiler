/* rt_sync.h - 同步原语抽象(Windows SRW/条件变量 ↔ POSIX mutex/condvar)
 *
 * 使用方:rt_sched_posix.c / rt_channel_posix.c(POSIX 移植版)。
 * Windows 侧原实现(rt_sched.c / rt_channel.c)保持原生 API 不动,
 * 本抽象只服务 POSIX;未来如需统一可再抽 rt_sync_win.c。
 *
 * 语义对齐:
 *   - 锁为非递归互斥(SRW 同为非递归)
 *   - 条件变量在 POSIX 上绑定 CLOCK_MONOTONIC,避免系统时间跳变
 *     (Windows 的 SleepConditionVariableSRW 是相对毫秒,无此问题)
 *   - 事件为手动复位(等价 CreateEventA(NULL, TRUE, ...))
 */
#ifndef MIRA_RT_SYNC_H
#define MIRA_RT_SYNC_H

#ifdef _WIN32
#include <windows.h>
typedef SRWLOCK mira_lock_t;
typedef CONDITION_VARIABLE mira_cond_t;
#define mira_lock_init(l)      InitializeSRWLock(l)
#define mira_lock_acquire(l)   AcquireSRWLockExclusive(l)
#define mira_lock_release(l)   ReleaseSRWLockExclusive(l)
#define mira_cond_init(c)      InitializeConditionVariable(c)
#define mira_cond_signal(c)    WakeConditionVariable(c)
#define mira_cond_broadcast(c) WakeAllConditionVariable(c)
#define mira_cond_wait(c, l)   SleepConditionVariableSRW((c), (l), INFINITE, 0)
#define mira_cond_timedwait_ms(c, l, ms) \
    SleepConditionVariableSRW((c), (l), (DWORD)(ms), 0)
#else
#include <pthread.h>
typedef pthread_mutex_t mira_lock_t;
typedef pthread_cond_t  mira_cond_t;
#define mira_lock_init(l)      pthread_mutex_init((l), NULL)
#define mira_lock_acquire(l)   pthread_mutex_lock(l)
#define mira_lock_release(l)   pthread_mutex_unlock(l)
void mira_cond_init(mira_cond_t *c);          /* CLOCK_MONOTONIC */
#define mira_cond_signal(c)    pthread_cond_signal(c)
#define mira_cond_broadcast(c) pthread_cond_broadcast(c)
#define mira_cond_wait(c, l)   pthread_cond_wait((c), (l))
void mira_cond_timedwait_ms(mira_cond_t *c, mira_lock_t *l, long ms);
#endif

/* 手动复位事件(等价 CreateEventA(manual-reset) + Set/Reset/Wait) */
typedef struct mira_event mira_event_t;
mira_event_t *mira_event_create(int initially_set);
void          mira_event_destroy(mira_event_t *e);
void          mira_event_set(mira_event_t *e);
void          mira_event_reset(mira_event_t *e);
void          mira_event_wait(mira_event_t *e);

#endif /* MIRA_RT_SYNC_H */
