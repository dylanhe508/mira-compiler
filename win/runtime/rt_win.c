/* rt_win.c - Windows-specific APIs (msgbox, sleep, shell, env, clipboard, etc.)
 * AND async/fiber support. This module pulls in user32.dll + kernel32.dll heavily. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

/* === Windows utility functions === */
void mira_win_msgbox(const char *msg) {
#ifdef _WIN32
	MessageBoxA(NULL, msg ? msg : "", "Mira", MB_OK);
#endif
}

void mira_win_sleep(long long ms) {
#ifdef _WIN32
	Sleep((DWORD)ms);
#endif
}

long long mira_win_shell(const char *cmd) {
	if (!cmd) return -1;
	return (long long)system(cmd);
}

char *mira_win_env(const char *name) {
	if (!name) return NULL;
#ifdef _WIN32
	char buf[4096];
	DWORD n = GetEnvironmentVariableA(name, buf, sizeof(buf));
	if (n == 0 || n >= sizeof(buf)) return NULL;
	char *r = malloc(n + 1);
	if (!r) return NULL;
	memcpy(r, buf, n + 1);
	return r;
#else
	return NULL;
#endif
}

void mira_win_env_set(const char *name, const char *value) {
#ifdef _WIN32
	if (name) SetEnvironmentVariableA(name, value);
#endif
}

char *mira_win_clip_get(void) {
#ifdef _WIN32
	if (!OpenClipboard(NULL)) return NULL;
	HANDLE h = GetClipboardData(CF_TEXT);
	if (!h) { CloseClipboard(); return NULL; }
	char *data = (char *)GlobalLock(h);
	char *r = data ? strdup(data) : NULL;
	GlobalUnlock(h);
	CloseClipboard();
	return r;
#else
	return NULL;
#endif
}

void mira_win_clip_set(const char *text) {
#ifdef _WIN32
	if (!text || !OpenClipboard(NULL)) return;
	EmptyClipboard();
	size_t len = strlen(text) + 1;
	HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, len);
	if (h) {
		memcpy(GlobalLock(h), text, len);
		GlobalUnlock(h);
		SetClipboardData(CF_TEXT, h);
	}
	CloseClipboard();
#endif
}

void mira_win_beep(void) {
#ifdef _WIN32
	MessageBeep(MB_OK);
#endif
}

void mira_win_beep_freq(long long freq, long long duration) {
#ifdef _WIN32
	Beep((DWORD)freq, (DWORD)duration);
#endif
}

void mira_win_set_title(const char *title) {
#ifdef _WIN32
	if (title) SetConsoleTitleA(title);
#endif
}

void mira_win_color_set(long long color) {
#ifdef _WIN32
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(h, (WORD)color);
#endif
}

void mira_win_cursor_move(long long x, long long y) {
#ifdef _WIN32
	COORD pos = { (SHORT)x, (SHORT)y };
	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
#endif
}

long long mira_win_screen_width(void) {
#ifdef _WIN32
	return GetSystemMetrics(SM_CXSCREEN);
#else
	return 0;
#endif
}

long long mira_win_screen_height(void) {
#ifdef _WIN32
	return GetSystemMetrics(SM_CYSCREEN);
#else
	return 0;
#endif
}

long long mira_win_pid(void) {
#ifdef _WIN32
	return (long long)GetCurrentProcessId();
#else
	return 0;
#endif
}

long long mira_win_tick(void) {
#ifdef _WIN32
	return (long long)GetTickCount64();
#else
	return 0;
#endif
}

/* High-resolution monotonic timestamp in nanoseconds. */
long long mira_win_tick_ns(void) {
#ifdef _WIN32
	LARGE_INTEGER counter, frequency;
	QueryPerformanceCounter(&counter);
	QueryPerformanceFrequency(&frequency);
	return (counter.QuadPart / frequency.QuadPart) * 1000000000LL +
	       (counter.QuadPart % frequency.QuadPart) * 1000000000LL / frequency.QuadPart;
#else
	return 0;
#endif
}

/* === Async/Fiber support === */
typedef void (*FiberFunc)(void);

typedef struct FiberTask {
	void *fiber;
	int finished;
	struct FiberTask *next;
	void *data_stack;
} FiberTask;

typedef struct {
	FiberFunc func;
	void *stack_ptr;
} FiberParam;

static void *g_main_fiber = NULL;
static FiberTask *g_task_head = NULL;
static FiberTask *g_task_tail = NULL;
static FiberTask *g_current_task = NULL;

typedef void* (__stdcall *PConvertThreadToFiber)(void*);
typedef void* (__stdcall *PCreateFiber)(SIZE_T, void*, void*);
typedef void  (__stdcall *PSwitchToFiber)(void*);

static PConvertThreadToFiber pConvertThreadToFiber = NULL;
static PCreateFiber pCreateFiber = NULL;
static PSwitchToFiber pSwitchToFiber = NULL;

static void init_fiber_api(void) {
	if (pConvertThreadToFiber) return;
	HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
	pConvertThreadToFiber = (PConvertThreadToFiber)GetProcAddress(hKernel32, "ConvertThreadToFiber");
	pCreateFiber = (PCreateFiber)GetProcAddress(hKernel32, "CreateFiber");
	pSwitchToFiber = (PSwitchToFiber)GetProcAddress(hKernel32, "SwitchToFiber");
	if (!pConvertThreadToFiber || !pCreateFiber || !pSwitchToFiber) {
		fprintf(stderr, "mira runtime: failed to load Windows Fiber API\n");
		exit(1);
	}
}

extern void *mem_alloc(long long size);

static void __stdcall fiber_stub(void *param) {
	FiberParam *p = (FiberParam *)param;
	FiberFunc func = p->func;
	void *stk = p->stack_ptr;
	free(p);
	__asm__ volatile ("mov %0, %%r12" : : "r" (stk) : "memory");
	if (func) func();
	if (g_current_task) g_current_task->finished = 1;
	pSwitchToFiber(g_main_fiber);
}

void mira_async_start(long long val_ptr) {
	init_fiber_api();
	if (!g_main_fiber) g_main_fiber = pConvertThreadToFiber(NULL);
	FiberTask *task = (FiberTask *)mem_alloc(sizeof(FiberTask));
	if (!task) return;
	task->data_stack = mem_alloc(4096);
	if (!task->data_stack) return;
	FiberParam *p = malloc(sizeof(FiberParam));
	p->func = (FiberFunc)val_ptr;
	p->stack_ptr = task->data_stack;
	task->fiber = pCreateFiber(0, fiber_stub, p);
	task->finished = 0;
	task->next = NULL;
	if (!g_task_head) { g_task_head = task; g_task_tail = task; }
	else { g_task_tail->next = task; g_task_tail = task; }
}

void mira_async_yield(void) {
	if (!g_main_fiber) g_main_fiber = ConvertThreadToFiber(NULL);
	if (g_current_task) {
		SwitchToFiber(g_main_fiber);
	} else {
		FiberTask *t = g_task_head;
		while (t) {
			if (!t->finished) {
				g_current_task = t;
				SwitchToFiber(t->fiber);
				g_current_task = NULL;
			}
			t = t->next;
		}
	}
}

/* === Native parallel tasks ===
 * Fibers above are deliberately cooperative and single-threaded.  These two
 * entry points are the explicit multi-core path: parallel returns an opaque
 * thread handle and join waits for exactly that task. */
typedef struct {
	FiberFunc func;
	void *data_stack;
} ParallelParam;

static DWORD WINAPI parallel_stub(void *opaque) {
	ParallelParam *p = (ParallelParam *)opaque;
	FiberFunc func = p->func;
	void *stk = p->data_stack;
	free(p);
	__asm__ volatile ("mov %0, %%r12" : : "r" (stk) : "memory");
	if (func) func();
	free(stk);
	return 0;
}

long long mira_parallel_start(long long val_ptr) {
	if (!val_ptr) return 0;
	ParallelParam *p = (ParallelParam *)malloc(sizeof(*p));
	if (!p) return 0;
	p->data_stack = malloc(4096);
	if (!p->data_stack) {
		free(p);
		return 0;
	}
	p->func = (FiberFunc)val_ptr;
	HANDLE thread = CreateThread(NULL, 0, parallel_stub, p, 0, NULL);
	if (!thread) {
		free(p->data_stack);
		free(p);
		return 0;
	}
	return (long long)(uintptr_t)thread;
}

void mira_parallel_join(long long handle) {
	HANDLE thread = (HANDLE)(uintptr_t)handle;
	if (!thread) return;
	WaitForSingleObject(thread, INFINITE);
	CloseHandle(thread);
}
