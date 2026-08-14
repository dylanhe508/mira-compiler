/* trap4:崩溃定位器增强版——打印 rbp、调用者地址(上一帧返回地址)。
 * 用法:LD_PRELOAD=/tmp/trap4.so ./程序
 * WSL1 下 gdb 不可用,靠 SIGSEGV handler + ucontext 定位。 */
#define _GNU_SOURCE
#include <stdio.h>
#include <signal.h>
#include <ucontext.h>
#include <dlfcn.h>
#include <stdint.h>
#include <unistd.h>

static void print_sym(uint64_t addr, const char *tag) {
    Dl_info d = {0};
    const char *sym = dladdr((void *)addr, &d) && d.dli_sname ? d.dli_sname : "?";
    fprintf(stderr, "[crash] %s=%llx sym=%s off=%llx\n", tag,
            (unsigned long long)addr, sym,
            (unsigned long long)(addr - (uint64_t)d.dli_fbase));
}

static void h(int sig, siginfo_t *si, void *uc) {
    ucontext_t *u = (ucontext_t *)uc;
    uint64_t rip = u->uc_mcontext.gregs[REG_RIP];
    uint64_t rsp = u->uc_mcontext.gregs[REG_RSP];
    uint64_t rbp = u->uc_mcontext.gregs[REG_RBP];
    fprintf(stderr, "[crash] rip=%llx rsp=%llx rsp_mod16=%llx rbp=%llx rbp_mod16=%llx\n",
            (unsigned long long)rip, (unsigned long long)rsp,
            (unsigned long long)(rsp & 15), (unsigned long long)rbp,
            (unsigned long long)(rbp & 15));
    print_sym(rip, "rip_sym");
    /* 逐帧回溯最多 8 层(帧链 [rbp] → 返回地址 [rbp+8]) */
    uint64_t fr = rbp;
    for (int i = 0; i < 8 && fr > 0x10000 && fr < 0x7fffffffffff; i++) {
        uint64_t ra = *(uint64_t *)(fr + 8);
        print_sym(ra, "back");
        fr = *(uint64_t *)fr;
    }
    _exit(1);
}

__attribute__((constructor)) static void init(void) {
    struct sigaction sa = {0};
    sa.sa_sigaction = h;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, 0);
    fprintf(stderr, "[trap4] handler installed\n");
}
