#include "../runtime/rt_sched.h"
#include "../runtime/rt_channel.h"
#include <stdio.h>

static MiraChannel *selected_channel;
static long long selected_value;

static void selecting_task(void *unused) {
    (void)unused;
    MiraSelectCase cases[2] = {
        { MIRA_SELECT_RECV, selected_channel, 0 },
        { MIRA_SELECT_SEND, selected_channel, 99 }
    };
    int index = -1;
    long long value = 0;
    if (mira_channel_select(cases, 1, 0, &index, &value) == 1 &&
        index == 0)
        selected_value = value;
}

static void sending_task(void *unused) {
    (void)unused;
    mira_channel_send(selected_channel, 42);
}

int main(void) {
    if (!mira_sched_init(1)) return 10;
    selected_channel = mira_channel_new(0);
    if (!selected_channel) return 11;

    MiraSelectCase default_case = { MIRA_SELECT_RECV, selected_channel, 0 };
    int index = 9;
    long long value = 0;
    if (mira_channel_select(&default_case, 1, 1, &index, &value) != 0 ||
        index != -1) return 12;

    if (!mira_go_start(selecting_task, NULL)) return 13;
    if (!mira_go_start(sending_task, NULL)) return 14;
    mira_sched_wait_all();
    mira_channel_close(selected_channel);
    mira_channel_free(selected_channel);
    mira_sched_shutdown();
    if (selected_value != 42) return 15;
    puts("runtime_select_test: PASS");
    return 0;
}
