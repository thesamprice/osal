/*
 *  NASA Docket No. GSC-18,370-1, and identified as "Operating System Abstraction Layer"
 *
 *  Copyright (c) 2019 United States Government as represented by
 *  the Administrator of the National Aeronautics and Space Administration.
 *  All Rights Reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/**
 * \file     os-impl-timebase.c
 * \ingroup  posix
 * \author   joseph.p.hickey@nasa.gov
 *
 * This file contains the OSAL Timebase API for POSIX systems.
 *
 * This implementation depends on the POSIX Timer API which may not be available
 * in older versions of the Linux kernel. It was developed and tested on
 * RHEL 5 ./ CentOS 5 with Linux kernel 2.6.18
 */

/****************************************************************************************
                                    INCLUDE FILES
 ***************************************************************************************/


#include "os-impl-timebase.h"
extern "C" {

#include "os-shared-timebase.h"
#include "os-shared-idmap.h"
#include "os-shared-common.h"
}
#include <algorithm>
/****************************************************************************************
                                EXTERNAL FUNCTION PROTOTYPES
 ***************************************************************************************/

/****************************************************************************************
                                INTERNAL FUNCTION PROTOTYPES
 ***************************************************************************************/

static int64 OS_UsecToMili(uint32 usecs);

/****************************************************************************************
                                     DEFINES
 ***************************************************************************************/

/*
 * Prefer to use the MONOTONIC clock if available, as it will not get distrupted by setting
 * the time like the REALTIME clock will.
 */
#ifndef OS_PREFERRED_CLOCK
#ifdef _QT_MONOTONIC_CLOCK
#define OS_PREFERRED_CLOCK CLOCK_MONOTONIC
#else
#define OS_PREFERRED_CLOCK CLOCK_REALTIME
#endif
#endif

/****************************************************************************************
                                     GLOBALS
 ***************************************************************************************/

OS_QTimeBase *OS_impl_timebase_table[OS_MAX_TIMEBASES];

/****************************************************************************************
                                INTERNAL FUNCTIONS
 ***************************************************************************************/



void OS_UsecsToTicks(uint32 usecs, int *ticks)
{
    *ticks = usecs / 1000;
    if(*ticks <= 0)
        *ticks = 1;
}

void OS_QTimeThread::run(){
    OS_TimeBase_CallbackThread(*timebase_id);
}

OS_QTimeBase::OS_QTimeBase()
{
    thread.timebase_id = &timebase_id;
    interval_ms = 1;
    name[0] = 0x0;
    reset_flag = 0x0;
    start_ms = 0;
    this->moveToThread(&timer_thread);
    timer.moveToThread(&timer_thread);
    connect(&timer_thread, SIGNAL(started()), this, SLOT(startTimer()));
    connect(&timer,        SIGNAL(timeout()), this, SLOT(timeout()));
    connect(&timer_thread, SIGNAL(finished()), this, SLOT(stop()));


    /*
    ** create the timebase sync mutex
    ** This gives a mechanism to synchronize updates to the timer chain with the
    ** expiration of the timer and processing the chain.
    ** Constructs a new mutex. The mutex is created in an unlocked state.
    */
    // handler_mutex = new QMutex();
    // timer_thread = new OS_QTimeBase();
    // sigMutex = new QMutex();

}

void OS_QTimeBase::start(){
    timer_thread.start();
    thread.start();
}
void OS_QTimeBase::stop(){

    timer_thread.quit();
    timeout(); /* Release so thread can run */
}

void OS_QTimeBase::startTimer()
{
    timer.setInterval(interval_ms);
    timer.start();

}


void OS_QTimeBase::timeout()
{
    if(tick_sem.available() == 0)
        tick_sem.release(1);
}


/*----------------------------------------------------------------
 *
 * Function: OS_UsecToTimespec
 *
 *  Purpose: Local helper routine, not part of OSAL API.
 *           Convert Microseconds to a POSIX timespec structure.
 *
 *-----------------------------------------------------------------*/
static int64 OS_UsecToMili(uint32 usecs)
{

    if (usecs < 1000)
    {
        return 1;
    }
    return ((int64) usecs)/1000;
} /* end OS_UsecToTimespec */

extern "C" {

/*----------------------------------------------------------------
 *
 * Function: OS_TimeBaseLock_Impl
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
void OS_TimeBaseLock_Impl(const OS_object_token_t *token)
{

    OS_QTimeBase *impl;

    impl = *OS_OBJECT_TABLE_GET(OS_impl_timebase_table, *token);
    impl->handler_mutex.lock();
    
} /* end OS_TimeBaseLock_Impl */

/*----------------------------------------------------------------
 *
 * Function: OS_TimeBaseUnlock_Impl
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
void OS_TimeBaseUnlock_Impl(const OS_object_token_t *token)
{

    OS_QTimeBase *impl;

    impl = *OS_OBJECT_TABLE_GET(OS_impl_timebase_table, *token);
    impl->handler_mutex.unlock();
} /* end OS_TimeBaseUnlock_Impl */


/****************************************************************************************
                                INITIALIZATION FUNCTION
 ***************************************************************************************/


/****************************************************************************************
                                   Time Base API
 ***************************************************************************************/


/*----------------------------------------------------------------
 *
 * Function: OS_TimeBase_WaitImpl
 *
 *  Purpose: Local helper routine, not part of OSAL API.
 *           Pends on the semaphore for the next timer tick
 *
 *-----------------------------------------------------------------*/
static uint32 OS_TimeBase_WaitImpl(osal_id_t timebase_id)
{
    OS_object_token_t                   token;
    OS_QTimeBase *impl;
    uint32                              tick_time;

    tick_time = 0;

    if (OS_ObjectIdGetById(OS_LOCK_MODE_NONE, OS_OBJECT_TYPE_OS_TIMEBASE, timebase_id, &token) == OS_SUCCESS)
    {
        impl = *OS_OBJECT_TABLE_GET(OS_impl_timebase_table, token);

        /*
         * Pend for the tick arrival
         */
        impl->tick_sem.acquire(1);


        /*
         * Determine how long this tick was.
         * Note that there are plenty of ways this become wrong if the timer
         * is reset right around the time a tick comes in.  However, it is
         * impossible to guarantee the behavior of a reset if the timer is running.
         * (This is not an expected use-case anyway; the timer should be set and forget)
         */
        if (impl->reset_flag == 0)
        {
            tick_time = impl->configured_interval_time;
        }
        else
        {
            tick_time        = impl->configured_start_time;
            impl->reset_flag = 0;
        }
    }

    return tick_time;
} /* end OS_TimeBase_WaitImpl */

/*----------------------------------------------------------------
 *
 * Function: OS_TimeBaseCreate_Impl
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_TimeBaseCreate_Impl(const OS_object_token_t *token)
{

    int32                               return_code = OS_SUCCESS;
    // struct sigevent                     evp;
    // struct timespec                     ts;
    OS_QTimeBase *local;
    OS_timebase_internal_record_t *     timebase;
    OS_VoidPtrValueWrapper_t                arg;

    local    = *OS_OBJECT_TABLE_GET(OS_impl_timebase_table, *token);
    timebase = OS_OBJECT_TABLE_GET(OS_timebase_table, *token);



    /*
     * Set up the necessary OS constructs
     *
     * If an external sync function is used then there is nothing to do here -
     * we simply call that function and it should synchronize to the time source.
     *
     * If no external sync function is provided then this will set up an RTEMS
     * timer to locally simulate the timer tick using the CPU clock.
     */
    local->simulate_flag = (timebase->external_sync == NULL);
    if (local->simulate_flag)
    {
        timebase->external_sync = OS_TimeBase_WaitImpl;
    }


    /*
     * Spawn a dedicated time base handler thread
     *
     * This alleviates the need to handle expiration in the context of a signal handler -
     * The handler thread can call a BSP synchronized delay implementation as well as the
     * application callback function.  It should run with elevated priority to reduce latency.
     *
     * Note the thread will not actually start running until this function exits and releases
     * the global table lock.
     */
    arg.opaque_arg = NULL;
    arg.id         = OS_ObjectIdFromToken(token);




    return return_code;
} /* end OS_TimeBaseCreate_Impl */

/*----------------------------------------------------------------
 *
 * Function: OS_TimeBaseSet_Impl
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_TimeBaseSet_Impl(const OS_object_token_t *token, uint32 start_time, uint32 interval_time)
{
    OS_VoidPtrValueWrapper_t            user_data;
    OS_QTimeBase *local;
    // struct itimerspec                   timeout;
    int32                               return_code;
    int                                 status;
    OS_timebase_internal_record_t *     timebase;
    int                      start_ticks;

    local       = *OS_OBJECT_TABLE_GET(OS_impl_timebase_table, *token);
    timebase    = OS_OBJECT_TABLE_GET(OS_timebase_table, *token);
    return_code = OS_SUCCESS;
    if (local->simulate_flag)
    {
        /*
        ** Note that UsecsToTicks() already protects against intervals
        ** less than os_clock_accuracy -- no need for extra checks which
        ** would actually possibly make it less accurate.
        **
        ** Still want to preserve zero, since that has a special meaning.
        */

        if (start_time <= 0)
        {
            interval_time = 0; /* cannot have interval without start */
        }

        if (interval_time <= 0)
        {
            local->interval_ms = 0;
        }
        else
        {
            OS_UsecsToTicks(interval_time, &local->interval_ms);
        }

        /*
        ** The defined behavior is to not arm the timer if the start time is zero
        ** If the interval time is zero, then the timer will not be re-armed.
        */
        if (start_time > 0)
        {

            /*
            ** Convert from Microseconds to the timeout
            */
            OS_UsecsToTicks(start_time, &start_ticks);

            user_data.opaque_arg = NULL;
            user_data.id         = OS_ObjectIdFromToken(token);
            local->timebase_id =  OS_ObjectIdToInteger(OS_ObjectIdFromToken(token));

            {
                local->configured_start_time    = (10000 * start_ticks) / OS_SharedGlobalVars.TicksPerSecond;
                local->configured_interval_time = (10000 * local->interval_ms) / OS_SharedGlobalVars.TicksPerSecond;
                local->configured_start_time *= 100;
                local->configured_interval_time *= 100;

                if (local->configured_start_time != start_time)
                {
                    OS_DEBUG("WARNING: timer %lu start_time requested=%luus, configured=%luus\n",
                             OS_ObjectIdToInteger(OS_ObjectIdFromToken(token)), (unsigned long)start_time,
                             (unsigned long)local->configured_start_time);
                }
                if (local->configured_interval_time != interval_time)
                {
                    OS_DEBUG("WARNING: timer %lu interval_time requested=%luus, configured=%luus\n",
                             OS_ObjectIdToInteger(OS_ObjectIdFromToken(token)), (unsigned long)interval_time,
                             (unsigned long)local->configured_interval_time);
                }

                if (local->interval_ms > 0)
                {
                    timebase->accuracy_usec = local->configured_interval_time;
                }
                else
                {
                    timebase->accuracy_usec = local->configured_start_time;
                }
            }

            local->start();
        }
    }

    if (local->reset_flag == 0 && return_code == OS_SUCCESS)
    {
        local->reset_flag = 1;
    }
    return return_code;
} /* end OS_TimeBaseSet_Impl */

/*----------------------------------------------------------------
 *
 * Function: OS_TimeBaseDelete_Impl
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_TimeBaseDelete_Impl(const OS_object_token_t *token)
{

    OS_QTimeBase *local;

    local = *OS_OBJECT_TABLE_GET(OS_impl_timebase_table, *token);

    /*
    ** Delete the timer
    */
    local->stop();
    // if (local->timer->isActive() == true )
    // {
    //     OS_DEBUG("Error deleting timer\n");
    //     return (OS_TIMER_ERR_INTERNAL);
    // }

    return OS_SUCCESS;
} /* end OS_TimeBaseDelete_Impl */

/*----------------------------------------------------------------
 *
 * Function: OS_TimeBaseGetInfo_Impl
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_TimeBaseGetInfo_Impl(const OS_object_token_t *token, OS_timebase_prop_t *timer_prop)
{
    OS_QTimeBase *local;

    local = *OS_OBJECT_TABLE_GET(OS_impl_timebase_table, *token);
    /* TODO figure out how to calc */
    timer_prop->accuracy = 1000;
    timer_prop->nominal_interval_time = local->interval_ms*1000;

    return OS_SUCCESS;

} /* end OS_TimeBaseGetInfo_Impl */

}



/******************************************************************************
 *  Function:  OS_QT_TimeBaseAPI_Impl_Init
 *
 *  Purpose:  Initialize the timer implementation layer
 *
 *  Arguments:
 *
 *  Return:
 */
int32 OS_QT_TimeBaseAPI_Impl_Init(void)
{
    /* https://doc.qt.io/qt-5/qelapsedtimer.html */
    OS_SharedGlobalVars.TicksPerSecond = (uint32)1000;
    /* 1000 micro seconds per milisecond */
    OS_SharedGlobalVars.MicroSecPerTick = 1000;


    osal_index_t        idx;
    // pthread_mutexattr_t mutex_attr;
    // struct timespec     clock_resolution;
    int32               return_code;

    return_code = OS_SUCCESS;

    // /* TODO
    // ** get the resolution of the selected clock
    // */
    // status = clock_getres(OS_PREFERRED_CLOCK, &clock_resolution);
    // if (status != 0)
    // {
    //     OS_DEBUG("failed in clock_getres: %s\n", strerror(status));
    //     return_code = OS_ERROR;
    //     break;
    // }

    // /* 
    // ** Convert to microseconds
    // ** Note that the resolution MUST be in the sub-second range, if not then
    // ** it looks like the POSIX timer API in the C library is broken.
    // ** Note for any flavor of RTOS we would expect <= 1ms.  Even a "desktop"
    // ** linux or development system should be <= 100ms absolute worst-case.
    // */
    // if (clock_resolution.tv_sec > 0)
    // {
    //     return_code = OS_TIMER_ERR_INTERNAL;
    //     break;
    // }

    /* Round to the nearest microsecond TODO obtain somehow */
    /* The accuracy also depends on the timer type. For Qt::PreciseTimer, QTimer will try to keep the accuracy at 1 millisecond. Precise timers will also never time out earlier than expected.
    */
    QT_GlobalVars.ClockAccuracyNsec = 1e6; /* 1 milisecond */
    /*
    ** Allow the mutex to use priority inheritance
    * TODO
    */
    // status = pthread_mutexattr_setprotocol(&mutex_attr, PTHREAD_PRIO_INHERIT);
    // if (status != 0)
    // {
    //     OS_DEBUG("Error: pthread_mutexattr_setprotocol failed: %s\n", strerror(status));
    //     return_code = OS_ERROR;
    //     break;
    // }

    for (idx = 0; idx < OS_MAX_TIMEBASES; ++idx)
    {
        /*
        ** Mark all timers as available
        */
        OS_impl_timebase_table[idx] = new OS_QTimeBase();
        // OS_impl_timebase_table[idx].handler_thread = 0x0;


    }


    /*
        * Pre-calculate the clock tick to microsecond conversion factor.
        */
    OS_SharedGlobalVars.TicksPerSecond = sysconf(_SC_CLK_TCK);
    if (OS_SharedGlobalVars.TicksPerSecond <= 0)
    {
        OS_DEBUG("Error: Unable to determine OS ticks per second: %s\n", strerror(errno));
        return_code = OS_ERROR;
    }

    /*
        * Calculate microseconds per tick
        *  - If the ratio is not an integer, this will round to the nearest integer value
        *  - This is used internally for reporting accuracy,
        *  - TicksPerSecond values over 2M will return zero
        */
    OS_SharedGlobalVars.MicroSecPerTick = (1000000 + (OS_SharedGlobalVars.TicksPerSecond / 2)) /
                                            OS_SharedGlobalVars.TicksPerSecond;


    return (return_code);
} /* end OS_QT_TimeBaseAPI_Impl_Init */


#include "inc/moc_os-impl-timebase.cpp"
