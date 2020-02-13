/*
 * Copyright © 2020 IBM Corporation
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


#ifndef SRC_FSHIP_SRV_CLIENT_CONTEXT_H_
#define SRC_FSHIP_SRV_CLIENT_CONTEXT_H_

#include "network/connection.h"
#include "network/connection_queue.h"

#include <event2/event.h>
#include <pthread.h>

struct dbrFShip_event_info;
struct dbrFShip_request_ctx_queue_t;
typedef struct dbrFShip_client_context
{
  dbBE_Connection_t *_conn;
  struct dbrFShip_request_ctx_queue *_pending;
  int _pending_requests;
  int _pending_responses;
  struct dbrFShip_event_info *_event;
  pthread_mutex_t _lock;
} dbrFShip_client_context_t;

typedef struct dbrFShip_event_info
{
  dbrFShip_client_context_t *_cctx;
  dbBE_Connection_queue_t *_queue;
  struct event *_event;
} dbrFShip_event_info_t;


static inline
dbrFShip_client_context_t* dbrFShip_client_ctx_create( struct dbrFShip_request_ctx_queue *rqueue,
                                                       dbBE_Connection_t *connection,
                                                       dbBE_Connection_queue_t *cqueue )
{
  if(( rqueue == NULL ) || ( connection == NULL ) || ( cqueue == NULL ))
    return NULL;

  dbrFShip_client_context_t *cctx = (dbrFShip_client_context_t*)calloc( 1, sizeof( dbrFShip_client_context_t ));
  if( cctx == NULL )
    return NULL;

  pthread_mutex_init( &cctx->_lock, NULL );
  cctx->_pending = rqueue;

  // bidirectional linking between connection and its context
  cctx->_conn = connection;
  connection->_context = (void*)cctx;

  dbrFShip_event_info_t *evinfo = (dbrFShip_event_info_t*)malloc( sizeof( dbrFShip_event_info_t ));
  if( evinfo == NULL )
  {
    free( cctx );
    return NULL;
  }
  evinfo->_cctx = cctx;
  evinfo->_queue = cqueue;
  cctx->_event = evinfo;

  gettimeofday( &connection->_last_alive, NULL );
  return cctx;
}

static inline
int dbrFShip_client_ctx_add_response( volatile dbrFShip_client_context_t *cctx )
{
  if( cctx == NULL )
    return -1;
  ++cctx->_pending_responses;
  --cctx->_pending_requests;
  return 0;
}

static inline
int dbrFShip_client_ctx_add_request( dbrFShip_client_context_t *cctx )
{
  return ( ++cctx->_pending_requests );
}

static inline
int dbrFShip_client_ctx_flush_responses( dbrFShip_client_context_t *cctx )
{
  cctx->_pending_responses = 0;
  return 0;
}


static inline
int dbrFShip_client_ctx_delete( dbrFShip_client_context_t *ctx )
{
  LOG( DBG_TRACE, stderr, "CCTX(%d) Del: %d:%d\n", ctx->_conn->_socket, ctx->_pending_requests, ctx->_pending_responses );
  if( ctx->_pending_requests + ctx->_pending_responses != 0 )
    return 1;

  dbBE_Connection_destroy( ctx->_conn );
  ctx->_pending_requests = 0;
  ctx->_pending_responses = 0;
  pthread_mutex_destroy( &ctx->_lock );
  free( ctx );
  return 0;
}


static inline
uint64_t dbrFShip_client_ctx_detach( dbrFShip_client_context_t *ctx )
{
  if( ctx->_pending_requests + ctx->_pending_responses == 0 )
    if( ctx->_event == NULL )
      return dbrFShip_client_ctx_delete( ctx );
  return 1;
}

// detach client ctx from libevents and connection queue
static inline
int dbrFShip_client_ctx_remove( dbBE_Connection_queue_t *queue,
                                dbrFShip_client_context_t **in_out_cctx )
{
  if(( queue == NULL ) || ( in_out_cctx == NULL ) || ( *in_out_cctx == NULL ))
    return -EINVAL;

  dbrFShip_client_context_t *cctx = *in_out_cctx;
  *in_out_cctx = NULL;
  pthread_mutex_lock( &cctx->_lock );

  if( cctx->_event != NULL )
  {
    if( cctx->_event->_event != NULL )
    {
      event_del( cctx->_event->_event );
      event_free( cctx->_event->_event );
    }
    cctx->_event->_event = NULL;
    free( cctx->_event );
    cctx->_event = NULL;
  }
  dbBE_Connection_queue_remove_connection( queue, cctx->_conn );

  pthread_mutex_unlock( &cctx->_lock );
  return dbrFShip_client_ctx_detach( cctx );
}



#endif /* SRC_FSHIP_SRV_CLIENT_CONTEXT_H_ */
