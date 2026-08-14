/* Linux 侧基线:4 线程 pthread 池消费 100k 任务(对照 Windows Thread Pool)。
 * 同一 WSL1 环境下与 bench_fast_linux.c(Mira 调度器)对比。 */
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdatomic.h>
#include <time.h>

enum { TASK_COUNT = 100000, WORKERS = 4 };

static atomic_int remaining;   /* 待消费任务数(取到 0 即退出) */
static atomic_int completed;

static void *worker(void *arg) {
    (void)arg;
    for (;;) {
        int r = atomic_fetch_sub_explicit(&remaining, 1, __ATOMIC_RELAXED);
        if (r <= 0) break;
        atomic_fetch_add_explicit(&completed, 1, __ATOMIC_RELAXED);
        sched_yield();   /* 模拟真实池的让出,防止独占 */
    }
    return NULL;
}

static double ms(const struct timespec *a, const struct timespec *b) {
    return (b->tv_sec - a->tv_sec) * 1000.0 + (b->tv_nsec - a->tv_nsec) / 1e6;
}

int main(void) {
    pthread_t th[WORKERS];
    struct timespec s, e;
    atomic_store(&remaining, TASK_COUNT);
    atomic_store(&completed, 0);
    clock_gettime(CLOCK_MONOTONIC, &s);
    for (int i = 0; i < WORKERS; ++i)
        pthread_create(&th[i], NULL, worker, NULL);
    for (int i = 0; i < WORKERS; ++i)
        pthread_join(th[i], NULL);
    clock_gettime(CLOCK_MONOTONIC, &e);
    printf("pthread_pool_ms=%.3f tasks=%d\n", ms(&s, &e), atomic_load(&completed));
    return completed == TASK_COUNT ? 0 : 1;
}
