#ifndef _queue_mqd_h_
#define _queue_mqd_h_

#include "common_types.h"
#include "osapi.h"

#ifdef OSAL_MQD_QUEUE 

#include <mqueue.h>


typedef struct
{
    int    free;
    mqd_t  id;
    uint32 max_size;
    char   name [OS_MAX_API_NAME];
    int    creator;
}OS_queue_internal_record_t;

#endif
#endif 