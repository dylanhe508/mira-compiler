#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int64_t clock_ns(void) {
    LARGE_INTEGER c, f; QueryPerformanceCounter(&c); QueryPerformanceFrequency(&f);
    return (c.QuadPart / f.QuadPart) * 1000000000LL +
           (c.QuadPart % f.QuadPart) * 1000000000LL / f.QuadPart;
}
static int64_t lcg(int64_t n) {
    uint64_t x=1,sum=0; for(int64_t i=0;i<n;i++){x=x*6364136223846793005ULL+1;sum+=x;} return (int64_t)sum;
}
static int64_t divmod_work(int64_t n) {
    int64_t sum=0; for(int64_t i=1;i<=n;i++) sum+=(i*17)%1009+i/1009; return sum;
}
static int64_t branch_pred(int64_t n) {
    int64_t sum=0,k=0; for(int64_t i=0;i<n;i++){if(k<90)sum+=i;else sum-=i;if(++k==100)k=0;} return sum;
}
static int64_t branch_random(int64_t n) {
    uint64_t x=1;int64_t sum=0; for(int64_t i=0;i<n;i++){x=x*6364136223846793005ULL+1;if((int64_t)x<0)sum+=i;else sum-=i;} return sum;
}
static int64_t nested(int64_t outer,int64_t inner) {
    int64_t sum=0; for(int64_t i=0;i<outer;i++)for(int64_t j=0;j<inner;j++)sum+=i+j; return sum;
}
int main(int argc,char **argv) {
    if(argc!=2)return 2; int64_t start=clock_ns(),result;
    if(!strcmp(argv[1],"lcg"))result=lcg(50000000);
    else if(!strcmp(argv[1],"divmod"))result=divmod_work(30000000);
    else if(!strcmp(argv[1],"branch_pred"))result=branch_pred(50000000);
    else if(!strcmp(argv[1],"branch_random"))result=branch_random(50000000);
    else if(!strcmp(argv[1],"nested"))result=nested(10000,10000);
    else return 3;
    int64_t end=clock_ns(); printf("result=\n%" PRId64 "\nelapsed_ns=\n%" PRId64 "\n",result,end-start); return 0;
}
