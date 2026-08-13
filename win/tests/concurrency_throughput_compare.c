#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <stdio.h>
#include "rt_sched.h"

enum { TASK_COUNT = 100000 };
static volatile LONG mira_count;
static volatile LONG win_count;

static double elapsed_ms(LARGE_INTEGER start, LARGE_INTEGER end,
                         LARGE_INTEGER frequency) {
    return (end.QuadPart - start.QuadPart) * 1000.0 / frequency.QuadPart;
}

static void mira_job(void *unused) {
    (void)unused;
    InterlockedIncrement(&mira_count);
}

static void CALLBACK win_job(PTP_CALLBACK_INSTANCE instance, void *context,
                             PTP_WORK work) {
    (void)instance;
    (void)context;
    (void)work;
    InterlockedIncrement(&win_count);
}

int main(void) {
    LARGE_INTEGER frequency, start, end;
    QueryPerformanceFrequency(&frequency);
    if (!mira_sched_init(0)) return 1;
    PTP_WORK work = CreateThreadpoolWork(win_job, NULL, NULL);
    if (!work) return 2;

    QueryPerformanceCounter(&start);
    for (int i = 0; i < TASK_COUNT; ++i)
        if (!mira_go_start(mira_job, NULL)) return 3;
    mira_sched_wait_all();
    QueryPerformanceCounter(&end);
    double mira_ms = elapsed_ms(start, end, frequency);

    QueryPerformanceCounter(&start);
    for (int i = 0; i < TASK_COUNT; ++i) SubmitThreadpoolWork(work);
    WaitForThreadpoolWorkCallbacks(work, FALSE);
    QueryPerformanceCounter(&end);
    double win_ms = elapsed_ms(start, end, frequency);

    printf("mira_tasks=%ld mira_ms=%.3f\n", mira_count, mira_ms);
    printf("win_tasks=%ld win_ms=%.3f\n", win_count, win_ms);
    CloseThreadpoolWork(work);
    mira_sched_shutdown();
    return mira_count == TASK_COUNT && win_count == TASK_COUNT ? 0 : 4;
}
