#ifndef _TIME_H
#define _TIME_H

#include <sys/types.h>

#ifndef _STRUCT_TIMEVAL_DEFINED
#define _STRUCT_TIMEVAL_DEFINED
struct timeval {
    time_t      tv_sec;
    suseconds_t tv_usec;
};
#endif

struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};

unsigned int time(unsigned int *tloc);
int gettimeofday(struct timeval *tv, struct timezone *tz);

#endif