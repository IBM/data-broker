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

#include "logutil.h"
#include "redis.h"
#include "result.h"
#include "request.h"
#include "parse.h"
#include "complete.h"

#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>  // malloc
#endif

#include <stddef.h>
#include <stdio.h>
#include <errno.h>

/*
 * receiver thread function, retrieves and parses Redis responses
 */

typedef struct dbBE_Redis_receiver_args
{
  dbBE_Redis_context_t *_backend;
  int _looping;
} dbBE_Redis_receiver_args_t;


void* dbBE_Redis_receiver( void *args )
{
  int rc = 0;
  dbBE_Redis_receiver_args_t *input = (dbBE_Redis_receiver_args_t*)args;
  if( args == NULL )
  {
    errno = EINVAL;
    return NULL;
  }

  /*
   * Input:
   *  - completion queue(s)
   *  - request queue filled by sender thread
   *  - redirection/retry queue for sender
   *  - list/map of receive buffers + Redis server instance references
   *
   */

  // check for any activity:
  //  - on the receive buffers/Redis sockets
  //  - on a notification/wake-up pipe for cancellations/urgent matters

  int receive_limit = 128 * 1024 * 1024;
  dbBE_Redis_connection_t *conn = NULL;

receive_more_responses:

  conn = dbBE_Redis_connection_mgr_get_active( input->_backend->_conn_mgr, 0 );
  if( conn == NULL )
    goto skip_receiving;

  dbBE_Redis_sr_buffer_t *sr_buf = dbBE_Transport_dbuffer_get_active( conn->_recvbuf );

  receive_limit = dbBE_Transport_sr_buffer_get_size( sr_buf );
  rc = dbBE_Redis_connection_recv( conn, sr_buf );
  if( rc <= 0 )
  {
    switch( rc )
    {
      case -ENOTEMPTY:
        rc = dbBE_Transport_sr_buffer_unprocessed( sr_buf );
        LOG( DBG_ERR, stderr, "Redis connection %d recv buffer not empty. remaining=%d %s\n",
             conn->_index, rc, dbBE_Transport_sr_buffer_get_processed_position( sr_buf ) );
        break;
      case -ENOTCONN:
        LOG( DBG_ERR, stderr, "Redis connection %d down\n", conn->_index );
        // intentionally no break
      default:
        LOG( DBG_ERR, stderr, "Recv from conn %d returned %d\n", conn->_index, rc );

        // drain the posted queue of this connection and place the requests for retry
        dbBE_Redis_request_t *request;
        while( ( request = dbBE_Redis_s2r_queue_pop( conn->_posted_q ) ) != NULL )
        {
          dbBE_Redis_s2r_queue_push( input->_backend->_retry_q, request );
        }
        // remove the connection from the locator index
        dbBE_Redis_locator_reassociate_conn_index( input->_backend->_locator,
                                                   conn->_index,
                                                   DBBE_REDIS_LOCATOR_INDEX_INVAL );

        // remove the connection from the connection mgr
        dbBE_Redis_connection_mgr_conn_fail( input->_backend->_conn_mgr, conn );

        // todo: cancel all remaining requests for cleanup

        // intentionally no break
      case 0:
        goto skip_receiving;
        break;
    }
  }

  dbBE_Redis_request_t *request = NULL;
  dbBE_Redis_result_t result;
  memset( &result, 0, sizeof( dbBE_Redis_result_t ) );

  // assume this is a new request
  int responses_remain = 0;
  receive_limit -= rc;

process_next_item:

  // when we received something:
  // fetch first request from sender's request queue
  if( responses_remain <= 0 )
    request = dbBE_Redis_s2r_queue_pop( conn->_posted_q );

  if( request == NULL )
  {
    LOG( DBG_ERR, stderr, "Serious backend protocol error. Expected posted request, but nothing found\n" );
    // todo: instead of skipping, generate an asynchronous error (i.e. one that doesn't belong to a request)
    // alternative: cancell all requests
    goto skip_receiving;
  }

  LOG( DBG_TRACE, stderr, "Processing request op=%d on conn %d\n", request->_user->_opcode, conn->_index );

  if( responses_remain <= 0 )
    responses_remain = request->_step->_resp_cnt - 1;
  else
    --responses_remain;

  // parse buffer for next complete response including nested arrays
  dbBE_Redis_result_cleanup( &result, 0 );









  sr_buf = dbBE_Transport_dbuffer_get_active( conn->_recvbuf );
  rc = dbBE_Redis_parse_sr_buffer( sr_buf, &result );

  // memcpy transport is not ready for partial string result handling
  if(( result._type == dbBE_REDIS_TYPE_STRING_PART ) && ( input->_backend->_transport == &dbBE_Memcopy_transport ))
  {
    // restore parsing position for the next loop
    dbBE_Transport_sr_buffer_rewind_processed_to( sr_buf, dbBE_Transport_sr_buffer_get_start( sr_buf ) );
    rc = -EAGAIN;
  }

  while( rc == -EAGAIN )
  {
    LOG( DBG_VERBOSE, stdout, "Incomplete recv. Trying to retrieve more data.\n" );
    rc = dbBE_Redis_connection_recv_more( conn, sr_buf );
    if( rc == 0 )
    {
      rc = -EAGAIN;
    }
    rc = dbBE_Redis_parse_sr_buffer( sr_buf, &result );

    // memcpy transport is not ready for partial string result handling
    if(( result._type == dbBE_REDIS_TYPE_STRING_PART ) && ( input->_backend->_transport == &dbBE_Memcopy_transport ))
    {
      // restore parsing position for the next loop
      dbBE_Transport_sr_buffer_rewind_processed_to( sr_buf, dbBE_Transport_sr_buffer_get_start( sr_buf ) );
      rc = -EAGAIN;
    }
  }

  // decide:
  //  - it's completed and goes to the completion queue
  //  - it's a redirect and needs to be returned to sender
  //    - extract the destination node and queue
  switch( result._type )
  {
    case dbBE_REDIS_TYPE_REDIRECT:
      // retrieve/connect the destination connection and add the index
      // to the request that's pushed to the sender-queue

      break;

    case dbBE_REDIS_TYPE_RELOCATE:
    {
      // unset the connection slot in old place
      dbBE_Redis_slot_bitmap_t *slots = dbBE_Redis_connection_get_slot_range( conn );
      dbBE_Redis_slot_bitmap_unset( slots, result._data._location._hash );

      // check connection exists,
      dbBE_Redis_connection_t *dest =
          dbBE_Redis_connection_mgr_get_connection_to( input->_backend->_conn_mgr,
                                                       result._data._location._address );
      if( dest != NULL )
      {
        // update the locator with hash
        dbBE_Redis_locator_assign_conn_index( input->_backend->_locator,
                                              dest->_index,
                                              result._data._location._hash );
      }
      else
      {
        /* a new connection required
         * this can mean: the cluster got scaled up
         * or we were talking to a replica and now get referred to the master
         * in either case, we should review/update the cluster info
         */

        switch( dbBE_Redis_connection_mgr_is_master( input->_backend->_conn_mgr, conn ) )
        {
          case 0:
          {
            // switch from replica to master and repeat the request
            LOG( DBG_ERR, stderr, "Received MOVED. Conn %d seems to be a replica\n", conn->_index );

            // repeat the current request
            dbBE_Redis_s2r_queue_push( input->_backend->_retry_q, request );

            // drain the posted queue of this connection and place the requests for retry
            while( ( request = dbBE_Redis_s2r_queue_pop( conn->_posted_q ) ) != NULL )
            {
              dbBE_Redis_s2r_queue_push( input->_backend->_retry_q, request );
            }
            // remove the connection from the locator index
            dbBE_Redis_locator_reassociate_conn_index( input->_backend->_locator,
                                                       conn->_index,
                                                       DBBE_REDIS_LOCATOR_INDEX_INVAL );

            // remove the connection from the connection mgr
            // todo: drain the response pipe to avoid stuck requests
            dbBE_Redis_connection_mgr_rm( input->_backend->_conn_mgr, conn );
            goto skip_receiving;
          }
          case 1:
          {
            // new unknown connection to a (new) master, need to update cluster info
            char address[ DBR_SERVER_URL_MAX_LENGTH ];
            snprintf( address, DBR_SERVER_URL_MAX_LENGTH, "sock://%s", result._data._location._address );

            dest = dbBE_Redis_connection_mgr_newlink( input->_backend->_conn_mgr, address );
            if( dest == NULL )
            {
              // unable to recreate connection, failing the request
              dbBE_Completion_t *completion = dbBE_Redis_complete_error(
                  request,
                  &result,
                  -ENOTCONN );
              dbBE_Redis_request_destroy( request );
              if( completion == NULL )
              {
                fprintf( stderr, "RedisBE: Failed to create error completion.\n");
                dbBE_Redis_result_cleanup( &result, 0 );
                goto skip_receiving;
              }
            }
            // update the connection slot in new destination
            dbBE_Redis_slot_bitmap_t *slots = dbBE_Redis_connection_get_slot_range( dest );
            dbBE_Redis_slot_bitmap_set( slots, result._data._location._hash );
            dbBE_Redis_locator_assign_conn_index( input->_backend->_locator, dest->_index, result._data._location._hash );
          }
            break;
          default:
            // error checking for master

            break;
        }
      }

      // push request to sender queue as is
      request->_location._data._conn_idx = dbBE_Redis_connection_get_index( dest );
      dbBE_Redis_s2r_queue_push( input->_backend->_retry_q, request );
      break;

    }
    default:
      if( request != NULL )
      {
        // in case we receive a CLUSTERDOWN error, assume this is recoverable and retry the request
        if(( result._type == dbBE_REDIS_TYPE_ERROR ) &&
            ( result._data._string._data != NULL ) &&
            ( strncmp( result._data._string._data, "CLUSTERDOWN", 11 ) == 0 ))
        {
          dbBE_Redis_s2r_queue_push( input->_backend->_retry_q, request );
          break;
        }

        switch( request->_user->_opcode )
        {
          case DBBE_OPCODE_PUT:
            rc = dbBE_Redis_process_put( request, &result );
            break;

          case DBBE_OPCODE_GET:
          case DBBE_OPCODE_READ:
            rc = dbBE_Redis_process_get( request, &result, input->_backend->_transport );
            break;

          case DBBE_OPCODE_REMOVE:
            rc = dbBE_Redis_process_remove( request, &result );
            break;

          case DBBE_OPCODE_MOVE:
            rc = dbBE_Redis_process_move( request, &result );
            break;

          case DBBE_OPCODE_DIRECTORY:
            rc = dbBE_Redis_process_directory( &request, &result,
                                               input->_backend->_transport,
                                               input->_backend->_retry_q,
                                               input->_backend->_conn_mgr );
            break;
          case DBBE_OPCODE_NSCREATE:
            rc = dbBE_Redis_process_nscreate( request, &result );
            rc = dbBE_Redis_process_nshandling( &input->_backend->_namespaces, request, &result, rc );
            break;

          case DBBE_OPCODE_NSQUERY:
            rc = dbBE_Redis_process_nsquery( request, &result, input->_backend->_transport );
            break;

          case DBBE_OPCODE_NSATTACH:
            rc = dbBE_Redis_process_nsattach( request, &result );
            rc = dbBE_Redis_process_nshandling( &input->_backend->_namespaces, request, &result, rc );
            break;

          case DBBE_OPCODE_NSDETACH:
            rc = dbBE_Redis_process_nsdetach( &request, &result,
                                              input->_backend->_retry_q,
                                              input->_backend->_conn_mgr,
                                              responses_remain );
            if(( rc == 0 ) && ( request != NULL ) && ( request->_step->_final != 0 ))
            {
              dbBE_Redis_namespace_list_t *tmp = dbBE_Redis_namespace_list_remove( input->_backend->_namespaces, request->_user->_ns_hdl );
              input->_backend->_namespaces = tmp;
            }
            break;

          case DBBE_OPCODE_NSDELETE:
            rc = dbBE_Redis_process_nsdelete( request,
                                              &result );
            break;

          default:
            fprintf( stderr, "RedisBE: Invalid command detected.\n" );
            rc = -ENOTSUP;
            // todo: further error handling required?
            break;

        }

        // there are cases where requests are just complete without causing completions or
        if( request == NULL )
          break;

        // do not attempt to queue a new request until all responses have been
        sr_buf = dbBE_Transport_dbuffer_get_active( conn->_recvbuf ); // re-get the active buffer in case it has changed while processing cmds
        if(( responses_remain > 0 ) && ( ! dbBE_Transport_sr_buffer_empty( sr_buf ) ))
          break;

        if( rc >= 0 )
        {
          // if this is the result stage, create upper layer completion
          // don't push the completion until the final stage!!
          if( request->_step->_result != 0 )
          {
            LOG( DBG_TRACE, stderr, "Completing request op=%d on conn %d\n", request->_user->_opcode, conn->_index );
            request->_completion = dbBE_Redis_complete_command(
                request,
                &result,
                rc );

            if( request->_completion == NULL )
            {
              LOG( DBG_ERR, stderr, "RedisBE: Failed to create completion.\n");
              dbBE_Redis_result_cleanup( &result, 0 );
              dbBE_Redis_request_destroy( request );
              goto skip_receiving;
            }
          }

          // if we haven't reached the final stage, we need to requeue the next stage request to the retry_q
          if( request->_step->_final == 0 )
          {
            dbBE_Redis_s2r_queue_push( input->_backend->_retry_q, request );
            dbBE_Redis_request_stage_transition( request );
          }
          else // final stage
          {
            if( dbBE_Completion_queue_push( input->_backend->_compl_q, request->_completion ) != 0 )
            {
              free( request->_completion );
              dbBE_Redis_request_destroy( request );
              LOG( DBG_ERR, stderr, "RedisBE: Failed to queue completion in final request stage.\n" );
              // todo: save the status to mark the request for cleanup during the next stages
            }
            dbBE_Redis_request_destroy( request );
            request = NULL;
          }
        }
        else // handle errors
        {
          if( rc == -EAGAIN )
          {
            dbBE_Redis_result_cleanup( &result, 0 );
            dbBE_Redis_s2r_queue_push( input->_backend->_retry_q, request );
            LOG( DBG_TRACE, stderr, "EAGAIN in parsing op=%d conn %d; remaining data=%ld\n", request->_user->_opcode,
                 conn->_index, dbBE_Transport_sr_buffer_unprocessed( sr_buf ) );

            if( ! dbBE_Transport_sr_buffer_empty( sr_buf ) )
            {
              LOG( DBG_TRACE, stderr, "Multiple responses in buffer of conn %d; remaining data=%ld\n",
                   conn->_index, dbBE_Transport_sr_buffer_unprocessed( sr_buf ) );
              goto process_next_item;
            }
            else
              goto skip_receiving;
          }

          // first pull any potentially existing completion from previous result stage(s) and clean it up
          dbBE_Completion_t *completion = request->_completion;
          if( completion != NULL )
          {
            memset( completion, 0, sizeof( dbBE_Completion_t ) );
            free( completion );
          }

          completion = dbBE_Redis_complete_error( request,
                                                  &result,
                                                  rc );
          dbBE_Redis_request_destroy( request );
          if( completion == NULL )
          {
            fprintf( stderr, "RedisBE: Failed to create error completion.\n");
            dbBE_Redis_result_cleanup( &result, 0 );
            goto skip_receiving;
          }
          if( dbBE_Completion_queue_push( input->_backend->_compl_q, completion ) != 0 )
          {
            free( completion );
            dbBE_Redis_request_destroy( request );
            fprintf( stderr, "RedisBE: Failed to queue completion.\n" );
            // todo: save the status to mark the request for cleanup during the next stages
          }
        }
      }
      else
      {
        // if there was no request in the posted queue, we have a serious protocol problem
    //      dbBE_Redis_create_error( request, input->_backend->_compl_q );
      }
      break;
  }

  // question: completion in order or out-of-order?
  // can only complete out-of-order because Gets would block the whole completion queue

  // repeat if the receive buffer contains more responses
  if( ! dbBE_Transport_sr_buffer_empty( sr_buf ) )
  {
    LOG( DBG_TRACE, stderr, "Multiple responses in buffer of conn %d; remaining data=%ld\n",
         conn->_index, dbBE_Transport_sr_buffer_unprocessed( sr_buf ) );
    goto process_next_item;
  }
  dbBE_Redis_result_cleanup( &result, 0 );

  if( receive_limit > 0 )
    goto receive_more_responses;

skip_receiving:
  return NULL;
}

void dbBE_Redis_receiver_trigger( dbBE_Redis_context_t *backend )
{
  dbBE_Redis_receiver_args_t *args = (dbBE_Redis_receiver_args_t*)malloc( sizeof( dbBE_Redis_receiver_args_t ) );
  if( args == NULL )
  {
    LOG( DBG_ERR, stderr, "Args allocation when trying to trigger the receiver." );
    return;
  }
  args->_backend = backend;
  args->_looping = 1;
  dbBE_Redis_receiver( (void*) args );
  free( args );
}
