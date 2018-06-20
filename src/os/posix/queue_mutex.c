
/****************************************************************************************
                                    INCLUDE FILES
****************************************************************************************/
#include "common_types.h"
#include "osapi.h"
#include "osqueue.h"
#define OS_VARIABLE_SIZED_QUEUE        (2)

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

/*Defines*/
#define OS_BASE_PORT 43000
#define UNINITIALIZED 0
#define MAX_PRIORITY 255
#ifndef PTHREAD_STACK_MIN
   #define PTHREAD_STACK_MIN 8092
#endif

/*
** Global data for the API
*/

/*
** Tables for the properties of objects
*/

    
/* queues */


int OS_QueueInit()
{
    int i =0;
    int ret = 0;
        /* Initialize Message Queue Table */

    for(i = 0; i < OS_MAX_QUEUES; i++)
    {
        OS_queue_table[i].free        = TRUE;
        OS_queue_table[i].id          = UNINITIALIZED;
        OS_queue_table[i].creator     = UNINITIALIZED;
        strcpy(OS_queue_table[i].name,"");
    }

   ret = pthread_mutex_init((pthread_mutex_t *) & OS_queue_table_mut,NULL);
   if ( ret != 0 )
   {

      return (OS_ERROR);
   }

    return ret;
}

/****************************************************************************************
                                MESSAGE QUEUE API
****************************************************************************************/

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

 Notes: the flahs parameter is unused.
 ---------------------------------------------------------------------------------------*/
int32 OS_QueueCreate (uint32 *queue_id, const char *queue_name, uint32 queue_depth,
                      uint32 data_size, uint32 flags)
{
    int                     i;
    uint32                  possible_qid;
    char                    name[OS_MAX_API_NAME+1];
    OS_queue_record_t      *queue;
    int pshared = 0;
    unsigned int starting_value_zero = 0;
    sem_t unamed;
    int status;

    if ( queue_id == NULL || queue_name == NULL)
    {
        return OS_INVALID_POINTER;
    }

    /* we don't want to allow names too long*/
    /* if truncated, two names might be the same */

    if (strlen(queue_name) > OS_MAX_API_NAME)
    {
        return OS_ERR_NAME_TOO_LONG;
    }

    /* Check Parameters */

    pthread_mutex_lock(&OS_queue_table_mut);

    for(possible_qid = 0; possible_qid < OS_MAX_QUEUES; possible_qid++)
    {
        if (OS_queue_table[possible_qid].free == TRUE)
            break;
    }

    if( possible_qid >= OS_MAX_QUEUES || OS_queue_table[possible_qid].free != TRUE)
    {
        pthread_mutex_unlock(&OS_queue_table_mut);
        return OS_ERR_NO_FREE_IDS;
    }

    /* Check to see if the name is already taken */
    /* pre-pend / to queue name */
    strcpy(name, "/");
    strcat(name, queue_name);

    for (i = 0; i < OS_MAX_QUEUES; i++)
    {
        if ((OS_queue_table[i].free == FALSE) &&
            strcmp ((char*) name, OS_queue_table[i].name) == 0)
        {
            pthread_mutex_unlock(&OS_queue_table_mut);
            printf("Name already taken %s\n",name);
            return OS_ERR_NAME_TAKEN;
        }
    }

    /* Set the possible task Id to not free so that
     * no other task can try to use it */

    OS_queue_table[possible_qid].free = FALSE;
    OS_queue_table[possible_qid].flags = flags;
    pthread_mutex_unlock(&OS_queue_table_mut);




    /* create message queue */
    /*
    ** store queue_descriptor
    */
    *queue_id = possible_qid;

    pthread_mutex_lock(&OS_queue_table_mut);

        queue = &OS_queue_table[*queue_id];
        queue->id = *queue_id;
        queue->free = FALSE;
        strncpy( queue->name, (char*) queue_name,OS_MAX_API_NAME);
        queue->creator = OS_FindCreator();

        queue->queue_depth = queue_depth;
        queue->first_byte = queue->last_byte = 0;
        queue->messages_in_pipe = 0;
        //Note each message starts with 4 bytes describing its length.
        queue->buffer_size = queue_depth* data_size + queue_depth* sizeof(uint32);

        queue->max_message_size = data_size;
        queue->buffer = (char*)malloc(queue->buffer_size);

        /*Sempaphore setup */
        status = sem_init(&unamed,pshared,starting_value_zero);
        if(status == 0)
        {
        	/*We have an unamed semaphore,
        	 * we will now malloc a memory location for it,
        	 * and copy the data into it.*/
        	queue->pipe_sema = malloc(sizeof(*queue->pipe_sema));
        	memcpy(queue->pipe_sema,&unamed,sizeof(unamed));
        }
        else
        {
        	/* Unamed semaphores are not supported on this device,
        	 * we are going to revert to a named semaphore.
        	 */
        	sem_t* temp = sem_open(queue->name,O_CREAT,0666,starting_value_zero);
        	if(temp == -1 || temp == SEM_FAILED)
        	{
            	perror("sem_init:");
            	queue->buffer = 0;
        	}

        	else
        	{
        		queue->pipe_sema =temp;
        	}

        }
        /*Clean out our semaphore on startup */
        status = 0;
		while(status == 0)
		{
			status = sem_trywait(queue->pipe_sema);
        }
        //Pipe mutex should not be locked.
        pthread_mutex_init(&queue->pipe_mutex , 0x0 );
        pthread_mutex_unlock(&queue->pipe_mutex);

    pthread_mutex_unlock(&OS_queue_table_mut);

    if(queue->buffer == 0)
    {
        queue->free = TRUE;
        return OS_ERROR;
    }

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
    char name[OS_MAX_API_NAME+1];

    /* Check to see if the queue_id given is valid */

    if (queue_id >= OS_MAX_QUEUES || OS_queue_table[queue_id].free == TRUE)
    {
       return OS_ERR_INVALID_ID;
    }

    /* Try to delete the queue */

    /* pre-pend / to queue name */
    //TODO Tear down semaphore, tear down mutex

    /* Try to delete and unlink the queue */


    /*
     * Now that the queue is deleted, remove its "presence"
     * in OS_message_q_table and OS_message_q_name_table
     */
    pthread_mutex_lock(&OS_queue_table_mut);

    OS_queue_table[queue_id].free = TRUE;
    strcpy(OS_queue_table[queue_id].name, "");
    OS_queue_table[queue_id].creator = UNINITIALIZED;
    OS_queue_table[queue_id].id = UNINITIALIZED;

    pthread_mutex_unlock(&OS_queue_table_mut);

    return OS_SUCCESS;

} /* end OS_QueueDelete */

/*---------------------------------------------------------------------------------------
 Name: OS_QueueGet

 Purpose: Receive a message on a message queue.  Will pend or timeout on the receive.
 Returns: OS_ERR_INVALID_ID if the given ID does not exist
 OS_ERR_INVALID_POINTER if a pointer passed in is NULL
 OS_QUEUE_EMPTY if the Queue has no messages on it to be recieved
 OS_QUEUE_TIMEOUT if the timeout was OS_PEND and the time expired
 OS_QUEUE_INVALID_SIZE if the size copied from the queue was not correct
 OS_SUCCESS if success
 ---------------------------------------------------------------------------------------*/
int osal_error_pipe = 0;
int32 OS_QueueGet (uint32 queue_id, void *data, uint32 size, uint32 *size_copied, int32 timeout)
{

    OS_queue_record_t * queue;
    int32 result = OS_SUCCESS;
    int status;
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

    queue = &OS_queue_table[queue_id];
    *size_copied = 0;
    int messages_len;


    if(timeout != OS_PEND && timeout != OS_CHECK)
       timeout = OS_CHECK;

    if(timeout == OS_PEND )
    { //Wait for data
		status = sem_wait(queue->pipe_sema);
		while(status != 0)
		{
			printf("SEM Get FAILED For queue %s\n", queue->name);
    	status = sem_wait(queue->pipe_sema);
//			perror("sem_wait:");
		}

    }
    else if(timeout == OS_CHECK)
	{
        status = sem_trywait(queue->pipe_sema);
        if(status == 0)
        {
            if(queue->messages_in_pipe == 0)
            {
                printf("Semophore said we had data, but there isn't data there.");
                return OS_QUEUE_EMPTY;
            }
            //There is data to be had
        }
		else if(queue->messages_in_pipe > 0) /*This should not have happend */
		{
			printf("ERRNO is set to %d with %d messages left in pipe \n,", errno,queue->messages_in_pipe);
			osal_error_pipe++;
			return OS_QUEUE_EMPTY;
		}
		else /*No messages in our pipe*/
    	{
    		return OS_QUEUE_EMPTY;
    	}

    }
    else /*timeout */
    { 
        
        /*pthread_mutex_timedlock*/
        /*TODO Impliement a timeout routine. see sem_timedwait()*/
        return OS_ERROR;
    }

    pthread_mutex_lock(&queue->pipe_mutex);

        /*Keeping my typing smaller.*/
        char* buffer = queue->buffer;
        /*Figure out the buffer length*/
        memcpy((char*)size_copied,&buffer[queue->first_byte],sizeof(*size_copied));

        if(*size_copied > size)
        {
            /*Kind of a graceful failure*/
            result = OS_QUEUE_INVALID_SIZE;
            *size_copied = size;
        }
        /*Copy the data out*/
        memcpy(data,&buffer[queue->first_byte + sizeof(*size_copied)], *size_copied); /*copy data*/

        /*Update the starting position.*/
        /*This is kind of inefficient but avoids memory fragmentation...*/
        queue->first_byte += (queue->max_message_size + sizeof(*size_copied)) ;
        queue->first_byte %= queue->buffer_size; /*Wrap around if needed...*/

        /*Update the message count.*/
        if(        queue->messages_in_pipe == 0)
        {
        	printf("Error this shouldn't have happened\n");
        }
        else
        {
        queue->messages_in_pipe--;
        }
        if(queue->messages_in_pipe > queue->queue_depth)
        {
        	printf("Error this shouldn't have happend\n");
        	queue->messages_in_pipe = queue->queue_depth;
        }


    pthread_mutex_unlock(&queue->pipe_mutex);


    if(((*size_copied) != size )&& ((queue->flags & OS_VARIABLE_SIZED_QUEUE) == 0))
    {
        /*size_copied = 0; //Makes it harder to debug, plus the buffer has already been copied into. */
        result = OS_QUEUE_INVALID_SIZE;
    }

    return result;

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

    OS_queue_record_t * queue;
    int32 result = OS_SUCCESS;
    int status= 0;
    char * buffer ;

    /*
    ** Check Parameters
    */
    if(queue_id >= OS_MAX_QUEUES || OS_queue_table[queue_id].free == TRUE)
    {
        result =  OS_ERR_INVALID_ID;
    }
    else if (data == NULL)
    {
        result =  OS_INVALID_POINTER;
    }

    if(result != OS_SUCCESS)
    {
        return result;
    }
    queue = &OS_queue_table[queue_id];
    pthread_mutex_lock(&queue->pipe_mutex);
        /* check if queue is full */
        if(queue->messages_in_pipe >= queue->queue_depth)
        {
            result = OS_QUEUE_FULL;
        }
        else if(queue->max_message_size < size)
        {
            result = OS_QUEUE_INVALID_SIZE;
        }
        else if((queue->max_message_size != size )&& ((queue->flags & OS_VARIABLE_SIZED_QUEUE) == 0))
        {
            /*size_copied = 0; //Makes it harder to debug, plus the buffer has already been coppied into. */
            result = OS_QUEUE_INVALID_SIZE;
        }
        /* send message */
        if(result != OS_SUCCESS)
        {
            pthread_mutex_unlock(&queue->pipe_mutex);
            return result;
        }
        buffer = queue->buffer;

        memcpy(&buffer[queue->last_byte],&size,sizeof(size));

        memcpy(&buffer[queue->last_byte + sizeof(size)],(char*)data,size);

        queue->last_byte += (queue->max_message_size + sizeof(size));
        queue->last_byte %= queue->buffer_size;
        queue->messages_in_pipe++;
    pthread_mutex_unlock(&queue->pipe_mutex);

    /*unlock the pipe as we have data*/
    //pthread_mutex_unlock (&queue->pipe_mutex_waiter );
    status = sem_post(queue->pipe_sema);
    if(status != 0)
    {
    	perror('QUEUE_PUT:');
    }

    return OS_SUCCESS;

} /* end OS_QueuePut */


/* --------------------- END POSIX MESSAGE QUEUE IMPLEMENTATION ---------------------- */

/*--------------------------------------------------------------------------------------
    Name: OS_QueueGetIdByName

    Purpose: This function tries to find a queue Id given the name of the queue. The
             id of the queue is passed back in queue_id

    Returns: OS_INVALID_POINTER if the name or id pointers are NULL
             OS_ERR_NAME_TOO_LONG the name passed in is too long
             OS_ERR_NAME_NOT_FOUND the name was not found in the table
             OS_SUCCESS if success

---------------------------------------------------------------------------------------*/

// int32 OS_QueueGetIdByName (uint32 *queue_id, const char *queue_name)
// {
//     uint32 i;

//     if(queue_id == NULL || queue_name == NULL)
//     {
//        return OS_INVALID_POINTER;
//     }

//     /* a name too long wouldn't have been allowed in the first place
//      * so we definitely won't find a name too long*/

//     if (strlen(queue_name) > OS_MAX_API_NAME)
//     {
//        return OS_ERR_NAME_TOO_LONG;
//     }

//     for (i = 0; i < OS_MAX_QUEUES; i++)
//     {
//         if (OS_queue_table[i].free != TRUE &&
//            (strcmp(OS_queue_table[i].name, (char*) queue_name) == 0 ))
//         {
//             *queue_id = i;
//             return OS_SUCCESS;
//         }
//     }

//     /* The name was not found in the table,
//      *  or it was, and the queue_id isn't valid anymore */
//     return OS_ERR_NAME_NOT_FOUND;

// }/* end OS_QueueGetIdByName */

// /*---------------------------------------------------------------------------------------
//     Name: OS_QueueGetInfo

//     Purpose: This function will pass back a pointer to structure that contains
//              all of the relevant info (name and creator) about the specified queue.

//     Returns: OS_INVALID_POINTER if queue_prop is NULL
//              OS_ERR_INVALID_ID if the ID given is not  a valid queue
//              OS_SUCCESS if the info was copied over correctly
// ---------------------------------------------------------------------------------------*/
// int32 OS_QueueGetInfo (uint32 queue_id, OS_queue_prop_t *queue_prop)
// {
//     /* Check to see that the id given is valid */

//     if (queue_prop == NULL)
//     {
//         return OS_INVALID_POINTER;
//     }

//     if (queue_id >= OS_MAX_QUEUES || OS_queue_table[queue_id].free == TRUE)
//     {
//         return OS_ERR_INVALID_ID;
//     }

//     /* put the info into the stucture */
//     pthread_mutex_lock(&OS_queue_table_mut);

//     queue_prop -> creator =   OS_queue_table[queue_id].creator;
//     strcpy(queue_prop -> name, OS_queue_table[queue_id].name);

//     pthread_mutex_unlock(&OS_queue_table_mut);

//     return OS_SUCCESS;

// } /* end OS_QueueGetInfo */

#endif /* OSAL_QUEUE_MUTEX */
