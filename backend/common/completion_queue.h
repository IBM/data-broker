/*
 * Copyright © 2018-2020 IBM Corporation
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

#ifndef BACKEND_INTERFACE_COMPLETION_QUEUE_H_
#define BACKEND_INTERFACE_COMPLETION_QUEUE_H_

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
  dbBE_Completion_t *_head;
  dbBE_Completion_t *_tail;
  size_t _len;
} dbBE_Completion_queue_t;


/*
 * create and initialize the queue
 */
static inline
dbBE_Completion_queue_t* dbBE_Completion_queue_create( const size_t size )
{
  return (dbBE_Completion_queue_t*)calloc( 1, sizeof( dbBE_Completion_queue_t ) );
}

/*
 * return the current length of the queue (number of queued completions)
 */
static inline
size_t dbBE_Completion_queue_len( dbBE_Completion_queue_t *queue )
{
  if( queue == NULL )
    return 0;
  return queue->_len;
}

/*
 * append an entry to the queue
 */
static inline
int dbBE_Completion_queue_push( dbBE_Completion_queue_t *queue,
                             dbBE_Completion_t *completion )
{
  if(( queue == NULL ) || ( completion == NULL ))
    return EINVAL;

  if( queue->_head == NULL )
    queue->_head = completion;

  if( queue->_tail != NULL )
    queue->_tail->_next = completion;

  queue->_tail = completion;
  completion->_next = NULL;
  ++queue->_len;
  return 0;
}

/*
 * return and remove the first entry from the queue
 */
static inline
dbBE_Completion_t* dbBE_Completion_queue_pop( dbBE_Completion_queue_t *queue )
{
  if( queue == NULL )
    return NULL;

  dbBE_Completion_t *ret = queue->_head;

  if( queue->_head != NULL )
  {
    // tail needs to be adjusted in case there's only one element
    if( queue->_tail == queue->_head)
      queue->_tail = queue->_head->_next;

    queue->_head = ret->_next;
  }
  else
    return NULL;

  // detach from queue
  if( ret != NULL )
  {
    ret->_next = NULL;
    --queue->_len;
  }
  return ret;
}

/*
 * delete an element, in case users cancel a completion
 */
static inline
int dbBE_Completion_queue_delete( dbBE_Completion_queue_t *queue,
                               dbBE_Completion_t *completion )
{
  if(( queue == NULL ) || ( completion == NULL ))
    return EINVAL;

  dbBE_Completion_t *to_del, *ptr = queue->_head;
  to_del = ptr;

  // test if it's the head to remove
  if( to_del == completion )
  {
    if( queue->_head == queue->_tail )
      queue->_tail = queue->_tail->_next;
    queue->_head = to_del->_next;
  }
  else
  { // scan the rest of the queue for the completion
    while( ptr->_next != NULL )
    {
      to_del = ptr->_next;

      if( to_del == completion )
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
int dbBE_Completion_queue_destroy( dbBE_Completion_queue_t *queue )
{
  if( queue == NULL )
  {
    return EINVAL;
  }

  // drain the queue
  while( dbBE_Completion_queue_pop( queue ) );

  memset( queue, 0, sizeof( dbBE_Completion_queue_t ) );
  free( queue );
  return 0;
}

#endif /* BACKEND_INTERFACE_COMPLETION_QUEUE_H_ */
