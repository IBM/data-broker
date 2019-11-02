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

#ifndef BACKEND_REDIS_PARSE_H_
#define BACKEND_REDIS_PARSE_H_

#include "common/data_transport.h"
#include "transports/sr_buffer.h"
#include "definitions.h"
#include "s2r_queue.h"
#include "conn_mgr.h"
#include "namespacelist.h"

#include <errno.h>

/************************************************
 *  parsing functions:
 */

/*
 * null-terminate a string in buffer p (buffer is modified!)
 * return the string length and the number of parsed bytes of p
 */
int64_t dbBE_Redis_nul_terminate_string( char *p, size_t *parsed, const int64_t limit );

/*
 * extract an integer from the beginning of p
 * return the parsed value and the number of parsed bytes
 * will return DBBE_REDIS_NAN if p contains non-numeric characters
 *
 * the limit determines the max number of bytes to be parsed
 * if the number is not terminated before the limit is reached,
 * the return will be -EAGAIN and *parsed will be set to 0
 * make sure to check that condition and retry the parsing
 * after another recv call
 */
int64_t dbBE_Redis_extract_integer( char *p, size_t *parsed, const int64_t limit );

/*
 * extract a redis bulk string from p (buffer is modified!)
 * returns the string length and points *p to the beginning of the string
 * the string is null-terminated and the number of parsed bytes is set
 *
 * the limit determines the max number of byte to be parsed
 * if the string is not terminated before the limit is reached,
 * the return will be -EAGAIN and the buffer parsing will be reset
 * to the place before the call
 */
int64_t dbBE_Redis_extract_bulk_string( char **p, size_t *parsed, const int64_t limit, size_t *actual_size );

/*
 * parse the input buffer
 * return the Redis result including its type and its size
 */
int dbBE_Redis_parse_sr_buffer( dbBE_Redis_sr_buffer_t *sr_buf,
                                dbBE_Redis_result_t *result );

/*
 * process the response based on the request
 */
int dbBE_Redis_process_response( dbBE_Redis_request_t *request,
                                 dbBE_Redis_data_t *result,
                                 size_t result_size,
                                 dbBE_REDIS_DATA_TYPE result_type,
                                 dbBE_Data_transport_t *transport );



/*
 * process the response data of a put request
 */
int dbBE_Redis_process_put( dbBE_Redis_request_t *request,
                            dbBE_Redis_result_t *result );

/*
 * process the response data of a get request
 */
int dbBE_Redis_process_get( dbBE_Redis_request_t *request,
                            dbBE_Redis_result_t *result,
                            dbBE_Data_transport_t *transport,
                            dbBE_Redis_connection_t *connection );

/*
 * process the response data of a move request and its stages
 */
int dbBE_Redis_process_move( dbBE_Redis_request_t *request,
                             dbBE_Redis_result_t *result,
                             dbBE_Redis_connection_t *conn );

/*
 * process the response data of a remove request
 */
int dbBE_Redis_process_remove( dbBE_Redis_request_t *request,
                               dbBE_Redis_result_t *result );

/*
 * process the response data of a directory request
 */
int dbBE_Redis_process_directory( dbBE_Redis_request_t **in_out_request,
                                  dbBE_Redis_result_t *result,
                                  dbBE_Data_transport_t *transport,
                                  dbBE_Redis_s2r_queue_t *post_queue,
                                  dbBE_Redis_connection_mgr_t *conn_mgr );


/*
 * post-process namespace create/attach to handle locally tracked namespace handles/structures
 */
int dbBE_Redis_process_nshandling( dbBE_Redis_namespace_list_t **s,
                                   dbBE_Redis_request_t *request,
                                   dbBE_Redis_result_t *result,
                                   int rc );

/*
 * process the response data of a name space create request
 */
int dbBE_Redis_process_nscreate( dbBE_Redis_request_t *request,
                                 dbBE_Redis_result_t *result );

/*
 * process the response data of a name space query request
 */
int dbBE_Redis_process_nsquery( dbBE_Redis_request_t *request,
                                 dbBE_Redis_result_t *result,
                                 dbBE_Data_transport_t *transport );

/*
 * process the response data of a name space attach request
 */
int dbBE_Redis_process_nsattach( dbBE_Redis_request_t *request,
                                 dbBE_Redis_result_t *result );

/*
 * process the response data of a name space detach request
 */
int dbBE_Redis_process_nsdetach( dbBE_Redis_request_t **in_out_request,
                                 dbBE_Redis_result_t *result,
                                 dbBE_Redis_s2r_queue_t *post_queue,
                                 dbBE_Redis_connection_mgr_t *conn_mgr,
                                 int remaining_responses );

/*
 * process the response data/stages of a name space delete request
 */
int dbBE_Redis_process_nsdelete( dbBE_Redis_request_t *request, // request might be dropped/modified
                                 dbBE_Redis_result_t *result );

/*
 * the nsquery processing will receive an array with all data from the name space hash
 * this data has to be put into a single string and then scattered out the user buffer
 */
int dbBE_Redis_process_nsquery( dbBE_Redis_request_t *request,
                                dbBE_Redis_result_t *result,
                                dbBE_Data_transport_t *transport );

/*
 * the iterator processing handles the response array of SCAN
 * the cursor will update the iterator, the keys will be cached
 * and if one connection is fully iterated, it continues with the next
 */
int dbBE_Redis_process_iterator( dbBE_Redis_request_t **in_out_request,
                                 dbBE_Redis_result_t *result,
                                 dbBE_Redis_s2r_queue_t *post_queue,
                                 dbBE_Redis_connection_mgr_t *conn_mgr );

/*
 * do generic result checks based on request, result, and types
 */
static inline
int dbBE_Redis_process_general( dbBE_Redis_request_t *request,
                                dbBE_Redis_result_t *result )
{
  int rc = 0;
  // input argument check
  if(( request == NULL ) || ( result == NULL ))
    return -EINVAL;

  dbBE_Redis_command_stage_spec_t *spec = request->_step;
  // does the request step have a valid spec?
  if( spec == NULL )
    rc = -EPROTO;

  // did we get the expected result type?
  if( result->_type !=  spec->_expect )
  {
    switch( result->_type )
    {
      case dbBE_REDIS_TYPE_REDIRECT:
        // initiate redirection via ASK
        // update request connection index
        break;

      case dbBE_REDIS_TYPE_RELOCATE:
        // initiate redirection via MOVE
        // update request connection index
        // update location manager
        break;
      case dbBE_REDIS_TYPE_STRING_PART:
        // partial strings are ok, if the expected type is char/string too
        if(( spec->_expect == dbBE_REDIS_TYPE_CHAR ) &&
            (( request->_user->_opcode == DBBE_OPCODE_READ ) ||
                ( request->_user->_opcode == DBBE_OPCODE_GET ) ||
                ( request->_user->_opcode == DBBE_OPCODE_MOVE )))
        {
          break;
        }
        // no break for partial strings of 'unprepared' commands
      default:
        rc = -EBADMSG;
        break;
    }

  }

  return rc;
}



#endif /* BACKEND_REDIS_PARSE_H_ */
