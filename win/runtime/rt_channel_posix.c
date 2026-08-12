/* rt_channel_posix.c - POSIX 通道(与 rt_channel.c 的 Windows 版算法完全一致)
 *
 * 移植映射:
 *   SRWLOCK / CONDITION_VARIABLE → rt_sync.h(mira_lock_t / mira_cond_t)
 *   SleepConditionVariableSRW(INFINITE) → mira_cond_wait(无限等待)
 *   GetTickCount → clock_gettime(CLOCK_MONOTONIC) 毫秒
 *   SwitchToThread → sched_yield
 */
#include "rt_channel.h"
#include "rt_sched.h"
#include "rt_sync.h"
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <sched.h>

typedef struct MiraSendWaiter {
    MiraTaskHandle task;
    long long value;
    int delivered;
    struct MiraSendWaiter *next;
} MiraSendWaiter;

typedef struct MiraRecvWaiter {
    MiraTaskHandle task;
    long long *value;
    int delivered;
    struct MiraRecvWaiter *next;
} MiraRecvWaiter;

struct MiraChannel {
    mira_lock_t lock;
    mira_cond_t can_send;
    mira_cond_t can_recv;
    long long *values;
    size_t capacity;
    size_t count;
    size_t head;
    size_t tail;
    long long handoff_value;
    int handoff_ready;
    int waiting_receivers;
    int closed;
    MiraSendWaiter *send_head;
    MiraSendWaiter *send_tail;
    MiraRecvWaiter *recv_head;
    MiraRecvWaiter *recv_tail;
};

static void push_sender(MiraChannel *channel, MiraSendWaiter *waiter) {
    waiter->next = NULL;
    if (channel->send_tail) channel->send_tail->next = waiter;
    else channel->send_head = waiter;
    channel->send_tail = waiter;
}

static MiraSendWaiter *pop_sender(MiraChannel *channel) {
    MiraSendWaiter *waiter = channel->send_head;
    if (!waiter) return NULL;
    channel->send_head = waiter->next;
    if (!channel->send_head) channel->send_tail = NULL;
    waiter->next = NULL;
    return waiter;
}

static void push_receiver(MiraChannel *channel, MiraRecvWaiter *waiter) {
    waiter->next = NULL;
    if (channel->recv_tail) channel->recv_tail->next = waiter;
    else channel->recv_head = waiter;
    channel->recv_tail = waiter;
}

static MiraRecvWaiter *pop_receiver(MiraChannel *channel) {
    MiraRecvWaiter *waiter = channel->recv_head;
    if (!waiter) return NULL;
    channel->recv_head = waiter->next;
    if (!channel->recv_head) channel->recv_tail = NULL;
    waiter->next = NULL;
    return waiter;
}

MiraChannel *mira_channel_new(long long capacity) {
    if (capacity < 0) return NULL;
    MiraChannel *channel = (MiraChannel *)calloc(1, sizeof(*channel));
    if (!channel) return NULL;
    mira_lock_init(&channel->lock);
    mira_cond_init(&channel->can_send);
    mira_cond_init(&channel->can_recv);
    channel->capacity = (size_t)capacity;
    if (channel->capacity) {
        channel->values = (long long *)malloc(channel->capacity * sizeof(long long));
        if (!channel->values) {
            free(channel);
            return NULL;
        }
    }
    return channel;
}

int mira_channel_send(MiraChannel *channel, long long value) {
    if (!channel) return 0;
    mira_lock_acquire(&channel->lock);
    if (channel->closed) {
        mira_lock_release(&channel->lock);
        return 0;
    }
    MiraRecvWaiter *receiver = pop_receiver(channel);
    if (receiver) {
        *receiver->value = value;
        receiver->delivered = 1;
        mira_task_wake(receiver->task);
        mira_lock_release(&channel->lock);
        return 1;
    }
    MiraTaskHandle task = mira_task_current();
    if (task && (!channel->capacity || channel->count == channel->capacity)) {
        MiraSendWaiter waiter = { task, value, 0, NULL };
        push_sender(channel, &waiter);
        mira_task_prepare_park();
        mira_cond_broadcast(&channel->can_recv);
        mira_lock_release(&channel->lock);
        mira_task_park();
        return waiter.delivered;
    }
    if (channel->capacity) {
        while (channel->count == channel->capacity && !channel->closed)
            mira_cond_wait(&channel->can_send, &channel->lock);
        if (channel->closed) {
            mira_lock_release(&channel->lock);
            return 0;
        }
        channel->values[channel->tail] = value;
        channel->tail = (channel->tail + 1) % channel->capacity;
        channel->count++;
        mira_cond_signal(&channel->can_recv);
        mira_lock_release(&channel->lock);
        return 1;
    }

    while (((!channel->waiting_receivers && !channel->recv_head) || channel->handoff_ready) &&
           !channel->closed)
        mira_cond_wait(&channel->can_send, &channel->lock);
    if (channel->closed) {
        mira_lock_release(&channel->lock);
        return 0;
    }
    receiver = pop_receiver(channel);
    if (receiver) {
        *receiver->value = value;
        receiver->delivered = 1;
        mira_task_wake(receiver->task);
        mira_lock_release(&channel->lock);
        return 1;
    }
    channel->handoff_value = value;
    channel->handoff_ready = 1;
    mira_cond_broadcast(&channel->can_recv);
    while (channel->handoff_ready && !channel->closed)
        mira_cond_wait(&channel->can_send, &channel->lock);
    int delivered = !channel->handoff_ready;
    mira_lock_release(&channel->lock);
    return delivered;
}

int mira_channel_recv(MiraChannel *channel, long long *value) {
    if (!channel || !value) return 0;
    mira_lock_acquire(&channel->lock);
    if (channel->capacity) {
        MiraTaskHandle task = mira_task_current();
        if (!channel->count && !channel->closed && task) {
            MiraRecvWaiter waiter = { task, value, 0, NULL };
            push_receiver(channel, &waiter);
            mira_task_prepare_park();
            mira_cond_broadcast(&channel->can_send);
            mira_lock_release(&channel->lock);
            mira_task_park();
            return waiter.delivered;
        }
        while (!channel->count && !channel->closed)
            mira_cond_wait(&channel->can_recv, &channel->lock);
        if (!channel->count) {
            mira_lock_release(&channel->lock);
            return 0;
        }
        *value = channel->values[channel->head];
        channel->head = (channel->head + 1) % channel->capacity;
        channel->count--;
        MiraSendWaiter *sender = pop_sender(channel);
        if (sender) {
            channel->values[channel->tail] = sender->value;
            channel->tail = (channel->tail + 1) % channel->capacity;
            channel->count++;
            sender->delivered = 1;
            mira_task_wake(sender->task);
        }
        mira_cond_signal(&channel->can_send);
        mira_lock_release(&channel->lock);
        return 1;
    }

    MiraSendWaiter *sender = pop_sender(channel);
    if (sender) {
        *value = sender->value;
        sender->delivered = 1;
        mira_task_wake(sender->task);
        mira_lock_release(&channel->lock);
        return 1;
    }
    if (channel->closed) {
        mira_lock_release(&channel->lock);
        return 0;
    }
    MiraTaskHandle task = mira_task_current();
    if (task) {
        MiraRecvWaiter waiter = { task, value, 0, NULL };
        push_receiver(channel, &waiter);
        mira_task_prepare_park();
        mira_cond_broadcast(&channel->can_send);
        mira_lock_release(&channel->lock);
        mira_task_park();
        return waiter.delivered;
    }

    channel->waiting_receivers++;
    mira_cond_signal(&channel->can_send);
    while (!channel->handoff_ready && !channel->send_head && !channel->closed)
        mira_cond_wait(&channel->can_recv, &channel->lock);
    sender = pop_sender(channel);
    if (sender) {
        *value = sender->value;
        sender->delivered = 1;
        channel->waiting_receivers--;
        mira_task_wake(sender->task);
        mira_lock_release(&channel->lock);
        return 1;
    }
    if (!channel->handoff_ready) {
        channel->waiting_receivers--;
        mira_lock_release(&channel->lock);
        return 0;
    }
    *value = channel->handoff_value;
    channel->handoff_ready = 0;
    channel->waiting_receivers--;
    mira_cond_signal(&channel->can_send);
    mira_lock_release(&channel->lock);
    return 1;
}

int mira_channel_close(MiraChannel *channel) {
    if (!channel) return 0;
    mira_lock_acquire(&channel->lock);
    if (channel->closed) {
        mira_lock_release(&channel->lock);
        return 0;
    }
    channel->closed = 1;
    MiraSendWaiter *sender;
    while ((sender = pop_sender(channel)) != NULL)
        mira_task_wake(sender->task);
    MiraRecvWaiter *receiver;
    while ((receiver = pop_receiver(channel)) != NULL)
        mira_task_wake(receiver->task);
    mira_cond_broadcast(&channel->can_send);
    mira_cond_broadcast(&channel->can_recv);
    mira_lock_release(&channel->lock);
    return 1;
}

void mira_channel_free(MiraChannel *channel) {
    if (!channel) return;
    free(channel->values);
    free(channel);
}

int mira_channel_try_send(MiraChannel *channel, long long value) {
    if (!channel) return -1;
    mira_lock_acquire(&channel->lock);
    if (channel->closed) {
        mira_lock_release(&channel->lock);
        return -1;
    }
    MiraRecvWaiter *receiver = pop_receiver(channel);
    if (receiver) {
        *receiver->value = value;
        receiver->delivered = 1;
        mira_task_wake(receiver->task);
        mira_lock_release(&channel->lock);
        return 1;
    }
    if (channel->capacity && channel->count < channel->capacity) {
        channel->values[channel->tail] = value;
        channel->tail = (channel->tail + 1) % channel->capacity;
        channel->count++;
        mira_cond_signal(&channel->can_recv);
        mira_lock_release(&channel->lock);
        return 1;
    }
    mira_lock_release(&channel->lock);
    return 0;
}

int mira_channel_try_recv(MiraChannel *channel, long long *value) {
    if (!channel || !value) return -1;
    mira_lock_acquire(&channel->lock);
    if (channel->count) {
        *value = channel->values[channel->head];
        channel->head = (channel->head + 1) % channel->capacity;
        channel->count--;
        MiraSendWaiter *sender = pop_sender(channel);
        if (sender) {
            channel->values[channel->tail] = sender->value;
            channel->tail = (channel->tail + 1) % channel->capacity;
            channel->count++;
            sender->delivered = 1;
            mira_task_wake(sender->task);
        }
        mira_cond_signal(&channel->can_send);
        mira_lock_release(&channel->lock);
        return 1;
    }
    MiraSendWaiter *sender = pop_sender(channel);
    if (sender) {
        *value = sender->value;
        sender->delivered = 1;
        mira_task_wake(sender->task);
        mira_lock_release(&channel->lock);
        return 1;
    }
    int result = channel->closed ? -1 : 0;
    mira_lock_release(&channel->lock);
    return result;
}

int mira_channel_select(MiraSelectCase *cases, int count, int has_default,
                        int *selected_index, long long *received_value) {
    if (!cases || count <= 0 || !selected_index) return -1;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    unsigned seed = (unsigned)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000) ^
                    (unsigned)(uintptr_t)mira_task_current();
    for (;;) {
        int closed_count = 0;
        int start = (int)(seed++ % (unsigned)count);
        for (int step = 0; step < count; ++step) {
            int index = (start + step) % count;
            int result;
            if (cases[index].kind == MIRA_SELECT_RECV) {
                long long value = 0;
                result = mira_channel_try_recv(cases[index].channel, &value);
                if (result == 1) {
                    *selected_index = index;
                    if (received_value) *received_value = value;
                    return 1;
                }
            } else {
                result = mira_channel_try_send(cases[index].channel, cases[index].value);
                if (result == 1) {
                    *selected_index = index;
                    return 1;
                }
            }
            if (result < 0) closed_count++;
        }
        if (has_default) {
            *selected_index = -1;
            return 0;
        }
        if (closed_count == count) {
            *selected_index = -1;
            return -1;
        }
        if (mira_task_current()) mira_task_yield();
        else sched_yield();
    }
}

long long mira_channel_new_value(long long capacity) {
    return (long long)(uintptr_t)mira_channel_new(capacity);
}

long long mira_channel_send_value(long long channel_value, long long value) {
    return mira_channel_send((MiraChannel *)(uintptr_t)channel_value, value);
}

long long mira_channel_recv_value(long long channel_value) {
    long long value = 0;
    if (!mira_channel_recv((MiraChannel *)(uintptr_t)channel_value, &value))
        return 0;
    return value;
}

long long mira_channel_close_value(long long channel_value) {
    return mira_channel_close((MiraChannel *)(uintptr_t)channel_value);
}

void mira_channel_free_value(long long channel_value) {
    mira_channel_free((MiraChannel *)(uintptr_t)channel_value);
}
