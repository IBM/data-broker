/*
 * Copyright © 2018 IBM Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <stddef.h>
#include <errno.h>
#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>  // malloc
#endif

#include <string.h>

#include "s2r_queue.h"

/*
 * create and initialize a request queue for redirects and retries
 */
dbBE_Redis_s2r_queue_t* dbBE_Redis_s2r_queue_create( const size_t size )
{
  dbBE_Redis_s2r_queue_t *queue = (dbBE_Redis_s2r_queue_t*)malloc( sizeof( dbBE_Redis_s2r_queue_t ) );
  if( queue == NULL )
  {
    return NULL;
  }

  memset( queue, 0, sizeof( dbBE_Redis_s2r_queue_t ) );

  return queue;
}

/*
 * cleanup and destroy a request queue for redirects and retries
 */
int dbBE_Redis_s2r_queue_destroy( dbBE_Redis_s2r_queue_t *queue )
{
  if( queue == NULL )
  {
    return EINVAL;
  }

  // drain the queue
  while( dbBE_Redis_s2r_queue_pop( queue ) );

  memset( queue, 0, sizeof( dbBE_Redis_s2r_queue_t ) );
  free( queue );
  return 0;
}

/*
 * return the current length of the queue (number of queued requests)
 */
size_t dbBE_Redis_s2r_queue_len( dbBE_Redis_s2r_queue_t *queue )
{
  if( queue == NULL )
    return 0;
  return queue->_len;
}


/*
 * append an entry to the retry queue
 */
int dbBE_Redis_s2r_queue_push( dbBE_Redis_s2r_queue_t *queue,
                                 dbBE_Redis_request_t *request )
{
  if(( queue == NULL ) || ( request == NULL ))
    return EINVAL;

  if( queue->_head == NULL )
    queue->_head = request;

  if( queue->_tail != NULL )
    queue->_tail->_next = request;

  queue->_tail = request;
  request->_next = NULL;
  ++queue->_len;
  return 0;
}

/*
 * retrieve the first entry from the queue
 */
dbBE_Redis_request_t* dbBE_Redis_s2r_queue_pop( dbBE_Redis_s2r_queue_t *queue )
{
  if( queue == NULL )
    return NULL;

  dbBE_Redis_request_t *ret = queue->_head;

  if( queue->_head != NULL )
    queue->_head = ret->_next;

  if(( queue->_tail != NULL ) && ( queue->_tail == ret ))
    queue->_tail = ret->_next;

  // detach from queue
  if( ret != NULL )
  {
    ret->_next = NULL;
    --queue->_len;
  }
  return ret;
}

/*
 * wipe all entries from the queue
 */
int dbBE_Redis_s2r_queue_flush( dbBE_Redis_s2r_queue_t *queue )
{
  if( queue == NULL )
    return EINVAL;

  dbBE_Redis_request_t *req;
  while( (req = dbBE_Redis_s2r_queue_pop( queue )) != NULL )
    dbBE_Redis_request_destroy( req );

  return 0;
}
