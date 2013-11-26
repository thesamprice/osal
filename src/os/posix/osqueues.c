#include "osapi.h"
#include "osposix.h"
#include <pthread.h>
/*
** This include must be put below the osapi.h
** include so it can pick up the define
*/
#ifndef OSAL_SOCKET_QUEUE
#ifndef __MACH__
	#include <mqueue.h>
#else
	#include <sys/fcntl.h>
#endif
#endif

#ifdef OSAL_SOCKET_QUEUE
/* queues */
typedef struct
{
    int free;
    int id;
    char name [OS_MAX_API_NAME];
    int creator;
}OS_queue_record_t;
#else
/* queues */
typedef struct
{
    int   free;
    mqd_t id;
    char  name [OS_MAX_API_NAME];
    int   creator;
}OS_queue_record_t;
#endif

OS_queue_record_t   OS_queue_table         [OS_MAX_QUEUES];
pthread_mutex_t OS_queue_table_mut;


int OS_Queue_Init()
{
	int32 return_code = OS_SUCCESS;
	int ret;
	int i;
	pthread_mutexattr_t mutex_attr ;
    /* Initialize Message Queue Table */

    for(i = 0; i < OS_MAX_QUEUES; i++)
    {
        OS_queue_table[i].free        = TRUE;
        OS_queue_table[i].id          = UNINITIALIZED;
        OS_queue_table[i].creator     = UNINITIALIZED;
        strcpy(OS_queue_table[i].name,"");
    }

    ret = pthread_mutex_init((pthread_mutex_t *) & OS_queue_table_mut,&mutex_attr);
    if ( ret != 0 )
    {
       return_code = OS_ERROR;
       return(return_code);
    }
    return return_code;
}

/*--------------------------------------------------------------------------------------
    Name: OS_QueueGetIdByName

    Purpose: This function tries to find a queue Id given the name of the queue. The
             id of the queue is passed back in queue_id

    Returns: OS_INVALID_POINTER if the name or id pointers are NULL
             OS_ERR_NAME_TOO_LONG the name passed in is too long
             OS_ERR_NAME_NOT_FOUND the name was not found in the table
             OS_SUCCESS if success

---------------------------------------------------------------------------------------*/

int32 OS_QueueGetIdByName (uint32 *queue_id, const char *queue_name)
{
    uint32 i;

    if(queue_id == NULL || queue_name == NULL)
    {
       return OS_INVALID_POINTER;
    }

    /* a name too long wouldn't have been allowed in the first place
     * so we definitely won't find a name too long*/

    if (strlen(queue_name) >= OS_MAX_API_NAME)
    {
       return OS_ERR_NAME_TOO_LONG;
    }

    for (i = 0; i < OS_MAX_QUEUES; i++)
    {
        if (OS_queue_table[i].free != TRUE &&
           (strcmp(OS_queue_table[i].name, (char*) queue_name) == 0 ))
        {
            *queue_id = i;
            return OS_SUCCESS;
        }
    }

    /* The name was not found in the table,
     *  or it was, and the queue_id isn't valid anymore */
    return OS_ERR_NAME_NOT_FOUND;

}/* end OS_QueueGetIdByName */

/*---------------------------------------------------------------------------------------
    Name: OS_QueueGetInfo

    Purpose: This function will pass back a pointer to structure that contains
             all of the relevant info (name and creator) about the specified queue.

    Returns: OS_INVALID_POINTER if queue_prop is NULL
             OS_ERR_INVALID_ID if the ID given is not  a valid queue
             OS_SUCCESS if the info was copied over correctly
---------------------------------------------------------------------------------------*/
int32 OS_QueueGetInfo (uint32 queue_id, OS_queue_prop_t *queue_prop)
{
    /* Check to see that the id given is valid */

    if (queue_prop == NULL)
    {
        return OS_INVALID_POINTER;
    }

    if (queue_id >= OS_MAX_QUEUES || OS_queue_table[queue_id].free == TRUE)
    {
        return OS_ERR_INVALID_ID;
    }

    /* put the info into the stucture */
    pthread_mutex_lock(&OS_queue_table_mut);

    queue_prop -> creator =   OS_queue_table[queue_id].creator;
    strcpy(queue_prop -> name, OS_queue_table[queue_id].name);

    pthread_mutex_unlock(&OS_queue_table_mut);

    return OS_SUCCESS;

} /* end OS_QueueGetInfo */
