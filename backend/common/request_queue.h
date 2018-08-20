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

#ifndef BACKEND_INTERFACE_REQUEST_QUEUE_H_
#define BACKEND_INTERFACE_REQUEST_QUEUE_H_

#include <stddef.h>
#include <errno.h>
#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>
#endif
#include <string.h>

#include "dbbe_api.h"

typedef struct
{
  dbBE_Request_t *_head;
  dbBE_Request_t *_tail;
  size_t _len;
} dbBE_Request_queue_t;


/*
 * create and initialize the queue
 */
static inline
dbBE_Request_queue_t* dbBE_Request_queue_create( const size_t size )
{
  return (dbBE_Request_queue_t*)calloc( 1, sizeof( dbBE_Request_queue_t ) );
}

/*
 * return the current length of the queue (number of queued requests)
 */
static inline
size_t dbBE_Request_queue_len( dbBE_Request_queue_t *queue )
{
  if( queue == NULL )
    return 0;
  return queue->_len;
}

/*
 * append an entry to the queue
 */
static inline
int dbBE_Request_queue_push( dbBE_Request_queue_t *queue,
                             dbBE_Request_t *request )
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
 * return and remove the first entry from the queue
 */
static
dbBE_Request_t* dbBE_Request_queue_pop( dbBE_Request_queue_t *queue )
{
  if( queue == NULL )
    return NULL;

  dbBE_Request_t *ret = queue->_head;

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
 * delete an element, in case users cancel a request
 */
static inline
int dbBE_Request_queue_delete( dbBE_Request_queue_t *queue,
                               dbBE_Request_t *request )
{
  if(( queue == NULL ) || ( request == NULL ))
    return EINVAL;

  dbBE_Request_t *to_del, *ptr = queue->_head;
  to_del = ptr;

  // test if it's the head to remove
  if( to_del == request )
  {
    if( queue->_head == queue->_tail )
      queue->_tail = queue->_tail->_next;
    queue->_head = to_del->_next;
  }
  else
  { // scan the rest of the queue for the request
    while( ptr->_next != NULL )
    {
      to_del = ptr->_next;

      if( to_del == request )
      {
        // do we need to remove the tail?
        if( to_del == queue->_tail )
          queue->_tail = ptr;

        // remove the entry
        ptr->_next = to_del->_next;
        break;
      }
      ptr = ptr->_next;
    }
  }

  if( to_del != NULL )
  {
    // detach the entry
    to_del->_next = NULL;
    --queue->_len;
    return 0;
  }
  return ENOENT;
}

/*
 * cleanup and destroy the queue
 */
static inline
int dbBE_Request_queue_destroy( dbBE_Request_queue_t *queue )
{
  if( queue == NULL )
  {
    return EINVAL;
  }

  // drain the queue
  while( dbBE_Request_queue_pop( queue ) );

  memset( queue, 0, sizeof( dbBE_Request_queue_t ) );
  free( queue );
  return 0;
}

#endif /* BACKEND_INTERFACE_REQUEST_QUEUE_H_ */
