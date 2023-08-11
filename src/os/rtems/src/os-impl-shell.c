/*
 *  NASA Docket No. GSC-18,370-1, and identified as "Operating System Abstraction Layer"
 *
 *  Copyright (c) 2023 United States Government as represented by
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
 * \file     os-impl-shell.c
 * \ingroup  rtems
 * \author   sergio.e.maldonado@nasa.gov
 *
 */

/****************************************************************************************
                                    INCLUDE FILES
****************************************************************************************/

#include "os-rtems.h"
#include "os-impl-io.h"
#include "os-shared-shell.h"
#include "os-shared-file.h"
#include "os-shared-filesys.h"
#include "os-shared-task.h"
#include "os-shared-idmap.h"
#include "os-shared-common.h"

#include <rtems/shell.h>

#define OS_SHELL_COPY_BLOCK_SIZE        512
#define OS_SHELL_CMD_TASK_STACK_SIZE    16384
#define OS_SHELL_CMD_TASK_PRIORITY      250
#define OS_SHELL_INTERPRETER_CMD        "#!joel"
#define OS_SHELL_TEMP_INPUT_FILE_NAME   "/cf/shell_tmp_in.txt"
#define OS_SHELL_TEMP_OUTPUT_FILE_NAME  "/cf/shell_tmp_out.txt"

/*----------------------------------------------------------------
 *
 * Function: OS_ShellOutputToFile_Impl
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_ShellOutputToFile_Impl(const OS_object_token_t *token, const char *Cmd)
{
    int32                           ReturnCode = OS_ERROR;
    int32                           Result;
    osal_id_t                       tmpFd;
    OS_impl_file_internal_record_t *out_impl;
    char                            localShellName[OS_MAX_API_NAME];
    char                            cmdBuf[OS_MAX_CMD_LEN];
    char                            input_path[OS_MAX_PATH_LEN];
    char                            output_path[OS_MAX_PATH_LEN];
    int32                           rd_size;
    int32                           wr_size;
    int32                           wr_total;
    uint8                           copyblock[OS_SHELL_COPY_BLOCK_SIZE];

    /* the input and output paths used by RTEMS must be local */
    Result = OS_TranslatePath(OS_SHELL_TEMP_INPUT_FILE_NAME, input_path);
    if (Result < OS_SUCCESS)
    {
        return Result;
    }

    Result = OS_TranslatePath(OS_SHELL_TEMP_OUTPUT_FILE_NAME, output_path);
    if (Result < OS_SUCCESS)
    {
        return Result;
    }
    
    snprintf(localShellName, sizeof(localShellName), "shll_%08lx", OS_ObjectIdToInteger(OS_TaskGetId()));

    /* Create a file to write the command to (or write over the old one) */
    Result = OS_OpenCreate(&tmpFd, OS_SHELL_TEMP_INPUT_FILE_NAME, OS_FILE_FLAG_CREATE | OS_FILE_FLAG_TRUNCATE, OS_READ_WRITE);
    if (Result < OS_SUCCESS)
    {
        return Result;
    }
    
    memset(cmdBuf, 0x0, sizeof(cmdBuf));

    /* RTEMS uses joel shell scripts so include the interpreter #! line */
    snprintf(cmdBuf, OS_MAX_CMD_LEN, "%s\n%s\n", OS_SHELL_INTERPRETER_CMD, Cmd);

    /* Copy the command to the file */
    OS_write(tmpFd, cmdBuf, OS_strnlen(cmdBuf, OS_MAX_CMD_LEN));

    /* Close the file descriptor */
    OS_close(tmpFd);

    /* Create a shell task that will run the command from the input_path, push output to output_path */
    Result = rtems_shell_script(
             localShellName,
             OS_SHELL_CMD_TASK_STACK_SIZE,
             OS_SHELL_CMD_TASK_PRIORITY,
             input_path,
             output_path,
             false,
             true,
             false);

    if (Result == RTEMS_SUCCESSFUL)
    {
        /* Get the output file fd */
        out_impl = OS_OBJECT_TABLE_GET(OS_impl_filehandle_table, *token);

        /* Setup to append output file */
        lseek(out_impl->fd, 0, SEEK_END);

        /* Open temp output file */
        ReturnCode = OS_OpenCreate(&tmpFd, OS_SHELL_TEMP_OUTPUT_FILE_NAME, OS_FILE_FLAG_NONE , OS_READ_ONLY);

        /* Copy temp output file to output file */
        while (ReturnCode == OS_SUCCESS)
        {
            rd_size = OS_read(tmpFd, copyblock, sizeof(copyblock));
            if (rd_size < 0)
            {
                ReturnCode = OS_ERROR;
                break;
            }
            if (rd_size == 0)
            {
                break;
            }
            wr_total = 0;
            while (wr_total < rd_size)
            {
                wr_size = write(out_impl->fd, &copyblock[wr_total], rd_size - wr_total);
                if (wr_size < 0)
                {
                    ReturnCode = OS_ERROR;
                    break;
                }
                wr_total += wr_size;
            }
        }

        /* Close the file descriptor */
        OS_close(tmpFd);
    }

    return ReturnCode;

} /* end OS_ShellOutputToFile_Impl */
