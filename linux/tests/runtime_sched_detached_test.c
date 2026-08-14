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
    long long handles = mira_sched_join_handle_creations();
    mira_sched_shutdown();
    if (completed != 10000) return 3;
    if (handles != 0) {
        fprintf(stderr, "detached tasks created %lld join handles\n", handles);
        return 4;
    }
    puts("runtime_sched_detached_test: PASS");
    return 0;
}
