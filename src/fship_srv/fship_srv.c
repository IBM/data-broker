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

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL DBG_INFO
#endif

#include "logutil.h"
#include "common/utility.h"
#include <libdatabroker.h>
#include "libdatabroker_int.h"
#include "common/resolve_addr.h"
#include "fship_srv.h"
#include "lib/backend.h"

#include "common/request_queue.h"
#include "common/completion.h"
#include "network/socket_io.h"

#include <errno.h> // errno
#include <unistd.h> // fork
#include <stdlib.h> // exit
#include <sys/socket.h> // socket
#include <pthread.h> // pthread_create/join/...
#include <event2/event.h> // libevent
#include <event2/thread.h> // evthread_use_pthreads
#include <string.h> // strncmp

#define DBR_MCTX_RC( a, rc ) ( (rc) == 0 ? (a) : (rc) )

int dbrFShip_main_context_destroy( dbrFShip_main_context_t *ctx, const int rc )
{
  if( ctx == NULL )
    return DBR_MCTX_RC( -EINVAL, rc );

  if( ctx->_mctx )
    dbrMain_exit();

  if( ctx->_cctx )
    free( ctx->_cctx );

  if( ctx->_r_buf )
    dbBE_Transport_sr_buffer_free( ctx->_r_buf );

  if( ctx->_s_buf )
    dbBE_Transport_sr_buffer_free( ctx->_s_buf );

  if( ctx->_conn_queue )
    dbBE_Connection_queue_destroy( ctx->_conn_queue );

  free( ctx );
  dbrMain_exit();

  return DBR_MCTX_RC( 0, rc );
}

dbrFShip_main_context_t* dbrFShip_main_context_create( dbrFShip_config_t *cfg )
{
  if( cfg == NULL )
    return NULL;

  dbrFShip_main_context_t *ctx = ( dbrFShip_main_context_t* )calloc( 1, sizeof( dbrFShip_main_context_t ));
  if( ctx == NULL )
    return NULL;

  memcpy( &ctx->_cfg, cfg, sizeof( dbrFShip_config_t ) );

  ctx->_mctx = dbrCheckCreateMainCTX();
  if( ctx->_mctx == NULL )
  {
    free( ctx );
    return NULL;
  }

  ctx->_conn_queue = dbBE_Connection_queue_create( DBR_FSHIP_CONNECTIONS_LIMIT );
  ctx->_r_buf = dbBE_Transport_sr_buffer_allocate( cfg->_max_mem >> 1 );
  if( ctx->_r_buf == NULL )
  {
    dbrFShip_main_context_destroy( ctx, -ENOMEM );
    return NULL;
  }

  ctx->_s_buf = dbBE_Transport_sr_buffer_allocate( cfg->_max_mem >> 1 );
  if( ctx->_s_buf == NULL )
  {
    dbrFShip_main_context_destroy( ctx, -ENOMEM );
    return NULL;
  }
  return ctx;
}

int main( int argc, char **argv )
{
  dbrFShip_config_t cfg;
  if( dbrFShip_parse_cmdline( argc, argv, &cfg ) != 0 )
    return 1;

  // daemonize if (-d)
  if( cfg._daemon != 0 )
  {
    pid_t pid = fork();
    if( pid < 0 )
    {
      LOG( DBG_ERR, stderr, "failed to fork fship_srv daemon.\n" );
      exit( 1 );
    }
    if( pid != 0 )
      exit( 0 );
  }

  dbrFShip_main_context_t *context = dbrFShip_main_context_create( &cfg );
  if( context == NULL )
    return ENOMEM;


  evthread_use_pthreads();
  struct event_base *evbase = event_base_new();

  // create/listen on passive socket
  //  socket/bind/listen/accept
  pthread_t listener;

  dbrFShip_threadio_t tio;
  memset( &tio, 0, sizeof( tio ));
  tio._evbase = evbase;
  tio._threadrc = 0;
  tio._cfg = &context->_cfg;
  tio._conn_queue = context->_conn_queue;
  tio._keep_running = 1;

  if( pthread_create( &listener, NULL, dbrFShip_listen_start, &tio ) != 0 )
  {
    return dbrFShip_main_context_destroy( context, -ECHILD );
  }


  // loop
  int rc = 0;
  while( 1 )
  {
    rc = dbrFShip_inbound( &tio, context );
    if( rc < 0 )
      break;

    rc = dbrFShip_outbound( &tio, context );
    if( rc < 0 )
      break;
  }

  tio._keep_running = 0;
  pthread_cancel( listener );
  pthread_join( listener, NULL );

  if( tio._threadrc != 0 )
    LOG( DBG_ERR, stderr, "Listener thread exited with rc=%d\n", tio._threadrc );

  return dbrFShip_main_context_destroy( context, rc );
}

int dbrFShip_inbound( dbrFShip_threadio_t *tio, dbrFShip_main_context_t *context )
{
  // check for new inbound requests
  //
  if( context->_total_pending > 0 )
    event_base_loop( tio->_evbase, EVLOOP_ONCE | EVLOOP_NONBLOCK );
  else
    event_base_loop( tio->_evbase, EVLOOP_ONCE );

  dbBE_Connection_t *active = dbBE_Connection_queue_pop( tio->_conn_queue );
  if( active == NULL )
    return 0;

  // find the corresponding client context
  dbrFShip_client_context_t *cctx = (dbrFShip_client_context_t*)active->_context;
  if( cctx == NULL ) // unknown connection context
  {
    LOG( DBG_ERR, stderr, "FATAL: Found active connection without valid connection context.\n" )
    return -1;
  }


  // recv() +  deserialize()
  dbBE_Request_t *req = NULL;
  ssize_t parsed = -EAGAIN;
  ssize_t total = 0;
  while( parsed == -EAGAIN )
  {
    LOG( DBG_TRACE, stderr, "BUF: avail=%"PRId64"; rem=%"PRId64"; size=%"PRId64"\n", dbBE_Transport_sr_buffer_available( context->_r_buf ),
         dbBE_Transport_sr_buffer_remaining( context->_r_buf ),
         dbBE_Transport_sr_buffer_get_size( context->_r_buf ) );
    ssize_t rcvd = dbBE_Socket_recv( cctx->_conn->_socket, context->_r_buf );
    if( rcvd <= 0 )
    {
      LOG( DBG_INFO, stderr, "Connection shutdown detected for socket %d; rc=%"PRId64"\n", cctx->_conn->_socket, rcvd );
      // make sure the connection is no longer part of the connection queue
      dbrFShip_client_remove( context->_conn_queue, &cctx );
      return 0;
    }

    if( rcvd > 0 )
    {
      dbBE_Transport_sr_buffer_add_data( context->_r_buf, rcvd, 0 );
      dbBE_Transport_sr_buffer_get_available_position( context->_r_buf )[0] = '\0'; // terminate to avoid contamination from previous serializations
      parsed = dbBE_Request_deserialize( dbBE_Transport_sr_buffer_get_processed_position( context->_r_buf ),
                                         dbBE_Transport_sr_buffer_unprocessed( context->_r_buf ),
                                         &req );
      LOG( DBG_TRACE, stderr, "Received %ld %s _ deserialzed=%ld\n", dbBE_Transport_sr_buffer_unprocessed( context->_r_buf ),
           (dbBE_Transport_sr_buffer_unprocessed( context->_r_buf ) < 100 ? dbBE_Transport_sr_buffer_get_processed_position( context->_r_buf ) : "long" ),
           parsed );
      ++context->_total_pending; // as soon as there's anything received, assume a pending request
      if( parsed > 0 )
      {
        total += parsed;
        dbBE_Transport_sr_buffer_advance( context->_r_buf, parsed );
        if( dbBE_Transport_sr_buffer_unprocessed( context->_r_buf ) == 0 )
          dbBE_Transport_sr_buffer_reset( context->_r_buf );
      }
    }
    if(( parsed < 0 ) && ( parsed != -EAGAIN ))
      return -1;
  }

  //   create request()
//    int64_t rc;
  dbrFShip_request_ctx_t *rctx = NULL;
  if( req->_opcode == DBBE_OPCODE_CANCEL )
  {
    rctx = dbrFShip_find_request( cctx, req );
    if( rctx != NULL )
    {
      context->_mctx->_be_ctx->_api->cancel( context->_mctx->_be_ctx->_context, rctx->_req );
      LOG( DBG_TRACE, stderr, "canceling %d\n", rctx->_req->_opcode );
    }
    return 0;
  }
  else
  {
    rctx = dbrFShip_create_request( req, cctx );
    dbBE_Request_handle_t be_req = context->_mctx->_be_ctx->_api->post( context->_mctx->_be_ctx->_context, req, 0 );
    LOG( DBG_TRACE, stderr, "posted %d\n", req->_opcode );
    if( be_req == NULL )
    {
      // todo: create error response with DBR_ERR_BE_POST
      return -1;
    }
  }
  return dbrFShip_request_ctx_queue_push( cctx->_pending, rctx );
}

int dbrFShip_outbound( dbrFShip_threadio_t *tio, dbrFShip_main_context_t *context )
{
  // check for completions

  dbBE_Completion_t *comp = NULL;
  if( (comp = context->_mctx->_be_ctx->_api->test_any( context->_mctx->_be_ctx->_context )) == NULL )
    return 0;

  dbrFShip_request_ctx_t *rctx = (dbrFShip_request_ctx_t*)comp->_user;
  if( rctx == NULL )
    return -EPROTO;

  // restore user ptr
  comp->_user = rctx->_user_in;

  // adjust the response SGE to prevent returning more data than available
  if( comp->_rc >= 0 )
    switch( rctx->_req->_opcode )
    {
      case DBBE_OPCODE_GET:
      case DBBE_OPCODE_READ:
      case DBBE_OPCODE_NSQUERY:
      case DBBE_OPCODE_DIRECTORY:
      {
        size_t rtotal = (size_t)comp->_rc;
        int i = 0;
        do
        {
          if( rtotal <= rctx->_req->_sge[i].iov_len )
            rctx->_req->_sge[i].iov_len = rtotal;
          rtotal -= rctx->_req->_sge[i].iov_len;
          ++i;
        } while(( i < rctx->_req->_sge_count ) && ( rtotal > 0));
        rctx->_req->_sge_count = i; // the amount of SGE needs to be adjusted
        break;
      }
      case DBBE_OPCODE_ITERATOR:
        // returns new iterator in comp->_rc and the sge[] is a key+len
        rctx->_req->_sge[0].iov_len = strnlen( (char*)rctx->_req->_sge[0].iov_base, rctx->_req->_sge[0].iov_len );
        break;
      default:
        break;
    }

  ssize_t serlen = dbBE_Completion_serialize( rctx->_req->_opcode, comp, rctx->_req->_sge, rctx->_req->_sge_count,
                                              dbBE_Transport_sr_buffer_get_available_position( context->_s_buf ),
                                              dbBE_Transport_sr_buffer_remaining( context->_s_buf ) );
  LOG( DBG_TRACE, stderr, "Completion serialize: op=%d; len=%"PRId64"\n", rctx->_req->_opcode, serlen );
  if( serlen < 0 )
    return (int)serlen;

  dbBE_Transport_sr_buffer_add_data( context->_s_buf, serlen, 0 );

  LOG( DBG_TRACE, stderr, "Sending %ld %s\n", dbBE_Transport_sr_buffer_unprocessed( context->_s_buf ),
       (dbBE_Transport_sr_buffer_unprocessed( context->_s_buf ) < 100 ? dbBE_Transport_sr_buffer_get_processed_position( context->_s_buf ) : "long" ) );

  ssize_t sent = 0;
  while(( sent >= 0 ) && ( (ssize_t)dbBE_Transport_sr_buffer_unprocessed( context->_s_buf ) > 0) )
  {
    sent = send( rctx->_cctx->_conn->_socket,
                 dbBE_Transport_sr_buffer_get_processed_position( context->_s_buf ),
                 dbBE_Transport_sr_buffer_unprocessed( context->_s_buf ),
                 0 );
    if( sent < 0 )
    {
      LOG( DBG_ERR, stderr, "Send error. Incomplete response rc=%"PRId64": %s\n", sent, strerror( errno ) );
      // don't break or exit here; buffer cleanup suggested
    }
    dbBE_Transport_sr_buffer_advance( context->_s_buf, sent );
    LOG( DBG_TRACE, stderr, "sent %"PRId64"/%"PRId64"/%"PRId64"\n",
         sent, dbBE_Transport_sr_buffer_unprocessed( context->_s_buf ),
         dbBE_Transport_sr_buffer_available( context->_s_buf ));
  }

  dbBE_Transport_sr_buffer_reset( context->_s_buf );

  // clean Request from rctx-queue (note: out-of-order completion possible, therefore assume we can't just q_pop())
  dbrFShip_completion_cleanup( rctx );
  --context->_total_pending;

  return 0;
}

void usage()
{
  fprintf( stderr, " fship_srv [options]\n\n"\
                   "   -h        display help\n"\
                   "   -d        run as daemon\n"\
                   "   -l <url>  listen at provided URL\n"\
                   "   -M <MB>   max buffering memory size in MB\n\n");
}

int dbrFShip_parse_cmdline( int argc, char **argv, dbrFShip_config_t *cfg )
{
  if( cfg == NULL )
    return -EINVAL;

  int option;
  cfg->_daemon = 0;
  cfg->_listenaddr = "localhost";
  cfg->_max_mem = 512 * 1024 * 1024; // reserve 512M by default
  while(( option = getopt(argc, argv, "dhl:M:")) != -1 )
  {
    // locally check common options; callback for extra options
    switch( option )
    {
      case 'h':
        usage();
        exit(0);
      case 'd': // daemonize
        cfg->_daemon = 1;
        break;
      case 'l': // listener
        cfg->_listenaddr = optarg;
        break;
      case 'M': // max memory for data buffering
        cfg->_max_mem = strtol( optarg, NULL, 10 ) * 1024 * 1024;
        break;
      default:
        usage();
        return -EINVAL;
    }
  }
  return 0;
}

void dbrFShip_connection_wakeup( evutil_socket_t socket, short ev_type, void *arg )
{
  LOG( DBG_TRACE, stderr, "FShip: Triggered callback for socket=%d, ev_type=%d, arg=%p\n", socket, ev_type, arg );

  dbrFShip_event_info_t *info = (dbrFShip_event_info_t*)arg;
  if( info == NULL )
  {
    LOG( DBG_ERR, stderr, "Triggered callback with NULL ptr argument.\n" );
    return;
  }

  dbBE_Connection_t *conn = info->_cctx->_conn;
  dbBE_Connection_queue_t *queue = info->_queue;

  if(( conn == NULL ) || ( queue == NULL ))
  {
    LOG( DBG_ERR, stderr, "Triggered callback with invalid connection=%p or queue=%p data\n", conn, queue );
    return;
  }

  if( socket != conn->_socket )
    LOG( DBG_ERR, stderr, "Event socket doesn't match event info %d != %d\n", socket, conn->_socket );

  LOG( DBG_TRACE, stderr,
       "Triggered callback for connection=%p, socket=%d, ev_type=%d\n",
       conn, conn->_socket, ev_type );
  if( ( ev_type & EV_READ ) != 0 )
  {
    LOG( DBG_TRACE, stderr, "Connection activated (sock=%d)\n", conn->_socket );
    dbBE_Connection_queue_push( queue, conn );
  }
  else if(( ev_type & EV_TIMEOUT ) != 0 )
  {
    char buf[2];
    LOG( DBG_VERBOSE, stderr, "FShip_event: Event timeout detected (sock=%d).\n", conn->_socket );
    ssize_t rcvd = recv( conn->_socket, buf, 1, MSG_PEEK | MSG_DONTWAIT );
    switch( rcvd )
    {
      case 0:
        dbrFShip_client_remove( info->_queue, &info->_cctx );
        break;
      case 1:
        dbBE_Connection_queue_push( queue, conn );
        break;
      default:
        break;
    }
  }
  else
  {
    LOG( DBG_ERR, stderr, "FShip_event: invalid event type triggered.\n" );
  }
}

void* dbrFShip_listen_start( void *arg )
{
  if( arg == NULL )
    return NULL;

  struct dbrFShip_threadio *tio = (struct dbrFShip_threadio*)arg;

  struct event_base *evbase = tio->_evbase;
  if( evbase == NULL )
    return NULL;

  int s;

  char *url = tio->_cfg->_listenaddr;
  struct addrinfo *addrs = dbBE_Common_resolve_address( url, 0 );
  struct addrinfo *cur = addrs;
  while( cur != NULL )
  {
    s = socket( AF_INET, SOCK_STREAM, 0 );
    if( s < 0 )
    {
      tio->_threadrc = -errno;
      break;
    }

    if( bind( s, cur->ai_addr, cur->ai_addrlen ) == 0 )
    {
      break;
    }

    tio->_threadrc = -errno;
    close( s );
    cur = cur->ai_next;
  }

  dbBE_Common_release_addrinfo( &addrs );

  if( s < 0 )
    return tio;

  int backlog = 128;
  if( listen( s, backlog ) != 0 )
  {
    tio->_threadrc = -errno;
    close( s );
    return tio;
  }

  while( tio->_keep_running )
  {
    struct sockaddr naddr;
    socklen_t naddrlen = sizeof( naddr );

    int nes = accept( s, &naddr, &naddrlen );
    if( nes > 0 )
    {
      dbBE_Connection_t *connection = dbBE_Connection_create();
      connection->_socket = nes;
      connection->_status = DBBE_CONNECTION_STATUS_AUTHORIZED;
      connection->_address = dbBE_Network_address_copy( &naddr, naddrlen );
      if( dbBE_Network_address_to_string( connection->_address, connection->_url, DBBE_URL_MAX_LENGTH ) == NULL )
      {
        LOG( DBG_ERR, stderr, "Network address translation to URL failed.\n" );
        break;
      }

      dbrFShip_client_context_t *cctx = (dbrFShip_client_context_t*)calloc( 1, sizeof( dbrFShip_client_context_t ));
      if( cctx == NULL )
        break;

      // bidirectional linking between connection and its context
      cctx->_conn = connection;
      cctx->_pending = dbrFShip_request_ctx_queue_create();
      connection->_context = (void*)cctx;
      gettimeofday( &connection->_last_alive, NULL );

      dbrFShip_event_info_t *evinfo = (dbrFShip_event_info_t*)malloc( sizeof( dbrFShip_event_info_t ));
      if( evinfo == NULL )
        break;
      evinfo->_cctx = cctx;
      evinfo->_queue = tio->_conn_queue;
      cctx->_event = evinfo;

      // add to libevent socket polling
      struct event* ev = event_new( evbase, nes, EV_READ | EV_PERSIST | EV_ET, dbrFShip_connection_wakeup, evinfo );
      if( ev == NULL )
        break;

      evinfo->_event = ev;

      struct timeval timeout;
      timeout.tv_sec = 1;
      timeout.tv_usec = 0;
      event_add( ev, &timeout );

      LOG( DBG_INFO, stderr, "New client connection on socket=%d\n", connection->_socket );
    }
  }

  LOG( DBG_INFO, stderr, "Closing listener\n" );
  close( s );
  return tio;
}

dbrFShip_request_ctx_t* dbrFShip_create_request( dbBE_Request_t *req, dbrFShip_client_context_t *cctx )
{
  dbrFShip_request_ctx_t* rctx = (dbrFShip_request_ctx_t*)calloc( 1, sizeof( dbrFShip_request_ctx_t ));
  if( rctx == NULL )
    return NULL;
  rctx->_user_in = req->_user;
  rctx->_cctx = cctx;
  rctx->_req = req;
  req->_user = rctx;

  switch( req->_opcode )
  {
    case DBBE_OPCODE_GET:
    case DBBE_OPCODE_READ:
    case DBBE_OPCODE_NSQUERY:
    case DBBE_OPCODE_ITERATOR:
      // since these requests come with an SGE header, we need to create space to store the data
      if( req->_sge_count > 0 )
      {
        char *req_buf = (char*)malloc( dbBE_SGE_get_len( req->_sge, req->_sge_count ) );
        if( req_buf == NULL )
          goto error;
        int i;
        size_t offset = 0;
        for( i = 0; i < req->_sge_count; ++i )
        {
          req->_sge[i].iov_base = &req_buf[ offset ];
          offset += req->_sge[i].iov_len;
        }
      }
      break;
    case DBBE_OPCODE_DIRECTORY:
      {
        char *req_buf = (char*)malloc( req->_sge[0].iov_len );
        if( req_buf == NULL )
          goto error;
        req->_sge[0].iov_base = req_buf;
        req->_sge[1].iov_base = NULL;
      }
      break;
    default:
      break;
  }

  return rctx;

error:
  free( rctx );
  return NULL;
}

int dbrFShip_completion_cleanup( dbrFShip_request_ctx_t *rctx )
{
  if(( rctx == NULL ) || ( rctx->_req == NULL ) || ( rctx->_cctx == NULL ))
    return -EINVAL;

  dbBE_Request_t *req = rctx->_req;
  dbrFShip_client_context_t *cctx = rctx->_cctx;

  switch( req->_opcode )
  {
    case DBBE_OPCODE_GET:
    case DBBE_OPCODE_READ:
    case DBBE_OPCODE_NSQUERY:
    case DBBE_OPCODE_ITERATOR:
      // an SGE buffer was created, so we have to free it here
      if( req->_sge_count > 0 )
        free( req->_sge[0].iov_base );
      break;
    case DBBE_OPCODE_DIRECTORY:
      if( req->_sge[0].iov_base != NULL )
        free( req->_sge[0].iov_base );
      // intentionally no break;
    default:
      break;
  }

  dbBE_Request_free( req );
  rctx->_req = NULL;
  rctx->_cctx = NULL;
  rctx->_user_in = NULL;
  if( rctx == cctx->_pending->_head )
  {
    rctx = dbrFShip_request_ctx_queue_pop( cctx->_pending );
    if( rctx != NULL )
      free( rctx );
  }
  return 0;
}

// clean up and free client context
int dbrFShip_client_remove( dbBE_Connection_queue_t *queue,
                            dbrFShip_client_context_t **in_out_cctx )
{
  if(( queue == NULL ) || ( in_out_cctx == NULL ) || ( *in_out_cctx == NULL ))
    return -EINVAL;

  dbrFShip_client_context_t *cctx = *in_out_cctx;

  if( cctx->_event != NULL )
  {
    event_del( cctx->_event->_event );
    event_free( cctx->_event->_event );
    free( cctx->_event );
  }
  dbBE_Connection_queue_remove_connection( queue, cctx->_conn );
  dbBE_Connection_destroy( cctx->_conn );
  free( cctx );
  *in_out_cctx = NULL;
  return 0;
}


dbrFShip_request_ctx_t* dbrFShip_find_request( dbrFShip_client_context_t *cctx,
                                               dbBE_Request_t *req )
{
  // input req is the cancellation request from remote that contains the orig-user handle to cancel
  return dbrFShip_request_ctx_find( cctx->_pending, req->_user );
}
