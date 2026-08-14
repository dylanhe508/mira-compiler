/* rt_time.c - Time functions */
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

long long mira_time_now(void) {
	return (long long)time(NULL);
}

void mira_time_sleep(long long milliseconds) {
	if (milliseconds <= 0) return;
#ifdef _WIN32
	Sleep((DWORD)milliseconds);
#else
	struct timespec duration;
	duration.tv_sec = (time_t)(milliseconds / 1000);
	duration.tv_nsec = (long)((milliseconds % 1000) * 1000000LL);
	nanosleep(&duration, NULL);
#endif
}

long long mira_time_ms(void) {
#ifdef _WIN32
	LARGE_INTEGER counter;
	LARGE_INTEGER frequency;
	QueryPerformanceCounter(&counter);
	QueryPerformanceFrequency(&frequency);
	return (long long)(counter.QuadPart / frequency.QuadPart) * 1000LL +
	       (long long)((counter.QuadPart % frequency.QuadPart) * 1000LL /
	                   frequency.QuadPart);
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (long long)tv.tv_sec * 1000 + (long long)(tv.tv_usec / 1000);
#endif
}

/* mira_win_tick_ns:高分辨率单调时钟,纳秒。Windows 侧实现在 rt_win.c
 * (QueryPerformanceCounter);Linux 侧用 CLOCK_MONOTONIC,语义一致
 * (同机同符号名,智策与用户代码零改动)。 */
long long mira_win_tick_ns(void) {
#ifndef _WIN32
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
#else
	return 0;
#endif
}
