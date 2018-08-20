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

#ifndef BACKEND_REDIS_CONNECTION_QUEUE_H_
#define BACKEND_REDIS_CONNECTION_QUEUE_H_

#include <inttypes.h>
#include <stddef.h>
#ifndef __APPLE__
#include <malloc.h>  // malloc
#endif
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <pthread.h>

#include "definitions.h"
#include "connection.h"

typedef struct dbBE_Redis_connection_queue
{
  volatile uint64_t _head;
  volatile uint64_t _tail;
  volatile dbBE_Redis_connection_t *_connections[ DBBE_REDIS_MAX_CONNECTIONS ];
  pthread_mutex_t _mutex;
} dbBE_Redis_connection_queue_t;


#define dbBE_Redis_connection_queue_head( queue ) ( (queue)->_head )
#define dbBE_Redis_connection_queue_tail( queue ) ( (queue)->_tail )

static inline
dbBE_Redis_connection_queue_t* dbBE_Redis_connection_queue_create()
{
  dbBE_Redis_connection_queue_t *queue = (dbBE_Redis_connection_queue_t*)malloc( sizeof( dbBE_Redis_connection_queue_t ) );
  if( queue == NULL )
    return NULL;

  memset( queue, 0, sizeof( dbBE_Redis_connection_queue_t ) );

  pthread_mutex_init( &queue->_mutex, NULL );
  return queue;
}

static inline
int dbBE_Redis_connection_queue_destroy( dbBE_Redis_connection_queue_t *queue )
{
  if( queue == NULL )
    return -EINVAL;

  pthread_mutex_destroy( &queue->_mutex );
  memset( queue, 0, sizeof( dbBE_Redis_connection_queue_t ) );
  free( queue );

  return 0;
}

static inline
dbBE_Redis_connection_t* dbBE_Redis_connection_queue_pop( dbBE_Redis_connection_queue_t *queue )
{
  if( queue == NULL )
    return NULL;

  pthread_mutex_lock( &queue->_mutex );

  uint64_t head = dbBE_Redis_connection_queue_head( queue );
  uint64_t tail = dbBE_Redis_connection_queue_tail( queue );
  if( head == tail )
  {
    pthread_mutex_unlock( &queue->_mutex );
    return NULL;
  }

  dbBE_Redis_connection_t *conn = (dbBE_Redis_connection_t*)queue->_connections[ tail % DBBE_REDIS_MAX_CONNECTIONS ];
  queue->_connections[ tail % DBBE_REDIS_MAX_CONNECTIONS ] = NULL;

  ++dbBE_Redis_connection_queue_tail( queue );

  // reset counters if queue is empty now
  if( dbBE_Redis_connection_queue_head( queue ) == dbBE_Redis_connection_queue_tail( queue ) )
  {
    dbBE_Redis_connection_queue_head( queue ) = 0;
    dbBE_Redis_connection_queue_tail( queue ) = 0;
  }

  pthread_mutex_unlock( &queue->_mutex );

  return conn;
}

static inline
int dbBE_Redis_connection_queue_push( dbBE_Redis_connection_queue_t *queue,
                                      dbBE_Redis_connection_t *conn )
{
  if(( queue == NULL ) || ( conn == NULL ))
    return -EINVAL;

  pthread_mutex_lock( &queue->_mutex );

  uint64_t head = dbBE_Redis_connection_queue_head( queue );
  uint64_t tail = dbBE_Redis_connection_queue_tail( queue );

  if(( head - tail >= DBBE_REDIS_MAX_CONNECTIONS ) || ( queue->_connections[ head ] != NULL ))
  {
    pthread_mutex_unlock( &queue->_mutex );
    return -ENOMEM;
  }

  queue->_connections[ head ] = conn;
  ++dbBE_Redis_connection_queue_head( queue );

  pthread_mutex_unlock( &queue->_mutex );

  return 0;
}

#endif /* BACKEND_REDIS_CONNECTION_QUEUE_H_ */
