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

#include "logutil.h"
#include "event_mgr.h"


#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#ifndef __APPLE__
#include <malloc.h>  // malloc
#endif
#include <event2/event.h>

void dbBE_Redis_event_mgr_callback( evutil_socket_t socket, short ev_type, void *arg );

/*
 * create and initialize the event mgr
 */
dbBE_Redis_event_mgr_t* dbBE_Redis_event_mgr_init( unsigned default_timeout )
{
  dbBE_Redis_event_mgr_t *evmgr = (dbBE_Redis_event_mgr_t*)malloc( sizeof( dbBE_Redis_event_mgr_t ) );
  if( evmgr == NULL )
  {
    LOG( DBG_ERR, stderr, "event_mgr_init: Failed to allocate event manager\n" );
    return NULL;
  }

  memset( evmgr, 0, sizeof( dbBE_Redis_event_mgr_t ) );

  evmgr->_active_queue = dbBE_Redis_connection_queue_create();
  if( evmgr->_active_queue == NULL )
  {
    LOG( DBG_ERR, stderr, "event_mgr_init: Failed to allocate active connection queue\n" );
    free( evmgr );
    return NULL;
  }

  evmgr->_evbase = event_base_new();
  if( evmgr->_evbase == NULL )
  {
    LOG( DBG_ERR, stderr, "event_mgr_init: Failed to initialize event manager\n" );
    dbBE_Redis_connection_queue_destroy( evmgr->_active_queue );
    free( evmgr );
    return NULL;
  }

  evmgr->_timeout.tv_sec = default_timeout;

  return evmgr;
}


/*
 * exit and destroy the event mgr
 */
int dbBE_Redis_event_mgr_exit( dbBE_Redis_event_mgr_t *ev_mgr )
{
  if( ev_mgr == NULL )
  {
    LOG( DBG_ERR, stderr, "event_mgr_exit: Invalid argument ev_mgr=%p\n", ev_mgr );
    return -EINVAL;
  }

  if( ev_mgr->_evbase != NULL )
  {
    event_base_free( ev_mgr->_evbase );
  }

  dbBE_Redis_connection_queue_destroy( ev_mgr->_active_queue );

  memset( ev_mgr, 0, sizeof( dbBE_Redis_event_mgr_t ) );
  free( ev_mgr );

  return 0;
}


int dbBE_Redis_event_mgr_add( dbBE_Redis_event_mgr_t *ev_mgr,
                              const dbBE_Redis_connection_t *conn )
{
  int rc = 0;

  if(( ev_mgr == NULL ) || ( conn == NULL ))
  {
    LOG( DBG_ERR, stderr, "event_mgr_add: Invalid argument: ev_mgr=%p, conn=%p\n", ev_mgr, conn );
    return -EINVAL;
  }

  if( conn->_socket <= 0 )
  {
    LOG( DBG_ERR, stderr, "event_mgr_add: conn=%p has incomplete state to add\n", conn );
    return -EBADF;
  }

  if( (unsigned int)conn->_index % DBBE_REDIS_MAX_CONNECTIONS != (unsigned int)conn->_index )
  {
    LOG( DBG_ERR, stderr, "event_mgr_add: connection index=%d out of range=%d\n", conn->_index, DBBE_REDIS_MAX_CONNECTIONS );
    return -ERANGE;
  }

  // check if this socket has an existing event registered already
  unsigned n;
  for( n = 0; n < DBBE_REDIS_MAX_CONNECTIONS; ++n )
  {
    struct event *ev = ev_mgr->_events[ n ];
    if( ev != NULL )
    {
      dbBE_Redis_event_info_t *ev_info = (dbBE_Redis_event_info_t*)event_get_callback_arg( ev );
      if( ev_info == NULL )
        continue;
      dbBE_Redis_connection_t *ev_conn = ev_info->_conn;
      if(( ev_conn == conn ) || ( ev_conn->_socket == conn->_socket ))
      {
        LOG( DBG_ERR, stderr, "event_mgr_add: requested socket=%d already registered\n", conn->_socket );
        return -EEXIST;
      }
    }
  }

  // create new event
  dbBE_Redis_event_info_t *info = (dbBE_Redis_event_info_t*)malloc( sizeof( dbBE_Redis_event_info_t ) );
  if( info == NULL )
  {
    LOG( DBG_ERR, stderr, "event_mgr_add: failed to allocate event info\n" );
    return -ENOMEM;
  }

  info->_conn = (dbBE_Redis_connection_t*)conn;
  info->_ev_mgr = ev_mgr;
  struct event *ev = event_new( ev_mgr->_evbase, conn->_socket, EV_READ | EV_PERSIST | EV_ET, dbBE_Redis_event_mgr_callback, (void*)info );
  if( ev == NULL )
  {
    LOG( DBG_ERR, stderr, "event_mgr_add: failed to allocate event.\n");
    free( info );
    return -ENOMEM;
  }

  ev_mgr->_events[ conn->_index ] = ev;

  if( event_add( ev, &ev_mgr->_timeout ) != 0 )
  {
    LOG( DBG_ERR, stderr, "event_mgr_add: failed to add event.\n" );
    event_free( ev );
    free( info );
    return -EFAULT;
  }

  return rc;
}

int dbBE_Redis_event_mgr_rearm( dbBE_Redis_event_mgr_t *ev_mgr,
                                const dbBE_Redis_connection_t *conn )
{
  int rc = 0;

  if(( ev_mgr == NULL ) || ( conn == NULL ) )
  {
    LOG( DBG_ERR, stderr, "event_mgr_rearm: Invalid argument: ev_mgr=%p, conn=%p\n", ev_mgr, conn );
    return -EINVAL;
  }

  if( (unsigned int)conn->_index % DBBE_REDIS_MAX_CONNECTIONS != (unsigned)conn->_index )
  {
    LOG( DBG_ERR, stderr, "event_mgr_rearm: connection index=%d out of range=%d\n", conn->_index, DBBE_REDIS_MAX_CONNECTIONS );
    return -ERANGE;
  }

  struct event *ev = ev_mgr->_events[ conn->_index ];
  if( ev == NULL )
  {
    LOG( DBG_INFO, stderr, "event_mgr_rearm: need to create new event for connection index=%d\n", conn->_index );
    return -ENOENT;
  }

  if( event_add( ev, &ev_mgr->_timeout ) != 0 )
  {
    LOG( DBG_ERR, stderr, "event_mgr_rearm: failed to rearm event.\n" );
    return -EFAULT;
  }
  return rc;
}


int dbBE_Redis_event_mgr_rm( dbBE_Redis_event_mgr_t *ev_mgr,
                             const dbBE_Redis_connection_t *conn )
{
  int rc = 0;

  if(( ev_mgr == NULL ) || ( conn == NULL ) )
  {
    LOG( DBG_ERR, stderr, "event_mgr_rm: Invalid argument: ev_mgr=%p, conn=%p\n", ev_mgr, conn );
    return -EINVAL;
  }

  if( (unsigned int)conn->_index % DBBE_REDIS_MAX_CONNECTIONS != (unsigned)conn->_index )
  {
    LOG( DBG_ERR, stderr, "event_mgr_rm: connection index=%d out of range=%d\n", conn->_index, DBBE_REDIS_MAX_CONNECTIONS );
    return -ERANGE;
  }

  struct event *ev = ev_mgr->_events[ conn->_index ];
  if( ev == NULL )
  {
    LOG( DBG_ERR, stderr, "event_mgr_rm: no event for connection index=%d\n", conn->_index );
    return -ENOENT;
  }

  // remove/free the info structure that's attached to the event
  dbBE_Redis_event_info_t *ev_info = (dbBE_Redis_event_info_t*)event_get_callback_arg( ev );
  // freeing later after successful event_del()

  if( event_del( ev ) != 0 )
  {
    LOG( DBG_ERR, stderr, "event_mgr_rm: failed to remove connection from event mgr\n" );
    return -EFAULT;
  }

  if( ev_info != NULL )
  {
    free( ev_info );
  }

  event_free( ev );

  // remove entry from tracking
  ev_mgr->_events[ conn->_index ] = NULL;

  return rc;
}


void dbBE_Redis_event_mgr_callback( evutil_socket_t socket, short ev_type, void *arg )
{
  LOG( DBG_TRACE, stderr, "Triggered callback for socket=%d, ev_type=%d, arg=%p\n", socket, ev_type, arg );

  dbBE_Redis_event_info_t *info = (dbBE_Redis_event_info_t*)arg;
  if( info == NULL )
  {
    LOG( DBG_ERR, stderr, "Triggered callback with NULL ptr argument.\n" );
    return;
  }

  dbBE_Redis_connection_t *conn = info->_conn;
  dbBE_Redis_event_mgr_t *ev_mgr = info->_ev_mgr;

  if(( conn == NULL ) || ( ev_mgr == NULL ))
  {
    LOG( DBG_ERR, stderr, "Triggered callback with invalid connection=%p or event mgr=%p data\n", conn, ev_mgr );
    return;
  }

  LOG( DBG_TRACE, stderr,
       "Triggered callback for connection=%p, socket=%d, index=%d, ev_type=%d\n",
       conn, conn->_socket, conn->_index, ev_type );
  if((( ev_type & EV_TIMEOUT ) != 0 ) && (( ev_type & EV_READ ) == 0 ))
  {
    LOG( DBG_INFO, stderr, "Connection timeout detected (idx=%d).\n", conn->_index );
  }
  else if( ( ev_type & EV_READ ) != 0 )
  {
    dbBE_Redis_connection_set_active( conn );
  }
  else
  {
    LOG( DBG_ERR, stderr, "event_mgr_callback: invalid event type triggered.\n" );
  }

  dbBE_Redis_connection_queue_push( dbBE_Redis_connection_mgr_getqueue( ev_mgr ),
                                    conn );
}


dbBE_Redis_connection_t* dbBE_Redis_event_mgr_next( dbBE_Redis_event_mgr_t *ev_mgr )
{
  if( ev_mgr == NULL )
  {
    LOG( DBG_ERR, stderr, "event_mgr_next: Invalid argument: ev_mgr=%p\n", ev_mgr );
    return NULL;
  }

  dbBE_Redis_connection_t *next = dbBE_Redis_connection_queue_pop( ev_mgr->_active_queue );
  LOG( DBG_VERBOSE, stderr, "event_mgr_next: active connection in queue: conn=%p\n", next );
  if( next != NULL )
    return next;

  LOG( DBG_TRACE, stderr, "event_mgr_next: starting loop\n" );
  event_base_loop( ev_mgr->_evbase, EVLOOP_ONCE | EVLOOP_NONBLOCK );
  next = dbBE_Redis_connection_queue_pop( ev_mgr->_active_queue );
  LOG( DBG_VERBOSE, stderr, "event_mgr_next: new active connection: conn=%p\n", next );

  return next;
}
