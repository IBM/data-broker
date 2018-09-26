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

#include <stdio.h>
#include <errno.h>
#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>  // malloc
#include <stdlib.h>
#endif
#include <string.h>
#include <inttypes.h>
#include <math.h>

#include <sys/socket.h>

#include "logutil.h"
#include <libdatabroker.h>
#include "../../src/libdatabroker_int.h"
#include "sr_buffer.h"
#include "result.h"
#include "parse.h"


// length of the ASK response including the trailing space
#define DBBE_REDIS_REDIRECT_RESPONSE_LEN ( 4 )

// length of the MOVED response including the trailing space
#define DBBE_REDIS_RELOCATE_RESPONSE_LEN ( 6 )

static inline
int return_error_clean_result( int rc, dbBE_Redis_result_t *result )
{
  int ret = dbBE_Redis_result_cleanup( result, 0 );
  result->_type = dbBE_REDIS_TYPE_INT;
  result->_data._integer = rc;
  return ret;
}


int64_t dbBE_Redis_nul_terminate_string( char *p, size_t *parsed, const int64_t limit )
{
  size_t res_len = 0;
  if(( p == NULL ) || ( parsed == NULL ))
  {
    errno = EINVAL;
    return 0;
  }
  char *end = strstr( p, "\r\n" );
  if(( end == NULL ) || ( end - p > limit ))
  {
    *parsed = 0;
    return -EAGAIN;
  }
  size_t len = (size_t)( (uintptr_t)end - (uintptr_t)p );
  res_len = len;

  if( *end == '\r' ) { end++; len++; }
  if( *end == '\n' ) { end++; len++; }

  // terminate the string
  //p[ res_len ] = '\0';

  *parsed = len;
  return (int64_t)res_len;
}

int64_t dbBE_Redis_extract_integer( char *p, size_t *parsed, const int64_t limit )
{
  size_t pp = 0;
  int64_t sign = 1;
  int64_t ret = 0;
  char *terminator;
  int64_t remaining = limit;

  if(( p == NULL ) || ( parsed == NULL ))
  {
    errno = EINVAL;
    return DBBE_REDIS_NAN;
  }

  if( *p == '-' )
  {
    sign = -1;
    ++p;
    ++pp;
    --remaining;
  }
  if( *p == '+' )
  {
    ++p;
    ++pp;
    --remaining;
  }
  while((*p != '\r') && (*p != '\0' ) && ( remaining > 0 ))
  {
    ret *= 10;
    int64_t digit = *p - '0';

    // as soon as we find a non-numeric symbol, the result needs to be 0
    if(( digit > 9 ) || ( digit < 0 ))
      goto exit_with_nan;
    else
      ret += digit;
    ++p;
    ++pp;
    --remaining;
  }

  if( remaining < 2 )
  {
    *parsed = 0;
    return -EAGAIN;
  }

  if( *p == '\r' ) { p++; pp++; }
  if( *p == '\n' ) { p++; pp++; }

  *parsed = pp;
  ret *= sign;
  return ret;

exit_with_nan:
  terminator = strstr(p, "\r\n");
  if( terminator == NULL )
    *parsed = pp + strlen( p );
  else
    *parsed = pp + (size_t)( (uintptr_t)terminator - (uintptr_t)p ) + 2;

  return DBBE_REDIS_NAN;
}

// function to find the terminator, strstr will not work because of zeroes in the data
char* dbBE_Redis_find_terminator( char *haystack, const int64_t limit )
{
  char *p = haystack;
  int64_t pos = 0;
  while( pos < limit )
  {
    while( ( pos < limit - 1 ) && ( *p != '\r'))
    {
      ++pos;
      ++p;
    }
    if( ( *p == '\r') && ( p[1] == '\n'))
    {
      LOG( DBG_ALL, stdout, "Found Terminator @%"PRId64"\n", pos );
      return p;
    }
    ++pos;
    ++p;
  }
  return NULL;
}

int64_t dbBE_Redis_extract_bulk_string( char **p, size_t *parsed, const int64_t limit )
{
  if(( p == NULL ) || ( *p == NULL ) || ( parsed == NULL ) || ( limit <= 0 ))
  {
    errno = EINVAL;
    return -EINVAL;
  }

  size_t processed = 0;
  int64_t exp_len = dbBE_Redis_extract_integer( *p, &processed, limit );

  // check for incomplete data
  if( (( exp_len == -EAGAIN ) && ( processed == 0 )) ||
      ( exp_len >= limit - (int64_t)processed ) )
   {
    *parsed = 0;
    return -EAGAIN;
  }

  // cover the redis NULL ptr case
  if( exp_len == -1 )
  {
    *p = NULL;
    exp_len = 0;
    *parsed = processed;
    return exp_len;
  }

  if( exp_len < 0 )
  {
    fprintf( stderr, "Error: Redis protocol parsing error. String len set to %"PRId64"\n", exp_len );
    *parsed = limit;
    return -EPROTO;
  }

  char *string = ( *p + processed );

  // check for incomplete data with only the \n missing
  if( ( string[exp_len] == '\r' ) && ( exp_len + 1 == limit - (int64_t)processed ) )
  {
    LOG( DBG_INFO, stderr, "String just missing the \\n part of terminator\n" );
    *parsed = 0;
    return -EAGAIN;
  }

  // check the terminator
  if(( string[exp_len] != '\r' ) || ( string[exp_len+1] != '\n' ))
  {
    LOG( DBG_ALL, stderr, "Terminator not in expected place: exp=%"PRId64"; lim=%"PRId64"; prcssd=%"PRId64"; %x %x|%x %x %x\n",
         exp_len, limit, processed, string[exp_len-2], string[exp_len-1], string[exp_len], string[exp_len+1], string[exp_len+2] );
    // if the terminator is not in the expected place, try to parse for the terminator and adjust or fail if terminator is not found
    char *terminator = dbBE_Redis_find_terminator( string, limit - processed );
    if( terminator == NULL )
    {
      int64_t ret = limit-processed;
      *parsed = limit;
      *p = string;
      LOG( DBG_ERR, stderr, "Error: Redis protocol parsing error. String len expected %"PRId64", actual %"PRId64"\n", exp_len, ret );
      return -EPROTO;
    }
    else
    {
      int64_t ret = ((int64_t)terminator - (int64_t)string);
      LOG( DBG_WARN, stderr, "Warning: Redis string len expected %"PRId64", actual %"PRId64". Using actual len to continue.\n", exp_len, ret );
      exp_len = ret;
    }
  }

  // if it's correct, we set the string and return successfully
  *parsed = processed + (exp_len + 2);
  //string[exp_len] = '\0';
  *p = string;
  return exp_len;
}


// try not to do anything here, really only do parsing and return of pointer into the rbuffer without copies
// if copies are needed, let the caller do that, it should know better...
// the only change happens on the buffer in-place by nul-terminating the strings
int dbBE_Redis_parse_sr_buffer_check( dbBE_Redis_sr_buffer_t *sr_buf,
                                      dbBE_Redis_result_t *result,
                                      const int toplevel )
{
  int rc = 0;
  if(( sr_buf == NULL ) || ( result == NULL ))
  {
    if( result != NULL )
    {
      result->_data._integer = -EINVAL;
      result->_type = dbBE_REDIS_TYPE_INVALID;
    }
    return -EINVAL;
  }

  if( dbBE_Redis_sr_buffer_empty( sr_buf ) )
  {
    result->_data._integer = -ENODATA;
    result->_type = dbBE_REDIS_TYPE_INVALID;
    return -ENODATA;
  }

  int64_t available = dbBE_Redis_sr_buffer_unprocessed( sr_buf );

  char type;

  // the current type of response from Redis
  char *start_parse = dbBE_Redis_sr_buffer_get_processed_position( sr_buf );
  type = *start_parse;
  dbBE_Redis_sr_buffer_advance( sr_buf, 1 );

  // pointing to the new reading position to start parsing
  char *p = dbBE_Redis_sr_buffer_get_processed_position( sr_buf );
  size_t parsed = 0;
  int64_t len = 0;

  available = dbBE_Redis_sr_buffer_unprocessed( sr_buf );

  switch( type )
  {
    case '-': // parse an error
      // retrieve the string
      len = dbBE_Redis_nul_terminate_string( p, &parsed, available );
      if( len > 0 )
      {
        size_t op_strlen = 0;
        result->_data._string._data = p;
        result->_data._string._size = len;
        result->_type = dbBE_REDIS_TYPE_ERROR;

        // check for MOVED
        // check for ASK
        if( strncmp( p, "ASK ", DBBE_REDIS_REDIRECT_RESPONSE_LEN) == 0 )
        {
          op_strlen = DBBE_REDIS_REDIRECT_RESPONSE_LEN;
          result->_type = dbBE_REDIS_TYPE_REDIRECT;
        }
        else
          if( strncmp( p, "MOVED ", DBBE_REDIS_RELOCATE_RESPONSE_LEN) == 0 )
          {
            op_strlen = DBBE_REDIS_RELOCATE_RESPONSE_LEN;
            result->_type = dbBE_REDIS_TYPE_RELOCATE;
          }
          else
          {
            // return error if not redirected
            result->_type = dbBE_REDIS_TYPE_ERROR;
            break;
          }

        // only get here for redirect/relocate cases to fill the result data
        errno = 0;
        char *addr_str = NULL;
        result->_data._location._hash = strtol( p + op_strlen,
                                                &addr_str,
                                                10 );
        if(( errno == ERANGE ) || ( addr_str == NULL ) || ( *addr_str == '\0' ) || ( *addr_str != ' ' ) )
        {
          fprintf( stderr, "Failed to parse redirect/relocate response.\n" );
          result->_type = dbBE_REDIS_TYPE_INVALID;
          result->_data._location._address = NULL;
          result->_data._location._hash = -1;
          break;
        }
        result->_data._location._address = addr_str + 1;
      }
      else // string not terminated, try to receive more data
      {
        rc = -EAGAIN;
        break;
      }
      break;

    case '+': // parse a simple string
      len = dbBE_Redis_nul_terminate_string( p, &parsed, available );
      if( len >= 0 )
      {
        result->_data._string._data = p;
        result->_data._string._size = len;
        result->_type = dbBE_REDIS_TYPE_CHAR;
      }
      else if( len == -EAGAIN )
        rc = -EAGAIN;
      else
      {
        rc = -EBADMSG;
        result->_type = dbBE_REDIS_TYPE_INVALID;
      }
      break;

    case ':': // parse an integer
      result->_data._integer = dbBE_Redis_extract_integer( p, &parsed, available );
      if(( result->_data._integer == -EAGAIN ) && ( parsed == 0 ))
      {
        rc = -EAGAIN;
        result->_data._integer = DBBE_REDIS_NAN;
      }

      result->_type = dbBE_REDIS_TYPE_INT;
      break;

    case '$': // parse a bulk string
    {
      char *str = p;
      len = dbBE_Redis_extract_bulk_string( &str, &parsed, available );
      if( len >= 0 )
      {
        result->_data._string._data = str;
        result->_data._string._size = len;
        result->_type = dbBE_REDIS_TYPE_CHAR;
      }
      else if( len == -EAGAIN )
        rc = -EAGAIN;
      else
      {
        rc = -EBADMSG;
        result->_type = dbBE_REDIS_TYPE_INVALID;
      }

      break;
    }

    case '*': // parse an array by just skipping the array size and repeat the first
    {
      int n;
      int64_t tmp_len = dbBE_Redis_extract_integer( p, &parsed, available );
      result->_data._array._len = (int)tmp_len;
      if(( result->_data._array._len == -EAGAIN ) && ( parsed == 0 ))
      {
        rc = -EAGAIN;
        break;
      }

      if( tmp_len == DBBE_REDIS_NAN )
      {
	result->_type = dbBE_REDIS_TYPE_INT;
	result->_data._integer = -EPROTO;
	break;
      }
      result->_data._array._data = (dbBE_Redis_result_t*)malloc( sizeof (dbBE_Redis_result_t ) * result->_data._array._len );
      memset( result->_data._array._data, 0, sizeof (dbBE_Redis_result_t ) * result->_data._array._len );

      dbBE_Redis_sr_buffer_advance( sr_buf, parsed );

      rc = 0;
      for( n = 0; (n < result->_data._array._len) && ( rc == 0 ); ++n )
        rc = dbBE_Redis_parse_sr_buffer_check( sr_buf, &result->_data._array._data[ n ], 0 );
      if(( rc == -EAGAIN ) || ( rc == -ENODATA ))
      {
        result->_type = dbBE_REDIS_TYPE_ARRAY;
        dbBE_Redis_result_cleanup( result, 0 );
        parsed = 0;
        rc = -EAGAIN;
        break;
      }
      result->_type = dbBE_REDIS_TYPE_ARRAY;
      parsed = 0;
      break;
    }
    default:
      // this is a parsing error
      fprintf( stderr, "DataBroker library error: Redis response parser: Protocol error.\n" );
      result->_type = dbBE_REDIS_TYPE_INVALID;
      rc = -EPROTO;
  }

  if( rc == -EAGAIN )
    dbBE_Redis_sr_buffer_rewind_processed_to( sr_buf, start_parse );
  else
  {
    dbBE_Redis_sr_buffer_advance( sr_buf, parsed );
    // terminate any strings in the result structure, ONLY if this is the top-level call
    if( toplevel != 0 )
      rc = dbBE_Redis_result_terminate_strings( result );

  }
  return rc;
}

/*
 * parse the input buffer
 * return the Redis result including its type and its size
 */
int dbBE_Redis_parse_sr_buffer( dbBE_Redis_sr_buffer_t *sr_buf,
                                dbBE_Redis_result_t *result )
{
  return dbBE_Redis_parse_sr_buffer_check( sr_buf, result, 1 );
}




int dbBE_Redis_process_put( dbBE_Redis_request_t *request,
                            dbBE_Redis_result_t *result )
{
  int rc = 0;

  rc = dbBE_Redis_process_general( request, result );

  if( rc == 0 )
  {
    if( result->_data._integer != 1 )
    {
      result->_data._integer = -ENOMEM;
    }
  }
  // todo: process and generate any error cases since there's not much else to do for a put

  return rc;
}

int dbBE_Redis_process_get( dbBE_Redis_request_t *request,
                            dbBE_Redis_result_t *result,
                            dbBE_Data_transport_t *transport )
{
  int rc = 0;

  rc = dbBE_Redis_process_general( request, result );

  if( rc == 0 )
  {
    // todo: do any error case processing/checking before kicking off the transport

    // result stage of GET: do the transport
    if( request->_step->_result != 0 )
    {
      // signal that key is not available
      if( result->_data._string._data == NULL )
      {
        dbBE_Redis_result_cleanup( result, 0 );  // clean up and set int error code
        result->_type = dbBE_REDIS_TYPE_INT;
        result->_data._integer = -ENOENT;
        return -ENOENT;
      }

      int64_t transferred = transport->scatter( (dbBE_Data_transport_device_t*)result->_data._string._data,
                                                result->_data._string._size,
                                                request->_user->_sge_count,
                                                request->_user->_sge );
      if( transferred != result->_data._string._size )
      {
        rc = -EBADMSG;
        dbBE_Redis_result_cleanup( result, 0 );  // clean up and set int error code
        result->_type = dbBE_REDIS_TYPE_INT;
        result->_data._integer = rc;
      }
      else
      {
        dbBE_Redis_result_cleanup( result, 0 );  // clean up and set transferred size
        result->_type = dbBE_REDIS_TYPE_INT;
        result->_data._integer = transferred;
      }
    }
  }

  return rc;
}


int dbBE_Redis_process_directory( dbBE_Redis_request_t **in_out_request,
                                  dbBE_Redis_result_t *result,
                                  dbBE_Data_transport_t *transport,
                                  dbBE_Redis_s2r_queue_t *post_queue,
                                  dbBE_Redis_connection_mgr_t *conn_mgr )
{
  dbBE_Redis_request_t *request = *in_out_request;

  int rc = 0;
  rc = dbBE_Redis_process_general( request, result );

  switch( request->_step->_stage )
  {
    case DBBE_REDIS_DIRECTORY_STAGE_META:
      if( rc == 0 )
      {
        char* b = (char*)request->_user->_sge[0]._data;
        b[0] = '\0';
        if(( result->_type != dbBE_REDIS_TYPE_ARRAY ) || ( result->_data._array._len <= 0 ))
        {
          rc = -ENOENT;
          result->_data._integer = rc;
        }
        else
        {
          // allocate a memory area to count inflight scans to know when the request is complete
          request->_status.reference = dbBE_Refcounter_allocate();
          if( request->_status.reference == NULL )
          {
            return_error_clean_result( -ENOMEM, result );
            break;
          }
          dbBE_Redis_request_t *scan_list = dbBE_Redis_connection_mgr_request_each( conn_mgr, request );
          while( scan_list != NULL )
          {
            dbBE_Redis_request_t *scan = scan_list;
            scan_list = scan_list->_next;

            dbBE_Redis_request_stage_transition( scan );
            rc = dbBE_Redis_s2r_queue_push( post_queue, scan );
            if( rc != 0 )
            {
              // todo: clean up and complete only if no other scan request was started
              return_error_clean_result( rc, result );
              break;
            }
            dbBE_Refcounter_up( request->_status.reference );
          }
          result->_data._integer = 0;
          *in_out_request = NULL;
        }
      }
      break;
    case DBBE_REDIS_DIRECTORY_STAGE_SCAN:
      dbBE_Refcounter_down( request->_status.reference );  // decrease the inflight count
      if( conn_mgr == NULL )
      {
        return_error_clean_result( -EINVAL, result );
        break;
      }

      if(( result->_type != dbBE_REDIS_TYPE_ARRAY ) || ( result->_data._array._len != 2 ))
      {
        return_error_clean_result( -EINVAL, result );
        break;
      }

      // parse the result array and accumulate the keys
      dbBE_Redis_result_t *subresult = &result->_data._array._data[1];
      int n;
      for( n=0; n<subresult->_data._array._len; ++n )
      {
        char *key = strstr( subresult->_data._array._data[ n ]._data._string._data, DBBE_REDIS_NAMESPACE_SEPARATOR );
        if( key == NULL )
        {
          return_error_clean_result( -EBADE, result );
          return -EBADE;
        }
        key += DBBE_REDIS_NAMESPACE_SEPARATOR_LEN;

        // We only support single-SGE requests for now, the check for single-SGE is done in the init-phase of the request
        ssize_t current_len = strnlen((char*)request->_user->_sge[0]._data, request->_user->_sge[0]._size );
        if(current_len > 0 )
        {
          key = key-1;
          key[0] = '\n';
        }
        ssize_t remaining = request->_user->_sge[0]._size - current_len;
        char *startloc = (char*)(request->_user->_sge[0]._data) + current_len;
        snprintf( startloc, remaining, "%s", key ); // append to the key list
      }
      // if cursor is not "0", then create another match request
      subresult = &result->_data._array._data[0];
      if(( subresult->_data._string._size != 1 ) ||
          ( subresult->_data._string._data[0] != '0' ))
      {
        // it returned a valid cursor, so we have to send another scan request
        // todo: this is invalid behavior/hack... don't touch the user data
        // assign a user key because user key of user request is not use
        request->_user->_key = strdup( subresult->_data._string._data );
        // do not transition - this request needs to repeat, just with a new cursor
        dbBE_Redis_s2r_queue_push( post_queue, request );
        dbBE_Refcounter_up( request->_status.reference );

        *in_out_request = NULL;
      }
      else
      {
        // if there are other requests in flight, we can drop this one
        if( dbBE_Refcounter_get( request->_status.reference ) != 0 )
        {
          *in_out_request = NULL;
          dbBE_Redis_request_destroy( request );
        }
        else
        {
          // completed: time to clean up the allocated mem structures
          dbBE_Redis_request_stage_transition( request );
        }
      }


      break;
    default:
      rc = -EPROTO;
      result->_type = dbBE_REDIS_TYPE_INT;
      result->_data._integer = rc;
      break;
  }

  return rc;
}

int dbBE_Redis_process_nscreate( dbBE_Redis_request_t *request,
                                 dbBE_Redis_result_t *result )
{
  int rc = 0;

  rc = dbBE_Redis_process_general( request, result );

  switch( request->_step->_stage )
  {
    case 0: // stage HSETNX
      if( rc == 0 )
      {
        if( result->_data._integer == 0 )                 // error: already exists
        {
          rc = -EEXIST;
          result->_data._integer = rc;
          result->_type = dbBE_REDIS_TYPE_INT;
        }
      }
      break;
    case 1: // stage HMSET
      if( rc == 0 )
      {
        if( strncmp( result->_data._string._data, "OK", result->_data._string._size ) != 0 )  // error: no OK returned
        {
          rc = -ENOENT;
          result->_data._integer = rc;
          result->_type = dbBE_REDIS_TYPE_INT;
        }
      }
      break;
    default:
      rc = -EPROTO;
      result->_type = dbBE_REDIS_TYPE_INT;
      result->_data._integer = rc;
      break;
  }
  return rc;
}

/*
 * the nsquery processing will receive an array with all data from the name space hash
 * this data has to be put into a single string and then scattered out the user buffer
 */
int dbBE_Redis_process_nsquery( dbBE_Redis_request_t *request,
                                 dbBE_Redis_result_t *result,
                                 dbBE_Data_transport_t *transport )
{
  int rc = 0;
  if( result == NULL )
    return -EINVAL;

  rc = dbBE_Redis_process_general( request, result );

  if( rc == 0 )
  {
    if( result->_data._array._len < 6 )
    {
      rc = -ENOENT; // if we don't get at least id, refcount and groups entries + values, we have the wrong one...
      dbBE_Redis_result_cleanup( result, 0 );
      result->_type = dbBE_REDIS_TYPE_INT;
      result->_data._integer = rc;
    }
    else
    {
      int item_count = result->_data._array._len;
      dbBE_Redis_data_t item[ item_count ];
      size_t total_len = 0;
      int n;
      for( n = 0; n < item_count; ++n )
      {
        if(( result->_data._array._data[ n ]._type == dbBE_REDIS_TYPE_CHAR ) &&
            ( result->_data._array._data[ n ]._data._string._size >= 0))
          total_len += result->_data._array._data[ n ]._data._string._size + 1; // add 1 to do make room for the ":" added later
        else
        {
          rc = -EBADMSG;
          dbBE_Redis_result_cleanup( result, 0 );
          result->_type = dbBE_REDIS_TYPE_INT;
          result->_data._integer = rc;
          break; // no further processing - it's broken here...
        }
      }
      if( rc == 0 )
      {
        // allocate intermediate buffer to hold the collected items
        char *res_str = (char*)malloc( total_len + 1 );
        if( res_str == NULL )
        {
          rc = -EBADMSG;
          dbBE_Redis_result_cleanup( result, 0 );
          result->_type = dbBE_REDIS_TYPE_INT;
          result->_data._integer = rc;
          return rc;
        }

        // reset and fill the buffer
        memset( res_str, 0, total_len + 1 );
        for( n = 0; n < item_count; ++n )
        {
          dbBE_Redis_result_t *item = &result->_data._array._data[ n ];
          strcat( res_str, item->_data._string._data );
          strcat( res_str, ":" );
        }

        // invoke the transport to copy the data to the user buffer
        int64_t transferred = transport->scatter( (dbBE_Data_transport_device_t*)res_str, total_len,
                                                  request->_user->_sge_count, request->_user->_sge );
        if( transferred != (int64_t)total_len )
        {
          rc = -EBADMSG;
          dbBE_Redis_result_cleanup( result, 0 );
          result->_type = dbBE_REDIS_TYPE_INT;
          result->_data._integer = rc;
        }
        else
        {
          dbBE_Redis_result_cleanup( result, 0 );
          result->_type = dbBE_REDIS_TYPE_INT;
          result->_data._integer = transferred;
        }
      }
    }
  }

  return rc;
}

int dbBE_Redis_process_nsattach( dbBE_Redis_request_t *request,
                                 dbBE_Redis_result_t *result )
{
  int rc = 0;

  rc = dbBE_Redis_process_general( request, result );

  switch( request->_step->_stage )
  {
    case 0:
      if( rc == 0 )
      {
        if( result->_data._integer == 0 ) // if the return signals: not existent, return error
        {
          rc = -EEXIST;
          result->_data._integer = rc;
        }
      }
      break;
    case 1:
      if( rc == 0 )
      {
        if( result->_data._integer < 2 )
        {
          rc = -EOVERFLOW;
          result->_data._integer = rc;
        }
      }
      break;
    default:
      rc = -EPROTO;
      result->_data._integer = rc;
      break;
  }

  return rc;
}


int dbBE_Redis_process_nsdetach( dbBE_Redis_request_t *request,
                                 dbBE_Redis_result_t *result )
{
  int rc = 0;

  rc = dbBE_Redis_process_general( request, result );

  switch( request->_step->_stage )
  {
    case 0:
      if( rc == 0 )
      {
        if( result->_data._integer == 0 ) // if the return signals: not existent, return error
        {
          rc = -EEXIST;
          result->_data._integer = rc;
        }
      }
      break;
    case 1:
      if( rc == 0 )
      {
        if( result->_data._integer < 0 )
        {
          rc = -EOVERFLOW;
          result->_data._integer = rc;
        }
      }
      break;
    default:
      rc = -EPROTO;
      result->_data._integer = rc;
      break;
  }
  return rc;
}

int dbBE_Redis_process_nsdelete( dbBE_Redis_request_t **in_out_request,
                                 dbBE_Redis_result_t *result,
                                 dbBE_Redis_s2r_queue_t *post_queue,
                                 dbBE_Redis_connection_mgr_t *conn_mgr )
{
  dbBE_Redis_request_t *request = *in_out_request;

  int rc = 0;
  rc = dbBE_Redis_process_general( request, result );

  switch( request->_step->_stage )
  {
    case DBBE_REDIS_NSDELETE_STAGE_DETACH:
      if( rc == 0 )
      {
        if( result->_data._integer < 0 )
        {
          rc = -EOVERFLOW;
          result->_data._integer = rc;
        }
        else
        {
          // allocate a memory area to count inflight deletes and scans to know when the request is complete
          request->_status.reference = dbBE_Refcounter_allocate();
          if( request->_status.reference == NULL )
          {
            return_error_clean_result( -ENOMEM, result );
            break;
          }
          dbBE_Redis_request_t *scan_list = dbBE_Redis_connection_mgr_request_each( conn_mgr, request );
          while( scan_list != NULL )
          {
            dbBE_Redis_request_t *scan = scan_list;
            scan_list = scan_list->_next;

            dbBE_Redis_request_stage_transition( scan );
            rc = dbBE_Redis_s2r_queue_push( post_queue, scan );
            if( rc != 0 )
            {
              // todo: clean up and complete only if no other scan request was started
              return_error_clean_result( rc, result );
              break;
            }
            dbBE_Refcounter_up( request->_status.reference );
          }
          result->_data._integer = 0;

          // TODO: corner case issue: if scan_list is empty (no connections), then we need to create completion instead of = NULL
          // TODO: check: is this a memleak, shouldn't we delete the inbound request here?
          *in_out_request = NULL;
        }
      }
      break;

      //
    case DBBE_REDIS_NSDELETE_STAGE_SCAN:
      dbBE_Refcounter_down( request->_status.reference );  // decrease the inflight count
      if( conn_mgr == NULL )
      {
        return_error_clean_result( -EINVAL, result );
        break;
      }

      if(( result->_type != dbBE_REDIS_TYPE_ARRAY ) || ( result->_data._array._len != 2 ))
      {
        return_error_clean_result( -EINVAL, result );
        break;
      }

      // todo: must not touch the user->key entry regardless of it being NULL for deletes or not
      if( request->_user->_key != NULL )
      {
        free( request->_user->_key );
        request->_user->_key = NULL;
      }

      // parse the result array and create key delete requests
      dbBE_Redis_result_t *subresult = &result->_data._array._data[1];
      int n;
      for( n=0; n<subresult->_data._array._len; ++n )
      {
        // create a new copy of the user request with the cursor-scanned data as the key
        dbBE_Request_t *delrequest = (dbBE_Request_t*)malloc( sizeof( dbBE_Request_t ) + request->_user->_sge_count * sizeof(dbBE_sge_t) );
        if( !delrequest )
          continue;
        memcpy( delrequest, request->_user, sizeof( dbBE_Request_t ) + request->_user->_sge_count * sizeof(dbBE_sge_t) );
        delrequest->_key = strdup( subresult->_data._array._data[ n ]._data._string._data );

        // place that user request into the deletion
        dbBE_Redis_request_t *delkey = dbBE_Redis_request_allocate( delrequest );
        if( ! delkey )
          continue;
        delkey->_conn_index = request->_conn_index;
        delkey->_next = request->_next;
        delkey->_step = request->_step;
        delkey->_status = request->_status;

        dbBE_Redis_request_stage_transition( delkey );
        rc = dbBE_Redis_s2r_queue_push( post_queue, delkey );
        if( rc != 0 )
          continue;
        dbBE_Refcounter_up( request->_status.reference );
      }
      // if cursor is not "0", then create another match request
      subresult = &result->_data._array._data[0];
      if(( subresult->_data._string._size != 1 ) ||
          ( subresult->_data._string._data[0] != '0' ))
      {
        // it returned a valid cursor, so we have to send another scan request
        // todo: this is invalid behavior/hack... don't touch the user data
        // assign a user key because user key of user request is not use
        request->_user->_key = strdup( subresult->_data._string._data );
        // do not transition - this request needs to repeat, just with a new cursor
        dbBE_Redis_s2r_queue_push( post_queue, request );
        dbBE_Refcounter_up( request->_status.reference );

        *in_out_request = NULL;
      }
      else
      {
        // if there are other requests in flight, we can drop this one
        if( dbBE_Refcounter_get( request->_status.reference ) != 0 )
        {
          *in_out_request = NULL;
          dbBE_Redis_request_destroy( request );
        }
        else
        {
          // completed: time to clean up the allocated mem structures
          // transition 2x to get to DELNS stage (second time done by caller)
          dbBE_Redis_request_stage_transition( request );
        }
      }

      break;

    case DBBE_REDIS_NSDELETE_STAGE_DELKEYS:
    {
      uint64_t ref = dbBE_Refcounter_down( request->_status.reference );
      if( rc == 0 )
      {
        if( result->_data._integer != 1 )
          rc = -ENOENT;
        result->_data._integer = rc; // set the result/return code for upper layers
      }

      // if there are other requests in flight, we can drop this one
      if( ref != 0 )
      {
        *in_out_request = NULL;
        dbBE_Redis_request_destroy( request );
      }
      break;
    }

    case DBBE_REDIS_NSDELETE_STAGE_DELNS:
    {
      uint64_t ref = dbBE_Refcounter_get( request->_status.reference );
      if( ref != 0 )
        // this is a bug: we must not reach this state with a refcount>0
        return -EFAULT;

      dbBE_Refcounter_destroy( request->_status.reference );
      request->_status.reference = NULL;

      if( rc == 0 )
      {
        if( result->_data._integer != 1 )
        {
          rc = -ENOENT;
          result->_data._integer = rc;
        }
        result->_data._integer = rc; // set the result/return code for upper layers

      }
      break;
    }
    default:
      break;
  }

  return rc;
}
