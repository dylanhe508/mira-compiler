/* Linux 侧 100k fast task 基准(与 tests/concurrency_fast_compare.c 同构)。
 * 对比对象:同机 Windows 侧 Mira 中位数 ~37.99ms、Thread Pool ~20.23ms。 */
#include "rt_sched.h"
#include <stdio.h>
#include <time.h>

enum { TASK_COUNT = 100000 };

static volatile int completed;

static void task(void *unused) {
    (void)unused;
    __atomic_add_fetch(&completed, 1, __ATOMIC_RELAXED);
}

static double ms(const struct timespec *a, const struct timespec *b) {
    return (b->tv_sec - a->tv_sec) * 1000.0 + (b->tv_nsec - a->tv_nsec) / 1e6;
}

int main(void) {
    if (!mira_sched_init(4)) return 1;
    struct timespec s, e;
    clock_gettime(CLOCK_MONOTONIC, &s);
    for (int i = 0; i < TASK_COUNT; ++i)
        if (!mira_go_start_fast(task, NULL)) return 2;
    mira_sched_wait_all();
    clock_gettime(CLOCK_MONOTONIC, &e);
    printf("mira_fast_ms=%.3f tasks=%d\n", ms(&s, &e), completed);
    mira_sched_shutdown();
    return completed == TASK_COUNT ? 0 : 3;
}
