#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static void write_text(const char *s) {
    DWORD length = 0, written;
    while (s[length]) ++length;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), s, length, &written, NULL);
}

static void write_int(long long value) {
    char buf[32];
    int pos = 31;
    unsigned long long magnitude;
    buf[pos--] = '\0';
    magnitude = value < 0 ? 0ULL - (unsigned long long)value
                          : (unsigned long long)value;
    do {
        buf[pos--] = (char)('0' + magnitude % 10);
        magnitude /= 10;
    } while (magnitude);
    if (value < 0) buf[pos--] = '-';
    write_text(&buf[pos + 1]);
}

void mira_print(int type, long long value) {
    if (type == 1)
        write_text((const char *)value);
    else
        write_int(value);
    write_text("\r\n");
}

long long mira_win_tick_ns(void) {
    LARGE_INTEGER counter, frequency;
    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&frequency);
    return (counter.QuadPart / frequency.QuadPart) * 1000000000LL +
           (counter.QuadPart % frequency.QuadPart) * 1000000000LL /
               frequency.QuadPart;
}

void *mem_alloc(long long size) {
    return HeapAlloc(GetProcessHeap(), 0, (SIZE_T)size);
}

void mem_free(void *ptr) {
    if (ptr) HeapFree(GetProcessHeap(), 0, ptr);
}
