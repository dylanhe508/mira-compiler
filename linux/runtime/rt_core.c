/* rt_core.c - Minimal entry point (always linked) */
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

/* Mira entry point (provided by the compiled .mira obj) */
#ifdef __GNUC__
__attribute__((weak)) void mira_main(void) {}
#else
void mira_main(void);
#endif

/* Globals defined here, used by other modules */
double mira_float_tmp;
long long g_alloc_count = 0;
long long g_alloc_bytes = 0;
static clock_t g_start_clock = 0;

/* Windows 上 _start 等价物是 mainCRTStartup(由 program.c 生成),它直接
 * 调用 mira_main + ExitProcess,不会走到这里的 main。
 * Linux 上 _start(由 program.c 生成)调用 mira_main + exit;
 * exit 等 libc 符号由自写 ELF 链接器标记为动态符号,输出 .dynamic +
 * PLT/GOT,系统 ld.so 在进程启动时解析(DF_BIND_NOW 非懒解析,
 * 链接过程零外部工具调用)。
 * 因此 main 函数在 Windows 上保留(供调试),Linux 上由 _start 取代。 */
#ifdef _WIN32
int main(void) {
	setvbuf(stdout, NULL, _IONBF, 0);
	g_start_clock = clock();
	mira_main();
	return 0;
}
#endif

void mira_cr(void) {
	printf("\n");
	fflush(stdout);
}
