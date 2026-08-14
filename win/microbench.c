/* WSL1 原语微基准:分解调度器每任务开销中各组件的真实成本。
 * 用法:gcc -O2 -o /tmp/mb microbench.c -lpthread && /tmp/mb
 * 每项 100k 次,输出 ns/次。 */
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum { N = 100000 };

static double ns(const struct timespec *a, const struct timespec *b) {
    return (b->tv_sec - a->tv_sec) * 1e9 + (b->tv_nsec - a->tv_nsec);
}

typedef union __attribute__((aligned(16))) Sl {
    struct { void *ptr; uint64_t tag; };
    __int128 raw;
} Sl;

static inline int cas16(__int128 *p, __int128 *e, __int128 d) {
    __int128 old = *e;
    int ok;
    __asm__ __volatile__(
        "lock cmpxchg16b (%[ptr])"
        : [ok] "=@ccz"(ok)
        : [ptr] "r"(p),
          "a"((uint64_t)old), "d"((uint64_t)(old >> 64)),
          "b"((uint64_t)d), "c"((uint64_t)(d >> 64))
        : "memory");
    if (!ok) *e = *p;
    return ok;
}

struct Node { struct Node *next; };

static void bench(const char *name, void (*fn)(void *), void *arg) {
    struct timespec a, b;
    clock_gettime(CLOCK_MONOTONIC, &a);
    fn(arg);
    clock_gettime(CLOCK_MONOTONIC, &b);
    printf("%-28s %8.1f ns/op\n", name, ns(&a, &b) / N);
}

static Sl sl_head;
static void *sl_nodes[N];

static void bench_slist(void *u) {
    (void)u;
    for (int i = 0; i < N; ++i) {
        struct Node *n = (struct Node *)sl_nodes[i];
        Sl exp = sl_head, des;
        do { n->next = exp.ptr; des.ptr = n; des.tag = exp.tag + 1; }
        while (!cas16(&sl_head.raw, &exp.raw, des.raw));
        exp = sl_head;
        while (exp.ptr) {
            struct Node *n = (struct Node *)exp.ptr;
            Sl des2; des2.ptr = n->next; des2.tag = exp.tag + 1;
            if (cas16(&sl_head.raw, &exp.raw, des2.raw)) break;
        }
    }
}

static pthread_mutex_t mx = PTHREAD_MUTEX_INITIALIZER;
static void bench_mutex(void *u) {
    (void)u;
    for (int i = 0; i < N; ++i) { pthread_mutex_lock(&mx); pthread_mutex_unlock(&mx); }
}

static pthread_key_t key;
static void *keyvals[N];
static void bench_tls(void *u) {
    (void)u;
    for (int i = 0; i < N; ++i) {
        pthread_setspecific(key, keyvals[i]);
        (void)pthread_getspecific(key);
    }
}

static volatile long cnt;
static void bench_atomic(void *u) {
    (void)u;
    for (int i = 0; i < N; ++i)
        __atomic_add_fetch(&cnt, 1, __ATOMIC_ACQ_REL);
}

static void bench_cond_signal(void *u) {
    (void)u;
    pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
    for (int i = 0; i < N; ++i) pthread_cond_signal(&cv);
}

int main(void) {
    pthread_key_create(&key, NULL);
    for (int i = 0; i < N; ++i) {
        sl_nodes[i] = malloc(sizeof(struct Node));
        keyvals[i] = malloc(8);
    }
    bench("slist push+pop (cmpxchg16b)", bench_slist, NULL);
    bench("mutex lock/unlock", bench_mutex, NULL);
    bench("pthread_getspecific+set", bench_tls, NULL);
    bench("__atomic_add_fetch ACQ_REL", bench_atomic, NULL);
    bench("pthread_cond_signal", bench_cond_signal, NULL);
    printf("tasks=%d\n", N);
    return 0;
}
