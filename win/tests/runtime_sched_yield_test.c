#include "../runtime/rt_sched.h"
#include <windows.h>
#include <stdio.h>

static volatile LONG position;
static int order[3];

static void yielding_task(void *unused) {
    (void)unused;
    order[InterlockedIncrement(&position) - 1] = 1;
    mira_task_yield();
    order[InterlockedIncrement(&position) - 1] = 3;
}

static void middle_task(void *unused) {
    (void)unused;
    order[InterlockedIncrement(&position) - 1] = 2;
}

int main(void) {
    if (!mira_sched_init(1)) return 10;
    if (!mira_go_start(yielding_task, NULL)) return 11;
    if (!mira_go_start(middle_task, NULL)) return 12;
    mira_sched_wait_all();
    mira_sched_shutdown();
    if (order[0] != 1 || order[1] != 2 || order[2] != 3) {
        fprintf(stderr, "order=%d,%d,%d\n", order[0], order[1], order[2]);
        return 13;
    }
    puts("runtime_sched_yield_test: PASS");
    return 0;
}
