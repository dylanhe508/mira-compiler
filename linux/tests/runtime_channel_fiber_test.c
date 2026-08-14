#include "../runtime/rt_sched.h"
#include "../runtime/rt_channel.h"
#include <stdio.h>

static MiraChannel *channel;
static long long received;

static void sender(void *unused) {
    (void)unused;
    mira_channel_send(channel, 42);
}

static void receiver(void *unused) {
    (void)unused;
    mira_channel_recv(channel, &received);
}

int main(void) {
    if (!mira_sched_init(1)) return 10;
    channel = mira_channel_new(0);
    if (!channel) return 11;
    if (!mira_go_start(sender, NULL)) return 12;
    if (!mira_go_start(receiver, NULL)) return 13;
    mira_sched_wait_all();
    mira_channel_close(channel);
    mira_channel_free(channel);
    mira_sched_shutdown();
    if (received != 42) return 14;
    puts("runtime_channel_fiber_test: PASS");
    return 0;
}
