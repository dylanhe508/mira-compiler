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
    for (int i = 0; i < 100000; ++i)
        if (!mira_go_start_fast(task, NULL)) return 2;
    mira_sched_wait_all();
    long long global_locks = mira_sched_fast_global_lock_acquisitions();
    mira_sched_shutdown();
    if (completed != 100000) return 3;
    if (global_locks != 0) {
        fprintf(stderr, "fast global lock acquisitions=%lld\n", global_locks);
        return 4;
    }
    puts("runtime_sched_sharded_test: PASS");
    return 0;
}
