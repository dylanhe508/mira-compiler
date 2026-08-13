#ifndef MIRA_RT_CHANNEL_H
#define MIRA_RT_CHANNEL_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MiraChannel MiraChannel;

MiraChannel *mira_channel_new(long long capacity);
int mira_channel_send(MiraChannel *channel, long long value);
int mira_channel_recv(MiraChannel *channel, long long *value);
int mira_channel_close(MiraChannel *channel);
void mira_channel_free(MiraChannel *channel);
int mira_channel_try_send(MiraChannel *channel, long long value);
int mira_channel_try_recv(MiraChannel *channel, long long *value);

typedef enum {
    MIRA_SELECT_RECV,
    MIRA_SELECT_SEND
} MiraSelectKind;

typedef struct {
    MiraSelectKind kind;
    MiraChannel *channel;
    long long value;
} MiraSelectCase;

int mira_channel_select(MiraSelectCase *cases, int count, int has_default,
                        int *selected_index, long long *received_value);

/* Scalar adapters used by generated Mira code. */
long long mira_channel_new_value(long long capacity);
long long mira_channel_send_value(long long channel, long long value);
long long mira_channel_recv_value(long long channel);
long long mira_channel_close_value(long long channel);
void mira_channel_free_value(long long channel);

#ifdef __cplusplus
}
#endif

#endif
