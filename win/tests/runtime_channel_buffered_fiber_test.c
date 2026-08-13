#include "rt_sched.h"
#include "rt_channel.h"
#include <stdio.h>

static MiraChannel *channel;
static long long received;

static void receiver(void *unused) {
    (void)unused;
    mira_channel_recv(channel, &received);
}

static void sender(void *unused) {
    (void)unused;
    mira_channel_send(channel, 99);
}

int main(void) {
    channel = mira_channel_new(1);
    if (!channel || !mira_sched_init(1)) return 1;
    mira_go_start(receiver, NULL);
    mira_go_start(sender, NULL);
    mira_sched_wait_all();
    mira_sched_shutdown();
    mira_channel_free(channel);
    if (received != 99) return 2;
    puts("runtime_channel_buffered_fiber_test: PASS");
    return 0;
}
