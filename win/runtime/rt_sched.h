#ifndef MIRA_RT_SCHED_H
#define MIRA_RT_SCHED_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*MiraTaskFn)(void *context);
typedef void *MiraTaskHandle;
typedef struct MiraJoinHandle MiraJoinHandle;

int mira_sched_init(int worker_count);
int mira_go_start(MiraTaskFn function, void *context);
int mira_go_start_fast(MiraTaskFn function, void *context);
MiraJoinHandle *mira_go_start_handle(MiraTaskFn function, void *context);
void mira_task_join(MiraJoinHandle *handle);
void mira_join_handle_release(MiraJoinHandle *handle);
void mira_task_yield(void);
MiraTaskHandle mira_task_current(void);
void mira_task_prepare_park(void);
void mira_task_park(void);
void mira_task_wake(MiraTaskHandle task);
void mira_sched_wait_all(void);
void mira_sched_shutdown(void);
int mira_sched_worker_count(void);
long long mira_sched_join_handle_creations(void);
long long mira_sched_task_allocations(void);
long long mira_sched_fiber_creations(void);
long long mira_sched_fast_global_lock_acquisitions(void);

/* Mira ABI adapters: lambdas currently have the signature void(void), while
 * the scheduler keeps a generic context-bearing C entry point internally. */
long long mira_go_start0(long long function_ptr);
long long mira_go_start_fast0(long long function_ptr);
void mira_go_join(long long handle);
void mira_go_yield(void);
void mira_go_wait_all(void);

#ifdef __cplusplus
}
#endif

#endif
