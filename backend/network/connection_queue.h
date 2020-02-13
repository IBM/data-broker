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

#ifndef BACKEND_NETWORK_CONNECTION_QUEUE_H_
#define BACKEND_NETWORK_CONNECTION_QUEUE_H_

#include "logutil.h"
#include "network/connection.h"


#include <inttypes.h>
#include <stddef.h>
#ifndef __APPLE__
#include <malloc.h>  // malloc
#endif
#include <stdlib.h> // calloc
#include <errno.h>
#include <string.h>

#include <pthread.h>

typedef struct dbBE_Connection_queue
{
  volatile uint64_t _head;
  volatile uint64_t _tail;
  volatile dbBE_Connection_t **_connections;
  unsigned _queue_size;
  pthread_mutex_t _mutex;
  pthread_cond_t _wakeup;
} dbBE_Connection_queue_t;


#define dbBE_Connection_queue_head( queue ) ( (queue)->_head )
#define dbBE_Connection_queue_tail( queue ) ( (queue)->_tail )

static inline
dbBE_Connection_queue_t* dbBE_Connection_queue_create( const int size )
{
  dbBE_Connection_queue_t *queue = (dbBE_Connection_queue_t*)malloc( sizeof( dbBE_Connection_queue_t ) );
  if( queue == NULL )
    return NULL;

  memset( queue, 0, sizeof( dbBE_Connection_queue_t ) );

  queue->_connections = (volatile dbBE_Connection_t**)calloc( size, sizeof( void* ) );
  if( queue->_connections == NULL )
  {
    free( queue );
    return NULL;
  }
  queue->_queue_size = size;

  pthread_mutex_init( &queue->_mutex, NULL );
  pthread_cond_init( &queue->_wakeup, NULL );
  return queue;
}

static inline
int dbBE_Connection_queue_destroy( dbBE_Connection_queue_t *queue )
{
  if( queue == NULL )
    return -EINVAL;

  pthread_cond_destroy( &queue->_wakeup );
  pthread_mutex_destroy( &queue->_mutex );
  free( queue->_connections );
  memset( queue, 0, sizeof( dbBE_Connection_queue_t ) );
  free( queue );

  return 0;
}

static inline
dbBE_Connection_t* dbBE_Connection_queue_pop_ext( dbBE_Connection_queue_t *queue, int blocking )
{
  volatile uint64_t head, tail;

  if( queue == NULL )
    return NULL;

  LOG( DBG_TRACE, stderr, "Connection queue: %"PRId64":%"PRId64"\n", dbBE_Connection_queue_head( queue ), dbBE_Connection_queue_tail( queue ) );
  pthread_mutex_lock( &queue->_mutex );

deleted_slot_retry:

  head = dbBE_Connection_queue_head( queue );
  tail = dbBE_Connection_queue_tail( queue );
  if( head == tail )
  {
    // use the empty queue status to reset the counters
    dbBE_Connection_queue_head( queue ) = 0;
    dbBE_Connection_queue_tail( queue ) = 0;

    // if blocking operation is requested, then this will wait for conditional var signalled by push()
    if( blocking )
    {
      pthread_cond_wait( &queue->_wakeup, &queue->_mutex );
      goto deleted_slot_retry;
    }

    pthread_mutex_unlock( &queue->_mutex );
    return NULL;
  }

  // repeated check of the tail because there maybe holes in the queue caused by removals
  dbBE_Connection_t *conn = NULL;
  while(( conn == NULL ) && ( tail < head ))
  {
    conn = (dbBE_Connection_t*)queue->_connections[ tail % queue->_queue_size ];
    queue->_connections[ tail % queue->_queue_size ] = NULL;
    ++tail;
  }

  // update actual queue tail
  dbBE_Connection_queue_tail( queue ) = tail;

  // reset counters if queue is empty now
  if( dbBE_Connection_queue_head( queue ) == dbBE_Connection_queue_tail( queue ) )
  {
    dbBE_Connection_queue_head( queue ) = 0;
    dbBE_Connection_queue_tail( queue ) = 0;
  }

  if( conn == NULL )
    goto deleted_slot_retry;

  dbBE_Connection_set_inactivate( conn );

  pthread_mutex_unlock( &queue->_mutex );

  return conn;
}

static inline
dbBE_Connection_t* dbBE_Connection_queue_pop( dbBE_Connection_queue_t *queue )
{
  return dbBE_Connection_queue_pop_ext( queue, 0 );
}

static inline
dbBE_Connection_t* dbBE_Connection_queue_pop_wait( dbBE_Connection_queue_t *queue )
{
  return dbBE_Connection_queue_pop_ext( queue, 1 );
}

static inline
int dbBE_Connection_queue_push( dbBE_Connection_queue_t *queue,
                                      dbBE_Connection_t *conn )
{
  if(( queue == NULL ) || ( conn == NULL ))
    return -EINVAL;

  pthread_mutex_lock( &queue->_mutex );

  volatile uint64_t head = dbBE_Connection_queue_head( queue );
  volatile uint64_t tail = dbBE_Connection_queue_tail( queue );

  if(( head - tail >= queue->_queue_size ) || ( queue->_connections[ head % queue->_queue_size ] != NULL ))
  {
    LOG( DBG_WARN, stderr, "connection queue full or inconsistent %ld:%ld\n", head, tail )
    pthread_mutex_unlock( &queue->_mutex );
    return -ENOMEM;
  }

  dbBE_Connection_set_active( conn );
  queue->_connections[ head % queue->_queue_size ] = conn;
  ++dbBE_Connection_queue_head( queue );

  pthread_cond_signal( &queue->_wakeup );
  pthread_mutex_unlock( &queue->_mutex );

  LOG( DBG_TRACE, stderr, "queue push: %"PRId64":%"PRId64"\n", dbBE_Connection_queue_head( queue ), dbBE_Connection_queue_tail( queue ) );

  return 0;
}

static inline
int dbBE_Connection_queue_remove_connection( dbBE_Connection_queue_t *queue,
                                                   const dbBE_Connection_t *conn )
{
  if(( queue == NULL ) || ( conn == NULL ))
    return -EINVAL;

  pthread_mutex_lock( &queue->_mutex );

  uint64_t n;
  uint64_t head = dbBE_Connection_queue_head( queue );
  uint64_t tail = dbBE_Connection_queue_tail( queue );

  if( head == tail )
  {
    pthread_mutex_unlock( &queue->_mutex );
    return 0;
  }

  for( n=tail; n<head; ++n )
    if( queue->_connections[ n % queue->_queue_size ] == conn )
      queue->_connections[ n % queue->_queue_size ] = NULL;

  pthread_mutex_unlock( &queue->_mutex );

  return 0;
}

#endif /* BACKEND_NETWORK_CONNECTION_QUEUE_H_ */
