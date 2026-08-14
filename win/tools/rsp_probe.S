/* 测量进程入口 rsp 的真实对齐值(mod 16)。
 * 纯汇编 + syscall,不依赖 libc。 */
.text
.globl _start
_start:
    movq %rsp, %rax
    andq $15, %rax
    cmpq $9, %rax
    jbe 1f
    addq $7, %rax          /* 10-15 → 'A'-'F' */
1:
    addq $48, %rax         /* → '0'-'9' 或 'A'-'F' */
    leaq buf(%rip), %rsi
    movb %al, 0(%rsi)      /* 写入 ASCII */
    movq $1, %rax          /* write */
    movq $1, %rdi          /* fd = 1 */
    movq $2, %rdx          /* len = 2 */
    syscall
    movq $60, %rax         /* exit */
    xorq %rdi, %rdi
    syscall

.data
buf: .ascii "?R\n"
