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

#include <stddef.h>
#include <errno.h>
#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>  // malloc
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "logutil.h"
#include "../common/data_transport.h"

#include "parse.h"


#include "definitions.h"
#include <common/utility.h>
#include "redis.h"
#include "result.h"
#include "cluster_info.h"

const dbBE_api_t g_dbBE =
    { .initialize = Redis_initialize,
      .exit = Redis_exit,
      .post = Redis_post,
      .cancel = Redis_cancel,
      .test = Redis_test,
      .test_any = Redis_test_any
    };

/*
 * initialize the system library contexs
 */
dbBE_Handle_t Redis_initialize(void)
{
  dbBE_Redis_context_t *context = (dbBE_Redis_context_t*)malloc( sizeof( dbBE_Redis_context_t ) );
  if( context == NULL )
  {
    LOG( DBG_ERR, stderr, "dbBE_Redis_context_t::initialize: Failed to allocate backend context.\n" );
    return NULL;
  }

  memset( context, 0, sizeof( dbBE_Redis_context_t ) );

  // protocol spec allocation
  dbBE_Redis_command_stage_spec_t *spec = dbBE_Redis_command_stages_spec_init();
  if( spec == NULL )
  {
    LOG( DBG_ERR, stderr, "dbBE_Redis_context_t::initialize: Failed to allocate protocol spec.\n" );
    Redis_exit( context );
    return NULL;
  }

  context->_spec = spec;

  // create locator
  dbBE_Redis_locator_t *locator = dbBE_Redis_locator_create();
  if( locator == NULL )
  {
    LOG( DBG_ERR, stderr, "dbBE_Redis_context_t::initialize: Failed to allocate locator.\n" );
    Redis_exit( context );
    return NULL;
  }

  context->_locator = locator;

  dbBE_Request_queue_t *work_q = dbBE_Request_queue_create( DBBE_REDIS_WORK_QUEUE_DEPTH );
  if( work_q == NULL )
  {
    LOG( DBG_ERR, stderr, "dbBE_Redis_context_t::initialize: Failed to allocate work queue.\n" );
    Redis_exit( context );
    return NULL;
  }

  context->_work_q = work_q;

  dbBE_Completion_queue_t *compl_q = dbBE_Completion_queue_create( DBBE_REDIS_WORK_QUEUE_DEPTH );
  if( compl_q == NULL )
  {
    LOG( DBG_ERR, stderr, "dbBE_Redis_context_t::initialize: Failed to allocate completion queue.\n" );
    Redis_exit( context );
    return NULL;
  }

  context->_compl_q = compl_q;

  // create send-to-receive queue
  dbBE_Redis_s2r_queue_t *retry_q = dbBE_Redis_s2r_queue_create( 1 );
  if( retry_q == NULL )
  {
    LOG( DBG_ERR, stderr, "dbBE_Redis_context_t::initialize: Failed to allocate retry-queue.\n" );
    Redis_exit( context );
    return NULL;
  }

  context->_retry_q = retry_q;

  dbBE_Request_set_t *cancel = dbBE_Request_set_create( DBBE_REDIS_WORK_QUEUE_DEPTH );
  if( cancel == NULL )
  {
    LOG( DBG_ERR, stderr, "dbBE_Redis_context_t::initialize: Failed to allocate cancellation set.\n" );
    Redis_exit( context );
    return NULL;
  }

  context->_cancellations = cancel;

  dbBE_Redis_sr_buffer_t *sbuf = dbBE_Transport_sr_buffer_allocate( DBBE_REDIS_SR_BUFFER_LEN );
  if( sbuf == NULL )
  {
    LOG( DBG_ERR, stderr, "dbBE_Redis_context_t::initialize: Failed to allocate sender buffer.\n" );
    Redis_exit( context );
    return NULL;
  }

  context->_sender_buffer = sbuf;

  int *sender_conns = (int*)calloc( DBBE_REDIS_COALESCED_MAX * DBBE_REDIS_MAX_CONNECTIONS + 1, sizeof( int ));
  if( sender_conns == NULL )
  {
    LOG( DBG_ERR, stderr, "dbBE_Redis_context_t::initialize: Failed to allocate sender connection array.\n" );
    Redis_exit( context );
    return NULL;
  }

  context->_sender_connections = sender_conns;

  dbBE_Data_transport_t *transport = &dbBE_Smallcopy_transport;
  context->_transport = transport;

  // create config
  dbBE_Redis_conn_mgr_config_t config;
  config._rbuf_len = transport->_recv_buffer_len;
  config._sbuf_len = transport->_send_buffer_len;

  // create connection mgr
  dbBE_Redis_connection_mgr_t *conn_mgr = dbBE_Redis_connection_mgr_init( &config );
  if( conn_mgr == NULL )
  {
    LOG( DBG_ERR, stderr, "dbBE_Redis_context_t::initialize: Failed to initialize connection mgr.\n" );
    Redis_exit( context );
    return NULL;
  }

  context->_conn_mgr = conn_mgr;

  // initialize an empty list of namespaces
  context->_namespaces = NULL;

  dbBE_Redis_iterator_list_t iterators = dbBE_Redis_iterator_list_allocate();
  if( iterators == NULL )
  {
    LOG( DBG_ERR, stderr, "dbBE_Redis_context_t::initialize: Failed to initialize iterators\n" );
    Redis_exit( context );
    return NULL;
  }
  context->_iterators = iterators;

  int rc;
  if( ( rc = dbBE_Redis_connect_initial( context )) != 0 )
  {
    LOG( DBG_ERR, stderr, "dbBE_Redis_context_t::initialize: Failed to connect to Redis. rc=%d\n", rc );
    Redis_exit( context );
    return NULL;
  }

  return (dbBE_Handle_t*)context;
}

/*
 * destroy/exit the system library context
 */
int Redis_exit( dbBE_Handle_t be )
{
  int rc = 0;
  int temp = 0;
  if( be != NULL )
  {
    dbBE_Redis_context_t *context = (dbBE_Redis_context_t*)be;
    dbBE_Redis_connection_mgr_exit( context->_conn_mgr );
    temp = dbBE_Redis_iterator_list_destroy( context->_iterators );
    if(( temp != 0 ) && ( rc == 0 )) rc = temp;
    temp = dbBE_Redis_locator_destroy( context->_locator );
    if(( temp != 0 ) && ( rc == 0 )) rc = temp;
    temp = dbBE_Request_set_destroy( context->_cancellations );
    if(( temp != 0 ) && ( rc == 0 )) rc = temp;
    temp = dbBE_Redis_cluster_info_destroy( context->_cluster_info );
    if(( temp != 0 ) && ( rc == 0 )) rc = temp;
    temp = dbBE_Redis_namespace_list_clean( context->_namespaces );
    if(( temp != 0 ) && ( rc == 0 )) rc = temp;
    if( context->_sender_connections != NULL )
      free( context->_sender_connections );
    dbBE_Transport_sr_buffer_free( context->_sender_buffer );
    temp = dbBE_Redis_s2r_queue_destroy( context->_retry_q );
    if(( temp != 0 ) && ( rc == 0 )) rc = temp;
    temp = dbBE_Completion_queue_destroy( context->_compl_q );
    if(( temp != 0 ) && ( rc == 0 )) rc = temp;
    temp = dbBE_Request_queue_destroy( context->_work_q );
    if(( temp != 0 ) && ( rc == 0 )) rc = temp;
    dbBE_Redis_command_stages_spec_destroy( context->_spec );
    memset( context, 0, sizeof( dbBE_Redis_context_t ) );
    free( context );
    if( gScrapSpace != NULL )
    {
      free( gScrapSpace );
      gScrapSpace = NULL;
    }
    context = NULL;
  }

  return rc;
}

int dbBE_Redis_request_sanity_check( dbBE_Request_t *request )
{
  int rc = 0;
  switch( request->_opcode )
  {
    case DBBE_OPCODE_NSDETACH:
    case DBBE_OPCODE_NSDELETE:
      if( request->_key != NULL )
        rc = EINVAL;
      else
        rc = dbBE_Redis_namespace_validate( request->_ns_hdl );
      break;
    case DBBE_OPCODE_REMOVE:
    case DBBE_OPCODE_GET:
    case DBBE_OPCODE_READ:
    case DBBE_OPCODE_PUT:
      if( request->_key == NULL )
        rc = EINVAL;
      else
        rc = dbBE_Redis_namespace_validate( request->_ns_hdl );
      break;
    case DBBE_OPCODE_MOVE:
      if( request->_sge_count != 2 )
        rc = EINVAL;
      if( rc == 0 )
        rc = dbBE_Redis_namespace_validate( request->_ns_hdl );
      if( rc == 0 )
        rc = dbBE_Redis_namespace_validate( request->_sge[0].iov_base );
      break;
    case DBBE_OPCODE_DIRECTORY: // only single-SGE request supported by the RedisBE
      if( request->_sge_count != 2 )
        rc = ENOTSUP;
      if(( request->_sge[0].iov_base == NULL ) || ( request->_sge[0].iov_len < 1 ) ||
          ( request->_sge[1].iov_base != NULL ) || ( request->_sge[1].iov_len < 1 ))
        rc = EINVAL;
      break;
    case DBBE_OPCODE_ITERATOR:
      if(( request->_sge_count != 1 ) && ( request->_sge[0].iov_base != request->_key ))
        rc = EINVAL;
      break;
    case DBBE_OPCODE_UNSPEC:
    case DBBE_OPCODE_CANCEL:
    case DBBE_OPCODE_NSCREATE:
    case DBBE_OPCODE_NSATTACH:
    case DBBE_OPCODE_NSQUERY:
    case DBBE_OPCODE_NSADDUNITS:
    case DBBE_OPCODE_NSREMOVEUNITS:
    case DBBE_OPCODE_MAX:
      break;
  }
  return rc;
}

/*
 * post a new request to the backend
 */
dbBE_Request_handle_t Redis_post( dbBE_Handle_t be,
                                  dbBE_Request_t *request,
                                  int trigger )
{
  if(( be == NULL ) || ( request == NULL))
  {
    errno = EINVAL;
    return NULL;
  }

  if( dbBE_Redis_request_sanity_check( request ) != 0 )
  {
    errno = EINVAL;
    return NULL;
  }

  dbBE_Redis_context_t *rbe = ( dbBE_Redis_context_t* )be;

  // check if there's space in the queue
  if( dbBE_Request_queue_len( rbe->_work_q ) >= DBBE_REDIS_WORK_QUEUE_DEPTH )
  {
    dbBE_Redis_sender_trigger( rbe );
    errno = EAGAIN;
    return NULL;
  }

  // queue to posting queue
  int rc = dbBE_Request_queue_push( rbe->_work_q, request );

  if( rc != 0 )
  {
    errno = ENOENT;
    return NULL;
  }

  if( trigger )
    dbBE_Redis_sender_trigger( rbe );

  dbBE_Request_handle_t rh = (dbBE_Request_handle_t*)request;
  return rh;
}

/*
 * cancel a request
 */
int Redis_cancel( dbBE_Handle_t be,
                  dbBE_Request_handle_t request )
{
  if(( be == NULL ) || ( request == NULL))
  {
    return EINVAL;
  }

  dbBE_Redis_context_t *rbe = ( dbBE_Redis_context_t* )be;
  dbBE_Request_t *be_req = (dbBE_Request_t*)request;

  if( dbBE_Request_set_insert( rbe->_cancellations, be_req ) == ENOSPC )
    return EAGAIN;
  return 0;
}

/*
 * test for completion of a particular posted request
 * returns the status of the request
 */
dbBE_Completion_t* Redis_test( dbBE_Handle_t be,
                               dbBE_Request_handle_t request )
{
  errno = ENOSYS;
  return NULL;
}

/*
 * fetch the first completed request from the completion queue
 * or return NULL if nothing is completed since the last call
 */
dbBE_Completion_t* Redis_test_any( dbBE_Handle_t be )
{
  if( be == NULL )
  {
    errno = EINVAL;
    return NULL;
  }

  dbBE_Redis_context_t *rbe = ( dbBE_Redis_context_t* )be;

  if( dbBE_Completion_queue_len( rbe->_compl_q ) == 0 )
  {
    // if completion queue is empty, see if we can make some progress on requests to change that.
    dbBE_Redis_sender_trigger( rbe );
    // if then still no completion, give up here and let the caller retry later
    if( dbBE_Completion_queue_len( rbe->_compl_q ) == 0 )
    {
      errno = EAGAIN;
      return NULL;
    }
  }

  dbBE_Completion_t *compl = dbBE_Completion_queue_pop( rbe->_compl_q );

  return compl;
}

/*
 * create the initial connection to Redis with srbuffers by extracting the url from the ENV variable
 */
int dbBE_Redis_connect_initial( dbBE_Redis_context_t *ctx )
{
  int rc = 0;
  if( ctx == NULL )
  {
    errno = EINVAL;
    return -EINVAL;
  }

  char *env_url = dbBE_Extract_env( DBR_SERVER_HOST_ENV, DBR_SERVER_DEFAULT_HOST );
  if( env_url == NULL )
  {
    errno = ENODEV;
    return -ENODEV;
  }

  LOG(DBG_VERBOSE, stderr, "url=%s\n", env_url );

  dbBE_Redis_connection_t *initial_conn = dbBE_Redis_connection_mgr_newlink( ctx->_conn_mgr, env_url );
  if( initial_conn == NULL )
  {
    rc = -ENOTCONN;
    goto exit_connect;
  }

  // find out if we need to tear down the initial connection later because it's a connection to a replica
  int replica_conn = dbBE_Redis_connection_mgr_is_master( ctx->_conn_mgr, initial_conn );
  if( replica_conn < 0 )
  {
    rc = -EAFNOSUPPORT;
    goto exit_connect;
  }
  replica_conn = 1 - replica_conn; // flip the logic, since so far it contains 1 if it's a master connection

  dbBE_Redis_cluster_info_t *cl_info = dbBE_Redis_connection_mgr_get_cluster_info( ctx->_conn_mgr );
  if( cl_info == NULL )
  {
    rc = -ENOMSG;
    goto exit_connect;
  }

  // replica connection needs to be torn down to make sure the initial connections are all master connections
  if( replica_conn != 0 )
  {
    dbBE_Redis_connection_mgr_rm( ctx->_conn_mgr, initial_conn );
    dbBE_Redis_connection_destroy( initial_conn );
    initial_conn = NULL;
  }

  ctx->_cluster_info = cl_info;

  int n, s;
  for( n = 0; n < dbBE_Redis_cluster_info_getsize( cl_info ); ++n )
  {
    dbBE_Redis_server_info_t *si = dbBE_Redis_cluster_info_get_server( cl_info, n );
    if( si == NULL )
    {
      LOG( DBG_ERR, stderr, "No server info available for node %d\n", n );
      rc = -ENOTCONN;
      goto exit_connect;
    }

    dbBE_Redis_hash_slot_t first_slot = dbBE_Redis_server_info_get_first_slot( si );
    dbBE_Redis_hash_slot_t last_slot = dbBE_Redis_server_info_get_last_slot( si );

    for( s = 0; s < dbBE_Redis_server_info_getsize( si ); ++ s )
    {
      char *url = dbBE_Redis_server_info_get_replica( si, s );
      if( url == NULL )
      {
        LOG( DBG_ERR, stderr, "No replica url available for node %d:%d\n", n, s );
        rc = -ENOTCONN;
        goto exit_connect;
      }

      // replica connections will be created only if a master goes down
      if( s > 0 )
        continue;

      // check and create connection
      // check connection exists,
      dbBE_Redis_connection_t *dest =
          dbBE_Redis_connection_mgr_get_connection_to( ctx->_conn_mgr, url );

      if( dest == NULL )
        dest = dbBE_Redis_connection_mgr_newlink( ctx->_conn_mgr, url );

      if( dest == NULL )
      {
        rc = -ENOLINK;
        goto exit_connect;
      }

      dbBE_Redis_connection_assign_slot_range( dest,
                                               first_slot,
                                               last_slot );

      // update locator
      dbBE_Redis_locator_associate_range_conn_index( ctx->_locator, first_slot, last_slot, dest->_index );
    }
  }

exit_connect:
  free( env_url );
  return rc;
}
