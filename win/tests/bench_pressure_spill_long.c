#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

__attribute__((noinline)) static int64_t hot_step(int64_t x, int64_t salt) {
    return (x * INT64_C(17) + salt) & INT64_MAX;
}

__attribute__((noinline)) static int64_t pressure_call(int64_t x) {
    int64_t a=x+1,b=x*3+2,c=x*5+3,d=x*7+4,e=x*11+5,f=x*13+6;
    int64_t g=x*17+7,h=x*19+8,i=x*23+9,j=x*29+10,k=x*31+11,l=x*37+12;
    int64_t called=hot_step(x,99);
    return (called+a*3+b*5+c*7+d*11+e*13+f*17+g*19+h*23+i*29+
            j*31+k*37+l*41) & INT64_MAX;
}

int main(void) {
    int64_t total=0;
    for (int64_t n=0;n<INT64_C(5000000);++n)
        total=(total+pressure_call(n+24680)) & INT64_MAX;
    printf("%" PRId64 "\n",total);
    return 0;
}
