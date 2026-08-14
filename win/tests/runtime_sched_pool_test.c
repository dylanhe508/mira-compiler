#include "rt_sched.h"
#include <stdio.h>
#include <windows.h>

static volatile LONG completed;

static void task(void *unused) {
    (void)unused;
    InterlockedIncrement(&completed);
}

int main(void) {
    if (!mira_sched_init(4)) return 1;
    for (int batch = 0; batch < 10; ++batch) {
        for (int i = 0; i < 1000; ++i)
            if (!mira_go_start(task, NULL)) return 2;
        mira_sched_wait_all();
    }
    long long allocations = mira_sched_task_allocations();
    mira_sched_shutdown();
    if (completed != 10000) return 3;
    if (allocations > 1500) {
        fprintf(stderr, "task allocations=%lld, expected reuse\n", allocations);
        return 4;
    }
    puts("runtime_sched_pool_test: PASS");
    return 0;
}
