#ifndef _osapi_apple_h_
#define _osapi_apple_h_ 

#ifdef __APPLE__

#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>

#include <mach/mach_time.h>



/* Timer_t is a WORK IN PROGRESS 
see 
https://developer.apple.com/legacy/library/documentation/Darwin/Reference/ManPages/man3/dispatch_source_create.3.html
http://oss.sgi.com/archives/xfs/2015-09/msg00462.html
Only did enough work to make this thing compile in mac, doesn't actually work ...
*/
#define CLOCK_REALTIME ITIMER_REAL
#define itimerspec itimerval

/*
#define    CLOCK_REALTIME    0x2d4e1588
#define    CLOCK_MONOTONIC   0x0
*/

typedef uint64_t timer_t;
typedef double   timer_c;
typedef clock_id_t clockid_t;


#ifndef SIGRTMAX
#define SIGRTMAX SIGUSR1
#endif

int clock_getres(clockid_t clk_id, struct timespec *res);
int clock_gettime(clockid_t clk_id, struct timespec *tp);
int clock_settime(clockid_t clk_id, const struct timespec *tp);



#endif /* __APPLE__ */
#endif
