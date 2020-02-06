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

#include "logutil.h"
#include "common/utility.h"
#include "common/dbbe_api.h"
#include "common/sge.h"
#include "common/completion.h"
#include "network/definitions.h"
#include "network/connection.h"
#include "network/socket_io.h"
#include "fship.h"

const dbBE_api_t g_dbBE =
    { .initialize = FShip_initialize,
      .exit = FShip_exit,
      .post = FShip_post,
      .cancel = FShip_cancel,
      .test = FShip_test,
      .test_any = FShip_test_any
    };

int dbBE_FShip_connect_initial( dbBE_FShip_context_t *ctx );

dbBE_Completion_t* dbBE_FShip_complete_error( dbBE_Request_t *req,
                                              dbBE_Completion_t *cmpl,
                                              DBR_Errorcode_t err )
{
  cmpl->_status = err;
  return cmpl;
}

dbBE_Handle_t FShip_initialize( void )
{
  dbBE_FShip_context_t *be = (dbBE_FShip_context_t*)malloc( sizeof( dbBE_FShip_context_t ));
  if( be == NULL )
    return NULL;

  dbBE_Request_queue_t *work_q = dbBE_Request_queue_create( DBBE_FSHIP_WORK_QUEUE_DEPTH );
  if( work_q == NULL )
  {
    LOG( DBG_ERR, stderr, "dbBE_FShip_context_t::initialize: Failed to allocate work queue.\n" );
    FShip_exit( be );
    return NULL;
  }

  be->_work_q = work_q;

  dbBE_Completion_queue_t *compl_q = dbBE_Completion_queue_create( DBBE_FSHIP_WORK_QUEUE_DEPTH );
  if( compl_q == NULL )
  {
    LOG( DBG_ERR, stderr, "dbBE_FShip_context_t::initialize: Failed to allocate completion queue.\n" );
    FShip_exit( be );
    return NULL;
  }
  be->_compl_q = compl_q;

  dbBE_Redis_sr_buffer_t *sbuf = dbBE_Transport_sr_buffer_allocate( DBBE_FSHIP_BUFFER_SIZE );
  if( sbuf == NULL )
  {
    LOG( DBG_ERR, stderr, "dbBE_FShip_context_t::initialize: Failed to allocate send buffer.\n" );
    FShip_exit( be );
    return NULL;
  }
  be->_sbuf = sbuf;

  dbBE_Redis_sr_buffer_t *rbuf = dbBE_Transport_sr_buffer_allocate( DBBE_FSHIP_BUFFER_SIZE );
  if( rbuf == NULL )
  {
    LOG( DBG_ERR, stderr, "dbBE_FShip_context_t::initialize: Failed to allocate recv buffer.\n" );
    FShip_exit( be );
    return NULL;
  }
  be->_rbuf = rbuf;



  dbBE_Transport_sge_buffer_t *sge_buf = dbBE_Transport_sge_buffer_create();
  if( sge_buf == NULL )
  {
    LOG( DBG_ERR, stderr, "dbBE_FShip_context_t::initialize: Failed to allocate sge-buffer.\n" );
    FShip_exit( be );
    return NULL;
  }
  be->_sge_buf = sge_buf;

  int rc;
  if( ( rc = dbBE_FShip_connect_initial( be )) != 0 )
  {
    errno = -rc;
    LOG( DBG_ERR, stderr, "dbBE_FShip_context_t::initialize: Failed to connect. %s\n", strerror( -rc ) );
    FShip_exit( be );
    return NULL;
  }

  return be;
}


int FShip_exit( dbBE_Handle_t be )
{
  if( be == NULL )
    return -EINVAL;

  dbBE_FShip_context_t *ctx = (dbBE_FShip_context_t*)be;
  if( ctx != NULL )
  {
    if( ctx->_connection )
    {
      dbBE_Connection_unlink( ctx->_connection );
      dbBE_Connection_destroy( ctx->_connection );
    }

    if( ctx->_sge_buf )
      dbBE_Transport_sge_buffer_destroy( ctx->_sge_buf );

    if( ctx->_rbuf )
      dbBE_Transport_sr_buffer_free( ctx->_rbuf );

    if( ctx->_sbuf )
      dbBE_Transport_sr_buffer_free( ctx->_sbuf );

    if( ctx->_compl_q )
      dbBE_Completion_queue_destroy( ctx->_compl_q );

    if( ctx->_work_q )
      dbBE_Request_queue_destroy( ctx->_work_q );

    memset( ctx, 0, sizeof( dbBE_FShip_context_t ));
    free( ctx );
  }
  return 0;
}

int dbBE_FShip_sanity_check( dbBE_Request_t *req )
{
  if( req == NULL )
    return -EINVAL;

  switch( req->_opcode )
  {
    default:
      break;
  }
  return 0;
}

dbBE_Request_handle_t FShip_post( dbBE_Handle_t be,
                                  dbBE_Request_t *request,
                                  int trigger )
{
  if(( be == NULL ) || ( request == NULL ))
    return NULL;

  // sanity checking
  if( dbBE_FShip_sanity_check( request ) != 0 )
    return NULL;

  // connection check
  dbBE_FShip_context_t *fctx = (dbBE_FShip_context_t*)be;
  if(( fctx->_connection == NULL ) && ( dbBE_FShip_connect_initial( fctx ) != 0 ))
    return NULL;

  if(( ! dbBE_Connection_RTS( fctx->_connection ) ) &&
      (( dbBE_Connection_recoverable( fctx->_connection ) == DBBE_CONNECTION_RECOVERABLE ) &&
          ( dbBE_Connection_reconnect( fctx->_connection ) != 0 )) )
    return NULL;

  // create and store request context to find after completion
  dbBE_FShip_request_context_t *rctx = NULL;
  if( request->_opcode != DBBE_OPCODE_CANCEL )
  {
    rctx = (dbBE_FShip_request_context_t*)calloc( 1, sizeof( dbBE_FShip_request_context_t ) );
    if( rctx == NULL )
      return NULL;
    rctx->_request = request;
    rctx->_ulp_user = request->_user;
    request->_user = rctx;
  }
  else
  {
    // cancellations already have the rctx of the request included as _user
    rctx = request->_user;
  }

  // queue the request
  if( dbBE_Request_queue_push( fctx->_work_q, request ) != 0 )
    return NULL;

  // serialize
  dbBE_sge_t *sge = dbBE_Transport_sge_buffer_get_current( fctx->_sge_buf );
  sge->iov_base = dbBE_Transport_sr_buffer_get_available_position( fctx->_sbuf );

  ssize_t serlen = dbBE_Request_serialize( dbBE_Request_queue_pop( fctx->_work_q ),
                                           dbBE_Transport_sr_buffer_get_start( fctx->_sbuf ),
                                           dbBE_Transport_sr_buffer_get_size( fctx->_sbuf ));
  if( serlen < 0 )
    return NULL;

  sge->iov_len = serlen;
  dbBE_Transport_sge_buffer_add( fctx->_sge_buf, 1 );

  // make sure to trigger if a certain threshold of sbuf is full
  trigger = (( trigger != 0 ) || ( dbBE_Transport_sr_buffer_full( fctx->_sbuf, dbBE_Transport_sr_buffer_get_size( fctx->_sbuf ) << 3 ) ));

  if( trigger )
  {
    // fship
    ssize_t slen = dbBE_Socket_send( fctx->_connection->_socket, fctx->_sge_buf );
    if( slen < 0 )
      return NULL;
  }

  return (dbBE_Request_handle_t)request;
}


int FShip_cancel( dbBE_Handle_t be,
                  dbBE_Request_handle_t request )
{
  // create cancellation request
  dbBE_Request_t *c = dbBE_Request_allocate( 0 );
  if( c == NULL )
    return -ENOMEM;

  dbBE_Request_t *ber = (dbBE_Request_t*)request;

  c->_opcode = DBBE_OPCODE_CANCEL;
  c->_ns_hdl = ber->_ns_hdl;
  c->_key = "";
  c->_user = ber->_user;

  LOG( DBG_ALL, stderr, "Cancellation: %p with rctx %p\n", request, ber->_user )

  // post cancellation request
  dbBE_Request_handle_t cancel = FShip_post( be, c, 1 );
  if( cancel == c )
    free( c );
  else
    return DBR_ERR_BE_GENERAL;

  return 0;
}


dbBE_Completion_t* FShip_test( dbBE_Handle_t be,
                               dbBE_Request_handle_t request )
{
  return NULL;
}

dbBE_Completion_t* FShip_test_any( dbBE_Handle_t be )
{
  if( be == NULL )
    return NULL;

  dbBE_FShip_context_t *fctx = (dbBE_FShip_context_t*)be;
  if(( ! dbBE_Connection_RTS( fctx->_connection ) ) &&
      (( dbBE_Connection_recoverable( fctx->_connection ) == DBBE_CONNECTION_RECOVERABLE ) &&
          ( dbBE_Connection_reconnect( fctx->_connection ) != 0 )) )
    return NULL;

  dbBE_Completion_t *cmpl = NULL;
  ssize_t parsed = -EAGAIN;
  ssize_t total = 0;

  // receive any potential replies
  ssize_t rcvd = dbBE_Socket_recv( fctx->_connection->_socket, fctx->_rbuf );
  if(( rcvd == 0 ) && ( errno == EAGAIN ))
    return NULL;

  if( rcvd == 0 )
  {
    dbBE_Connection_unlink( fctx->_connection );
    return NULL;
  }


  if( rcvd < 0 )
    return NULL;

  dbBE_Transport_sr_buffer_add_data( fctx->_rbuf, rcvd, 0 );
  dbBE_Transport_sr_buffer_get_available_position( fctx->_rbuf )[0] = '\0'; // terminate just in case there's old stuff
  LOG( DBG_ALL, stderr, "received: %"PRId64": %s\n", rcvd, dbBE_Transport_sr_buffer_get_start( fctx->_rbuf ) );

  // deserialize
  dbBE_sge_t *sge = NULL;
  int sge_count = 0;
  parsed = dbBE_Completion_deserialize( dbBE_Transport_sr_buffer_get_processed_position( fctx->_rbuf ),
                                        dbBE_Transport_sr_buffer_available( fctx->_rbuf ),
                                        &cmpl, &sge, &sge_count );
  if( parsed > 0 )
  {
    total += parsed;
    dbBE_Transport_sr_buffer_advance( fctx->_rbuf, parsed );
    if( dbBE_Transport_sr_buffer_unprocessed( fctx->_rbuf ) == 0 )
      dbBE_Transport_sr_buffer_reset( fctx->_rbuf );
  }
  else
    return NULL;

  // restore request reference and upper layer user ptr
  dbBE_FShip_request_context_t *rctx = (dbBE_FShip_request_context_t*)cmpl->_user;
  if( rctx == NULL )
  {
    LOG( DBG_ERR, stderr, "Found completion with NULL-ptr request context\n" );
    return NULL;
  }

  dbBE_Request_t *req = (dbBE_Request_t*)rctx->_request;
  if( req == NULL )
  {
    LOG( DBG_ERR, stderr, "Found completion with NULL-ptr request\n" );
    return NULL;
  }
  req->_user = rctx->_ulp_user;
  cmpl->_user = rctx->_ulp_user;

  // process (SGE placements)
  if( sge_count < 0 )
    dbBE_FShip_complete_error( req, cmpl, DBR_ERR_BE_GENERAL );

  if( sge_count > req->_sge_count )
    dbBE_FShip_complete_error( req, cmpl, DBR_ERR_UBUFFER );

  int i;
  for( i=0; i<sge_count; ++i)
  {
    if( sge[i].iov_len <= req->_sge[i].iov_len )
      memcpy( req->_sge[i].iov_base, sge[i].iov_base, sge[i].iov_len );
    else
      dbBE_FShip_complete_error( req, cmpl, DBR_ERR_UBUFFER );
    if( sge[i].iov_len < req->_sge[i].iov_len )
      ((char*)req->_sge[i].iov_base)[ sge[i].iov_len ] = '\0'; // do some kind of termination because there are some APIs that expect strings in this place
  }

  return cmpl;
}


/*
 * create the initial connection to Redis with srbuffers by extracting the url from the ENV variable
 */
int dbBE_FShip_connect_initial( dbBE_FShip_context_t *ctx )
{
  int rc = 0;
  if( ctx == NULL )
    return -EINVAL;

  char *env_url = dbBE_Extract_env( DBR_SERVER_HOST_ENV, DBR_SERVER_DEFAULT_HOST );
  if( env_url == NULL )
    return -ENODEV;

  LOG(DBG_VERBOSE, stderr, "url=%s\n", env_url );

  char *authfile = dbBE_Extract_env( DBR_SERVER_AUTHFILE_ENV, DBR_SERVER_DEFAULT_AUTHFILE );
  if( authfile == NULL )
  {
    rc = -ENOENT;
    goto exit_connect;
  }
  LOG( DBG_VERBOSE, stderr, "authfile=%s\n", authfile );


  dbBE_Connection_t *new_conn = dbBE_Connection_create();
  if( new_conn == NULL )
  {
    rc = -ENOMEM;
    goto exit_connect;
  }

  dbBE_Network_address_t *srv_addr = dbBE_Connection_link( new_conn, env_url, authfile );
  if( srv_addr == NULL )
  {
    rc = -ENOTCONN;
    dbBE_Connection_destroy( new_conn );
    goto exit_connect;
  }

  dbBE_Connection_noblock( new_conn );

  ctx->_connection = new_conn;

exit_connect:
  if( authfile != NULL ) free( authfile );
  if( env_url != NULL ) free( env_url );
  return rc;
}
