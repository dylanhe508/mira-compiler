#include "../runtime/rt_sched.h"
#include <windows.h>
#include <stdio.h>

static volatile LONG completed;

static void count_task(void *context) {
    (void)context;
    InterlockedIncrement(&completed);
}

int main(void) {
    enum { TASK_COUNT = 10000, WORKERS = 4 };
    if (!mira_sched_init(WORKERS)) return 10;
    for (int i = 0; i < TASK_COUNT; ++i) {
        if (!mira_go_start(count_task, NULL)) return 11;
    }
    mira_sched_wait_all();
    int workers = mira_sched_worker_count();
    mira_sched_shutdown();
    if (completed != TASK_COUNT) {
        fprintf(stderr, "completed=%ld expected=%d\n", completed, TASK_COUNT);
        return 12;
    }
    if (workers != WORKERS) {
        fprintf(stderr, "workers=%d expected=%d\n", workers, WORKERS);
        return 13;
    }
    puts("runtime_sched_test: PASS");
    return 0;
}
