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

/*
 * File:  bsp_start.c
 *
 * Purpose:
 *   OSAL BSP main entry point.
 */

#define _USING_RTEMS_INCLUDES_

/*
**  Include Files
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <bsp.h>
#include <rtems.h>
#include <rtems/error.h>

#include "generic_rtems_bsp_internal.h"

/*
**  External Declarations
*/

/*
** Global variables
*/
OS_BSP_GenericRtemsGlobalData_t OS_BSP_GenericRtemsGlobal;

/* ---------------------------------------------------------
    OS_BSP_GetReturnStatus()

     Helper function to convert an OSAL status code into
     a code suitable for returning to the OS.
   --------------------------------------------------------- */
rtems_status_code OS_BSP_GetReturnStatus(void)
{
    rtems_status_code retcode;
    const char *      StatusStr;

    switch (OS_BSP_Global.AppStatus)
    {
    case OS_SUCCESS:
        /* translate OS_SUCCESS to the system RTEMS_SUCCESSFUL value */
        StatusStr = "SUCCESS";
        retcode = RTEMS_SUCCESSFUL;
        break;

    default:
        /* translate anything else to a generic non-success code,
         * this basically just means the main task exited */
        StatusStr = "ERROR";
        retcode = RTEMS_TASK_EXITTED;
        break;
    }

    printf("\nApplication exit status: %s (%d)\n\n", StatusStr, (int)OS_BSP_Global.AppStatus);
    rtems_task_wake_after(100);

    return retcode;
}

/*----------------------------------------------------------------
   OS_BSP_Lock_Impl
   See full description in header
 ------------------------------------------------------------------*/
void OS_BSP_Lock_Impl(void)
{
    rtems_status_code status;
    status = rtems_semaphore_obtain(OS_BSP_GenericRtemsGlobal.AccessMutex, RTEMS_WAIT, RTEMS_NO_TIMEOUT);
    if (status != RTEMS_SUCCESSFUL)
    {
        BSP_DEBUG("rtems_semaphore_obtain: %s\n", rtems_status_text(status));
    }
}

/*----------------------------------------------------------------
   OS_BSP_Unlock_Impl
   See full description in header
 ------------------------------------------------------------------*/
void OS_BSP_Unlock_Impl(void)
{
    rtems_status_code status;
    status = rtems_semaphore_release(OS_BSP_GenericRtemsGlobal.AccessMutex);
    if (status != RTEMS_SUCCESSFUL)
    {
        BSP_DEBUG("rtems_semaphore_release: %s\n", rtems_status_text(status));
    }
}

/* ---------------------------------------------------------
    OS_BSP_Shutdown_Impl()

     Helper function to abort the running task
   --------------------------------------------------------- */
void OS_BSP_Shutdown_Impl(void)
{
   rtems_task_suspend(rtems_task_self());
}

/*
 ** A simple entry point to start from the loader.
 ** Since, this is using an external RTEMS kernel that uses the Init function 
 ** Instead, this is the entry point to the cFS from the RKI image.
 */
int OS_BSPMain(void)
{
    rtems_status_code status;

    /*
     * Initially clear the global object
     */
    memset(&OS_BSP_Global, 0, sizeof(OS_BSP_Global));
    memset(&OS_BSP_GenericRtemsGlobal, 0, sizeof(OS_BSP_GenericRtemsGlobal));

    /*
     * Initialize the low level access sem
     */
    status = rtems_semaphore_create(rtems_build_name('B', 'S', 'P', '\0'), 1,
                                    RTEMS_PRIORITY | RTEMS_BINARY_SEMAPHORE | RTEMS_INHERIT_PRIORITY, 0,
                                    &OS_BSP_GenericRtemsGlobal.AccessMutex);
    if (status != RTEMS_SUCCESSFUL)
    {
        BSP_DEBUG("rtems_semaphore_create: %s\n", rtems_status_text(status));
    }

    /*
     * Call application specific entry point.
     * This should set up all user tasks and resources, then return
     */
    OS_Application_Startup();

    /*
     * OS_Application_Run() implements the background task.
     * The user application may provide this, or a default implementation
     * is used which just calls OS_IdleLoop().
     */
    OS_Application_Run();

    /*
     * Return to shell with the current status code
     */
    return OS_BSP_GetReturnStatus();

}
