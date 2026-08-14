/* 纯 C 线程复现:验证 WSL1 线程栈初始 rsp 是否错位。
 * 若此程序也崩在 pthread_cond_wait 内 movaps → WSL1 环境问题,与 Mira 无关。 */
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t c = PTHREAD_COND_INITIALIZER;

void *f(void *p) {
    printf("worker start\n");
    pthread_mutex_lock(&m);
    pthread_cond_wait(&c, &m);
    pthread_mutex_unlock(&m);
    printf("worker end\n");
    return 0;
}

int main(void) {
    pthread_t t;
    int rc = pthread_create(&t, 0, f, 0);
    printf("created rc=%d\n", rc);
    usleep(200000);
    printf("main end\n");
    return 0;
}
