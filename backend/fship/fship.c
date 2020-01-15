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
#include "network/definitions.h"
#include "network/connection.h"
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
  // sanity checking
  if( dbBE_FShip_sanity_check( request ) != 0 )
    return NULL;

  // connection check

  // serialize + (wait?)

  // fship

  return NULL;
}


int FShip_cancel( dbBE_Handle_t be,
                  dbBE_Request_handle_t request )
{
  // create cancellation request
  dbBE_Request_t *c = dbBE_Request_allocate( 0 );
  if( c == NULL )
    return -ENOMEM;

  char kbuffer[16];
  c->_key = kbuffer;
  if( snprintf( c->_key, 16, "%p", (void*)request ) < 0 ) // serialize request hdl
  {
    free( c );
    return -EBADMSG;
  }

  // post cancellation request
  dbBE_Request_handle_t cancel = FShip_post( be, c, 1 );

  // wait for cancel to complete
  dbBE_Completion_t *compl = NULL;
  while( ( compl = FShip_test( be, cancel )) == NULL );

  return 0;
}


dbBE_Completion_t* FShip_test( dbBE_Handle_t be,
                               dbBE_Request_handle_t request )
{
  return NULL;
}

dbBE_Completion_t* FShip_test_any( dbBE_Handle_t be )
{
  // receive any potential replies
  // deserialize
  // process (SGE placements)
  // queue to completion queue

  return NULL;
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

  ctx->_connection = new_conn;

exit_connect:
  if( authfile != NULL ) free( authfile );
  if( env_url != NULL ) free( env_url );
  return rc;
}
