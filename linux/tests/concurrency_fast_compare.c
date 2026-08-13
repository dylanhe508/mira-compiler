#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <stdio.h>
#include "rt_sched.h"

enum { TASK_COUNT = 100000 };
#ifndef MIRA_WORKERS
#define MIRA_WORKERS 0
#endif
static volatile LONG mira_count, win_count;

static void mira_job(void *unused) {
    (void)unused;
    InterlockedIncrement(&mira_count);
}

static void CALLBACK win_job(PTP_CALLBACK_INSTANCE instance, void *context,
                             PTP_WORK work) {
    (void)instance; (void)context; (void)work;
    InterlockedIncrement(&win_count);
}

static double ms(LARGE_INTEGER a, LARGE_INTEGER b, LARGE_INTEGER f) {
    return (b.QuadPart - a.QuadPart) * 1000.0 / f.QuadPart;
}

int main(void) {
    LARGE_INTEGER frequency, start, end;
    QueryPerformanceFrequency(&frequency);
    if (!mira_sched_init(MIRA_WORKERS)) return 1;
    PTP_WORK work = CreateThreadpoolWork(win_job, NULL, NULL);
    if (!work) return 2;
    QueryPerformanceCounter(&start);
    for (int i = 0; i < TASK_COUNT; ++i)
        if (!mira_go_start_fast(mira_job, NULL)) return 3;
    mira_sched_wait_all();
    QueryPerformanceCounter(&end);
    printf("mira_fast_ms=%.3f tasks=%ld\n", ms(start, end, frequency), mira_count);
    QueryPerformanceCounter(&start);
    for (int i = 0; i < TASK_COUNT; ++i) SubmitThreadpoolWork(work);
    WaitForThreadpoolWorkCallbacks(work, FALSE);
    QueryPerformanceCounter(&end);
    printf("win_pool_ms=%.3f tasks=%ld\n", ms(start, end, frequency), win_count);
    CloseThreadpoolWork(work);
    mira_sched_shutdown();
    return mira_count == TASK_COUNT && win_count == TASK_COUNT ? 0 : 4;
}
