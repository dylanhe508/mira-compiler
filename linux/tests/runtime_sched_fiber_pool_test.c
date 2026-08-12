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
    for (int i = 0; i < 10000; ++i)
        if (!mira_go_start(task, NULL)) return 2;
    mira_sched_wait_all();
    long long fibers = mira_sched_fiber_creations();
    mira_sched_shutdown();
    if (completed != 10000) return 3;
    if (fibers > 16) {
        fprintf(stderr, "fiber creations=%lld, expected worker-local reuse\n", fibers);
        return 4;
    }
    puts("runtime_sched_fiber_pool_test: PASS");
    return 0;
}
