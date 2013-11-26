#ifdef OSAL_SOCKET_QUEUE
/****************************************************************************************
                                MESSAGE QUEUE API
****************************************************************************************/

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
   int                     tmpSkt;
   int                     returnStat;
   struct sockaddr_in      servaddr;
   int                     i;
   uint32                  possible_qid;

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
    for (i = 0; i < OS_MAX_QUEUES; i++)
    {
        if ((OS_queue_table[i].free == FALSE) &&
                strcmp ((char*) queue_name, OS_queue_table[i].name) == 0)
        {
            pthread_mutex_unlock(&OS_queue_table_mut);
            return OS_ERR_NAME_TAKEN;
        }
    }

    /* Set the possible task Id to not free so that
     * no other task can try to use it */

    OS_queue_table[possible_qid].free = FALSE;

    pthread_mutex_unlock(&OS_queue_table_mut);

    tmpSkt = socket(AF_INET, SOCK_DGRAM, 0);
    if ( tmpSkt == -1 )
    {
        pthread_mutex_lock(&OS_queue_table_mut);
        OS_queue_table[possible_qid].free = TRUE;
        pthread_mutex_unlock(&OS_queue_table_mut);

        printf("Failed to create a socket on OS_QueueCreate. errno = %d\n",errno);
        return OS_ERROR;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_port        = htons(OS_BASE_PORT + possible_qid);
    servaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

   /*
   ** bind the input socket to a pipe
   ** port numbers are OS_BASE_PORT + queue_id
   */
   returnStat = bind(tmpSkt,(struct sockaddr *)&servaddr, sizeof(servaddr));

   if ( returnStat == -1 )
   {
        pthread_mutex_lock(&OS_queue_table_mut);
        OS_queue_table[possible_qid].free = TRUE;
        pthread_mutex_unlock(&OS_queue_table_mut);

        printf("bind failed on OS_QueueCreate. errno = %d\n",errno);
        return OS_ERROR;
   }

   /*
   ** store socket handle
   */
   *queue_id = possible_qid;

    pthread_mutex_lock(&OS_queue_table_mut);

   OS_queue_table[*queue_id].id = tmpSkt;
   OS_queue_table[*queue_id].free = FALSE;
   strcpy( OS_queue_table[*queue_id].name, (char*) queue_name);
   OS_queue_table[*queue_id].creator = OS_FindCreator();

    pthread_mutex_unlock(&OS_queue_table_mut);

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
    /* Check to see if the queue_id given is valid */

    if (queue_id >= OS_MAX_QUEUES || OS_queue_table[queue_id].free == TRUE)
    {
        return OS_ERR_INVALID_ID;
    }

    /* Try to delete the queue */

    if(close(OS_queue_table[queue_id].id) !=0)
    {
        return OS_ERROR;
    }

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
            OS_INVALID_POINTER if a pointer passed in is NULL
            OS_QUEUE_EMPTY if the Queue has no messages on it to be recieved
            OS_QUEUE_TIMEOUT if the timeout was OS_PEND and the time expired
            OS_QUEUE_INVALID_SIZE if the size copied from the queue was not correct
            OS_ERROR if there was an error waiting for the timeout
            OS_SUCCESS if success
---------------------------------------------------------------------------------------*/
int32 OS_QueueGet (uint32 queue_id, void *data, uint32 size, uint32 *size_copied, int32 timeout)
{
   int sizeCopied;
   int flags;

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

   /*
   ** Read the socket for data
   */
   if (timeout == OS_PEND)
   {
      fcntl(OS_queue_table[queue_id].id,F_SETFL,0);
      /*
      ** A signal can interrupt the recvfrom call, so the call has to be done with
      ** a loop
      */
      do
      {
         sizeCopied = recvfrom(OS_queue_table[queue_id].id, data, size, 0, NULL, NULL);
      } while ( sizeCopied == -1 && errno == EINTR );

      if(sizeCopied != size )
      {
         *size_copied = sizeCopied;
         return(OS_QUEUE_INVALID_SIZE);
      }
   }
   else if (timeout == OS_CHECK)
   {
      flags = fcntl(OS_queue_table[queue_id].id, F_GETFL, 0);
      fcntl(OS_queue_table[queue_id].id,F_SETFL,flags|O_NONBLOCK);

      sizeCopied = recvfrom(OS_queue_table[queue_id].id, data, size, 0, NULL, NULL);

      fcntl(OS_queue_table[queue_id].id,F_SETFL,flags);

      if (sizeCopied == -1 && errno == EWOULDBLOCK )
      {
         *size_copied = 0;
         return(OS_QUEUE_EMPTY);
      }
      else if(sizeCopied != size )
      {
         *size_copied = sizeCopied;
         return(OS_QUEUE_INVALID_SIZE);
      }

   }
   else /* timeout */
   {
      int    rv;
      int    sock = OS_queue_table[queue_id].id;
      struct timeval tv_timeout;
      fd_set fdset;

      FD_ZERO( &fdset );
      FD_SET( sock, &fdset );

      /*
      ** Translate the timeout value from milliseconds into
      ** seconds and nanoseconds.
      */
      tv_timeout.tv_usec = (timeout % 1000) * 1000;
      tv_timeout.tv_sec = timeout / 1000;

      /*
      ** Use select to wait for data to come in on the socket
      ** TODO: If the select call is interrupted, the timeout should be
      ** re-computed to avoid having to delay for the full time.
      **
      */
      do
      {
         FD_ZERO( &fdset );
         FD_SET( sock, &fdset );
         rv = select( sock+1, &fdset, NULL, NULL, &tv_timeout );
      } while ( rv == -1 && errno == EINTR );

      if( rv > 0 )
      {
         /* got a packet within the timeout */
         sizeCopied = recvfrom(OS_queue_table[queue_id].id, data, size, 0, NULL, NULL);

         if ( sizeCopied == size )
         {
            *size_copied = sizeCopied;
            return OS_SUCCESS;
         }
         else
         {
            *size_copied = sizeCopied;
            return OS_QUEUE_INVALID_SIZE;
         }
      }
      else if ( rv < 0 )
      {
         /*
         ** Need to handle Select error codes here
         ** This might need a new error code: OS_QUEUE_TIMEOUT_ERROR
         */
         printf("Bad return value from select: %d, sock = %d\n", rv, sock);
         *size_copied = 0;
         return OS_ERROR;
      }

      /*
      ** If rv == 0, then the select timed out with no data
      */
      return(OS_QUEUE_TIMEOUT);

   } /* END timeout */

   /*
   ** Should never really get here.
   */
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
int32 OS_QueuePut (uint32 queue_id, void *data, uint32 size, uint32 flags)
{

   struct sockaddr_in serva;
   static int         socketFlags = 0;
   int                bytesSent    = 0;
   int                tempSkt      = 0;

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

   /*
   ** specify the IP addres and port number of destination
   */
   memset(&serva, 0, sizeof(serva));
   serva.sin_family      = AF_INET;
   serva.sin_port        = htons(OS_BASE_PORT + queue_id);
   serva.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

   /*
   ** open a temporary socket to transfer the packet to MR
   */
   tempSkt = socket(AF_INET, SOCK_DGRAM, 0);

   /*
   ** send the packet to the message router task (MR)
   */
   bytesSent = sendto(tempSkt,(char *)data, size, socketFlags,
                    (struct sockaddr *)&serva, sizeof(serva));

   if( bytesSent == -1 )
   {
      close(tempSkt);
      return(OS_ERROR);
   }

   if( bytesSent != size )
   {
      close(tempSkt);
      return(OS_QUEUE_FULL);
   }

   /*
   ** close socket
   */
   close(tempSkt);

   return OS_SUCCESS;
} /* end OS_QueuePut */

#endif
