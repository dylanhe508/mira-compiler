#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

static DWORD WINAPI sleeper(void *unused) {
    (void)unused;
    Sleep(120);
    return 0;
}

int main(void) {
    LARGE_INTEGER frequency, start, end;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);
    HANDLE first = CreateThread(NULL, 0, sleeper, NULL, 0, NULL);
    HANDLE second = CreateThread(NULL, 0, sleeper, NULL, 0, NULL);
    HANDLE handles[2] = { first, second };
    WaitForMultipleObjects(2, handles, TRUE, INFINITE);
    QueryPerformanceCounter(&end);
    CloseHandle(first);
    CloseHandle(second);
    printf("%.3f\n", (end.QuadPart - start.QuadPart) * 1000.0 /
                     frequency.QuadPart);
    return 0;
}
