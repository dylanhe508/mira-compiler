/* rt_error.c - try/throw/catch error handling */
#include <setjmp.h>

#ifdef _WIN32
__declspec(dllimport) void __stdcall ExitProcess(unsigned int code);
__declspec(dllimport) unsigned long __stdcall TlsAlloc(void);
__declspec(dllimport) int __stdcall TlsFree(unsigned long index);
__declspec(dllimport) void *__stdcall TlsGetValue(unsigned long index);
__declspec(dllimport) int __stdcall TlsSetValue(unsigned long index, void *value);
__declspec(dllimport) void *__stdcall GetProcessHeap(void);
__declspec(dllimport) void *__stdcall HeapAlloc(void *heap, unsigned long flags, size_t bytes);
#else
#include <stdlib.h>
#include <pthread.h>
#endif

#define MIRA_TRY_STACK_MAX 32
typedef struct {
	jmp_buf try_stack[MIRA_TRY_STACK_MAX];
	int try_depth;
	char error_msg[512];
} MiraErrorState;

#ifdef _WIN32
#define TLS_OUT_OF_INDEXES 0xffffffffUL
#define HEAP_ZERO_MEMORY 0x00000008UL
static volatile long long mira_error_tls = -1;

__attribute__((noreturn)) static void error_fatal(void) {
	ExitProcess(1);
	__builtin_unreachable();
}

/* Errors are isolated per worker.  A throw on one parallel task must never
 * jump into another thread's handler. */
static MiraErrorState *error_state(void) {
	long long slot = mira_error_tls;
	if (slot < 0) {
		unsigned long fresh = TlsAlloc();
		if (fresh == TLS_OUT_OF_INDEXES) error_fatal();
		long long won = __sync_val_compare_and_swap(
			&mira_error_tls, -1, (long long)fresh);
		if (won != -1) {
			TlsFree(fresh);
			slot = won;
		} else {
			slot = (long long)fresh;
		}
	}
	MiraErrorState *state = (MiraErrorState *)TlsGetValue((unsigned long)slot);
	if (!state) {
		state = (MiraErrorState *)HeapAlloc(
			GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*state));
		if (!state || !TlsSetValue((unsigned long)slot, state)) error_fatal();
	}
	return state;
}
#else /* POSIX:错误状态按线程隔离,pthread key + calloc */
static pthread_key_t mira_error_key;
static volatile int mira_error_key_ok = 0;

static void error_state_destroy(void *p) {
	free(p);
}

__attribute__((noreturn)) static void error_fatal(void) {
	exit(1);
	__builtin_unreachable();
}

static MiraErrorState *error_state(void) {
	if (!mira_error_key_ok) {
		/* 双检初始化,对应 Windows 版的 CAS 竞态处理 */
		static pthread_mutex_t init_lock = PTHREAD_MUTEX_INITIALIZER;
		pthread_mutex_lock(&init_lock);
		if (!mira_error_key_ok) {
			if (pthread_key_create(&mira_error_key, error_state_destroy) != 0)
				error_fatal();
			mira_error_key_ok = 1;
		}
		pthread_mutex_unlock(&init_lock);
	}
	MiraErrorState *state = (MiraErrorState *)pthread_getspecific(mira_error_key);
	if (!state) {
		state = (MiraErrorState *)calloc(1, sizeof(*state));
		if (!state || pthread_setspecific(mira_error_key, state) != 0)
			error_fatal();
	}
	return state;
}
#endif

typedef void (*mira_fn_t)(void);
int mira_try_call(mira_fn_t fn) {
	MiraErrorState *state = error_state();
	if (state->try_depth >= MIRA_TRY_STACK_MAX) {
		error_fatal();
	}
	int idx = state->try_depth++;
	if (setjmp(state->try_stack[idx]) == 0) {
		fn(); state->try_depth--; return 0;
	} else {
		return 1;
	}
}

void *mira_try_begin(void) {
	MiraErrorState *state = error_state();
	if (state->try_depth >= MIRA_TRY_STACK_MAX) {
		error_fatal();
	}
	return &state->try_stack[state->try_depth++];
}

void mira_try_end(void) {
	MiraErrorState *state = error_state();
	if (state->try_depth > 0) state->try_depth--;
}

void mira_throw(const char *msg) {
	MiraErrorState *state = error_state();
	const char *source = msg ? msg : "unknown error";
	unsigned int i = 0;
	while (source[i] && i + 1 < sizeof(state->error_msg)) {
		state->error_msg[i] = source[i];
		i++;
	}
	state->error_msg[i] = '\0';
	if (state->try_depth > 0) {
		state->try_depth--;
		longjmp(state->try_stack[state->try_depth], 1);
	}
	error_fatal();
}

const char *mira_get_error(void) {
	MiraErrorState *state = error_state();
	return state->error_msg[0] ? state->error_msg : "no error";
}
