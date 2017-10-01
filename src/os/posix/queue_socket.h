#include "common_types.h"
#include "osapi.h"

#ifndef _queue_socket_h_
#define _queue_socket_h_


typedef struct OS_queue_internal_record_t
{
    int    free;
    int    id;
    uint32 max_size;
    char   name [OS_MAX_API_NAME];
    int    creator;
}OS_queue_internal_record_t;


#endif /* _queue_socket_h_ */