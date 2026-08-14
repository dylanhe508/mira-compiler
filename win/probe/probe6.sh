#!/bin/bash
cd /tmp/mt/mira
echo "===== st_noif 全二进制搜可疑慢指令 ====="
objdump -d st_noif | grep -E "rdtsc|cpuid|pause|rep |xchg|lock |cli|sti|syscall|int |iret|swapgs|wrmsr|rdmsr|hlt|in |out " | head -20
echo "===== ldd ====="
ldd st_noif 2>&1 | head -8
echo "===== 写 ptrace RIP 采样器 ====="
cat > /tmp/rip_sampler.c <<'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>

/* 采样目标进程 RIP:attach -> GETREGS -> detach,统计分布 */
static unsigned long long hits[1 << 20]; /* 每 4KB 页一个桶 */

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "用法: %s <pid> <秒>\n", argv[0]); return 1; }
    int pid = atoi(argv[1]);
    int seconds = atoi(argv[2]);
    int samples = 0;
    long long t_start = (long long)time(NULL);
    while (time(NULL) - t_start < seconds) {
        if (ptrace(PTRACE_ATTACH, pid, 0, 0) < 0) { perror("attach"); break; }
        int st;
        waitpid(pid, &st, 0);
        struct user_regs_struct regs;
        if (ptrace(PTRACE_GETREGS, pid, 0, &regs) == 0) {
            unsigned long long page = regs.rip >> 12;
            if (page < (1 << 20)) { hits[page]++; samples++; }
        }
        ptrace(PTRACE_DETACH, pid, 0, 0);
        usleep(2000);
    }
    printf("总采样 %d 次\n", samples);
    for (unsigned long long p = 0; p < (1 << 20); p++) {
        if (hits[p]) printf("页 0x%llx: %llu 次 (%.1f%%)\n", p << 12, hits[p], 100.0 * hits[p] / samples);
    }
    return 0;
}
EOF
gcc -O2 -o /tmp/rip_sampler /tmp/rip_sampler.c && echo "采样器编译成功"
./st_noif > /dev/null 2>&1 &
PID=$!
sleep 1
echo "===== 采样 st_noif 15 秒 ====="
/tmp/rip_sampler $PID 15
kill $PID 2>/dev/null; wait 2>/dev/null
