#ifdef __MACH__
#include "osmac_stuff.h"
/*TODO Find someone that made timers and use those....*/
int clock_getres(int clk, struct timespec *tm)
{
  return 0;
}

int timer_create(int clockid, struct sigevent * evp, timer_t * timerid)
{
	return -1;
}
int timer_delete(timer_t timerid)
{
	return 0;
}

int timer_settime(timer_t timerid, int flags,
                         const struct itimerspec *new_value,
                         struct itimerspec * old_value)
{
	return -1;
}
int timer_gettime(timer_t timerid, struct itimerspec *curr_value)
{
	return -1;
}

void clock_gettime(int blah, struct timespec *ts)
{

	clock_serv_t cclock;
	mach_timespec_t mts;
	host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
	clock_get_time(cclock, &mts);
	mach_port_deallocate(mach_task_self(), cclock);
	ts->tv_sec = mts.tv_sec;
	ts->tv_nsec = mts.tv_nsec;
}


#endif
