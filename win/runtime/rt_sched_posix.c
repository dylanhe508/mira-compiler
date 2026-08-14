/* rt_sched_posix.c - POSIX 调度器(与 rt_sched.c 的 Windows 版算法完全一致)
 *
 * 移植映射:
 *   SRWLOCK            → mira_lock_t(pthread_mutex_t,非递归)
 *   CONDITION_VARIABLE → mira_cond_t(CLOCK_MONOTONIC)
 *   SLIST_HEADER       → 16 字节 tag-CAS 无锁栈(防 ABA)
 *   TLS(TlsAlloc)      → pthread_key_t
 *   CreateEventA(...)  → mira_event_t(手动复位)
 *   CreateThread       → pthread_create
 *   GetSystemInfo      → sysconf(_SC_NPROCESSORS_ONLN)
 *   Fiber API          → rt_fiber.h(汇编协程切换)
 *   Interlocked*       → GCC __atomic_*
 *   SwitchToThread     → sched_yield()
 *
 * 与 Windows 版的行为差异(仅影响性能,不影响语义):
 *   - 非 worker 线程提交 fast 任务时,目标 worker 用 pthread_self()
 *     位模式哈希(Windows 用 GetCurrentThreadId),分布等价。
 *   - direct 任务调 yield:Windows 版 SwitchToFiber(自身)为未定义行为,
 *     这里等价改为 sched_yield()(状态仍置 READY,效果一致)。
 */
#include "rt_sched.h"
#include "rt_sync.h"
#include "rt_fiber.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sched.h>

/* 调试:调度器事件跟踪(MIRA_FIBER_TRACE 环境变量开启) */
static int sched_trace(void) {
    static int on = -1;
    if (on < 0) on = getenv("MIRA_FIBER_TRACE") != NULL;
    return on;
}

typedef enum {
    MIRA_TASK_NEW,
    MIRA_TASK_RUNNING,
    MIRA_TASK_PARKING,
    MIRA_TASK_PARKED,
    MIRA_TASK_READY,
    MIRA_TASK_DONE
} MiraTaskState;

typedef struct MiraWorker MiraWorker;
typedef struct MiraFiber MiraFiber;
typedef struct MiraJoinWaiter {
    MiraTaskHandle task;
    struct MiraJoinWaiter *next;
} MiraJoinWaiter;

typedef struct MiraJoinHandle {
    mira_lock_t lock;
    mira_event_t *event;
    volatile long references;
    int completed;
    MiraJoinWaiter *waiters;
} MiraJoinHandle;

typedef struct __attribute__((aligned(16))) MiraTask {
    struct MiraTask *pool_next; /* 无锁任务池链(偏移 0,16 对齐) */
    MiraTaskFn function;
    void *context;
    MiraFiber *carrier;
    MiraWorker *owner;
    MiraTaskState state;
    int direct;
    MiraJoinHandle *join_handle;
    struct MiraTask *next;
} MiraTask;

enum { MIRA_TASKS_PER_BLOCK = 1024 };
typedef struct MiraTaskBlock {
    struct MiraTaskBlock *next;
    MiraTask tasks[MIRA_TASKS_PER_BLOCK];
} MiraTaskBlock;

struct MiraWorker {
    pthread_t thread;
    mira_fiber_ctx *scheduler_fiber;
    mira_lock_t fast_lock;
    MiraTask *fast_head;
    MiraTask *fast_tail;
    MiraTask *ready_head;
    MiraTask *ready_tail;
    MiraFiber *free_fibers;
    volatile long completed;      /* 本 worker 累计完成任务数(仅本 worker 写) */
    MiraTask *rel_head;           /* 待批量回收的任务链(本地,无竞争) */
    MiraTask *rel_tail;
    int rel_count;
};

struct MiraFiber {
    mira_fiber_ctx *handle;
    MiraWorker *owner;
    MiraTask *task;
    MiraFiber *next;
};

/* 无锁任务池头:指针 + tag,16 字节 CAS(cmpxchg16b)防 ABA。
 * Windows 的 SLIST 是同样的"指针+tag"设计。 */
typedef union __attribute__((aligned(16))) MiraSlistHead {
    struct {
        MiraTask *ptr;
        uint64_t tag;
    };
    __int128 raw;
} MiraSlistHead;

/* 16 字节无锁 CAS(cmpxchg16b),替代 GCC 对 __atomic_compare_exchange_16 的
 * libatomic 库调用 —— 避免产物依赖 libatomic.so.1,且少一次 PLT 开销。
 * 语义与 __atomic_compare_exchange_n(strong) 一致:
 *   成功返回 1;失败返回 0 并把 *expected 更新为内存当前值。
 * lock 前缀是全屏障,强于 RELEASE/ACQUIRE,语义安全。 */
static inline int mira_cas16(__int128 *ptr, __int128 *expected, __int128 desired) {
    __int128 old = *expected;
    int ok;
    __asm__ __volatile__(
        "lock cmpxchg16b (%[ptr])"
        : [ok] "=@ccz"(ok)
        : [ptr] "r"(ptr),
          "a"((uint64_t)old), "d"((uint64_t)(old >> 64)),
          "b"((uint64_t)desired), "c"((uint64_t)(desired >> 64))
        : "memory");
    if (!ok) *expected = *ptr;
    return ok;
}

static void slist_push(MiraSlistHead *head, MiraTask *task) {
    MiraSlistHead expected = *head, desired;
    do {
        task->pool_next = expected.ptr;
        desired.ptr = task;
        desired.tag = expected.tag + 1;
    } while (!mira_cas16(&head->raw, &expected.raw, desired.raw));
}

static MiraTask *slist_pop(MiraSlistHead *head) {
    MiraSlistHead expected = *head;
    while (expected.ptr) {
        MiraSlistHead desired;
        desired.ptr = expected.ptr->pool_next;
        desired.tag = expected.tag + 1;
        if (mira_cas16(&head->raw, &expected.raw, desired.raw))
            return expected.ptr;
        /* 失败:expected 已被重载为最新头,重试 */
    }
    return NULL;
}

/* 一次 CAS 从池头摘下最多 max 个节点(WSL1 仿真下 lock cmpxchg16b
 * 约 120-280ns/次,批量摘取把池访问摊薄;见 microbench.c)。 */
static MiraTask *slist_pop_batch(MiraSlistHead *head, int max) {
    MiraSlistHead expected = *head;
    while (expected.ptr) {
        MiraTask *first = expected.ptr;
        MiraTask *t = first;
        int n = 1;
        while (t->pool_next && n < max) {
            t = t->pool_next;
            n++;
        }
        MiraSlistHead desired;
        desired.ptr = t->pool_next;
        desired.tag = expected.tag + 1;
        if (mira_cas16(&head->raw, &expected.raw, desired.raw)) {
            t->pool_next = NULL; /* 截断,摘下链与池分离 */
            return first;
        }
    }
    return NULL;
}

typedef struct {
    mira_lock_t lock;
    mira_cond_t ready;
    MiraTask *new_head;
    MiraTask *new_tail;
    MiraSlistHead free_tasks;
    mira_lock_t task_pool_lock;
    MiraTaskBlock *task_blocks;
    MiraWorker *workers;
    mira_event_t *idle_event;
    volatile long active_tasks;
    volatile long next_fast_worker;
    volatile long waiting_workers;  /* 正在 condvar 上等待的 worker 数 */
    volatile long wake_pending;     /* 已有未消费的唤醒信号(按需 signal) */
    int worker_count;
    int stopping;
    int initialized;
} MiraScheduler;

static MiraScheduler scheduler;
static pthread_key_t worker_tls;
static pthread_key_t task_tls;
static pthread_key_t cache_tls;   /* 提交者批量取任务缓存(见 task_acquire) */
static int worker_tls_ok = 0;
static int task_tls_ok = 0;
static int cache_tls_ok = 0;
static volatile long long join_handle_creations;
static volatile long long task_allocations;
static volatile long long fiber_creations;
static volatile long long fast_global_lock_acquisitions;

static MiraTask *task_acquire(void) {
    /* 提交者 TLS 批量缓存:一次 cmpxchg16b 从池摘 32 个,本地逐个分发。
     * cmpxchg16b 是调度器单任务最大开销项(WSL1 仿真 ~120-280ns/次),
     * 批量后摊薄到 ~10ns/任务;多提交者各自 TLS 缓存,天然无竞争。 */
    MiraTask *head = (MiraTask *)pthread_getspecific(cache_tls);
    if (head) {
        pthread_setspecific(cache_tls, head->pool_next);
        memset(head, 0, sizeof(*head));
        return head;
    }
    head = slist_pop_batch(&scheduler.free_tasks, 32);
    if (head) {
        pthread_setspecific(cache_tls, head->pool_next);
        memset(head, 0, sizeof(*head));
        return head;
    }
    /* 池空:锁内二次确认后分配新块,整块直接挂 TLS 缓存(0 次原子) */
    mira_lock_acquire(&scheduler.task_pool_lock);
    MiraTask *pooled = slist_pop(&scheduler.free_tasks);
    if (pooled) {
        mira_lock_release(&scheduler.task_pool_lock);
        memset(pooled, 0, sizeof(*pooled));
        return pooled;
    }
    MiraTaskBlock *block = (MiraTaskBlock *)calloc(1, sizeof(*block));
    if (!block) {
        mira_lock_release(&scheduler.task_pool_lock);
        return NULL;
    }
    block->next = scheduler.task_blocks;
    scheduler.task_blocks = block;
    for (int i = 0; i < MIRA_TASKS_PER_BLOCK - 1; ++i)
        block->tasks[i].pool_next = &block->tasks[i + 1];
    block->tasks[MIRA_TASKS_PER_BLOCK - 1].pool_next = NULL;
    __atomic_add_fetch(&task_allocations, MIRA_TASKS_PER_BLOCK, __ATOMIC_RELAXED);
    pthread_setspecific(cache_tls, &block->tasks[1]);
    mira_lock_release(&scheduler.task_pool_lock);
    memset(&block->tasks[0], 0, sizeof(block->tasks[0]));
    return &block->tasks[0];
}

static void task_release(MiraTask *task) {
    if (!task) return;
    task->function = NULL;
    task->context = NULL;
    task->carrier = NULL;
    task->owner = NULL;
    task->join_handle = NULL;
    task->direct = 0;
    task->state = MIRA_TASK_DONE;
    task->next = NULL;
    slist_push(&scheduler.free_tasks, task);
}

static MiraWorker *get_current_worker(void) {
    return worker_tls_ok ? (MiraWorker *)pthread_getspecific(worker_tls) : NULL;
}

static MiraTask *get_current_task(void) {
    return task_tls_ok ? (MiraTask *)pthread_getspecific(task_tls) : NULL;
}

static void queue_push(MiraTask **head, MiraTask **tail, MiraTask *task) {
    task->next = NULL;
    if (*tail) (*tail)->next = task;
    else *head = task;
    *tail = task;
}

static MiraTask *queue_pop(MiraTask **head, MiraTask **tail) {
    MiraTask *task = *head;
    if (!task) return NULL;
    *head = task->next;
    if (!*head) *tail = NULL;
    task->next = NULL;
    return task;
}

static MiraTask *worker_pop_fast(MiraWorker *worker) {
    mira_lock_acquire(&worker->fast_lock);
    MiraTask *task = queue_pop(&worker->fast_head, &worker->fast_tail);
    mira_lock_release(&worker->fast_lock);
    return task;
}

static MiraTask *worker_steal_fast(MiraWorker *worker) {
    if (!scheduler.workers) return NULL;
    int start = (int)(worker - scheduler.workers);
    for (int step = 1; step < scheduler.worker_count; ++step) {
        MiraWorker *victim = &scheduler.workers[(start + step) % scheduler.worker_count];
        MiraTask *task = worker_pop_fast(victim);
        if (task) return task;
    }
    return NULL;
}

void mira_join_handle_release(MiraJoinHandle *handle) {
    if (!handle) return;
    if (__atomic_sub_fetch(&handle->references, 1, __ATOMIC_ACQ_REL) == 0) {
        mira_event_destroy(handle->event);
        free(handle);
    }
}

static void complete_join_handle(MiraJoinHandle *handle) {
    if (!handle) return;
    mira_lock_acquire(&handle->lock);
    handle->completed = 1;
    MiraJoinWaiter *waiter = handle->waiters;
    handle->waiters = NULL;
    mira_event_set(handle->event);
    while (waiter) {
        MiraJoinWaiter *next = waiter->next;
        mira_task_wake(waiter->task);
        waiter = next;
    }
    mira_lock_release(&handle->lock);
    mira_join_handle_release(handle);
}

/* task_fiber_entry 是死循环:任务完成后切回调度器,等待下一个任务
 * 挂在 carrier->task 上。与 Windows 版逐字等价。 */
static void task_fiber_entry(void *opaque) {
    MiraFiber *carrier = (MiraFiber *)opaque;
    for (;;) {
        MiraTask *task = carrier->task;
        task->function(task->context);
        task->state = MIRA_TASK_DONE;
        mira_fiber_dump("task->sched(完成切回)", carrier->handle);
        mira_fiber_switch(carrier->handle, carrier->owner->scheduler_fiber);
    }
}

static MiraFiber *worker_acquire_fiber(MiraWorker *worker) {
    MiraFiber *carrier = worker->free_fibers;
    if (carrier) {
        worker->free_fibers = carrier->next;
        carrier->next = NULL;
        return carrier;
    }
    carrier = (MiraFiber *)calloc(1, sizeof(*carrier));
    if (!carrier) return NULL;
    carrier->owner = worker;
    carrier->handle = mira_fiber_create(64 * 1024, task_fiber_entry, carrier);
    if (!carrier->handle) {
        free(carrier);
        return NULL;
    }
    __atomic_add_fetch(&fiber_creations, 1, __ATOMIC_RELAXED);
    return carrier;
}

static void worker_release_fiber(MiraWorker *worker, MiraFiber *carrier) {
    mira_fiber_dump("release_fiber", carrier->handle);
    carrier->task = NULL;
    carrier->next = worker->free_fibers;
    worker->free_fibers = carrier;
}

/* 任务完成记账:写本 worker 私有计数,wait_all 求和判断,
 * 取代每任务一次共享 active_tasks 原子写(4 worker 争同一缓存行)。
 * 完成后求所有 worker 完成数总和:等于已提交数即最后完成者,
 * set idle_event 唤醒 wait_all。全程只读共享缓存行,无写竞争。 */
static void worker_note_done(MiraWorker *worker) {
    long done = ++worker->completed;
    long total = done;
    for (int i = 0; i < scheduler.worker_count; ++i) {
        MiraWorker *w = &scheduler.workers[i];
        if (w != worker)
            total += __atomic_load_n(&w->completed, __ATOMIC_RELAXED);
    }
    if (total == __atomic_load_n(&scheduler.active_tasks, __ATOMIC_ACQUIRE))
        mira_event_set(scheduler.idle_event);
}

/* 本地批量回收:任务先攒进 worker 私有链,满 32 个一次 CAS 接入
 * 无锁任务池,把每任务的 cmpxchg16b 摊薄 32 倍(worker 侧回收竞争)。 */
static void task_release_local(MiraWorker *worker, MiraTask *task) {
    task->pool_next = NULL;
    if (worker->rel_head) worker->rel_tail->pool_next = task;
    else worker->rel_head = task;
    worker->rel_tail = task;
    if (++worker->rel_count >= 32) {
        MiraSlistHead expected = scheduler.free_tasks, desired;
        do {
            worker->rel_tail->pool_next = expected.ptr;
            desired.ptr = worker->rel_head;
            desired.tag = expected.tag + 1;
        } while (!mira_cas16(&scheduler.free_tasks.raw, &expected.raw,
                             desired.raw));
        worker->rel_head = worker->rel_tail = NULL;
        worker->rel_count = 0;
    }
}

static void *scheduler_worker(void *opaque) {
    MiraWorker *worker = (MiraWorker *)opaque;
    pthread_setspecific(worker_tls, worker);
    if (sched_trace())
        fprintf(stderr, "[sched] worker %p 启动,创建调度器 fiber\n", (void *)worker);
    worker->scheduler_fiber = mira_fiber_convert_thread();
    if (!worker->scheduler_fiber) return NULL;

    for (;;) {
        /* 无锁快速路径:fast 队列只被 direct 提交者使用,pop 到后
         * owner/state/执行/收尾全部 worker 私有,不需要全局锁。
         * 轮转提交下命中率高(≥99%),绕开每任务一次全局锁竞争。
         * 与 Windows 版行为一致:direct 任务无 carrier、收尾必 DONE。 */
        MiraTask *task = worker_pop_fast(worker);
        if (task) {
            task->owner = worker;
            task->state = MIRA_TASK_RUNNING;
            pthread_setspecific(task_tls, task);
            task->function(task->context);
            task->state = MIRA_TASK_DONE;
            pthread_setspecific(task_tls, NULL);
            if (task->state == MIRA_TASK_READY) {
                /* 理论上 fast 任务不会 READY/PARKED;防御性走锁内重投 */
                mira_lock_acquire(&scheduler.lock);
                queue_push(&worker->ready_head, &worker->ready_tail, task);
                mira_cond_signal(&scheduler.ready);
                mira_lock_release(&scheduler.lock);
            } else if (task->state != MIRA_TASK_PARKED) {
                complete_join_handle(task->join_handle);
                task_release_local(worker, task);
                worker_note_done(worker);
            }
            continue;
        }
        if (!task) task = worker_steal_fast(worker);
        mira_lock_acquire(&scheduler.lock);
        if (!task) task = queue_pop(&scheduler.new_head, &scheduler.new_tail);
        if (!task) task = queue_pop(&worker->ready_head, &worker->ready_tail);
        while (!task && !scheduler.stopping) {
            __atomic_add_fetch(&scheduler.waiting_workers, 1, __ATOMIC_ACQ_REL);
            mira_cond_timedwait_ms(&scheduler.ready, &scheduler.lock, 1);
            __atomic_sub_fetch(&scheduler.waiting_workers, 1, __ATOMIC_ACQ_REL);
            /* 醒来后清除 pending,允许后续提交者再次按需 signal;
             * 即使被抢空继续等待,1ms 轮询也兜底。 */
            __atomic_store_n(&scheduler.wake_pending, 0, __ATOMIC_RELEASE);
            task = queue_pop(&scheduler.new_head, &scheduler.new_tail);
            if (!task) task = queue_pop(&worker->ready_head, &worker->ready_tail);
            if (!task) {
                mira_lock_release(&scheduler.lock);
                task = worker_pop_fast(worker);
                if (!task) task = worker_steal_fast(worker);
                mira_lock_acquire(&scheduler.lock);
            }
        }
        if (!task && scheduler.stopping) {
            mira_lock_release(&scheduler.lock);
            MiraFiber *carrier = worker->free_fibers;
            while (carrier) {
                MiraFiber *next = carrier->next;
                if (sched_trace())
                    fprintf(stderr, "[sched] worker %p destroy carrier=%p\n",
                            (void *)worker, (void *)carrier);
                mira_fiber_destroy(carrier->handle);
                free(carrier);
                carrier = next;
            }
            worker->free_fibers = NULL;
            /* 退出前把本地批量回收链一次接入任务池,防任务泄漏 */
            if (worker->rel_head) {
                MiraSlistHead expected = scheduler.free_tasks, desired;
                do {
                    worker->rel_tail->pool_next = expected.ptr;
                    desired.ptr = worker->rel_head;
                    desired.tag = expected.tag + 1;
                } while (!mira_cas16(&scheduler.free_tasks.raw, &expected.raw,
                                     desired.raw));
                worker->rel_head = worker->rel_tail = NULL;
                worker->rel_count = 0;
            }
            mira_fiber_destroy(worker->scheduler_fiber);
            worker->scheduler_fiber = NULL;
            return NULL;
        }
        if (sched_trace())
            fprintf(stderr, "[sched] worker %p 取到任务 %p state=%d direct=%d\n",
                    (void *)worker, (void *)task, task->state, task->direct);
        if (!task->owner && !task->direct) {
            task->owner = worker;
            task->carrier = worker_acquire_fiber(worker);
            if (sched_trace())
                fprintf(stderr, "[sched] worker %p 为任务 %p 分配 carrier=%p\n",
                        (void *)worker, (void *)task, (void *)task->carrier);
            if (task->carrier) task->carrier->task = task;
            else task->state = MIRA_TASK_DONE;
        }
        if (task->direct) {
            task->owner = worker;
            task->state = MIRA_TASK_RUNNING;
        } else {
            task->state = task->carrier ? MIRA_TASK_RUNNING : MIRA_TASK_DONE;
        }
        mira_lock_release(&scheduler.lock);

        pthread_setspecific(task_tls, task);
        if (task->direct) {
            task->function(task->context);
            task->state = MIRA_TASK_DONE;
        } else if (task->carrier) {
            mira_fiber_dump("sched->task(切入)", task->carrier->handle);
            mira_fiber_switch(worker->scheduler_fiber, task->carrier->handle);
        }
        pthread_setspecific(task_tls, NULL);

        if (sched_trace())
        fprintf(stderr, "[sched] worker %p 收尾:任务 %p state=%d\n",
                (void *)worker, (void *)task, task->state);
    if (task->state == MIRA_TASK_READY) {
            mira_lock_acquire(&scheduler.lock);
            queue_push(&worker->ready_head, &worker->ready_tail, task);
            mira_cond_signal(&scheduler.ready);
            mira_lock_release(&scheduler.lock);
        } else if (task->state == MIRA_TASK_PARKED) {
            /* A channel or select waiter owns the task until wake-up. */
        } else {
            complete_join_handle(task->join_handle);
            if (task->carrier) {
                worker_release_fiber(worker, task->carrier);
                task->carrier = NULL;
            }
            task_release_local(worker, task);
            worker_note_done(worker);
        }
    }
}

int mira_sched_init(int worker_count) {
    if (scheduler.initialized) return 1;
    if (worker_count <= 0) {
        long ncpu = sysconf(_SC_NPROCESSORS_ONLN);
        worker_count = ncpu > 0 ? (int)ncpu : 1;
        if (worker_count < 1) worker_count = 1;
    }
    mira_lock_init(&scheduler.lock);
    mira_lock_init(&scheduler.task_pool_lock);
    mira_cond_init(&scheduler.ready);
    memset(&scheduler.free_tasks, 0, sizeof(scheduler.free_tasks));
    if (!worker_tls_ok && pthread_key_create(&worker_tls, NULL) == 0)
        worker_tls_ok = 1;
    if (!task_tls_ok && pthread_key_create(&task_tls, NULL) == 0)
        task_tls_ok = 1;
    if (!cache_tls_ok && pthread_key_create(&cache_tls, NULL) == 0)
        cache_tls_ok = 1;
    if (!worker_tls_ok || !task_tls_ok || !cache_tls_ok) {
        return 0;
    }
    scheduler.idle_event = mira_event_create(1);
    if (!scheduler.idle_event) {
        return 0;
    }
    scheduler.workers = (MiraWorker *)calloc((size_t)worker_count, sizeof(MiraWorker));
    if (!scheduler.workers) {
        mira_event_destroy(scheduler.idle_event);
        scheduler.idle_event = NULL;
        return 0;
    }
    scheduler.worker_count = worker_count;
    for (int i = 0; i < worker_count; ++i)
        mira_lock_init(&scheduler.workers[i].fast_lock);
    scheduler.initialized = 1;
    if (sched_trace())
        fprintf(stderr, "[sched] init worker_count=%d\n", worker_count);
    for (int i = 0; i < worker_count; ++i) {
        if (pthread_create(&scheduler.workers[i].thread, NULL,
                           scheduler_worker, &scheduler.workers[i]) != 0) {
            scheduler.worker_count = i;
            mira_sched_shutdown();
            return 0;
        }
    }
    return 1;
}

static int submit_task(MiraTaskFn function, void *context,
                       MiraJoinHandle *join_handle, int direct) {
    if (!function) return 0;
    if (!scheduler.initialized && !mira_sched_init(0)) return 0;
    MiraTask *task = task_acquire();
    if (!task) return 0;
    task->function = function;
    task->context = context;
    task->state = MIRA_TASK_NEW;
    task->join_handle = join_handle;
    task->direct = direct;

    if (sched_trace())
        fprintf(stderr, "[sched] submit_task direct=%d function=%p\n",
                direct, (void *)(uintptr_t)function);
    if (direct) {
        if (scheduler.stopping || scheduler.worker_count <= 0) {
            task_release(task);
            return 0;
        }
        /* 与 Windows 版相同的"同提交者固定同一 inbox"策略:
         * worker 线程投自己的队列;非 worker 线程轮转(Round-Robin)
         * 分散到各 worker inbox,避免全部任务挤进同一把锁、
         * 其他 worker 全靠窃取。 */
        MiraWorker *origin = get_current_worker();
        MiraWorker *target = origin ? origin : &scheduler.workers[
            (unsigned long)__atomic_fetch_add(&scheduler.next_fast_worker, 1,
                __ATOMIC_RELAXED) % (unsigned)scheduler.worker_count];
        if (__atomic_add_fetch(&scheduler.active_tasks, 1,
                __ATOMIC_ACQ_REL) == 1)
            mira_event_reset(scheduler.idle_event);
        mira_lock_acquire(&target->fast_lock);
        queue_push(&target->fast_head, &target->fast_tail, task);
        mira_lock_release(&target->fast_lock);
        /* 按需唤醒:worker 忙时(无等待者)完全免 signal,省掉每任务
         * 一次 condvar 原子/屏障;有等待者且无 pending 唤醒时才 signal。 */
        if (__atomic_load_n(&scheduler.waiting_workers, __ATOMIC_ACQUIRE) > 0 &&
            __atomic_exchange_n(&scheduler.wake_pending, 1, __ATOMIC_ACQ_REL) == 0)
            mira_cond_signal(&scheduler.ready);
        return 1;
    }

    mira_lock_acquire(&scheduler.lock);
    if (scheduler.stopping) {
        mira_lock_release(&scheduler.lock);
        task_release(task);
        return 0;
    }
    if (__atomic_add_fetch(&scheduler.active_tasks, 1,
            __ATOMIC_ACQ_REL) == 1)
        mira_event_reset(scheduler.idle_event);
    queue_push(&scheduler.new_head, &scheduler.new_tail, task);
    if (__atomic_load_n(&scheduler.waiting_workers, __ATOMIC_ACQUIRE) > 0 &&
        __atomic_exchange_n(&scheduler.wake_pending, 1, __ATOMIC_ACQ_REL) == 0)
        mira_cond_signal(&scheduler.ready);
    mira_lock_release(&scheduler.lock);
    return 1;
}

static MiraJoinHandle *start_handle_mode(MiraTaskFn function, void *context,
                                         int direct) {
    if (!function) return NULL;
    MiraJoinHandle *handle = (MiraJoinHandle *)calloc(1, sizeof(*handle));
    if (!handle) return NULL;
    mira_lock_init(&handle->lock);
    handle->event = mira_event_create(0);
    handle->references = 2;
    if (!handle->event) {
        free(handle);
        return NULL;
    }
    __atomic_add_fetch(&join_handle_creations, 1, __ATOMIC_RELAXED);
    if (!submit_task(function, context, handle, direct)) {
        __atomic_sub_fetch(&join_handle_creations, 1, __ATOMIC_RELAXED);
        mira_event_destroy(handle->event);
        free(handle);
        return NULL;
    }
    return handle;
}

MiraJoinHandle *mira_go_start_handle(MiraTaskFn function, void *context) {
    return start_handle_mode(function, context, 0);
}

int mira_go_start(MiraTaskFn function, void *context) {
    return submit_task(function, context, NULL, 0);
}

int mira_go_start_fast(MiraTaskFn function, void *context) {
    return submit_task(function, context, NULL, 1);
}

void mira_task_join(MiraJoinHandle *handle) {
    if (!handle) return;
    MiraTask *current_task = get_current_task();
    if (!current_task) {
        mira_event_wait(handle->event);
        return;
    }
    MiraJoinWaiter waiter;
    waiter.task = (MiraTaskHandle)current_task;
    mira_lock_acquire(&handle->lock);
    if (handle->completed) {
        mira_lock_release(&handle->lock);
        return;
    }
    waiter.next = handle->waiters;
    handle->waiters = &waiter;
    mira_task_prepare_park();
    mira_lock_release(&handle->lock);
    mira_task_park();
}

void mira_task_yield(void) {
    MiraTask *current_task = get_current_task();
    MiraWorker *current_worker = get_current_worker();
    if (!current_task || !current_worker) {
        sched_yield();
        return;
    }
    current_task->state = MIRA_TASK_READY;
    if (current_task->carrier)
        mira_fiber_switch(current_task->carrier->handle, current_worker->scheduler_fiber);
    else
        sched_yield(); /* direct 任务无 fiber:等价 Windows 的切回自身 */
}

MiraTaskHandle mira_task_current(void) {
    return (MiraTaskHandle)get_current_task();
}

void mira_task_prepare_park(void) {
    MiraTask *current_task = get_current_task();
    if (!current_task) return;
    mira_lock_acquire(&scheduler.lock);
    if (current_task->state == MIRA_TASK_RUNNING)
        current_task->state = MIRA_TASK_PARKING;
    mira_lock_release(&scheduler.lock);
}

void mira_task_park(void) {
    MiraTask *current_task = get_current_task();
    MiraWorker *current_worker = get_current_worker();
    if (!current_task || !current_worker) return;
    int should_switch = 0;
    mira_lock_acquire(&scheduler.lock);
    if (current_task->state == MIRA_TASK_PARKING) {
        current_task->state = MIRA_TASK_PARKED;
        should_switch = 1;
    } else if (current_task->state == MIRA_TASK_READY) {
        current_task->state = MIRA_TASK_RUNNING;
    }
    mira_lock_release(&scheduler.lock);
    if (should_switch && current_task->carrier) {
        mira_fiber_dump("task->sched(park)", current_task->carrier->handle);
        mira_fiber_switch(current_task->carrier->handle, current_worker->scheduler_fiber);
    }
}

void mira_task_wake(MiraTaskHandle handle) {
    MiraTask *task = (MiraTask *)handle;
    if (!task || !task->owner) return;
    mira_lock_acquire(&scheduler.lock);
    if (task->state == MIRA_TASK_PARKING) {
        task->state = MIRA_TASK_READY;
    } else if (task->state == MIRA_TASK_PARKED) {
        task->state = MIRA_TASK_READY;
        queue_push(&task->owner->ready_head, &task->owner->ready_tail, task);
        mira_cond_broadcast(&scheduler.ready);
    }
    mira_lock_release(&scheduler.lock);
}

void mira_sched_wait_all(void) {
    if (!scheduler.initialized) return;
    for (;;) {
        long done = 0;
        for (int i = 0; i < scheduler.worker_count; ++i)
            done += __atomic_load_n(&scheduler.workers[i].completed,
                                    __ATOMIC_RELAXED);
        if (done == __atomic_load_n(&scheduler.active_tasks, __ATOMIC_ACQUIRE))
            return;
        /* 未完成:复位事件防残留 set 造成虚假唤醒,复位后再核对
         * 一次(防"完成者 set 恰在 reset 前"丢失唤醒),然后阻塞。 */
        mira_event_reset(scheduler.idle_event);
        done = 0;
        for (int i = 0; i < scheduler.worker_count; ++i)
            done += __atomic_load_n(&scheduler.workers[i].completed,
                                    __ATOMIC_RELAXED);
        if (done == __atomic_load_n(&scheduler.active_tasks, __ATOMIC_ACQUIRE))
            return;
        mira_event_wait(scheduler.idle_event);
    }
}

int mira_sched_worker_count(void) {
    return scheduler.initialized ? scheduler.worker_count : 0;
}

long long mira_sched_join_handle_creations(void) {
    return __atomic_load_n(&join_handle_creations, __ATOMIC_RELAXED);
}

long long mira_sched_task_allocations(void) {
    return __atomic_load_n(&task_allocations, __ATOMIC_RELAXED);
}

long long mira_sched_fiber_creations(void) {
    return __atomic_load_n(&fiber_creations, __ATOMIC_RELAXED);
}

long long mira_sched_fast_global_lock_acquisitions(void) {
    return __atomic_load_n(&fast_global_lock_acquisitions, __ATOMIC_RELAXED);
}

void mira_sched_shutdown(void) {
    if (!scheduler.initialized) return;
    mira_sched_wait_all();
    mira_lock_acquire(&scheduler.lock);
    scheduler.stopping = 1;
    mira_cond_broadcast(&scheduler.ready);
    mira_lock_release(&scheduler.lock);
    for (int i = 0; i < scheduler.worker_count; ++i)
        pthread_join(scheduler.workers[i].thread, NULL);
    free(scheduler.workers);
    MiraTaskBlock *block = scheduler.task_blocks;
    while (block) {
        MiraTaskBlock *next = block->next;
        free(block);
        block = next;
    }
    mira_event_destroy(scheduler.idle_event);
    if (worker_tls_ok) { pthread_key_delete(worker_tls); worker_tls_ok = 0; }
    if (task_tls_ok) { pthread_key_delete(task_tls); task_tls_ok = 0; }
    if (cache_tls_ok) { pthread_key_delete(cache_tls); cache_tls_ok = 0; }
    scheduler.workers = NULL;
    scheduler.idle_event = NULL;
    scheduler.new_head = scheduler.new_tail = NULL;
    scheduler.task_blocks = NULL;
    scheduler.worker_count = 0;
    scheduler.stopping = 0;
    scheduler.active_tasks = 0;
    join_handle_creations = 0;
    task_allocations = 0;
    fiber_creations = 0;
    fast_global_lock_acquisitions = 0;
    scheduler.next_fast_worker = 0;
    scheduler.initialized = 0;
}

typedef struct {
    void (*function)(void);
} MiraNoArgTask;

static void run_no_arg_task(void *opaque) {
    MiraNoArgTask *task = (MiraNoArgTask *)opaque;
    void (*function)(void) = task->function;
    free(task);
    function();
}

long long mira_go_start0(long long function_ptr) {
    if (!function_ptr) return 0;
    MiraNoArgTask *task = (MiraNoArgTask *)malloc(sizeof(*task));
    if (!task) return 0;
    task->function = (void (*)(void))(uintptr_t)function_ptr;
    MiraJoinHandle *handle = mira_go_start_handle(run_no_arg_task, task);
    if (!handle) {
        free(task);
        return 0;
    }
    return (long long)(uintptr_t)handle;
}

long long mira_go_start_fast0(long long function_ptr) {
    if (!function_ptr) return 0;
    MiraNoArgTask *task = (MiraNoArgTask *)malloc(sizeof(*task));
    if (!task) return 0;
    task->function = (void (*)(void))(uintptr_t)function_ptr;
    MiraJoinHandle *handle = start_handle_mode(run_no_arg_task, task, 1);
    if (!handle) {
        free(task);
        return 0;
    }
    return (long long)(uintptr_t)handle;
}

void mira_go_join(long long handle_value) {
    MiraJoinHandle *handle = (MiraJoinHandle *)(uintptr_t)handle_value;
    if (!handle) return;
    mira_task_join(handle);
    mira_join_handle_release(handle);
}

void mira_go_yield(void) {
    mira_task_yield();
}

void mira_go_wait_all(void) {
    mira_sched_wait_all();
}
