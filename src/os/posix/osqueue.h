#ifndef __OS_QUEUE_H_
#define __OS_QUEUE_H_

#if OSAL_QUEUE_MUTEX || 1

/*
** This include must be put below the osapi.h
** include so it can pick up the define
*/
#include <sys/fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <errno.h>
#include "common_types.h"
typedef struct
{
    uint32 queue_depth;      /*Maximum messages allowed in pipe*/
    uint32 message_type;
    uint32 max_message_size; /*max message size*/
    uint32 suspend_type;     /*Suspend type.*/
    uint32 messages_in_pipe; /*messages in pipe*/
    char * buffer;           /*Buffer holding pipe data.*/
    int buffer_size;         /*Buffer size, = max_message_size * max_message_count + max_message_count*4*/
    int first_byte;          /*First byte, to store the next message at.*/
    int last_byte;           /*First byte to pull data from.*/

    /*If the mutex is locked that means we dont have any messsages in our pipe.*/
    /*Protects queue from multiple updates.*/
    pthread_mutex_t pipe_mutex;

    sem_t* pipe_sema; /* Pipe Semaphore*/
    int free;
    int id;
    char name [OS_MAX_API_NAME];
    int creator;
    uint32 flags;

}OS_queue_record_t;

typedef OS_queue_record_t OS_queue_internal_record_t;
#endif

extern pthread_mutex_t OS_queue_table_mut;
extern OS_queue_internal_record_t   OS_queue_table         [OS_MAX_QUEUES];
#endif
