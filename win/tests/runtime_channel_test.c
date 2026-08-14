#include "../runtime/rt_sched.h"
#include "../runtime/rt_channel.h"
#include <stdio.h>

typedef struct {
    MiraChannel *channel;
    long long value;
} SendContext;

static void send_task(void *opaque) {
    SendContext *context = (SendContext *)opaque;
    if (!mira_channel_send(context->channel, context->value))
        context->value = -1;
}

int main(void) {
    if (!mira_sched_init(2)) return 10;

    MiraChannel *unbuffered = mira_channel_new(0);
    SendContext context = { unbuffered, 42 };
    if (!mira_go_start(send_task, &context)) return 11;
    long long value = 0;
    if (!mira_channel_recv(unbuffered, &value) || value != 42) return 12;
    mira_sched_wait_all();
    mira_channel_close(unbuffered);
    if (mira_channel_recv(unbuffered, &value)) return 13;
    mira_channel_free(unbuffered);

    MiraChannel *buffered = mira_channel_new(2);
    if (!mira_channel_send(buffered, 7)) return 14;
    if (!mira_channel_send(buffered, 9)) return 15;
    if (!mira_channel_recv(buffered, &value) || value != 7) return 16;
    if (!mira_channel_recv(buffered, &value) || value != 9) return 17;
    mira_channel_close(buffered);
    if (mira_channel_send(buffered, 11)) return 18;
    mira_channel_free(buffered);

    mira_sched_shutdown();
    puts("runtime_channel_test: PASS");
    return 0;
}
