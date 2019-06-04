/*
 * Copyright © 2018,2019 IBM Corporation
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

#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>  // malloc
#endif

#include <stddef.h>
#include <stdio.h>

#include "logutil.h"
#include "../common/completion_queue.h"
#include "redis.h"
#include "create.h"
#include "complete.h"


typedef struct dbBE_Redis_sender_args
{
  dbBE_Redis_context_t *_backend;
  int _looping;
} dbBE_Redis_sender_args_t;

int dbBE_Redis_create_send_error( dbBE_Completion_queue_t *cq, dbBE_Redis_request_t *request, int error )
{
  dbBE_Redis_result_t result = { ._type = dbBE_REDIS_TYPE_INT, ._data = -1 };
  dbBE_Completion_t *completion = dbBE_Redis_complete_error(
      request,
      &result,
      error );
  dbBE_Redis_request_destroy( request );
  if( completion != NULL )
  {
    if( dbBE_Completion_queue_push( cq, completion ) != 0 )
    {
      free( completion );
      dbBE_Redis_request_destroy( request );
      fprintf( stderr, "RedisBE: Failed to queue send-error completion.\n" );
    }
  }
  else
    fprintf( stderr, "RedisBE: Failed to create send-error completion.\n");

  return 0;
}

static inline
int dbBE_Redis_cmd_stage_needs_rekeying( dbBE_Redis_request_t *request )
{
  int check = 0;
  check += ( request->_step->_stage == 0 ); // all first-stage requests need to get checked
  check += ( request->_user->_opcode == DBBE_OPCODE_MOVE ); // MOVE cmd needs re-keying for each stage
  check += (( request->_user->_opcode == DBBE_OPCODE_NSDETACH ) && ( request->_step->_stage == DBBE_REDIS_NSDETACH_STAGE_DELNS ) );

  return check;
}


static
dbBE_Redis_request_t* dbBE_Redis_sender_acquire_request( dbBE_Redis_context_t *backend )
{
  // check for any activity according to priority
  //  - request shelf (anything that had to wait because of broken connections)
  //  - repeat/multistage/redirect (anything that needs an additional iteration)
  //  - new user requests
  dbBE_Redis_request_t *request = NULL; // todo: pick from shelf
  dbBE_Request_t *user_req = NULL;

  do
  {
    if( request == NULL )
      request = dbBE_Redis_s2r_queue_pop( backend->_retry_q );

    if( request == NULL )
    {
      user_req = dbBE_Request_queue_pop( backend->_work_q );
      if( user_req != NULL )
        request = dbBE_Redis_request_allocate( user_req );
    }

    // if there's really nothing to do: skip
    if( request == NULL )
      return NULL;


    // Check if this request has been cancelled before continuing to process it
    if( dbBE_Request_set_delete( backend->_cancellations, request->_user) != 0 )
    {
      dbBE_Completion_t *completion = dbBE_Redis_complete_cancel(
          request,
          DBR_ERR_CANCELLED );

      if( completion != NULL )
        if( dbBE_Completion_queue_push( backend->_compl_q, completion ) != 0 )
        {
          free( completion );
          dbBE_Redis_request_destroy( request );
          fprintf( stderr, "RedisBE: Failed to queue completion.\n" );
          // todo: save the status to mark the request for cleanup during the next stages
        }

      // clean the RedisBE request struct
      dbBE_Redis_request_destroy( request );
      request = NULL;
    }
  } while( request == NULL ); // repeat in case there was a cancellation
  return request;
}

static
dbBE_Redis_connection_t* dbBE_Redis_sender_find_connection( dbBE_Redis_context_t *backend,
                                                            dbBE_Redis_request_t *request )
{
  dbBE_Redis_connection_t *conn = NULL;

  /*
   * Do the location check/retrieval each time and also for multi-stage requests
   * because the key might have changed and then the conn-index would be off.
   */
  if( dbBE_Redis_cmd_stage_needs_rekeying( request ) != 0 )
  {
    char keybuffer[ DBBE_REDIS_MAX_KEY_LEN ];
    if( dbBE_Redis_create_key( request, keybuffer, DBBE_REDIS_MAX_KEY_LEN ) < 0 )
    {
      dbBE_Redis_create_send_error( backend->_compl_q, request, -EINVAL );
      return NULL;
    }

    /*
     * use locator to retrieve address
     * unless it's a redirect (ASK) which directly contains
     * a direct connection pointer for temporary requesting a different server
     */
    uint16_t slot = dbBE_Redis_locator_hash( keybuffer, strnlen( keybuffer, DBBE_REDIS_MAX_KEY_LEN ) );
    if( request->_location._type != DBBE_REDIS_REQUEST_LOCATION_TYPE_CONNECTION )
    {
      request->_location._data._conn_idx = dbBE_Redis_locator_get_conn_index( backend->_locator, slot );
      if( request->_location._data._conn_idx == DBBE_REDIS_LOCATOR_INDEX_INVAL )
        request->_location._type = DBBE_REDIS_REQUEST_LOCATION_TYPE_UNKNOWN;
      else
        request->_location._type = DBBE_REDIS_REQUEST_LOCATION_TYPE_SLOT;
    }

    if( request->_location._type == DBBE_REDIS_REQUEST_LOCATION_TYPE_UNKNOWN )
    {
      dbBE_Redis_create_send_error( backend->_compl_q, request, -ENOTCONN );
      return NULL;
    }
  }

  // connection mgr to retrieve the sr_buffer + socket
  if( request->_location._type == DBBE_REDIS_REQUEST_LOCATION_TYPE_SLOT )
    conn = dbBE_Redis_connection_mgr_get_connection_at( backend->_conn_mgr, request->_location._data._conn_idx );
  else
    conn = request->_location._data._connection;

  return conn;
}

/*
 * sender function, creates requests to redis
 */
void* dbBE_Redis_sender( void *args )
{
  int rc = 0;

  dbBE_Redis_sender_args_t *input = (dbBE_Redis_sender_args_t*)args;
  if( args == NULL )
  {
    errno = EINVAL;
    return NULL;
  }

  if( input->_backend == NULL )
  {
    fprintf( stderr, "FATAL: No backend defined.");
    return NULL;
  }


  /*
   * check server connections,
   * fail any request if there's a connection missing
   * because the cluster will not accept new requests anyway
   */
  if( dbBE_Redis_locator_hash_covered( input->_backend->_locator ) == 0 )
  {
    dbBE_Redis_connection_recoverable_t recoverable = dbBE_Redis_connection_mgr_conn_recover(
        input->_backend->_conn_mgr,
        input->_backend->_locator,
        &( input->_backend->_cluster_info ) );

    switch( recoverable )
    {
      case DBBE_REDIS_CONNECTION_RECOVERABLE:  // recoverable but not yet recovered
        goto skip_sending;
        break;
      case DBBE_REDIS_CONNECTION_RECOVERED: // recovered
        // nothing to do, we're good to continue
        break;
      case DBBE_REDIS_CONNECTION_UNRECOVERABLE: // not recoverable at the moment
        LOG(DBG_ERR, stderr, "Unrecoverable cluster connection. Completing all requests as failed.\n")
      default: // unrecognized
      {
        // flush queues
        dbBE_Redis_request_t *request;
        while( ( request = dbBE_Redis_s2r_queue_pop( input->_backend->_retry_q )) != NULL )
          dbBE_Redis_create_send_error( input->_backend->_compl_q, request, -ENOTCONN );
        return NULL;
        break;
      }
    }
  }

  dbBE_Redis_request_t *request = NULL;
  int send_limit = 128 * 1024 * 1024;

acquire_another_request:

  request = dbBE_Redis_sender_acquire_request( input->_backend );
  if( request == NULL )
    goto skip_sending;


  // find out which connection to use
  dbBE_Redis_connection_t *conn = dbBE_Redis_sender_find_connection( input->_backend, request );

  if( conn == NULL )
  {
    fprintf( stderr, "Failed to get back-end connection.\n" );

    // todo: might have to create completion (unless there are more sub-tasks in flight)
    rc = -ENOMSG;
    goto skip_sending;
  }

  if( ! dbBE_Redis_connection_RTS( conn ) )
  {
    LOG( DBG_ERR, stderr, "Associated connection not ready to send\n" );
    rc = -ENOTCONN;
    goto skip_sending;
  }

  // create_command assembles an SGE list
  // entries either come directly from user or from send buffer
  // when complete, connection.send() fires the assembled data
  dbBE_sge_t cmd[ DBBE_SGE_MAX ];
  rc = dbBE_Redis_create_command_sge( request, input->_backend->_sender_buffer, cmd );
  if( rc < 0 )
  {
    LOG( DBG_ERR, stderr, "Failed to create command. rc=%d\n", rc );
    rc = -ENOMSG;
    goto skip_sending;
  }

  rc = dbBE_Redis_connection_send_cmd( conn, cmd, rc );
  if( rc < 0 )
  {
    LOG( DBG_ERR, stderr, "Failed to send command. rc=%d\n", rc );
    dbBE_Transport_sr_buffer_reset( input->_backend->_sender_buffer );
    goto skip_sending;
  }
  send_limit -= rc;
  dbBE_Transport_sr_buffer_reset( input->_backend->_sender_buffer );

  // store request to posted requests queue
  rc = dbBE_Redis_s2r_queue_push( conn->_posted_q, request );
  if( rc != 0 )
  {
    rc = -ENOMSG;
    goto skip_sending;
  }

  if( send_limit > 0 )
    goto acquire_another_request;

skip_sending:
  dbBE_Redis_receiver_trigger( input->_backend );

  // complete the request with an error
  //dbBE_Redis_create_error( request, input->_backend->_compl_q );

  // clean up
  free( args );
  return NULL;
}

void dbBE_Redis_sender_trigger( dbBE_Redis_context_t *backend )
{
  dbBE_Redis_sender_args_t *args = (dbBE_Redis_sender_args_t*)malloc( sizeof( dbBE_Redis_sender_args_t ) );
  args->_backend = backend;
  args->_looping = 1;
  dbBE_Redis_sender( (void*) args );
}
