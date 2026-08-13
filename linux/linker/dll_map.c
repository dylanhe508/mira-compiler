/*
 * dll_map.c 閳?DLL 閸戣姤鏆熼弰鐘茬殸閸滃瞼顑侀崣鐑藉櫢閸涜棄鎮曢敍鍫濇惐鐢矁銆冮悧鍫礆
 */
#include "linker.h"
#include "../hash.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

/* 闂堟瑦鈧焦妲х亸鍕殶閹?*/
static const struct { const char *sym; const char *dll; } dll_entries[] = {
	/* kernel32.dll */
	{"ExitProcess",              "kernel32.dll"},
	{"GetStdHandle",             "kernel32.dll"},
	{"Sleep",                    "kernel32.dll"},
	{"GetTickCount64",           "kernel32.dll"},
	{"QueryPerformanceCounter",  "kernel32.dll"},
	{"QueryPerformanceFrequency","kernel32.dll"},
	{"GetCurrentProcessId",      "kernel32.dll"},
	{"GetCurrentThreadId",       "kernel32.dll"},
	{"SetConsoleCursorPosition", "kernel32.dll"},
	{"SetConsoleTextAttribute",  "kernel32.dll"},
	{"SetConsoleTitleA",         "kernel32.dll"},
	{"SetEnvironmentVariableA",  "kernel32.dll"},
	{"GetEnvironmentVariableA",  "kernel32.dll"},
	{"Beep",                     "kernel32.dll"},
	{"IsBadReadPtr",             "kernel32.dll"},
	{"GlobalAlloc",              "kernel32.dll"},
	{"GlobalFree",               "kernel32.dll"},
	{"GlobalLock",               "kernel32.dll"},
	{"GlobalUnlock",             "kernel32.dll"},
	{"ConvertThreadToFiber",     "kernel32.dll"},
	{"CreateFiber",              "kernel32.dll"},
	{"GetProcAddress",           "kernel32.dll"},
	{"GetModuleHandleA",         "kernel32.dll"},
	{"SwitchToFiber",            "kernel32.dll"},
	{"CreateThread",             "kernel32.dll"},
	{"WaitForSingleObject",      "kernel32.dll"},
	{"CloseHandle",              "kernel32.dll"},
	{"TlsAlloc",                 "kernel32.dll"},
	{"TlsFree",                  "kernel32.dll"},
	{"TlsGetValue",              "kernel32.dll"},
	{"TlsSetValue",              "kernel32.dll"},
	{"GetProcessHeap",           "kernel32.dll"},
	{"HeapAlloc",                "kernel32.dll"},
	{"__imp_ConvertThreadToFiber", "kernel32.dll"},
	{"__imp_CreateFiber",          "kernel32.dll"},
	{"__imp_SwitchToFiber",        "kernel32.dll"},
	{"ConvertFiberToThread",        "kernel32.dll"},
	{"DeleteFiber",                 "kernel32.dll"},
	{"CreateEventA",                "kernel32.dll"},
	{"SetEvent",                    "kernel32.dll"},
	{"ResetEvent",                  "kernel32.dll"},
	{"GetSystemInfo",               "kernel32.dll"},
	{"SwitchToThread",              "kernel32.dll"},
	{"GetTickCount",                "kernel32.dll"},
	{"InitializeSRWLock",           "kernel32.dll"},
	{"AcquireSRWLockExclusive",     "kernel32.dll"},
	{"ReleaseSRWLockExclusive",     "kernel32.dll"},
	{"InitializeConditionVariable", "kernel32.dll"},
	{"SleepConditionVariableSRW",   "kernel32.dll"},
	{"WakeConditionVariable",       "kernel32.dll"},
	{"WakeAllConditionVariable",    "kernel32.dll"},
	{"InitializeSListHead",         "kernel32.dll"},
	{"InterlockedPopEntrySList",    "kernel32.dll"},
	{"InterlockedPushEntrySList",   "kernel32.dll"},
	{"__imp_InitializeSListHead",       "kernel32.dll"},
	{"__imp_InterlockedPopEntrySList",  "kernel32.dll"},
	{"__imp_InterlockedPushEntrySList", "kernel32.dll"},
	{"__imp_ConvertFiberToThread",        "kernel32.dll"},
	{"__imp_DeleteFiber",                 "kernel32.dll"},
	{"__imp_CreateEventA",                "kernel32.dll"},
	{"__imp_SetEvent",                    "kernel32.dll"},
	{"__imp_ResetEvent",                  "kernel32.dll"},
	{"__imp_GetSystemInfo",               "kernel32.dll"},
	{"__imp_SwitchToThread",              "kernel32.dll"},
	{"__imp_GetTickCount",                "kernel32.dll"},
	{"__imp_InitializeSRWLock",           "kernel32.dll"},
	{"__imp_InitializeConditionVariable", "kernel32.dll"},
	{"__imp_SleepConditionVariableSRW",   "kernel32.dll"},
	{"__imp_WakeConditionVariable",       "kernel32.dll"},
	{"__imp_WakeAllConditionVariable",    "kernel32.dll"},
	/* user32.dll */
	{"MessageBoxA",              "user32.dll"},
	{"MessageBeep",              "user32.dll"},
	{"GetSystemMetrics",         "user32.dll"},
	{"OpenClipboard",            "user32.dll"},
	{"CloseClipboard",           "user32.dll"},
	{"EmptyClipboard",           "user32.dll"},
	{"GetClipboardData",         "user32.dll"},
	{"SetClipboardData",         "user32.dll"},
	/* msvcrt.dll */
	{"printf",                   "msvcrt.dll"},
	{"fprintf",                  "msvcrt.dll"},
	{"puts",                     "msvcrt.dll"},
	{"putchar",                  "msvcrt.dll"},
	{"scanf",                    "msvcrt.dll"},
	{"snprintf",                 "msvcrt.dll"},
	{"__ms_vsnprintf",           "msvcrt.dll"},
	{"_vsnprintf",               "msvcrt.dll"},
	{"malloc",                   "msvcrt.dll"},
	{"calloc",                   "msvcrt.dll"},
	{"realloc",                  "msvcrt.dll"},
	{"free",                     "msvcrt.dll"},
	{"memcpy",                   "msvcrt.dll"},
	{"memmove",                  "msvcrt.dll"},
	{"memset",                   "msvcrt.dll"},
	{"strlen",                   "msvcrt.dll"},
	{"strdup",                   "msvcrt.dll"},
	{"strstr",                   "msvcrt.dll"},
	{"strtoll",                  "msvcrt.dll"},
	{"fopen",                    "msvcrt.dll"},
	{"fclose",                   "msvcrt.dll"},
	{"fread",                    "msvcrt.dll"},
	{"fwrite",                   "msvcrt.dll"},
	{"fseek",                    "msvcrt.dll"},
	{"ftell",                    "msvcrt.dll"},
	{"fflush",                   "msvcrt.dll"},
	{"fgets",                    "msvcrt.dll"},
	{"getenv",                   "msvcrt.dll"},
	{"system",                   "msvcrt.dll"},
	{"exit",                     "msvcrt.dll"},
	{"rand",                     "msvcrt.dll"},
	{"srand",                    "msvcrt.dll"},
	{"clock",                    "msvcrt.dll"},
	{"_setjmpex",                "msvcrt.dll"},
	{"_setjmp",                  "msvcrt.dll"},
	{"pow",                      "msvcrt.dll"},
	{"sqrt",                     "msvcrt.dll"},
	{"setvbuf",                  "msvcrt.dll"},
	{"isprint",                  "msvcrt.dll"},
	{"isspace",                  "msvcrt.dll"},
	{"longjmp",                  "msvcrt.dll"},
	{"_time64",                  "msvcrt.dll"},
	{"__acrt_iob_func",          "msvcrt.dll"},
	{"_setjmpex",                "msvcrt.dll"},
	{"strcmp",                   "msvcrt.dll"},
	{"remove",                  "msvcrt.dll"},
	{"time",                    "msvcrt.dll"},
	{"floor",                   "msvcrt.dll"},
	{"ceil",                    "msvcrt.dll"},
	/* GDI and OpenGL */
	{"ChoosePixelFormat",        "gdi32.dll"},
	{"SetPixelFormat",           "gdi32.dll"},
	{"SwapBuffers",              "gdi32.dll"},
	{"wglCreateContext",         "opengl32.dll"},
	{"wglMakeCurrent",           "opengl32.dll"},
	{"wglDeleteContext",         "opengl32.dll"},
	{"glClearColor",             "opengl32.dll"},
	{"glClear",                  "opengl32.dll"},
	{"glRectf",                  "opengl32.dll"},
	/* lwmgl 绗﹀彿宸茬Щ闄わ細閫氳繃 import-ext + dll-map JSON 鍔ㄦ€佹敞鍐?*/
	{NULL, NULL}
};

static const struct { const char *from; const char *to; } rename_entries[] = {
	{"__acrt_iob_func", "__iob_func"},
	{"snprintf",        "_snprintf"},
	{"strdup",          "_strdup"},
	{"strtoll",         "_strtoi64"},
	{"__ms_vsnprintf",  "_vsnprintf"},
	{NULL, NULL}
};

static HashTable ht_dll    = {0};
static HashTable ht_rename = {0};
static int dll_map_ready = 0;

static void dll_map_init(void) {
	if (dll_map_ready) return;
	ht_init(&ht_dll, 128);
	for (int i = 0; dll_entries[i].sym; i++)
		ht_set(&ht_dll, dll_entries[i].sym, (void *)dll_entries[i].dll);
	ht_init(&ht_rename, 16);
	for (int i = 0; rename_entries[i].from; i++)
		ht_set(&ht_rename, rename_entries[i].from, (void *)rename_entries[i].to);
	dll_map_ready = 1;
}

/* 鍔ㄦ€佹敞鍐屼竴鏉?鍑芥暟鍚?>DLL 鏄犲皠锛堜緵 dll_ext 璋冪敤锛?*/
void dll_map_register(const char *func_name, const char *dll_name) {
	dll_map_init();
	/* 澶嶅埗瀛楃涓诧紝閬垮厤澶栭儴閲婃斁鍚庡搱甯岃〃鎮┖ */
	ht_set(&ht_dll, func_name, (void *)dll_name);
}

char *find_dll_in_dir(const char *funcname, const char *dir_path) {
	char search_path[MAX_PATH];
	snprintf(search_path, sizeof(search_path), "%s\\*.dll", dir_path);

	WIN32_FIND_DATAA fd;
	HANDLE hFind = FindFirstFileA(search_path, &fd);
	if (hFind == INVALID_HANDLE_VALUE) return NULL;

	char *found_dll = NULL;
	do {
		char full_path[MAX_PATH];
		snprintf(full_path, sizeof(full_path), "%s\\%s", dir_path, fd.cFileName);

		/* 鐏忔繆鐦崝鐘烘祰 DLL 楠炶埖鐓￠幍鍓ь儊閸?*/
		HMODULE hMod = LoadLibraryExA(full_path, NULL, DONT_RESOLVE_DLL_REFERENCES);
		if (hMod) {
			FARPROC proc = GetProcAddress(hMod, funcname);
			if (proc) {
				found_dll = strdup(fd.cFileName);
				FreeLibrary(hMod);
				break;
			}
			FreeLibrary(hMod);
		}
	} while (FindNextFileA(hFind, &fd));

	FindClose(hFind);
	return found_dll;
}

const char *find_dll_for(const char *funcname) {
	dll_map_init();
	const char *name = funcname;
	if (strncmp(name, "__imp_", 6) == 0) name += 6;
	const char *dll = (const char *)ht_get(&ht_dll, name);
	if (dll) return dll;

	/* 閼奉亜濮╅幒銏＄ゴ閹锋挸鐫?DLL */
	/* 1. 灏濊瘯褰撳墠鐩綍鐨?libs-dll */
	char *ext_dll = find_dll_in_dir(name, "libs-dll");
	/* 2. 鐏忔繆鐦稉?mira.exe 閸氬瞼楠囬惃?libs-dll 閻╊喖缍?*/
	if (!ext_dll) {
		char exe_path[MAX_PATH];
		if (GetModuleFileNameA(NULL, exe_path, sizeof(exe_path))) {
			char *lIR_slash = strrchr(exe_path, '\\');
			if (lIR_slash) *lIR_slash = '\0';
			char libs_dir[MAX_PATH];
			snprintf(libs_dir, sizeof(libs_dir), "%s\\libs-dll", exe_path);
			ext_dll = find_dll_in_dir(name, libs_dir);
		}
	}

	if (ext_dll) {
		printf("Linked external definition %s to %s\n", name, ext_dll);
		ht_set(&ht_dll, name, ext_dll);
		return ext_dll;
	}
	/* 杩欎簺绗﹀彿鐢?PE 閾炬帴鍣ㄥ唴閮ㄧ敓鎴愬瓨鏍癸紝涓嶉渶瑕佸閮?DLL */
	if (strcmp(name, "__main") == 0 ||
	    strcmp(name, "__intrinsic_setjmpex") == 0 ||
	    strcmp(name, "_setjmpex") == 0 ||
	    strcmp(name, "mainCRTStartup") == 0 ||
	    strncmp(name, "mira_", 5) == 0) {
		return NULL;  /* 内部符号，PE 链接器会处理 */
	}
	/* 鏈壘鍒?DLL 鏄犲皠锛岄潤榛樿繑鍥烇紱鐪熸鐨勯敊璇細鍦?symbols.c / pe.c 鎶ュ憡 */
	return NULL;
}

const char *rename_import(const char *name) {
	dll_map_init();
	const char *to = (const char *)ht_get(&ht_rename, name);
	return to ? to : name;
}

const char *import_func_name(const char *sym) {
	const char *name = sym;
	if (strncmp(name, "__imp_", 6) == 0) name += 6;
	return rename_import(name);
}

