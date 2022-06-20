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
 * \file
 *
 * \ingroup  posix
 *
 */

#ifndef OS_IMPL_TIMEBASE_H
#define OS_IMPL_TIMEBASE_H

#include <osconfig.h>
#include <QThread>
#include <QWaitCondition>
#include <QMutex>
#include <QTimer>
#include <QSemaphore>
#include "common_types.h"
#include <list>



class QTimerThread : public QObject
{
    Q_OBJECT
public:
    QTimerThread();

private slots:
    void started();
    void timeout();

public:
    osal_id_t timebase_id;
    QThread m_workerThread;
    QTimer m_myTimer;
};



typedef struct
{
    int start_ms;
    int interval_ms;
    QTimerThread *timer_thread;
    QWaitCondition sigWaiter;
    QMutex *sigMutex;
    QMutex *handler_mutex;
    QSemaphore *tick_sem;
    // OS_impl_task_internal_record_t handler_thread;
    // pthread_t       handler_thread;
    char name[OS_MAX_API_NAME];
    uint8          reset_flag;
    uint8          simulate_flag;
    uint32         configured_start_time;
    uint32         configured_interval_time;

    // struct timespec softsleep;

} OS_impl_timebase_internal_record_t;

/****************************************************************************************
                                   GLOBAL DATA
 ***************************************************************************************/

extern OS_impl_timebase_internal_record_t OS_impl_timebase_table[OS_MAX_TIMEBASES];

#endif /* INCLUDE_OS_IMPL_TIMEBASE_H_ */
