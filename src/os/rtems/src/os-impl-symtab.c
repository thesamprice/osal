/************************************************************************
 * NASA Docket No. GSC-18,719-1, and identified as “core Flight System: Bootes”
 *
 * Copyright (c) 2024 United States Government as represented by the
 * Administrator of the National Aeronautics and Space Administration.
 * All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ************************************************************************/

/**
 * \file
 * \ingroup  RTEMS
 * \author   sergio.e.maldonado@nasa.gov
 *
 */

/****************************************************************************************
                                    INCLUDE FILES
****************************************************************************************/

#include "os-rtems.h"
#include "os-impl-symtab.h"
#include "os-shared-module.h"

#include <stdio.h>
#include <string.h> /* memset() */
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/stat.h>

#include <rtems.h>
#include <rtems/inttypes.h>
#include <rtems/printer.h>
#include <rtems/rtl/rtl.h>

#define DEBUG_PRINT_SYMBOLS

typedef struct
{
    char    SymbolName[OS_MAX_SYM_LEN];
    cpuaddr SymbolAddress;
} SymbolRecord_t;

/**
 * Object print data.
 */
typedef struct
{
#ifdef DEBUG_PRINT_SYMBOLS
    const rtems_printer* printer;      /**< The RTEMS printer. */
#endif
    rtems_rtl_data*      rtl;          /**< The RTL data. */
    int                  indent;       /**< Spaces to indent. */
} rtl_obj_iterator;

/* A global for storing the state in a SymbolDump call */
SymbolDumpState_t OS_RTEMS_SymbolDumpState;

typedef bool (*rtems_chain_iterator) (rtems_chain_node* node, void* data);

extern bool
rtems_rtl_chain_iterate (rtems_chain_control* chain,
                         rtems_chain_iterator iterator,
                         void*                data);


/****************************************************************************************
                                SYMBOL TABLE API
 ***************************************************************************************/

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_GenericSymbolLookup_Impl(cpuaddr *SymbolAddress, const char *SymbolName)
{
    rtems_rtl_obj_sym* sym = NULL;

    /*
    ** Check parameters
    */
    if ((SymbolAddress == NULL) || (SymbolName == NULL))
    {
        return OS_INVALID_POINTER;
    }

    /*
    ** Lookup the entry point
    **
    */    
    sym = rtems_rtl_symbol_global_find (SymbolName);
    if (!sym)
    {
        return OS_ERROR;
    }

    *SymbolAddress = (cpuaddr)sym->value;

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_SymbolLookup_Impl(cpuaddr *SymbolAddress, const char *SymbolName)
{
    return OS_GenericSymbolLookup_Impl(SymbolAddress, SymbolName);
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_ModuleSymbolLookup_Impl(const OS_object_token_t *token, cpuaddr *SymbolAddress, const char *SymbolName)
{
    /*
     * NOTE: this is currently exactly the same as OS_SymbolLookup_Impl().
     *
     * Ideally this should get a SYMTAB_ID from the MODULE_ID and search only
     * for the symbols provided by that module - but it is not clear if RTEMS
     * offers this capability.
     */
    return OS_GenericSymbolLookup_Impl(SymbolAddress, SymbolName);
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
static bool
OS_SymTableIterator_Impl (rtems_chain_node* node, void* data)
{
    int     max_len = 0;
    int     s;
    SymbolRecord_t     symRecord;
    size_t             NextSize;
    int                status;
    SymbolDumpState_t *state;

#ifdef DEBUG_PRINT_SYMBOLS
    rtl_obj_iterator* iter = data;
#endif
    rtems_rtl_obj* obj = (rtems_rtl_obj*) node;

    /*
     * Rather than passing the state pointer through the generic "int" arg,
     * use a global.  This is OK because dumps are serialized externally.
     */
    state = &OS_RTEMS_SymbolDumpState;

    for (s = 0; s < obj->global_syms; ++s)
    {
        int         len;

        len = strlen (obj->global_table[s].name);
        if (len > max_len)
            max_len = len;
    }

    for (s = 0; s < obj->global_syms; ++s)
    {
        const char* sym = obj->global_table[s].name;
#ifdef DEBUG_PRINT_SYMBOLS
        rtems_printf (iter->printer, "%-*c%-*s = %p\n", iter->indent + 1 + 2, ' ',
                      max_len, sym, obj->global_table[s].value);
#endif
        /*
        ** Copy symbol name
        */
        strncpy(symRecord.SymbolName, sym, sizeof(symRecord.SymbolName) - 1);
        symRecord.SymbolName[sizeof(symRecord.SymbolName) - 1] = '\0';

        /*
        ** Check to see if the max length of each symbol name has been reached
        */
        if (memchr(sym, 0, OS_MAX_SYM_LEN) == NULL)
        {
            symRecord.SymbolName[sizeof(symRecord.SymbolName) - 2] = '*';
            OS_printf("%s(): symbol name too long %s\n", __func__,symRecord.SymbolName);
            state->StatusCode = OS_ERR_NAME_TOO_LONG;
        }

        /*
        ** Check to see if the maximum size of the file has been reached
        */
        NextSize = state->CurrSize + sizeof(symRecord);
        if (NextSize > state->Sizelimit)
        {
            /*
            ** We exceeded the maximum size, so tell vxWorks to stop
            ** However this is not considered an error, just a stop condition.
            */
            OS_printf("%s(): symbol table size exceeded\n", __func__);
            state->StatusCode = OS_ERR_OUTPUT_TOO_LARGE;
            return false;
        }

        /*
        ** Save symbol address
        */
        symRecord.SymbolAddress = (cpuaddr)obj->global_table[s].value;

        /*
        ** Write entry in file
        */
        status = write(state->fd, (char *)&symRecord, sizeof(symRecord));
        /* There is a problem if not all bytes were written OR if we get an error
         * value, < 0. */
        if (status < (int)sizeof(symRecord))
        {
            state->StatusCode = OS_ERROR;
            return false;
        }

        state->CurrSize = NextSize;
    }

    return true;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_SymbolTableDump_Impl(const char *filename, size_t size_limit)
{
    SymbolDumpState_t*   state;

    rtl_obj_iterator iter = { 0 };
#ifdef DEBUG_PRINT_SYMBOLS
    rtems_printer printer;
    rtems_print_printer_printf (&printer);
    iter.printer = &printer;
    iter.indent = 1;
#endif
    iter.rtl = rtems_rtl_lock ();

    /*
     * Rather than passing the state pointer through the generic "int" arg,
     * use a global.  This is OK because dumps are serialized externally.
     */
    state = &OS_RTEMS_SymbolDumpState;

    memset(state, 0, sizeof(*state));
    state->Sizelimit = size_limit;

    /*
    ** Open file
    */
    state->fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (state->fd < 0)
    {
        OS_printf("open(%s): error: %s\n", filename, strerror(errno));
        state->StatusCode = OS_ERROR;
    }
    else
    {
        /*
        ** Iterate the symbol table
        */
        if (!rtems_rtl_chain_iterate (&iter.rtl->objects,
                                      OS_SymTableIterator_Impl,
                                      &iter))
        {
            state->StatusCode = OS_ERROR;
        }
        close(state->fd);
    }

    /*
     * If output size was zero this means a failure of the rtems_rtl_chain_iterate call,
     * in that it didn't iterate over anything at all.
     */
    if (state->StatusCode == OS_SUCCESS && state->CurrSize == 0)
    {
        OS_printf("%s(): No symbols found!\n", __func__);
        state->StatusCode = OS_ERROR;
    }

    rtems_rtl_unlock ();

    return state->StatusCode;
}
