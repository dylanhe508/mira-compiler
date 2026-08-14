/* rt_sync_posix.c - POSIX 同步原语实现(条件变量 + 手动复位事件) */
#include "rt_sync.h"
#include <stdlib.h>
#include <time.h>

/* 条件变量绑定 CLOCK_MONOTONIC:timedwait 用相对毫秒换算绝对时间,
 * 不受系统时钟跳变影响(对齐 Windows 相对毫秒语义)。 */
void mira_cond_init(mira_cond_t *c) {
    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    pthread_cond_init(c, &attr);
    pthread_condattr_destroy(&attr);
}

void mira_cond_timedwait_ms(mira_cond_t *c, mira_lock_t *l, long ms) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    ts.tv_sec += ms / 1000;
    ts.tv_nsec += (long)(ms % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000L;
    }
    pthread_cond_timedwait(c, l, &ts);
}

struct mira_event {
    mira_lock_t lock;
    mira_cond_t cond;
    int state; /* 手动复位:set 置 1,reset 清 0 */
};

mira_event_t *mira_event_create(int initially_set) {
    mira_event_t *e = (mira_event_t *)calloc(1, sizeof(mira_event_t));
    if (!e) return NULL;
    mira_lock_init(&e->lock);
    mira_cond_init(&e->cond);
    e->state = initially_set ? 1 : 0;
    return e;
}

void mira_event_destroy(mira_event_t *e) {
    if (!e) return;
    pthread_cond_destroy(&e->cond);
    pthread_mutex_destroy(&e->lock);
    free(e);
}

void mira_event_set(mira_event_t *e) {
    if (!e) return;
    mira_lock_acquire(&e->lock);
    e->state = 1;
    mira_cond_broadcast(&e->cond);
    mira_lock_release(&e->lock);
}

void mira_event_reset(mira_event_t *e) {
    if (!e) return;
    mira_lock_acquire(&e->lock);
    e->state = 0;
    mira_lock_release(&e->lock);
}

void mira_event_wait(mira_event_t *e) {
    if (!e) return;
    mira_lock_acquire(&e->lock);
    while (!e->state)
        mira_cond_wait(&e->cond, &e->lock);
    mira_lock_release(&e->lock);
}
