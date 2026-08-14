#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include "rt_sched.h"
#include <windows.h>
#include <stdint.h>
#include <stdlib.h>

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

struct MiraJoinHandle {
    SRWLOCK lock;
    HANDLE event;
    volatile LONG references;
    int completed;
    MiraJoinWaiter *waiters;
};

typedef struct __attribute__((aligned(16))) MiraTask {
    SLIST_ENTRY pool_entry;
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
    HANDLE thread;
    void *scheduler_fiber;
    SRWLOCK fast_lock;
    MiraTask *fast_head;
    MiraTask *fast_tail;
    MiraTask *ready_head;
    MiraTask *ready_tail;
    MiraFiber *free_fibers;
};

struct MiraFiber {
    void *handle;
    MiraWorker *owner;
    MiraTask *task;
    MiraFiber *next;
};

typedef struct {
    SRWLOCK lock;
    CONDITION_VARIABLE ready;
    MiraTask *new_head;
    MiraTask *new_tail;
    SLIST_HEADER free_tasks;
    SRWLOCK task_pool_lock;
    MiraTaskBlock *task_blocks;
    MiraWorker *workers;
    HANDLE idle_event;
    volatile LONG active_tasks;
    volatile LONG next_fast_worker;
    int worker_count;
    int stopping;
    int initialized;
} MiraScheduler;

static MiraScheduler scheduler;
static DWORD worker_tls = TLS_OUT_OF_INDEXES;
static DWORD task_tls = TLS_OUT_OF_INDEXES;
static volatile LONG64 join_handle_creations;
static volatile LONG64 task_allocations;
static volatile LONG64 fiber_creations;
static volatile LONG64 fast_global_lock_acquisitions;

static MiraTask *task_acquire(void) {
    PSLIST_ENTRY pooled = InterlockedPopEntrySList(&scheduler.free_tasks);
    if (pooled) {
        MiraTask *task = CONTAINING_RECORD(pooled, MiraTask, pool_entry);
        ZeroMemory(task, sizeof(*task));
        return task;
    }

    AcquireSRWLockExclusive(&scheduler.task_pool_lock);
    pooled = InterlockedPopEntrySList(&scheduler.free_tasks);
    if (pooled) {
        ReleaseSRWLockExclusive(&scheduler.task_pool_lock);
        MiraTask *task = CONTAINING_RECORD(pooled, MiraTask, pool_entry);
        ZeroMemory(task, sizeof(*task));
        return task;
    }

    MiraTaskBlock *block = (MiraTaskBlock *)calloc(1, sizeof(*block));
    if (!block) {
        ReleaseSRWLockExclusive(&scheduler.task_pool_lock);
        return NULL;
    }
    block->next = scheduler.task_blocks;
    scheduler.task_blocks = block;
    for (int i = 1; i < MIRA_TASKS_PER_BLOCK; ++i)
        InterlockedPushEntrySList(&scheduler.free_tasks,
                                 &block->tasks[i].pool_entry);
    InterlockedAdd64(&task_allocations, MIRA_TASKS_PER_BLOCK);
    MiraTask *task = &block->tasks[0];
    ReleaseSRWLockExclusive(&scheduler.task_pool_lock);
    return task;
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
    InterlockedPushEntrySList(&scheduler.free_tasks, &task->pool_entry);
}

static MiraWorker *get_current_worker(void) {
    return worker_tls == TLS_OUT_OF_INDEXES ? NULL :
        (MiraWorker *)TlsGetValue(worker_tls);
}

static MiraTask *get_current_task(void) {
    return task_tls == TLS_OUT_OF_INDEXES ? NULL :
        (MiraTask *)TlsGetValue(task_tls);
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
    AcquireSRWLockExclusive(&worker->fast_lock);
    MiraTask *task = queue_pop(&worker->fast_head, &worker->fast_tail);
    ReleaseSRWLockExclusive(&worker->fast_lock);
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
    if (InterlockedDecrement(&handle->references) == 0) {
        CloseHandle(handle->event);
        free(handle);
    }
}

static void complete_join_handle(MiraJoinHandle *handle) {
    if (!handle) return;
    AcquireSRWLockExclusive(&handle->lock);
    handle->completed = 1;
    MiraJoinWaiter *waiter = handle->waiters;
    handle->waiters = NULL;
    SetEvent(handle->event);
    while (waiter) {
        MiraJoinWaiter *next = waiter->next;
        mira_task_wake(waiter->task);
        waiter = next;
    }
    ReleaseSRWLockExclusive(&handle->lock);
    mira_join_handle_release(handle);
}

static VOID WINAPI task_fiber_entry(void *opaque) {
    MiraFiber *carrier = (MiraFiber *)opaque;
    for (;;) {
        MiraTask *task = carrier->task;
        task->function(task->context);
        task->state = MIRA_TASK_DONE;
        SwitchToFiber(carrier->owner->scheduler_fiber);
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
    carrier->handle = CreateFiber(64 * 1024, task_fiber_entry, carrier);
    if (!carrier->handle) {
        free(carrier);
        return NULL;
    }
    InterlockedIncrement64(&fiber_creations);
    return carrier;
}

static void worker_release_fiber(MiraWorker *worker, MiraFiber *carrier) {
    carrier->task = NULL;
    carrier->next = worker->free_fibers;
    worker->free_fibers = carrier;
}

static DWORD WINAPI scheduler_worker(void *opaque) {
    MiraWorker *worker = (MiraWorker *)opaque;
    TlsSetValue(worker_tls, worker);
    worker->scheduler_fiber = ConvertThreadToFiber(NULL);
    if (!worker->scheduler_fiber) return 1;

    for (;;) {
        MiraTask *task = worker_pop_fast(worker);
        if (!task) task = worker_steal_fast(worker);
        AcquireSRWLockExclusive(&scheduler.lock);
        if (!task) task = queue_pop(&scheduler.new_head, &scheduler.new_tail);
        if (!task) task = queue_pop(&worker->ready_head, &worker->ready_tail);
        while (!task && !scheduler.stopping) {
            SleepConditionVariableSRW(&scheduler.ready, &scheduler.lock, 1, 0);
            task = queue_pop(&scheduler.new_head, &scheduler.new_tail);
            if (!task) task = queue_pop(&worker->ready_head, &worker->ready_tail);
            if (!task) {
                ReleaseSRWLockExclusive(&scheduler.lock);
                task = worker_pop_fast(worker);
                if (!task) task = worker_steal_fast(worker);
                AcquireSRWLockExclusive(&scheduler.lock);
            }
        }
        if (!task && scheduler.stopping) {
            ReleaseSRWLockExclusive(&scheduler.lock);
            MiraFiber *carrier = worker->free_fibers;
            while (carrier) {
                MiraFiber *next = carrier->next;
                DeleteFiber(carrier->handle);
                free(carrier);
                carrier = next;
            }
            worker->free_fibers = NULL;
            ConvertFiberToThread();
            return 0;
        }
        if (!task->owner && !task->direct) {
            task->owner = worker;
            task->carrier = worker_acquire_fiber(worker);
            if (task->carrier) task->carrier->task = task;
            else task->state = MIRA_TASK_DONE;
        }
        if (task->direct) {
            task->owner = worker;
            task->state = MIRA_TASK_RUNNING;
        } else {
            task->state = task->carrier ? MIRA_TASK_RUNNING : MIRA_TASK_DONE;
        }
        ReleaseSRWLockExclusive(&scheduler.lock);

        TlsSetValue(task_tls, task);
        if (task->direct) {
            task->function(task->context);
            task->state = MIRA_TASK_DONE;
        } else if (task->carrier) {
            SwitchToFiber(task->carrier->handle);
        }
        TlsSetValue(task_tls, NULL);

        if (task->state == MIRA_TASK_READY) {
            AcquireSRWLockExclusive(&scheduler.lock);
            queue_push(&worker->ready_head, &worker->ready_tail, task);
            WakeConditionVariable(&scheduler.ready);
            ReleaseSRWLockExclusive(&scheduler.lock);
        } else if (task->state == MIRA_TASK_PARKED) {
            /* A channel or select waiter owns the task until wake-up. */
        } else {
            complete_join_handle(task->join_handle);
            if (task->carrier) {
                worker_release_fiber(worker, task->carrier);
                task->carrier = NULL;
            }
            task_release(task);
            if (InterlockedDecrement(&scheduler.active_tasks) == 0)
                SetEvent(scheduler.idle_event);
        }
    }
}

int mira_sched_init(int worker_count) {
    if (scheduler.initialized) return 1;
    if (worker_count <= 0) {
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        worker_count = (int)info.dwNumberOfProcessors;
        if (worker_count < 1) worker_count = 1;
    }
    InitializeSRWLock(&scheduler.lock);
    InitializeSRWLock(&scheduler.task_pool_lock);
    InitializeConditionVariable(&scheduler.ready);
    InitializeSListHead(&scheduler.free_tasks);
    worker_tls = TlsAlloc();
    task_tls = TlsAlloc();
    if (worker_tls == TLS_OUT_OF_INDEXES || task_tls == TLS_OUT_OF_INDEXES) {
        if (worker_tls != TLS_OUT_OF_INDEXES) TlsFree(worker_tls);
        if (task_tls != TLS_OUT_OF_INDEXES) TlsFree(task_tls);
        worker_tls = task_tls = TLS_OUT_OF_INDEXES;
        return 0;
    }
    scheduler.idle_event = CreateEventA(NULL, TRUE, TRUE, NULL);
    if (!scheduler.idle_event) {
        TlsFree(worker_tls);
        TlsFree(task_tls);
        worker_tls = task_tls = TLS_OUT_OF_INDEXES;
        return 0;
    }
    scheduler.workers = (MiraWorker *)calloc((size_t)worker_count, sizeof(MiraWorker));
    if (!scheduler.workers) {
        CloseHandle(scheduler.idle_event);
        scheduler.idle_event = NULL;
        return 0;
    }
    scheduler.worker_count = worker_count;
    for (int i = 0; i < worker_count; ++i)
        InitializeSRWLock(&scheduler.workers[i].fast_lock);
    scheduler.initialized = 1;
    for (int i = 0; i < worker_count; ++i) {
        scheduler.workers[i].thread =
            CreateThread(NULL, 0, scheduler_worker, &scheduler.workers[i], 0, NULL);
        if (!scheduler.workers[i].thread) {
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

    if (direct) {
        if (scheduler.stopping || scheduler.worker_count <= 0) {
            task_release(task);
            return 0;
        }
        /* Keep one producer on one inbox.  Round-robin submission makes the
         * producer contend with every worker lock and creates severe tail
         * latency for tiny tasks.  Other workers already steal safely. */
        MiraWorker *origin = get_current_worker();
        MiraWorker *target = origin ? origin : &scheduler.workers[
            (unsigned long)GetCurrentThreadId() %
            (unsigned)scheduler.worker_count];
        if (InterlockedIncrement(&scheduler.active_tasks) == 1)
            ResetEvent(scheduler.idle_event);
        AcquireSRWLockExclusive(&target->fast_lock);
        queue_push(&target->fast_head, &target->fast_tail, task);
        ReleaseSRWLockExclusive(&target->fast_lock);
        WakeConditionVariable(&scheduler.ready);
        return 1;
    }

    AcquireSRWLockExclusive(&scheduler.lock);
    if (scheduler.stopping) {
        ReleaseSRWLockExclusive(&scheduler.lock);
        task_release(task);
        return 0;
    }
    if (InterlockedIncrement(&scheduler.active_tasks) == 1)
        ResetEvent(scheduler.idle_event);
    queue_push(&scheduler.new_head, &scheduler.new_tail, task);
    WakeConditionVariable(&scheduler.ready);
    ReleaseSRWLockExclusive(&scheduler.lock);
    return 1;
}

static MiraJoinHandle *start_handle_mode(MiraTaskFn function, void *context,
                                         int direct) {
    if (!function) return NULL;
    MiraJoinHandle *handle = (MiraJoinHandle *)calloc(1, sizeof(*handle));
    if (!handle) return NULL;
    InitializeSRWLock(&handle->lock);
    handle->event = CreateEventA(NULL, TRUE, FALSE, NULL);
    handle->references = 2;
    if (!handle->event) {
        free(handle);
        return NULL;
    }
    InterlockedIncrement64(&join_handle_creations);
    if (!submit_task(function, context, handle, direct)) {
        InterlockedDecrement64(&join_handle_creations);
        CloseHandle(handle->event);
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
        WaitForSingleObject(handle->event, INFINITE);
        return;
    }
    MiraJoinWaiter waiter;
    waiter.task = (MiraTaskHandle)current_task;
    AcquireSRWLockExclusive(&handle->lock);
    if (handle->completed) {
        ReleaseSRWLockExclusive(&handle->lock);
        return;
    }
    waiter.next = handle->waiters;
    handle->waiters = &waiter;
    mira_task_prepare_park();
    ReleaseSRWLockExclusive(&handle->lock);
    mira_task_park();
}

void mira_task_yield(void) {
    MiraTask *current_task = get_current_task();
    MiraWorker *current_worker = get_current_worker();
    if (!current_task || !current_worker) {
        SwitchToThread();
        return;
    }
    current_task->state = MIRA_TASK_READY;
    SwitchToFiber(current_worker->scheduler_fiber);
}

MiraTaskHandle mira_task_current(void) {
    return (MiraTaskHandle)get_current_task();
}

void mira_task_prepare_park(void) {
    MiraTask *current_task = get_current_task();
    if (!current_task) return;
    AcquireSRWLockExclusive(&scheduler.lock);
    if (current_task->state == MIRA_TASK_RUNNING)
        current_task->state = MIRA_TASK_PARKING;
    ReleaseSRWLockExclusive(&scheduler.lock);
}

void mira_task_park(void) {
    MiraTask *current_task = get_current_task();
    MiraWorker *current_worker = get_current_worker();
    if (!current_task || !current_worker) return;
    int should_switch = 0;
    AcquireSRWLockExclusive(&scheduler.lock);
    if (current_task->state == MIRA_TASK_PARKING) {
        current_task->state = MIRA_TASK_PARKED;
        should_switch = 1;
    } else if (current_task->state == MIRA_TASK_READY) {
        current_task->state = MIRA_TASK_RUNNING;
    }
    ReleaseSRWLockExclusive(&scheduler.lock);
    if (should_switch) SwitchToFiber(current_worker->scheduler_fiber);
}

void mira_task_wake(MiraTaskHandle handle) {
    MiraTask *task = (MiraTask *)handle;
    if (!task || !task->owner) return;
    AcquireSRWLockExclusive(&scheduler.lock);
    if (task->state == MIRA_TASK_PARKING) {
        task->state = MIRA_TASK_READY;
    } else if (task->state == MIRA_TASK_PARKED) {
        task->state = MIRA_TASK_READY;
        queue_push(&task->owner->ready_head, &task->owner->ready_tail, task);
        WakeAllConditionVariable(&scheduler.ready);
    }
    ReleaseSRWLockExclusive(&scheduler.lock);
}

void mira_sched_wait_all(void) {
    if (scheduler.initialized)
        WaitForSingleObject(scheduler.idle_event, INFINITE);
}

int mira_sched_worker_count(void) {
    return scheduler.initialized ? scheduler.worker_count : 0;
}

long long mira_sched_join_handle_creations(void) {
    return (long long)InterlockedCompareExchange64(&join_handle_creations, 0, 0);
}

long long mira_sched_task_allocations(void) {
    return (long long)InterlockedCompareExchange64(&task_allocations, 0, 0);
}

long long mira_sched_fiber_creations(void) {
    return (long long)InterlockedCompareExchange64(&fiber_creations, 0, 0);
}

long long mira_sched_fast_global_lock_acquisitions(void) {
    return (long long)InterlockedCompareExchange64(
        &fast_global_lock_acquisitions, 0, 0);
}

void mira_sched_shutdown(void) {
    if (!scheduler.initialized) return;
    mira_sched_wait_all();
    AcquireSRWLockExclusive(&scheduler.lock);
    scheduler.stopping = 1;
    WakeAllConditionVariable(&scheduler.ready);
    ReleaseSRWLockExclusive(&scheduler.lock);
    for (int i = 0; i < scheduler.worker_count; ++i) {
        if (!scheduler.workers[i].thread) continue;
        WaitForSingleObject(scheduler.workers[i].thread, INFINITE);
        CloseHandle(scheduler.workers[i].thread);
    }
    free(scheduler.workers);
    MiraTaskBlock *block = scheduler.task_blocks;
    while (block) {
        MiraTaskBlock *next = block->next;
        free(block);
        block = next;
    }
    CloseHandle(scheduler.idle_event);
    if (worker_tls != TLS_OUT_OF_INDEXES) TlsFree(worker_tls);
    if (task_tls != TLS_OUT_OF_INDEXES) TlsFree(task_tls);
    worker_tls = task_tls = TLS_OUT_OF_INDEXES;
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
