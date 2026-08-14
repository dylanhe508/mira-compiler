#include "../runtime/rt_sched.h"
#include <windows.h>
#include <stdio.h>

static volatile LONG stage;
static volatile LONG joined;

static void target_task(void *unused) {
    (void)unused;
    InterlockedExchange(&stage, 1);
    mira_task_yield();
    InterlockedExchange(&stage, 2);
}

static void joining_task(void *opaque) {
    MiraJoinHandle *handle = (MiraJoinHandle *)opaque;
    mira_task_join(handle);
    if (stage == 2) InterlockedExchange(&joined, 1);
}

int main(void) {
    if (!mira_sched_init(1)) return 10;
    MiraJoinHandle *handle = mira_go_start_handle(target_task, NULL);
    if (!handle) return 11;
    if (!mira_go_start(joining_task, handle)) return 12;
    mira_sched_wait_all();
    mira_task_join(handle);
    mira_join_handle_release(handle);
    mira_sched_shutdown();
    if (!joined || stage != 2) {
        fprintf(stderr, "stage=%ld joined=%ld\n", stage, joined);
        return 13;
    }
    puts("runtime_sched_join_test: PASS");
    return 0;
}
