#ifdef __MACH__
#ifndef OSMAC_STUFF_H
#define OSMAC_STUFF_H

#include <time.h>
#include <sys/time.h>
#include <mach/clock.h>
#include <mach/mach.h>
#include <stdint.h>
#include <signal.h>
#define CLOCK_REALTIME (0)


void clock_gettime(int blah, struct timespec *ts);

/* Mac won't use named queues. */
#define OSAL_SOCKET_QUEUE (1)


#ifndef SIGRTMAX
#define SIGRTMAX SIGUSR1
#endif

typedef uint64_t timer_t;
typedef double   timer_c;


struct itimerspec {
      struct timespec it_value;
      struct timespec it_interval;
};

/*TODO */
inline int clock_getres(int clk, struct timespec *tm);

inline int timer_create(int clockid, struct sigevent * evp, timer_t * timerid);
inline int timer_delete(timer_t timerid);

inline int timer_settime(timer_t timerid, int flags,
                         const struct itimerspec *new_value,
                         struct itimerspec * old_value);
inline int timer_gettime(timer_t timerid, struct itimerspec *curr_value);

#endif
#endif
