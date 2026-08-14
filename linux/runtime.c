/* Mira runtime 鈥?print / exit / 鍐呭瓨绛夎緟鍔╁嚱鏁帮紙鏍堝紡璇硶鐢熸垚浠ｇ爜鐢級 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <ctype.h>
#include <math.h>
#include <setjmp.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

/* === try/throw 閿欒澶勭悊 === */
#define MIRA_TRY_STACK_MAX 32
static jmp_buf mira_try_stack[MIRA_TRY_STACK_MAX];
static int mira_try_depth = 0;
static char *mira_error_msg = NULL;

/* mira_try_call: 鎵ц鍑芥暟鎸囬拡锛屾崟鑾?throw銆傝繑鍥?0=鎴愬姛, 1=寮傚父 */
typedef void (*mira_fn_t)(void);
int mira_try_call(mira_fn_t fn) {
	if (mira_try_depth >= MIRA_TRY_STACK_MAX) {
		fprintf(stderr, "mira: try stack overflow\n");
		exit(1);
	}
	int idx = mira_try_depth++;
	if (setjmp(mira_try_stack[idx]) == 0) {
		/* 姝ｅ父璺緞 */
		fn();
		mira_try_depth--;
		return 0;
	} else {
		/* 寮傚父璺緞锛坙ongjmp 璺冲洖锛?*/
		return 1;
	}
}

/* mira_try_begin: 杩斿洖 jmp_buf 鎸囬拡渚?IR 鍐呰仈 setjmp 浣跨敤 */
void *mira_try_begin(void) {
	if (mira_try_depth >= MIRA_TRY_STACK_MAX) {
		fprintf(stderr, "mira: try stack overflow\n");
		exit(1);
	}
	return &mira_try_stack[mira_try_depth++];
}

/* mira_try_end: 姝ｅ父閫€鍑?try 鍧?*/
void mira_try_end(void) {
	if (mira_try_depth > 0) mira_try_depth--;
}

/* mira_throw: 鎶涘嚭寮傚父锛岃烦杞埌鏈€杩戠殑 try 鐐?*/
void mira_throw(const char *msg) {
	if (mira_error_msg) free(mira_error_msg);
	mira_error_msg = msg ? strdup(msg) : strdup("unknown error");
	if (mira_try_depth > 0) {
		mira_try_depth--;
		longjmp(mira_try_stack[mira_try_depth], 1);
	}
	/* 娌℃湁 try 鎹曡幏 鈫?鐩存帴缁堟 */
	fprintf(stderr, "mira: unhandled error: %s\n", mira_error_msg);
	exit(1);
}

/* mira_get_error: 鑾峰彇鏈€鍚庣殑閿欒淇℃伅 */
const char *mira_get_error(void) {
	return mira_error_msg ? mira_error_msg : "no error";
}

/* 鑻ユ眹缂栨湭瀹氫箟 mira_main锛堜緥濡?out.asm 涓虹┖锛夛紝鐢ㄦ寮辩鍙烽伩鍏嶉摼鎺ラ敊璇?*/
#ifdef __GNUC__
__attribute__((weak)) void mira_main(void) {}
#else
void mira_main(void); /* 闈?GCC 鏃朵粛鐢辨眹缂栨彁渚?*/
#endif

/* 由汇编导出的全局符号（在 codegen.c 里生成） */
extern long long mira_var_count;
extern long long mira_vars[];         /* .bss 閲岀殑鍙橀噺妲芥暟缁勶紙姣忔Ы 8 瀛楄妭锛?*/
extern const char *mira_var_names[];  /* 鍙橀噺鍚嶅瓧绗︿覆琛?*/

/* 渚?print 浣跨敤鐨?float 涓存椂缂撳啿鍖猴紙C 瀹氫箟锛屾眹缂栧彲鍐欏叆锛?*/
double mira_float_tmp;

/* 简单运行时统计信息 */
static long long g_alloc_count = 0;
static long long g_alloc_bytes = 0;
static clock_t   g_start_clock = 0;

int main(void) {
	setvbuf(stdout, NULL, _IONBF, 0);
	g_start_clock = clock();
	mira_main();
	return 0;
}

/* 统一 print：type: 0=int, 1=string(ptr), 2=float(ptr-to-double), 3=bool */
void mira_print(int type, long long val_lo) {
	switch (type) {
		case 0:
			printf("%lld\n", val_lo);
			break;
		case 1:
			if ((const char *)val_lo)
				printf("%s\n", (const char *)val_lo);
			break;
		case 2:
			/* val_lo 涓?double* 鎸囬拡 */
			if (val_lo) printf("%g\n", *(double *)(void *)val_lo);
			break;
		case 3:
			printf("%s\n", val_lo ? "true" : "false");
			break;
		default:
			printf("%lld\n", val_lo);
			break;
	}
	fflush(stdout);
}

/* struct: 鍒涘缓/閲婃斁缁撴瀯浣撳疄渚?*/
void *mira_struct_new(long long size) {
	void *p = calloc(1, (size_t)size);
	if (!p) { fprintf(stderr, "mira: struct alloc failed (%lld bytes)\n", size); exit(1); }
	return p;
}

void mira_struct_free(void *p) {
	if (p) free(p);
}

/* read: 浠?stdin 璇讳竴涓暣鏁帮紝杩斿洖璇ュ€硷紙渚?codegen 鍘嬫爤锛?*/
long long mira_read_int(void) {
	long long x;
	if (scanf("%lld", &x) != 1)
		return 0;
	return x;
}

/* input: 浠?stdin 璇讳竴琛岋紝杩斿洖瀛楃涓叉寚閽堬紙malloc 鍒嗛厤锛岀敤瀹屽悗闇€ free锛?*/
char *mira_input(void) {
	static char buf[4096];
	if (!fgets(buf, sizeof(buf), stdin))
		return NULL;
	size_t len = strlen(buf);
	if (len > 0 && buf[len - 1] == '\n') {
		buf[len - 1] = '\0';
		len--;
	}
	char *s = malloc(len + 1);
	if (!s) return NULL;
	memcpy(s, buf, len + 1);
	return s;
}

/* --- 内存管理：allocate/free/move/erase/dump --- */

void *mem_alloc(long long size) {
	if (size <= 0 || size > 0x7fffffff) return NULL;
	void *p = malloc((size_t)size);
	if (p) {
		g_alloc_count++;
		g_alloc_bytes += size;
	}
	return p;
}

void mem_free(void *ptr) {
	if (!ptr) return;
	free(ptr);
}

void mem_move(void *dst, void *src, long long size) {
	if (!dst || !src || size <= 0) return;
	memmove(dst, src, (size_t)size);
}

void mem_erase(void *dst, long long size) {
	if (!dst || size <= 0) return;
	memset(dst, 0, (size_t)size);
}

/* 绠€鍗曞唴瀛樺崄鍏繘鍒?dump锛歛ddr size -> 鎵撳嵃 size 瀛楄妭 */
void mira_mem_dump(void *addr, long long size) {
	unsigned char *p = (unsigned char *)addr;
	if (!p || size <= 0) {
		printf("[dump] invalid address or size (%p, %lld)\n", addr, size);
		return;
	}
	long long remaining = size;
	while (remaining > 0) {
		int line = (int)(remaining > 16 ? 16 : remaining);
		printf("%p: ", (void *)p);
		for (int i = 0; i < line; i++) {
			printf("%02X ", p[i]);
		}
		printf(" | ");
		for (int i = 0; i < line; i++) {
			unsigned char c = p[i];
			putchar(isprint(c) ? c : '.');
		}
		putchar('\n');
		p += line;
		remaining -= line;
	}
}

/* .s锛氭墦鍗?Mira 鏁版嵁鏍堬紙mira_stack..r12 涔嬮棿锛夛紝涓嶄慨鏀规爤 */
void mira_dump_data_stack(void *base, void *sp) {
	long long depth = 0;
	if (sp && base && sp >= base) {
		depth = ((char *)sp - (char *)base) / 8;
	}
	printf("=== data stack (depth=%lld) ===\n", depth);
	long long *v = (long long *)base;
	for (long long i = 0; i < depth; i++) {
		printf("[%3lld] %lld (0x%llx)\n", i, v[i], (unsigned long long)v[i]);
	}
}

/* .var/.vars：基于编译期生成的变量名表和 mira_vars 槽，打印变量信息 */
void mira_dump_var_slot(int slot) {
	if (slot < 0 || (long long)slot >= mira_var_count) {
		printf("var[%d]: <invalid slot>\n", slot);
		return;
	}
	long long *vars = mira_vars;
	long long value = vars[slot];
	const char *name = mira_var_names ? mira_var_names[slot] : "?";
	printf("var %s [slot %d] @%p = %lld (0x%llx)\n",
	       name, slot, (void *)&vars[slot], value, (unsigned long long)value);
}

void mira_dump_vars(void) {
	printf("=== vars (count=%lld) ===\n", mira_var_count);
	for (int i = 0; (long long)i < mira_var_count; i++) {
		mira_dump_var_slot(i);
	}
}

/* .stats锛氱畝鍗曟墽琛岀粺璁?*/
void mira_stats(void) {
	double seconds = 0.0;
	if (g_start_clock != 0) {
		seconds = (double)(clock() - g_start_clock) / (double)CLOCKS_PER_SEC;
	}
	printf("=== .stats ===\n");
	printf("alloc_count  = %lld\n", g_alloc_count);
	printf("alloc_bytes  = %lld\n", g_alloc_bytes);
	printf("var_count    = %lld\n", mira_var_count);
	printf("elapsed_time = %.3f s\n", seconds);
}

/* 调试/断点相关：用操作系统/调试器实现真正的单步，Mira 里只提供挂钩 */
void mira_debug_break(void) {
	/* 寮傛/闈為樆濉炵増锛氫粎鎵撳嵃鎻愮ず锛屼笉鍐嶇湡姝ｈЕ鍙戞柇鐐逛腑鏂?*/
	printf("[break] breakpoint hit (non-blocking)\n");
}

void mira_debug_step(void) {
	/* 异步/非阻塞版：打印当前位置提示后直接返回，不等待输入 */
	printf("[step] reached step point (non-blocking)\n");
}

void mira_debug_next(void) {
	/* 异步/非阻塞版：同 step，占位用 */
	printf("[next] reached next point (non-blocking)\n");
}

void mira_debug_continue(void) {
	printf("[continue] 继续执行\n");
}

void mira_dump_return_stack(void) {
	printf("[.r] 当前实现直接使用 CPU 调用栈，没有单独的“返回栈”，此命令仅占位。\n");
}

void mira_dump_type_stack(void) {
	printf("[.t] 类型信息只在编译期跟踪，运行期已擦除，暂不支持真实类型栈 dump。\n");
}

void mira_backtrace(void) {
	printf("[.backtrace] 鏈疄鐜拌繍琛屾椂鍥炴函锛岃浣跨敤绯荤粺璋冭瘯鍣紙濡?VS / windbg锛夈€俓n");
}

void mira_where(void) {
	printf("[.where] 褰撳墠鐗堟湰娌℃湁琛屽彿/婧愪綅缃俊鎭槧灏勩€俓n");
}

void mira_watch_not_supported(void) {
	printf("[watch] 监视点需要更重的插桩，当前运行时未实现。\n");
}

/* --- 列表：布局 [length (8字节)][elem0][elem1]...，ptr 指向表头 --- */
void *mira_list_new(long long size) {
	if (size <= 0 || size > 0x7fffffff) return NULL;
	long long nbytes = 8 + size * 8;
	void *p = mem_alloc(nbytes);
	if (!p) return NULL;
	*(long long *)p = size;
	return p;
}

long long mira_list_len(void *ptr) {
	if (!ptr) return 0;
	return *(long long *)ptr;
}

long long mira_list_get(void *ptr, long long index) {
	if (!ptr) return 0;
	long long len = *(long long *)ptr;
	if (index < 0 || index >= len) return 0;
	return *(long long *)((char *)ptr + 8 + index * 8);
}

void mira_list_set(void *ptr, long long index, long long value) {
	if (!ptr) return;
	long long len = *(long long *)ptr;
	if (index < 0 || index >= len) return;
	*(long long *)((char *)ptr + 8 + index * 8) = value;
}

void mira_list_free(void *ptr) {
	mem_free(ptr);
}

/* --- 瀛楀吀锛氬竷灞€ [count (8)][cap (8)][k0][v0][k1][v1]...锛岀嚎鎬ф煡鎵?--- */
void *mira_dict_new(long long cap) {
	if (cap <= 0 || cap > 0x7fffffff) return NULL;
	long long nbytes = 16 + cap * 16;
	void *p = mem_alloc(nbytes);
	if (!p) return NULL;
	*(long long *)p = 0;
	*(long long *)((char *)p + 8) = cap;
	return p;
}

void mira_dict_set(void *ptr, long long key, long long value) {
	if (!ptr) return;
	long long *count = (long long *)ptr;
	long long *cap = (long long *)ptr + 1;
	long long *pairs = (long long *)ptr + 2;
	for (long long i = 0; i < *count; i++) {
		if (pairs[i * 2] == key) {
			pairs[i * 2 + 1] = value;
			return;
		}
	}
	if (*count < *cap) {
		pairs[*count * 2] = key;
		pairs[*count * 2 + 1] = value;
		(*count)++;
	}
}

long long mira_dict_get(void *ptr, long long key) {
	if (!ptr) return 0;
	long long count = *(long long *)ptr;
	long long *pairs = (long long *)ptr + 2;
	for (long long i = 0; i < count; i++) {
		if (pairs[i * 2] == key)
			return pairs[i * 2 + 1];
	}
	return 0;
}

long long mira_dict_has(void *ptr, long long key) {
	if (!ptr) return 0;
	long long count = *(long long *)ptr;
	long long *pairs = (long long *)ptr + 2;
	for (long long i = 0; i < count; i++) {
		if (pairs[i * 2] == key)
			return 1;
	}
	return 0;
}

void mira_dict_free(void *ptr) {
	mem_free(ptr);
}

/* --- 瀛楃涓叉搷浣滐細str 涓?char*锛宮alloc 鍒嗛厤锛岀敤瀹屽悗闇€ free --- */
long long mira_str_len(const char *s) {
	return s ? (long long)strlen(s) : 0;
}

char *mira_str_concat(const char *a, const char *b) {
	if (!a) a = "";
	if (!b) b = "";
	size_t la = strlen(a), lb = strlen(b);
	char *r = malloc(la + lb + 1);
	if (!r) return NULL;
	memcpy(r, a, la + 1);
	memcpy(r + la, b, lb + 1);
	return r;
}

char *mira_str_copy(const char *s) {
	if (!s) return NULL;
	size_t n = strlen(s) + 1;
	char *r = malloc(n);
	if (!r) return NULL;
	memcpy(r, s, n);
	return r;
}

/* --- 绫诲瀷杞崲 --- */
char *mira_int_to_str(long long x) {
	char buf[32];
	snprintf(buf, sizeof(buf), "%lld", x);
	return mira_str_copy(buf);
}

/* 智能 to_str：如果值是字符串指针则复制，否则按整数转换 */
char *mira_to_str(long long x) {
	/* 鍚彂寮忓垽鏂細澶т簬 0x10000 鐨勫€煎緢鍙兘鏄寚閽?*/
	if (x > 0x10000 || x < -0x7FFFFFFF) {
		/* 灏濊瘯璇诲彇浣滀负鎸囬拡 鈥?妫€鏌ユ槸鍚︿负鍙瀛楃涓?*/
		const char *p = (const char *)(intptr_t)x;
		/* Windows: 鐢?IsBadReadPtr 鎴?SEH锛涚畝鍖栫増锛氱洿鎺ュ皾璇?*/
#ifdef _WIN32
		if (!IsBadReadPtr(p, 1)) {
			/* 妫€鏌ュ墠鍑犱釜瀛楄妭鏄惁鍙墦鍗?*/
			int ok = 1;
			for (int i = 0; i < 4 && p[i]; i++) {
				unsigned char c = (unsigned char)p[i];
				if (c != 0 && c < 0x20 && c != '\t' && c != '\n' && c != '\r') { ok = 0; break; }
			}
			if (ok) return mira_str_copy(p);
		}
#else
		/* Unix: 绠€鍖栧亣璁惧彲璇?*/
		return mira_str_copy(p);
#endif
	}
	/* 鍥為€€鍒版暣鏁拌浆鎹?*/
	char buf[32];
	snprintf(buf, sizeof(buf), "%lld", x);
	return mira_str_copy(buf);
}

long long mira_str_to_int(const char *s) {
	if (!s) return 0;
	return (long long)strtoll(s, NULL, 10);
}

/* int -> float锛氳繑鍥?double 鐨?64 浣嶄綅妯″紡 */
long long mira_int_to_float(long long i) {
	union { long long ll; double d; } u;
	u.d = (double)i;
	return u.ll;
}

/* float -> int锛氳緭鍏?double 鐨?64 浣嶄綅妯″紡 */
long long mira_float_to_int(long long bits) {
	union { long long ll; double d; } u;
	u.ll = bits;
	return (long long)u.d;
}

/* --- 列表扩展：list-push 追加（需扩容），list-pop 取末 --- */
/* 绠€鍖栫増 list-push锛歭ist value -> 灏?value 杩藉姞鍒版湯灏撅紝鍐呴儴 realloc */
void *mira_list_push(void *ptr, long long value) {
	if (!ptr) return NULL;
	long long *len = (long long *)ptr;
	long long n = *len;
	long long newcap = n + 1;
	void *p = realloc(ptr, 8 + newcap * 8);
	if (!p) return ptr;
	*(long long *)p = newcap;
	*(long long *)((char *)p + 8 + n * 8) = value;
	return p;
}

long long mira_list_pop(void *ptr) {
	if (!ptr) return 0;
	long long *len = (long long *)ptr;
	if (*len <= 0) return 0;
	(*len)--;
	return *(long long *)((char *)ptr + 8 + (*len) * 8);
}

/* --- 字典扩展：dict-keys 返回键列表（ptr），dict-count --- */
void *mira_dict_keys(void *ptr) {
	if (!ptr) return NULL;
	long long count = *(long long *)ptr;
	if (count <= 0) return mira_list_new(0);
	void *keys = mira_list_new(count);
	if (!keys) return NULL;
	long long *pairs = (long long *)ptr + 2;
	for (long long i = 0; i < count; i++)
		mira_list_set(keys, (int)i, pairs[i * 2]);
	return keys;
}

long long mira_dict_count(void *ptr) {
	return ptr ? *(long long *)ptr : 0;
}

/* --- 鏂囦欢鎿嶄綔锛歱ath 涓?char* --- */
char *mira_file_read(const char *path) {
	if (!path) return NULL;
	FILE *f = fopen(path, "rb");
	if (!f) return NULL;
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (sz < 0) { fclose(f); return NULL; }
	char *buf = malloc((size_t)sz + 1);
	if (!buf) { fclose(f); return NULL; }
	size_t n = fread(buf, 1, (size_t)sz, f);
	buf[n] = '\0';
	fclose(f);
	return buf;
}

long long mira_file_write(const char *path, const char *content) {
	if (!path) return -1;
	if (!content) content = "";
	FILE *f = fopen(path, "wb");
	if (!f) return -1;
	size_t len = strlen(content);
	if (len > 0 && fwrite(content, 1, len, f) != len) { fclose(f); return -1; }
	fclose(f);
	return 0;
}

long long mira_file_append(const char *path, const char *content) {
	if (!path) return -1;
	if (!content) content = "";
	FILE *f = fopen(path, "ab");
	if (!f) return -1;
	size_t len = strlen(content);
	if (fwrite(content, 1, len, f) != len) { fclose(f); return -1; }
	fclose(f);
	return 0;
}

long long mira_file_exists(const char *path) {
	if (!path) return 0;
	FILE *f = fopen(path, "rb");
	if (!f) return 0;
	fclose(f);
	return 1;
}

long long mira_file_delete(const char *path) {
	if (!path) return -1;
	return remove(path) == 0 ? 0 : -1;
}

/* str-eq: 姣旇緝涓や釜瀛楃涓叉槸鍚︾浉绛?*/
long long mira_str_eq(const char *a, const char *b) {
	if (!a && !b) return 1;
	if (!a || !b) return 0;
	return strcmp(a, b) == 0 ? 1 : 0;
}

/* cr: 鎵撳嵃鎹㈣绗?*/
void mira_cr(void) {
	printf("\n");
	fflush(stdout);
}

/* --- 闅忔満鏁?--- */
long long mira_random(void) {
	return (long long)rand();
}

long long mira_random_range(long long min, long long max) {
	if (min >= max) return min;
	return min + (long long)(rand() % (unsigned long long)(max - min));
}

void mira_random_seed(unsigned long seed) {
	srand((unsigned int)seed);
}

/* --- 鏁板 --- */
long long mira_abs(long long x) {
	return x < 0 ? -x : x;
}

long long mira_min(long long a, long long b) {
	return a < b ? a : b;
}

long long mira_max(long long a, long long b) {
	return a > b ? a : b;
}

/* f-sqrt: double bits -> double bits */
long long mira_f_sqrt(long long bits) {
	union { long long ll; double d; } u;
	u.ll = bits;
	u.d = sqrt(u.d);
	return u.ll;
}

/* f-pow: base_bits exp_bits -> double bits */
long long mira_f_pow(long long base_bits, long long exp_bits) {
	union { long long ll; double d; } u, v;
	u.ll = base_bits;
	v.ll = exp_bits;
	u.d = pow(u.d, v.d);
	return u.ll;
}

/* f-floor / f-ceil: double bits -> double bits */
long long mira_f_floor(long long bits) {
	union { long long ll; double d; } u;
	u.ll = bits;
	u.d = floor(u.d);
	return u.ll;
}

long long mira_f_ceil(long long bits) {
	union { long long ll; double d; } u;
	u.ll = bits;
	u.d = ceil(u.d);
	return u.ll;
}

/* --- 鏃堕棿 --- */
long long mira_time_now(void) {
	return (long long)time(NULL);
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

/* --- 瀛楃涓叉墿灞?--- */
long long mira_str_contains(const char *s, const char *sub) {
	if (!s || !sub) return 0;
	return strstr(s, sub) != NULL ? 1 : 0;
}

char *mira_str_trim(const char *s) {
	if (!s) return NULL;
	while (*s && isspace((unsigned char)*s)) s++;
	size_t len = strlen(s);
	while (len > 0 && isspace((unsigned char)s[len - 1])) len--;
	char *r = malloc(len + 1);
	if (!r) return NULL;
	memcpy(r, s, len);
	r[len] = '\0';
	return r;
}

/* str-at: s index -> 绱㈠紩澶勫瓧绗︾殑 ASCII 鐮侊紝瓒婄晫杩斿洖 -1 */
long long mira_str_at(const char *s, long long index) {
	if (!s || index < 0) return -1;
	size_t len = strlen(s);
	if ((size_t)index >= len) return -1;
	return (unsigned char)s[index];
}

/* str-substr: s start n -> 浠?start 璧?n 涓瓧绗︾殑鏂颁覆锛坢alloc锛岄渶 free锛?*/
char *mira_str_substr(const char *s, long long start, long long n) {
	if (!s || start < 0 || n <= 0) return NULL;
	size_t len = strlen(s);
	if ((size_t)start >= len) return mira_str_copy("");
	if ((size_t)(start + n) > len) n = (long long)(len - start);
	char *r = malloc((size_t)n + 1);
	if (!r) return NULL;
	memcpy(r, s + start, (size_t)n);
	r[n] = '\0';
	return r;
}

/* --- 寮傛 / 鍗忕▼ (Windows Fibers) --- */

typedef void (*FiberFunc)(void);

typedef struct FiberTask {
	void *fiber;
	bool finished;
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

/* Dynamic Fiber API Pointers */
typedef void* (__stdcall *PConvertThreadToFiber)(void*);
typedef void* (__stdcall *PCreateFiber)(SIZE_T, void*, void*);
typedef void  (__stdcall *PSwitchToFiber)(void*);

static PConvertThreadToFiber pConvertThreadToFiber = NULL;
static PCreateFiber pCreateFiber = NULL;
static PSwitchToFiber pSwitchToFiber = NULL;

static void init_fiber_api() {
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

/* 绾ょ▼鍏ュ彛妗?*/
static void __stdcall fiber_stub(void *param) {
	FiberParam *p = (FiberParam *)param;
	FiberFunc func = p->func;
	void *stk = p->stack_ptr;
	free(p); /* 释放参数结构 */
	
	/* 鍒濆鍖?Mira 鏁版嵁鏍堟寚閽?(r12) */
	/* GCC 内联汇编：将 stk 移入 r12 */
	__asm__ volatile ("mov %0, %%r12" : : "r" (stk) : "memory");
	
	fprintf(stderr, "[Runtime] Fiber started. Stack: %p\n", stk);
	if (func) func();
	
	/* 任务结束 */
	fprintf(stderr, "[Runtime] Fiber finished.\n");
	if (g_current_task) g_current_task->finished = true;
	
	/* 鍒囨崲鍥炰富绾ょ▼ */
	pSwitchToFiber(g_main_fiber);
}

/* 启动异步任务：输入代码块地址（函数指针） */
void mira_async_start(long long val_ptr) {
	init_fiber_api();
	fprintf(stderr, "[Runtime] Async start called. Func: %p\n", (void*)val_ptr);
	fflush(stderr);
	if (!g_main_fiber) {
		fprintf(stderr, "[Runtime] Calling pConvertThreadToFiber...\n"); fflush(stderr);
		g_main_fiber = pConvertThreadToFiber(NULL);
		fprintf(stderr, "[Runtime] g_main_fiber: %p\n", g_main_fiber); fflush(stderr);
	}
	
	FiberTask *task = (FiberTask *)mem_alloc(sizeof(FiberTask));
	if (!task) return;
	
	task->data_stack = mem_alloc(4096);
	if (!task->data_stack) return;
	
	FiberParam *p = malloc(sizeof(FiberParam));
	p->func = (FiberFunc)val_ptr;
	p->stack_ptr = task->data_stack;
	
	fprintf(stderr, "[Runtime] Calling pCreateFiber...\n"); fflush(stderr);
	task->fiber = pCreateFiber(0, fiber_stub, p);
	fprintf(stderr, "[Runtime] task->fiber: %p\n", task->fiber); fflush(stderr);
	
	task->finished = false;
	task->next = NULL;
	
	/* 鍔犲叆闃熷垪 */
	if (!g_task_head) {
		g_task_head = task;
		g_task_tail = task;
	} else {
		g_task_tail->next = task;
		g_task_tail = task;
	}
}

/* 鍒囨崲鏉冿細Main -> Task 鎴?Task -> Main */
void mira_async_yield(void) {
	if (!g_main_fiber) {
		g_main_fiber = ConvertThreadToFiber(NULL);
	}
	
	if (g_current_task) {
		/* 鍦ㄤ换鍔′腑锛氬垏鎹㈠洖涓荤氦绋?*/
		SwitchToFiber(g_main_fiber);
	} else {
		/* 鍦ㄤ富绾ょ▼涓細璋冨害涓嬩竴涓换鍔?*/
		/* 绠€鍗曡疆杞細鎵句竴涓湭瀹屾垚鐨勪换鍔?*/
		FiberTask *t = g_task_head;
		while (t) {
			if (!t->finished) {
				g_current_task = t;
				SwitchToFiber(t->fiber);
				g_current_task = NULL; /* 杩斿洖涓荤氦绋嬪悗锛屽綋鍓嶄换鍔＄疆绌?*/
				return;
			}
			t = t->next;
		}
	}
}

/* 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺? * Windows x64 API 鍖呰鍑芥暟
 * 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺?*/
#ifdef _WIN32

/* msgbox: hWnd=NULL, text, title, flags 鈫?result */
long long mira_win_msgbox(void *hwnd, const char *text, const char *title, long long flags) {
	return (long long)MessageBoxA((HWND)hwnd, text ? text : "", title ? title : "Mira", (UINT)flags);
}

/* sleep: ms */
void mira_win_sleep(long long ms) {
	Sleep((DWORD)(ms > 0 ? ms : 0));
}

/* shell: cmd 鈫?exit code */
long long mira_win_shell(const char *cmd) {
	if (!cmd) return -1;
	return (long long)system(cmd);
}

/* env: key 鈫?value string (static buffer, don't free) */
const char *mira_win_env(const char *key) {
	if (!key) return "";
	static char buf[4096];
	DWORD n = GetEnvironmentVariableA(key, buf, sizeof(buf));
	if (n == 0 || n >= sizeof(buf)) return "";
	return buf;
}

/* env-set: key, value */
void mira_win_env_set(const char *key, const char *value) {
	if (!key) return;
	SetEnvironmentVariableA(key, value);
}

/* clipboard-get 鈫?string (malloc'd, caller frees) */
char *mira_win_clip_get(void) {
	if (!OpenClipboard(NULL)) return mira_str_copy("");
	HANDLE h = GetClipboardData(CF_TEXT);
	if (!h) { CloseClipboard(); return mira_str_copy(""); }
	char *data = (char *)GlobalLock(h);
	char *result = data ? mira_str_copy(data) : mira_str_copy("");
	GlobalUnlock(h);
	CloseClipboard();
	return result;
}

/* clipboard-set: text */
void mira_win_clip_set(const char *text) {
	if (!text) return;
	size_t len = strlen(text) + 1;
	HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, len);
	if (!h) return;
	char *buf = (char *)GlobalLock(h);
	memcpy(buf, text, len);
	GlobalUnlock(h);
	if (OpenClipboard(NULL)) {
		EmptyClipboard();
		SetClipboardData(CF_TEXT, h);
		CloseClipboard();
	} else {
		GlobalFree(h);
	}
}

/* beep: MessageBeep */
void mira_win_beep(void) {
	MessageBeep(MB_OK);
}

/* beep-freq: freq, duration 鈫?Beep() */
void mira_win_beep_freq(long long freq, long long duration) {
	Beep((DWORD)(freq > 0 ? freq : 440), (DWORD)(duration > 0 ? duration : 200));
}

/* console-title: set title */
void mira_win_set_title(const char *title) {
	if (title) SetConsoleTitleA(title);
}

/* color-set: set console text attribute */
void mira_win_color_set(long long attr) {
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(h, (WORD)attr);
}

/* cursor-move: x, y */
void mira_win_cursor_move(long long x, long long y) {
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
	COORD pos = {(SHORT)x, (SHORT)y};
	SetConsoleCursorPosition(h, pos);
}

/* screen-width: GetSystemMetrics */
long long mira_win_screen_width(void) {
	return (long long)GetSystemMetrics(SM_CXSCREEN);
}

/* screen-height */
long long mira_win_screen_height(void) {
	return (long long)GetSystemMetrics(SM_CYSCREEN);
}

/* pid */
long long mira_win_pid(void) {
	return (long long)GetCurrentProcessId();
}

/* tick: GetTickCount64 */
long long mira_win_tick(void) {
	return (long long)GetTickCount64();
}

/* High-resolution monotonic timestamp in nanoseconds. */
long long mira_win_tick_ns(void) {
	LARGE_INTEGER counter, frequency;
	QueryPerformanceCounter(&counter);
	QueryPerformanceFrequency(&frequency);
	return (counter.QuadPart / frequency.QuadPart) * 1000000000LL +
	       (counter.QuadPart % frequency.QuadPart) * 1000000000LL / frequency.QuadPart;
}

#endif /* _WIN32 */


