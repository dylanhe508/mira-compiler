/* trap5:崩溃定位器——打印全部寄存器 + 栈顶 64 字节 + 帧回溯。
 * 用法:LD_PRELOAD=/tmp/trap5.so ./程序
 * 针对 rip=0(空指针跳转/ret 到 0)场景:打印 rax/rbx/rcx/rdx/rsi/rdi/r8-r15,
 * 帮助判断崩溃是 call *%reg(reg=0)、jmp 0 还是 fiber 伪帧损坏(ret 到 0)。 */
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
    uint64_t rax = u->uc_mcontext.gregs[REG_RAX];
    uint64_t rbx = u->uc_mcontext.gregs[REG_RBX];
    uint64_t rcx = u->uc_mcontext.gregs[REG_RCX];
    uint64_t rdx = u->uc_mcontext.gregs[REG_RDX];
    uint64_t rsi = u->uc_mcontext.gregs[REG_RSI];
    uint64_t rdi = u->uc_mcontext.gregs[REG_RDI];
    uint64_t r8  = u->uc_mcontext.gregs[REG_R8];
    uint64_t r9  = u->uc_mcontext.gregs[REG_R9];
    uint64_t r10 = u->uc_mcontext.gregs[REG_R10];
    uint64_t r11 = u->uc_mcontext.gregs[REG_R11];
    uint64_t r12 = u->uc_mcontext.gregs[REG_R12];
    uint64_t r13 = u->uc_mcontext.gregs[REG_R13];
    uint64_t r14 = u->uc_mcontext.gregs[REG_R14];
    uint64_t r15 = u->uc_mcontext.gregs[REG_R15];
    fprintf(stderr,
            "[crash] rip=%llx rsp=%llx rbp=%llx rsp_mod16=%llx\n"
            "[regs]  rax=%llx rbx=%llx rcx=%llx rdx=%llx\n"
            "[regs]  rsi=%llx rdi=%llx r8=%llx r9=%llx\n"
            "[regs]  r10=%llx r11=%llx r12=%llx r13=%llx r14=%llx r15=%llx\n",
            (unsigned long long)rip, (unsigned long long)rsp,
            (unsigned long long)rbp, (unsigned long long)(rsp & 15),
            (unsigned long long)rax, (unsigned long long)rbx,
            (unsigned long long)rcx, (unsigned long long)rdx,
            (unsigned long long)rsi, (unsigned long long)rdi,
            (unsigned long long)r8,  (unsigned long long)r9,
            (unsigned long long)r10, (unsigned long long)r11,
            (unsigned long long)r12, (unsigned long long)r13,
            (unsigned long long)r14, (unsigned long long)r15);
    print_sym(rip, "rip_sym");
    /* 栈顶 64 字节 */
    fprintf(stderr, "[stack]");
    for (int i = 0; i < 8; i++) {
        uint64_t v = *(uint64_t *)(rsp + i * 8);
        fprintf(stderr, " %llx", (unsigned long long)v);
    }
    fprintf(stderr, "\n");
    /* 逐帧回溯最多 10 层 */
    uint64_t fr = rbp;
    for (int i = 0; i < 10 && fr > 0x10000 && fr < 0x7fffffffffff; i++) {
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
    fprintf(stderr, "[trap5] handler installed\n");
}
