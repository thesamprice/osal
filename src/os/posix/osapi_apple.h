#ifndef _osapi_apple_h_
#define _osapi_apple_h_ 

#ifdef __APPLE__

#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>

#include <mach/mach_time.h>

#define CLOCK_REALTIME (0)

/* Timer_t is a WORK IN PROGRESS 
see 
https://developer.apple.com/legacy/library/documentation/Darwin/Reference/ManPages/man3/dispatch_source_create.3.html
http://oss.sgi.com/archives/xfs/2015-09/msg00462.html
Only did enough work to make this thing compile in mac, doesn't actually work ...
*/
#define CLOCK_REALTIME ITIMER_REAL
#define itimerspec itimerval
typedef uint64_t timer_t;
typedef double   timer_c;
typedef clock_id_t clockid_t;


#ifndef SIGRTMAX
#define SIGRTMAX SIGUSR1
#endif

#endif /* __APPLE__ */
#endif