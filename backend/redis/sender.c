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

  return check;
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
   * Input:
   *  - user request queue(s)
   *  - redirection/retry queue from receiver
   *  - list/map of send buffers + Redis server instance references
   */

  // check for any activity:
  //  - on the redirect/retry queue (priority!!)
  dbBE_Redis_request_t *request = dbBE_Redis_s2r_queue_pop( input->_backend->_retry_q );
  dbBE_Request_t *user_req = NULL;

  //  - on the request queue
  if( request == NULL )
  {
    user_req = dbBE_Request_queue_pop( input->_backend->_work_q );
    if( user_req == NULL )
    {
      goto skip_sending;
    }

    request = dbBE_Redis_request_allocate( user_req );
    if( request == NULL )
    {
      goto skip_sending;
    }
  }

  /*
   * Do the location check/retrieval each time and also for multi-stage requests
   * because the key might have changed and then the conn-index would be off.
   * Also, check the hash-coverage in case there were any connection failures.
   */
  if( dbBE_Redis_cmd_stage_needs_rekeying( request ) != 0 )
  {
    char keybuffer[ DBBE_REDIS_MAX_KEY_LEN ];
    if( dbBE_Redis_create_key( request, keybuffer, DBBE_REDIS_MAX_KEY_LEN ) < 0 )
    {
      dbBE_Redis_create_send_error( input->_backend->_compl_q, request, -EINVAL );
      goto skip_sending;
    }

    // use locator to retrieve address (unless it's a redirect - ASK redirects are temporary and will not update the locator mapping)
    uint16_t slot = dbBE_Redis_locator_hash( keybuffer, strnlen( keybuffer, DBBE_REDIS_MAX_KEY_LEN ) );
    request->_conn_index = dbBE_Redis_locator_get_conn_index( input->_backend->_locator, slot );

    if( request->_conn_index == DBBE_REDIS_LOCATOR_INDEX_INVAL )
    {
      dbBE_Redis_create_send_error( input->_backend->_compl_q, request, -ENOTCONN );
      goto skip_sending;
    }
  }

  /*
   * check server connections,
   * fail any request if there's a connection missing
   * because the cluster will not accept new requests anyway
   */
  if( dbBE_Redis_locator_hash_covered( input->_backend->_locator ) == 0 )
  {
    if( dbBE_Redis_connection_mgr_conn_recover( input->_backend->_conn_mgr, input->_backend->_locator ) == 0 )
    {
      dbBE_Redis_create_send_error( input->_backend->_compl_q, request, -ENOTCONN );
      goto skip_sending;
    }
  }

  // check for any cancelled requests and process cancellations
  if( dbBE_Request_set_delete( input->_backend->_cancellations, request->_user) != 0 )
  {
    dbBE_Completion_t *completion = dbBE_Redis_complete_cancel(
        request,
        rc );

    if( completion != NULL )
      if( dbBE_Completion_queue_push( input->_backend->_compl_q, completion ) != 0 )
      {
        free( completion );
        dbBE_Redis_request_destroy( request );
        fprintf( stderr, "RedisBE: Failed to queue completion.\n" );
        // todo: save the status to mark the request for cleanup during the next stages
      }

    // clean the RedisBE request struct
    dbBE_Redis_request_destroy( request );
    request = NULL;
    rc = 0;
    goto skip_sending;
  }

  // connection mgr to retrieve the sr_buffer + socket
  dbBE_Redis_connection_t *conn = dbBE_Redis_connection_mgr_get_connection_at( input->_backend->_conn_mgr, request->_conn_index );
  if( conn == NULL )
  {
    fprintf( stderr, "Failed to get back-end connection %d.\n", request->_conn_index );

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
  dbBE_Transport_sr_buffer_reset( input->_backend->_sender_buffer );

  // store request to posted requests queue
  rc = dbBE_Redis_s2r_queue_push( conn->_posted_q, request );
  if( rc != 0 )
  {
    rc = -ENOMSG;
    goto skip_sending;
  }


skip_sending:
  if(( rc < 0 ) && ( user_req != NULL ))
  {
    dbBE_Redis_request_destroy( request );
    request = NULL;
  }
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
