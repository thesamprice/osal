#include "queue_mqd.h"
#ifdef OSAL_MQD_QUEUE

/* queues */


/* ---------------------- POSIX MESSAGE QUEUE IMPLEMENTATION ------------------------- */
/*---------------------------------------------------------------------------------------
 Name: OS_QueueCreate
 
 Purpose: Create a message queue which can be refered to by name or ID
 
 Returns: OS_INVALID_POINTER if a pointer passed in is NULL
 OS_ERR_NAME_TOO_LONG if the name passed in is too long
 OS_ERR_NO_FREE_IDS if there are already the max queues created
 OS_ERR_NAME_TAKEN if the name is already being used on another queue
 OS_ERROR if the OS create call fails
 OS_SUCCESS if success
 
 Notes: the flags parameter is unused.
 ---------------------------------------------------------------------------------------*/
 int32 OS_QueueCreate (uint32 *queue_id, const char *queue_name, uint32 queue_depth,
    uint32 data_size, uint32 flags)
{
int                     i;
pid_t                   process_id;
mqd_t                   queueDesc;
struct mq_attr          queueAttr;   
uint32                  possible_qid;
char                    name[OS_MAX_API_NAME * 2];
char                    process_id_string[OS_MAX_API_NAME+1];
sigset_t                previous;
sigset_t                mask;

if ( queue_id == NULL || queue_name == NULL)
{
return OS_INVALID_POINTER;
}

/* we don't want to allow names too long*/
/* if truncated, two names might be the same */

if (strlen(queue_name) >= OS_MAX_API_NAME)
{
return OS_ERR_NAME_TOO_LONG;
}

/* Check Parameters */

OS_InterruptSafeLock(&OS_queue_table_mut, &mask, &previous); 

for(possible_qid = 0; possible_qid < OS_MAX_QUEUES; possible_qid++)
{
if (OS_queue_table[possible_qid].free == TRUE)
break;
}

if( possible_qid >= OS_MAX_QUEUES || OS_queue_table[possible_qid].free != TRUE)
{
OS_InterruptSafeUnlock(&OS_queue_table_mut, &previous); 
return OS_ERR_NO_FREE_IDS;
}

/* Check to see if the name is already taken */

for (i = 0; i < OS_MAX_QUEUES; i++)
{
if ((OS_queue_table[i].free == FALSE) &&
strcmp ((char*) queue_name, OS_queue_table[i].name) == 0)
{
OS_InterruptSafeUnlock(&OS_queue_table_mut, &previous); 
return OS_ERR_NAME_TAKEN;
}
} 

/* Set the possible task Id to not free so that
* no other task can try to use it */

OS_queue_table[possible_qid].free = FALSE;

OS_InterruptSafeUnlock(&OS_queue_table_mut, &previous); 

/* set queue attributes */
queueAttr.mq_maxmsg  = queue_depth;
queueAttr.mq_msgsize = data_size;

/*
** Construct the queue name:
** The name will consist of "/<process_id>.queue_name"
*/

/* pre-pend / to queue name */
strcpy(name, "/");

/*
** Get the process ID 
*/
process_id = getpid();
sprintf(process_id_string, "%d", process_id);
strcat(name, process_id_string);
strcat(name,"."); 

/*
** Add the name that was passed in
*/
strcat(name, queue_name);

/* 
** create message queue 
*/
queueDesc = mq_open(name, O_CREAT | O_RDWR, 0666, &queueAttr);

if ( queueDesc == -1 )
{
OS_InterruptSafeLock(&OS_queue_table_mut, &mask, &previous); 
OS_queue_table[possible_qid].free = TRUE;
OS_InterruptSafeUnlock(&OS_queue_table_mut, &previous); 

#ifdef OS_DEBUG_PRINTF
printf("OS_QueueCreate Error. errno = %d\n",errno);
#endif
if( errno ==EINVAL)
{
printf("Your queue depth may be too large for the\n");
printf("OS to handle. Please check the msg_max\n");
printf("parameter located in /proc/sys/fs/mqueue/msg_max\n");
printf("on your Linux file system and raise it if you\n");
printf(" need to or run as root\n");
}
return OS_ERROR;
}

/*
** store queue_descriptor
*/
*queue_id = possible_qid;

OS_InterruptSafeLock(&OS_queue_table_mut, &mask, &previous); 

OS_queue_table[*queue_id].id = queueDesc;
OS_queue_table[*queue_id].free = FALSE;
OS_queue_table[*queue_id].max_size = data_size;
strcpy( OS_queue_table[*queue_id].name, (char*) queue_name);
OS_queue_table[*queue_id].creator = OS_FindCreator();

OS_InterruptSafeUnlock(&OS_queue_table_mut, &previous); 

return OS_SUCCESS;

}/* end OS_QueueCreate */

/*--------------------------------------------------------------------------------------
Name: OS_QueueDelete

Purpose: Deletes the specified message queue.

Returns: OS_ERR_INVALID_ID if the id passed in does not exist
OS_ERROR if the OS call to delete the queue fails 
OS_SUCCESS if success

Notes: If There are messages on the queue, they will be lost and any subsequent
calls to QueueGet or QueuePut to this queue will result in errors
---------------------------------------------------------------------------------------*/
int32 OS_QueueDelete (uint32 queue_id)
{
pid_t      process_id;
char       name[OS_MAX_API_NAME+1];
char       process_id_string[OS_MAX_API_NAME+1];
sigset_t   previous;
sigset_t   mask;

/* Check to see if the queue_id given is valid */

if (queue_id >= OS_MAX_QUEUES || OS_queue_table[queue_id].free == TRUE)
{
return OS_ERR_INVALID_ID;
}

/*
** Construct the queue name:
** The name will consist of "/<process_id>.queue_name"
*/

/* pre-pend / to queue name */
strcpy(name, "/");

/*
** Get the process ID
*/
process_id = getpid();
sprintf(process_id_string, "%d", process_id);
strcat(name, process_id_string);
strcat(name,".");

strcat(name, OS_queue_table[queue_id].name);

/* Try to delete and unlink the queue */
if((mq_close(OS_queue_table[queue_id].id) == -1) || (mq_unlink(name) == -1))
{
return OS_ERROR;
}

/* 
* Now that the queue is deleted, remove its "presence"
* in OS_message_q_table and OS_message_q_name_table 
*/
OS_InterruptSafeLock(&OS_queue_table_mut, &mask, &previous); 

OS_queue_table[queue_id].free = TRUE;
strcpy(OS_queue_table[queue_id].name, "");
OS_queue_table[queue_id].creator = UNINITIALIZED;
OS_queue_table[queue_id].max_size = 0;
OS_queue_table[queue_id].id = UNINITIALIZED;

OS_InterruptSafeUnlock(&OS_queue_table_mut, &previous); 

return OS_SUCCESS;

} /* end OS_QueueDelete */

/*---------------------------------------------------------------------------------------
Name: OS_QueueGet

Purpose: Receive a message on a message queue.  Will pend or timeout on the receive.
Returns: OS_ERR_INVALID_ID if the given ID does not exist
OS_ERR_INVALID_POINTER if a pointer passed in is NULL
OS_QUEUE_EMPTY if the Queue has no messages on it to be recieved
OS_QUEUE_TIMEOUT if the timeout was OS_PEND and the time expired
OS_QUEUE_INVALID_SIZE if the size of the buffer passed in is not big enough for the 
                maximum size message 
OS_SUCCESS if success
---------------------------------------------------------------------------------------*/
int32 OS_QueueGet (uint32 queue_id, void *data, uint32 size, uint32 *size_copied, int32 timeout)
{
struct mq_attr  queueAttr;
int             sizeCopied = -1;
struct timespec ts;

/*
** Check Parameters 
*/
if(queue_id >= OS_MAX_QUEUES || OS_queue_table[queue_id].free == TRUE)
{
return OS_ERR_INVALID_ID;
}
else if( (data == NULL) || (size_copied == NULL) )
{
return OS_INVALID_POINTER;
}
else if( size < OS_queue_table[queue_id].max_size )
{
/* 
** The buffer that the user is passing in is potentially too small
** RTEMS will just copy into a buffer that is too small
*/
*size_copied = 0;
return(OS_QUEUE_INVALID_SIZE);
}

/*
** Read the message queue for data
*/
if (timeout == OS_PEND) 
{      
/*
** A signal can interrupt the mq_receive call, so the call has to be done with 
** a loop
*/
do 
{
sizeCopied = mq_receive(OS_queue_table[queue_id].id, data, size, NULL);
} while ((sizeCopied == -1) && (errno == EINTR));

if (sizeCopied == -1)
{
*size_copied = 0;
return(OS_ERROR);
}
else
{ 
*size_copied = sizeCopied;
}
}
else if (timeout == OS_CHECK)
{      
/* get queue attributes */
if(mq_getattr(OS_queue_table[queue_id].id, &queueAttr))
{
*size_copied = 0;
return (OS_ERROR);
}

/* check how many messages in queue */
if(queueAttr.mq_curmsgs > 0) 
{
do
{
sizeCopied  = mq_receive(OS_queue_table[queue_id].id, data, size, NULL);
} while ( sizeCopied == -1 && errno == EINTR );

if (sizeCopied == -1)
{
*size_copied = 0;
return(OS_ERROR);
}
else
{
*size_copied = sizeCopied;
}
} 
else 
{
*size_copied = 0;
return (OS_QUEUE_EMPTY);
}

}
else /* timeout */ 
{
OS_CompAbsDelayTime( timeout , &ts) ;

/*
** If the mq_timedreceive call is interrupted by a system call or signal,
** call it again.
*/
do
{
sizeCopied = mq_timedreceive(OS_queue_table[queue_id].id, data, size, NULL, &ts);
} while ( sizeCopied == -1 && errno == EINTR );

if((sizeCopied == -1) && (errno == ETIMEDOUT))
{
return(OS_QUEUE_TIMEOUT);
}
else if (sizeCopied == -1)
{
*size_copied = 0;
return(OS_ERROR);
}
else
{
*size_copied = sizeCopied;
}

} /* END timeout */

return OS_SUCCESS;

} /* end OS_QueueGet */

/*---------------------------------------------------------------------------------------
Name: OS_QueuePut

Purpose: Put a message on a message queue.

Returns: OS_ERR_INVALID_ID if the queue id passed in is not a valid queue
OS_INVALID_POINTER if the data pointer is NULL
OS_QUEUE_FULL if the queue cannot accept another message
OS_ERROR if the OS call returns an error
OS_SUCCESS if SUCCESS            

Notes: The flags parameter is not used.  The message put is always configured to
immediately return an error if the receiving message queue is full.
---------------------------------------------------------------------------------------*/
int32 OS_QueuePut (uint32 queue_id, const void *data, uint32 size, uint32 flags)
{
struct mq_attr  queueAttr;

/*
** Check Parameters 
*/
if(queue_id >= OS_MAX_QUEUES || OS_queue_table[queue_id].free == TRUE)
{
return OS_ERR_INVALID_ID;
}

if (data == NULL)
{
return OS_INVALID_POINTER;
}

/* get queue attributes */
if(mq_getattr(OS_queue_table[queue_id].id, &queueAttr))
{
return (OS_ERROR);
}

/* check if queue is full */
if(queueAttr.mq_curmsgs >= queueAttr.mq_maxmsg) 
{
return(OS_QUEUE_FULL);
}

/* send message */
if(mq_send(OS_queue_table[queue_id].id, data, size, 1) == -1) 
{
return(OS_ERROR);
}

return OS_SUCCESS;

} /* end OS_QueuePut */


#endif /* OSAL_MQD_QUEUE */
