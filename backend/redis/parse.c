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
#include "result.h"
#include "parse.h"
#include "connection.h"


// length of the ASK response including the trailing space
#define DBBE_REDIS_REDIRECT_RESPONSE_LEN ( 4 )

// length of the MOVED response including the trailing space
#define DBBE_REDIS_RELOCATE_RESPONSE_LEN ( 6 )

static inline
int return_error_clean_result( int rc, dbBE_Redis_result_t *result )
{
  dbBE_Redis_result_cleanup( result, 0 );
  result->_type = dbBE_REDIS_TYPE_INT;
  result->_data._integer = rc;
  return rc;
}

char* gScrapSpace = NULL;
#define DBBE_REDIS_SCRAP_SPACE_LEN ( 512ull * 1024ull * 1024ull )

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
      LOG( DBG_VERBOSE, stdout, "Found Terminator @%"PRId64"\n", pos );
      return p;
    }
    ++pos;
    ++p;
  }
  return NULL;
}

int64_t dbBE_Redis_extract_bulk_string( char **p, size_t *parsed, const int64_t limit, size_t *actual_size )
{
  if(( p == NULL ) || ( *p == NULL ) || ( parsed == NULL ) || ( limit <= 0 ))
  {
    errno = EINVAL;
    return -EINVAL;
  }

  size_t processed = 0;
  int64_t exp_len = dbBE_Redis_extract_integer( *p, &processed, limit );

  // check for incomplete data
  if(( exp_len == -EAGAIN ) && ( processed == 0 ))
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

  LOG( DBG_TRACE, stderr, "Parsing p/string length: %ld; avail: %ld\n", exp_len, limit );

  char *string = ( *p + processed );

  // available data is less than expected length plus termination: includes string-head-only case
  if(( exp_len >= limit - (int64_t)processed ))
  {
    // caller not interested in partial strings or we're just missing the terminator: EAGAIN
    if(( actual_size == NULL ) || ( exp_len == limit - (int64_t)processed ))
    {
      *parsed = 0;
      return -EAGAIN;
    }

    *parsed = processed;
    *p = string;
    *actual_size = exp_len;
    return -EOVERFLOW;
  }

  // check for incomplete data with only the \n missing
  if( ( string[exp_len] == '\r' ) && ( exp_len + 1 == limit - (int64_t)processed ) )
  {
    LOG( DBG_VERBOSE, stderr, "String just missing the \\n part of terminator\n" );
    *parsed = 0;
    return -EAGAIN;
  }

  // check the terminator
  if(( string[exp_len] != '\r' ) || ( string[exp_len+1] != '\n' ))
  {
    LOG( DBG_ERR, stderr, "Bulk String Terminator not in expected place: exp=%"PRId64"; lim=%"PRId64"; prcssd=%zd; %x %x|%x %x %x\n",
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

  if( dbBE_Transport_sr_buffer_empty( sr_buf ) )
  {
    result->_data._integer = -ENODATA;
    result->_type = dbBE_REDIS_TYPE_INVALID;
    return -ENODATA;
  }

  int64_t available = dbBE_Transport_sr_buffer_unprocessed( sr_buf );

  char type;

  // the current type of response from Redis
  char *start_parse = dbBE_Transport_sr_buffer_get_processed_position( sr_buf );
  type = *start_parse;
  dbBE_Transport_sr_buffer_advance( sr_buf, 1 );

  // pointing to the new reading position to start parsing
  char *p = dbBE_Transport_sr_buffer_get_processed_position( sr_buf );
  size_t parsed = 0;
  int64_t len = 0;

  available = dbBE_Transport_sr_buffer_unprocessed( sr_buf );

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
      size_t actual = 0;
      len = dbBE_Redis_extract_bulk_string( &str, &parsed, available, &actual );
      if( len >= 0 )
      {
        result->_data._string._data = str;
        result->_data._string._size = len;
        result->_type = dbBE_REDIS_TYPE_CHAR;
      }
      else
      {
        switch( len )
        {
          case -EAGAIN:
            rc = -EAGAIN;
            break;
          case -EOVERFLOW:
            // prevent partial strings in arrays
            if( toplevel == 0 )
            {
              rc = -EAGAIN;
              break;
            }
            // partial string received:
            result->_type = dbBE_REDIS_TYPE_STRING_PART;
            result->_data._pstring._total_size = actual;
            result->_data._pstring._data = str;
            result->_data._pstring._size = available - parsed;
            parsed = available; // make sure the remaining bytes of the string are considered as parsed
            rc = 0;
            break;
          default:
            rc = -EBADMSG;
            result->_type = dbBE_REDIS_TYPE_INVALID;
        }
      }
      LOG( DBG_TRACE, stderr, "Bulkstring extraction: type=%d\n", result->_type );
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

      dbBE_Transport_sr_buffer_advance( sr_buf, parsed );

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
      fprintf( stderr, "DataBroker library error: Redis response parser: Protocol error. t=%c\n", type );
      result->_type = dbBE_REDIS_TYPE_INVALID;
      rc = -EPROTO;
  }

  if( rc == -EAGAIN )
    dbBE_Transport_sr_buffer_rewind_processed_to( sr_buf, start_parse );
  else
  {
    dbBE_Transport_sr_buffer_advance( sr_buf, parsed );
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

/*
 * assembling a recv-SGE for the remaining data and copy( scatter )  available data
 * into the user SGE.
 *  - find the correct SGE index and offset to start the receive
 *  - make sure to not receive more data than the pstring length
 *  - append a 2byte SGE for the \r\n terminator
 */
dbBE_Transport_sge_buffer_t* dbBE_Redis_parse_copy_assemble_sge( dbBE_Request_t *r,
                                                             dbBE_Redis_result_t *c,
                                                             dbBE_Transport_sge_buffer_t *sge_buf,
                                                             dbBE_Transport_dbuffer_t *dbuf,
                                                             dbBE_sge_t overflow )
{
  size_t c_off = c->_data._pstring._size;

  dbBE_Transport_sge_buffer_reset( sge_buf );
  dbBE_sge_t *sge = sge_buf->_cmd;

  int r_idx = 0; // new receive-SGE index
  int u_idx = 0; // user-SGE index

  char *value = c->_data._pstring._data;
  size_t remaining = c->_data._pstring._total_size; // remaining available data
  size_t pos = 0;
  ssize_t usize = remaining - dbBE_SGE_get_len( r->_sge, r->_sge_count );  // how much data cannot fit into user buffer

  errno = 0;
  while((( c_off > 0 ) || ( remaining > 0 )) && ( u_idx < r->_sge_count ))
  {
    if( c_off > 0 )
    {
      // if the remaining c_offset data fits into the next SGE:
      if( r->_sge[ u_idx ].iov_len >= c_off )
      {
        // copying is done here to be able to re-use the sr_buffer (transport would be the logical place
        memcpy( r->_sge[ u_idx ].iov_base, &value[ pos ], c_off );
        pos += c_off;
        remaining -= c_off;

        // we found the SGE where to start the receive; set starting point and remaining length
        sge[ r_idx ].iov_base = (void*)((char*)r->_sge[ u_idx ].iov_base + c_off );
        sge[ r_idx ].iov_len = r->_sge[ u_idx ].iov_len - c_off;
        if( sge[ r_idx ].iov_len > remaining ) // is this the last sge we could fill with remaining data?
        {
          sge[ r_idx ].iov_len = remaining;
          remaining = 0;
        }
        ++r_idx;
        ++u_idx;
        c_off = 0;
        continue;
      }
      else
      {
        // fill the user SGE with the available data
        size_t copylen = ( remaining < r->_sge[ u_idx ].iov_len ) ? remaining : r->_sge[ u_idx ].iov_len;
        memcpy( r->_sge[ u_idx ].iov_base, &value[ pos ], copylen );
        pos += copylen;
        c_off -= copylen;
        remaining -= copylen;

        // nothing to create in the new recv-SGE yet because:
        // if remaining == 0, we're done; if remaining != 0, we have a full user SGE
        ++u_idx;
        continue;
      }
    }

    // only get here if the available data has been (copied and) processed
    // so, this is only for the remaining unreceived data to fit/fill in
    size_t sge_len = ( remaining < r->_sge[ u_idx ].iov_len ) ? remaining : r->_sge[ u_idx ].iov_len;
    sge[ r_idx ].iov_base = r->_sge[ u_idx ].iov_base;
    sge[ r_idx ].iov_len = sge_len;
    ++r_idx;
    remaining -= sge_len;
    if( ++u_idx > r->_sge_count )
    {
      // user SGE too short/buffer too small
      errno = E2BIG;
      break;
    }
  }

  // at this point, we can be sure that the sr-buf is fully processed
  // or remaining data won't fit into user buffer so it can be dropped
  // (we got into this function because the space was limited anyway)
  dbBE_Redis_sr_buffer_t *sr_buf = dbBE_Transport_dbuffer_get_active( dbuf );
  dbBE_Transport_sr_buffer_reset( sr_buf );

  // to determine the amount of remaining bytes to receive, remove the remaining
  // uncopied data, it can be dropped because it is too much for the recv anyway
  usize -= c_off;
  c_off = 0;

  // if the remaining expected data is too big for the user buffer, we need to point to the overflow buffer
  if( usize > (ssize_t)dbBE_Transport_sr_buffer_get_size( sr_buf ) - 2 ) // -2 because of termination
  {
    if( overflow.iov_len < usize ) // does the data fit into the provided overflow buffer?
    {
      errno = E2BIG;
      return NULL;
    }
    sge[ r_idx ].iov_base = overflow.iov_base;
    sge[ r_idx ].iov_len = usize + 2;
    ++r_idx;
    ++sge_buf->_index;
  }

  // append another element to allow space for the redis protocol terminator
  // to be received at the end of the receive buffer
  // also allow subsequent available data to be received to make sure the
  // connection buffer gets drained
  sge_buf->_index = r_idx + 1;
  sge[ r_idx ].iov_base = dbBE_Transport_sr_buffer_get_available_position( sr_buf );
  sge[ r_idx ].iov_len = dbBE_Transport_sr_buffer_get_size( sr_buf ) - (dbBE_Transport_sr_buffer_get_size( sr_buf ) >> 3); // 7/8th of buffer for new data

  return sge_buf;
}

int dbBE_Redis_process_get( dbBE_Redis_request_t *request,
                            dbBE_Redis_result_t *result,
                            dbBE_Data_transport_t *transport,
                            dbBE_Redis_connection_t *connection )
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
      if( ( result->_type == dbBE_REDIS_TYPE_CHAR ) && ( result->_data._string._data == NULL ))
      {
        dbBE_Redis_result_cleanup( result, 0 );  // clean up and set int error code
        result->_type = dbBE_REDIS_TYPE_INT;
        result->_data._integer = -ENOENT;
        if( request->_user->_flags & DBBE_OPCODE_FLAGS_IMMEDIATE )
          return -ENOENT;
        else
          return -EAGAIN;
      }

      int64_t transferred = 0;
      int64_t data_len = 0;
      if( result->_type == dbBE_REDIS_TYPE_STRING_PART )
      {
        LOG( DBG_TRACE, stderr, "PARTIAL STRING: %ld/%ld\n", result->_data._pstring._size, result->_data._pstring._total_size );

        // prepare sge for another receive call
        if( gScrapSpace == NULL )
          gScrapSpace = (char*)malloc( DBBE_REDIS_SCRAP_SPACE_LEN );

        dbBE_sge_t overflow;
        overflow.iov_base = gScrapSpace;
        overflow.iov_len = DBBE_REDIS_SCRAP_SPACE_LEN;
        dbBE_Transport_sge_buffer_t *sge_buf = dbBE_Redis_parse_copy_assemble_sge( request->_user, result, &transport->_rSGE, connection->_recvbuf, overflow );
        dbBE_Redis_sr_buffer_t *sr_buf = dbBE_Transport_dbuffer_get_active( connection->_recvbuf );

        if( sge_buf != NULL )
        {
          dbBE_sge_t pstring;
          pstring.iov_base = result->_data._pstring._data;
          pstring.iov_len = result->_data._pstring._size;
          data_len = result->_data._pstring._total_size;
          transferred = transport->scatter( (dbBE_Data_transport_endpoint_t*)connection,
                                            dbBE_Redis_connection_recv_sge_w,
                                            &pstring,
                                            result->_data._pstring._total_size + 2, // terminator
                                            sge_buf->_index,
                                            sge_buf->_cmd );
        }
        else
        {
          transferred = -result->_data._pstring._total_size;
          rc = -ENOMEM;
        }
        // adjust for the redis protocol terminator
        if( transferred > 0 )
        {
          transferred -= 2; // remove the terminator from the amount of transferred data
          dbBE_Transport_sr_buffer_add_data( sr_buf, 2, 0 ); // we've received data ...
          dbBE_Transport_sr_buffer_advance( sr_buf, 2 ); // that's already processed
          transferred += result->_data._pstring._size; // add what's been already received
        }
        // there's another response in the recv buffer after the termination
        if( transferred > data_len )
        {
          int64_t remain = transferred - data_len;
          transferred = data_len;
          dbBE_Transport_sr_buffer_add_data( sr_buf, remain, 0 );
        }
      }
      else
      {
        dbBE_sge_t pstring;
        pstring.iov_base = result->_data._string._data;
        pstring.iov_len = result->_data._string._size;
        data_len = result->_data._string._size;
        transferred = transport->scatter( (dbBE_Data_transport_endpoint_t*)NULL,
                                          NULL,
                                          &pstring,
                                          result->_data._string._size,
                                          request->_user->_sge_count,
                                          request->_user->_sge );

        // nothing to do because for completely received data, we've already parsed/processed the sr_buffer
      }
      // reset/clean the recv and cmd buffers
      dbBE_Transport_sge_buffer_reset( &transport->_rSGE );

      if( transferred == -EAGAIN )
        return -EAGAIN;
      if( (transferred >= 0 ) && (
             ( transferred == data_len ) || (
                 ( (request->_user->_flags & DBBE_OPCODE_FLAGS_PARTIAL) != 0 ) && ( transferred < data_len )
             )
          )
        )
      {
        dbBE_Redis_result_cleanup( result, 0 );  // clean up and set transferred size
        result->_type = dbBE_REDIS_TYPE_INT;
        result->_data._integer = data_len;
      }
      else
      {
        rc = -EBADMSG;
        dbBE_Redis_result_cleanup( result, 0 );  // clean up and set int error code
        result->_type = dbBE_REDIS_TYPE_INT;
        result->_data._integer = rc;
      }
    }
  }

  return rc;
}


int dbBE_Redis_process_remove( dbBE_Redis_request_t *request,
                               dbBE_Redis_result_t *result )
{
  int rc = 0;
  rc = dbBE_Redis_process_general( request, result );
  if( rc == 0 )
  {
    switch( result->_data._integer )
    {
      case 0:
        dbBE_Redis_result_cleanup( result, 0 );
        result->_type = dbBE_REDIS_TYPE_INT;
        result->_data._integer = -ENOENT;
        rc = -ENOENT;
        break;

      case 1:
        rc = 0;
        break;

      default:
        LOG( DBG_ERR, stderr, "Remove found duplicate entries for %s\n", request->_user->_key );
        break;
    }
  }
  return rc;
}

int dbBE_Redis_process_move( dbBE_Redis_request_t *request,
                             dbBE_Redis_result_t *result )
{
  int rc = 0;
  rc = dbBE_Redis_process_general( request, result );

  switch( request->_step->_stage )
  {
    case DBBE_REDIS_MOVE_STAGE_DUMP:
      if( rc == 0 )
      {
        if( result->_data._string._data == NULL )
        {
          rc = return_error_clean_result( -ENOENT, result );
          break;
        }

        // create a mem-region to store the dumped data
        char *buf = (char*)calloc( result->_data._string._size + 8, sizeof( char ) );
        if( buf == NULL )
        {
          rc = return_error_clean_result( -ENOMEM, result );
          break;
        }

        memcpy( buf, result->_data._string._data, result->_data._string._size );
        request->_status.move.dumped_value = buf;
        request->_status.move.len = result->_data._string._size;
      }
      break;

    case DBBE_REDIS_MOVE_STAGE_RESTORE:
      if( request->_status.move.dumped_value != NULL )
        free( request->_status.move.dumped_value );
      request->_status.move.dumped_value = NULL;
      request->_status.move.len = 0;
      if( rc != 0 )
      {
        if(( result->_type == dbBE_REDIS_TYPE_ERROR ) &&
            ( result->_data._string._data != NULL ) &&
            ( strstr( result->_data._string._data, "BUSYKEY" ) != NULL ) )
        {
          rc = return_error_clean_result( -EEXIST, result );
          break;
        }
      }
      break;

    case DBBE_REDIS_MOVE_STAGE_DEL:
      if( rc == 0 )
        switch( result->_data._integer )
        {
          case 0:
            // the key had been removed in the meantime (data duplication?)
            rc = return_error_clean_result( -ENOENT, result );
            break;
          case 1:
            break;
          default:
            // todo: there have been modifications to the key-data!!! Potential data loss?
            rc = return_error_clean_result( -ESTALE, result );
            break;
        }
      break;

    default:
      LOG( DBG_ERR, stderr, "Invalid request stage (%d) while processing move cmd.\n", (int)request->_step->_stage );
      rc = return_error_clean_result( rc, result );
      break;
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
        char* b = (char*)request->_user->_sge[0].iov_base;
        b[0] = '\0';
        if(( result->_type != dbBE_REDIS_TYPE_ARRAY ) || ( result->_data._array._len <= 0 ))
        {
          rc = return_error_clean_result( -ENOENT, result );
          break;
        }
        else
        {
          // allocate a memory area to count inflight scans to know when the request is complete
          request->_status.directory.reference = dbBE_Refcounter_allocate();
          request->_status.directory.keycount = dbBE_Refcounter_allocate();
          if(( request->_status.directory.reference == NULL ) || ( request->_status.directory.keycount == NULL ))
          {
            rc = return_error_clean_result( -ENOMEM, result );
            break;
          }
          dbBE_Redis_request_t *scan_list = dbBE_Redis_connection_mgr_request_each( conn_mgr, request );
          while( scan_list != NULL )
          {
            dbBE_Redis_request_t *scan = scan_list;
            scan_list = scan_list->_next;

            scan->_status.directory.scankey = strdup("0");
            dbBE_Redis_request_stage_transition( scan );
            rc = dbBE_Redis_s2r_queue_push( post_queue, scan );
            if( rc != 0 )
            {
              // todo: clean up and complete only if no other scan request was started
              rc = return_error_clean_result( rc, result );
              break;
            }
            dbBE_Refcounter_up( request->_status.directory.reference );
          }
          result->_data._integer = 0;
          // if we created scan requests, we can safely delete the BE request because the state is carried within the new requests
          // actually have to delete it request because it would be a memleak otherwise
          if( dbBE_Refcounter_get( request->_status.directory.reference ) > 0 )
            dbBE_Redis_request_destroy( *in_out_request );
          *in_out_request = NULL;
        }
      }
      break;
    case DBBE_REDIS_DIRECTORY_STAGE_SCAN:
      dbBE_Refcounter_down( request->_status.directory.reference );  // decrease the inflight count
      if( conn_mgr == NULL )
      {
        rc = return_error_clean_result( -EINVAL, result );
        break;
      }

      if(( result->_type != dbBE_REDIS_TYPE_ARRAY ) || ( result->_data._array._len != 2 ))
      {
        rc = return_error_clean_result( -EINVAL, result );
        break;
      }

      int completed = 0;

      // parse the result array and accumulate the keys
      dbBE_Redis_result_t *subresult = &result->_data._array._data[1];
      int n;
      for( n=0; n<subresult->_data._array._len; ++n )
      {
        if( subresult->_data._array._data[ n ]._data._string._data == NULL )
          continue;
        char *key = strstr( subresult->_data._array._data[ n ]._data._string._data, DBBE_REDIS_NAMESPACE_SEPARATOR );
        if( key == NULL )
        {
          LOG( DBG_ERR, stderr, "no separator in this key, So it's not a proper DBR Key\n" );
          return return_error_clean_result( -EILSEQ, result );
        }
        key += DBBE_REDIS_NAMESPACE_SEPARATOR_LEN;

        // We only support single-SGE requests for now, the check for single-SGE is done in the init-phase of the request
        ssize_t current_len = strnlen((char*)request->_user->_sge[0].iov_base, request->_user->_sge[0].iov_len );
        if(current_len > 0 )
        {
          key = key-1;
          key[0] = '\n';
        }
        // ignore uplicate key returned by SCAN
#ifdef DBR_PREVENT_KEYDUPES
        if( strstr( (char*)request->_user->_sge[0].iov_base, key ) != NULL )
          continue;
#endif
//         are we finished by hitting the user-requested limit?
        completed = ( dbBE_Refcounter_get( request->_status.directory.keycount ) >= request->_user->_sge[1].iov_len );

        if( ! completed )
        {
          ssize_t remaining = request->_user->_sge[0].iov_len - current_len;
          char *startloc = (char*)(request->_user->_sge[0].iov_base) + current_len;
          snprintf( startloc, remaining, "%s", key ); // append to the key list
          dbBE_Refcounter_up( request->_status.directory.keycount );
        }
        else
          break;
      }
      // if cursor is not "0", then create another match request
      subresult = &result->_data._array._data[0];
      completed |= (( subresult->_data._string._data[0] == '0' ) && ( subresult->_data._string._size == 1 ));
      if( ! completed )
      {
        // it returned a valid cursor, so we have to send another scan request
        // todo: this is invalid behavior/hack... don't touch the user data
        // assign a user key because user key of user request is not use
        free( request->_status.directory.scankey );
        request->_status.directory.scankey = strdup( subresult->_data._string._data );
        // do not transition - this request needs to repeat, just with a new cursor
        dbBE_Redis_s2r_queue_push( post_queue, request );
        dbBE_Refcounter_up( request->_status.directory.reference );

        *in_out_request = NULL;
      }
      else
      {
        free( request->_status.directory.scankey );
        // if there are other requests in flight, we can drop this one
        if( dbBE_Refcounter_get( request->_status.directory.reference ) > 0 )
        {
          dbBE_Redis_request_destroy( request );
          *in_out_request = NULL;
        }
        else
        {
          // completed: time to clean up the allocated mem structures
          dbBE_Redis_result_cleanup( result, 0 );  // clean up and set transferred size
          dbBE_Refcounter_destroy( request->_status.directory.reference );
          dbBE_Refcounter_destroy( request->_status.directory.keycount );
          request->_status.directory.reference = NULL;
          request->_status.directory.keycount = NULL;
          result->_type = dbBE_REDIS_TYPE_INT;
          result->_data._integer = strnlen( (char*)request->_user->_sge[0].iov_base, request->_user->_sge[0].iov_len );
          rc = 0;
        }
      }

      break;
    default:
      rc = return_error_clean_result( -EPROTO, result );
      break;
  }

  return rc;
}

int dbBE_Redis_process_nshandling( dbBE_Redis_namespace_list_t **s,
                                   dbBE_Redis_request_t *request,
                                   dbBE_Redis_result_t *result,
                                   int rc )
{
  if(( rc != 0 ) || (( request != NULL ) && ( request->_step->_final == 0 )))
    return rc;

  dbBE_Redis_namespace_t *ns = NULL;
  switch( request->_user->_opcode )
  {
    case DBBE_OPCODE_NSCREATE:
    {
      ns = dbBE_Redis_namespace_create( request->_user->_key );
      if( ns == NULL )
        rc = return_error_clean_result( -errno, result );

      dbBE_Redis_namespace_list_t *tmp = dbBE_Redis_namespace_list_insert( *s, ns );
      if( tmp == NULL )
      {
        rc = return_error_clean_result( -errno, result );
        dbBE_Redis_namespace_destroy( ns ); // clean up
      }
      else
      {
        *s = tmp;
        dbBE_Redis_result_cleanup( result, 0 );
        result->_type = dbBE_REDIS_TYPE_INT;
        result->_data._integer = (uint64_t)ns;
      }
      break;
    }
    case DBBE_OPCODE_NSATTACH:
    {
      dbBE_Redis_namespace_list_t *tmp = dbBE_Redis_namespace_list_get( *s, request->_user->_key );
      if( tmp == NULL )
      {
        ns = dbBE_Redis_namespace_create( request->_user->_key );
        tmp = dbBE_Redis_namespace_list_insert( *s, ns );
        if( tmp == NULL )
        {
          dbBE_Redis_namespace_destroy( ns );
          rc = return_error_clean_result( -errno, result );
          break;
        }
        *s = tmp;
      }
      else // in case it was already there:
      {
        ns = tmp->_ns;
        if( ns == NULL )
        {
          LOG( DBG_ERR, stderr, "Fatal: found namespace with NULL entry when looking for %s.\n", request->_user->_key );
          rc = return_error_clean_result( -EINVAL, result );
          break;
        }
        uint32_t refcnt = dbBE_Redis_namespace_get_refcnt( ns );
        if( dbBE_Redis_namespace_attach( ns ) != (int)refcnt + 1 )
        {
          LOG( DBG_ERR, stderr, "Fatal: inconsistent reference count after attach for %s.\n", request->_user->_key );
          rc = return_error_clean_result( -EPROTO, result );
          break;
        }
        rc = 0;
        *s = tmp;
      }
      dbBE_Redis_result_cleanup( result, 0 );
      result->_type = dbBE_REDIS_TYPE_INT;
      result->_data._integer = (uint64_t)ns;
    }
    break;
    default:
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
          rc = return_error_clean_result( -EEXIST, result );
      }
      break;
    case 1: // stage HMSET
      if( rc == 0 )
      {
        if( strncmp( result->_data._string._data, "OK", result->_data._string._size ) != 0 )  // error: no OK returned
          rc = return_error_clean_result( -ENOENT, result );
        else
        {
          dbBE_Redis_result_cleanup( result, 0 );
          result->_type = dbBE_REDIS_TYPE_INT;
          result->_data._integer = 0;
        }
      }
      break;
    default:
      rc = return_error_clean_result( -EPROTO, result );
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
    if( result->_data._array._len < 8 )
    {
      rc = -ENOENT; // if we don't get at least id, refcount and groups entries + values, we have the wrong one...
      dbBE_Redis_result_cleanup( result, 0 );
      result->_type = dbBE_REDIS_TYPE_INT;
      result->_data._integer = rc;
    }
    else
    {
      int item_count = result->_data._array._len;
      size_t total_len = 0;
      int n;
      for( n = 0; n < item_count; ++n )
      {
        if(( result->_data._array._data[ n ]._type == dbBE_REDIS_TYPE_CHAR ) &&
            ( result->_data._array._data[ n ]._data._string._size >= 0))
          total_len += result->_data._array._data[ n ]._data._string._size + 1; // add 1 to do make room for the ":" added later
        else
        {
          rc = return_error_clean_result( -EBADMSG, result );
          break; // no further processing - it's broken here...
        }
      }
      if( rc == 0 )
      {
        // allocate intermediate buffer to hold the collected items
        char *res_str = (char*)malloc( total_len + 1 );
        if( res_str == NULL )
          return return_error_clean_result( -EBADMSG, result );

        // reset and fill the buffer
        memset( res_str, 0, total_len + 1 );
        for( n = 0; n < item_count; ++n )
        {
          dbBE_Redis_result_t *item = &result->_data._array._data[ n ];
          strcat( res_str, item->_data._string._data );
          strcat( res_str, ":" );
        }

        // invoke the transport to copy the data to the user buffer (from partial string, because we already have received it regardless of transport
        dbBE_sge_t pstring;
        pstring.iov_base = res_str;
        pstring.iov_len = total_len;
        int64_t transferred = transport->scatter( (dbBE_Data_transport_endpoint_t*)NULL,
                                                  NULL,
                                                  &pstring,
                                                  total_len,
                                                  request->_user->_sge_count, request->_user->_sge );
        free( res_str );
        if( transferred != (int64_t)total_len )
          rc = return_error_clean_result( -EBADMSG, result );
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
          rc = return_error_clean_result( -ENOENT, result );
      }
      break;
    case 1:
      if( rc == 0 )
      {
        if( result->_data._integer < 1 )
          rc = return_error_clean_result( -EOVERFLOW, result );
        else
        {
          dbBE_Redis_result_cleanup( result, 0 );
          result->_type = dbBE_REDIS_TYPE_INT;
          result->_data._integer = 0;
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

static
int dbBE_Redis_process_nsdetach_check_delete( dbBE_Redis_result_t *hincrby, dbBE_Redis_result_t *hmget )
{
  int n;
  int to_delete = 0x3; // 2 bits to make sure we only delete if refcnt==0 and marked deleted
  for( n=0; ( n<hmget->_data._array._len ) && ( to_delete == 0x3 ); ++n )
  {
    dbBE_Redis_result_t *nres = &hmget->_data._array._data[n];
    char *value = NULL;
    if( nres->_type == dbBE_REDIS_TYPE_CHAR )
      value = nres->_data._string._data;
    else
      return -EBADMSG;

#define DBBE_REDIS_META_DELETE_FLAG ( 0x1 )

#define DBBE_REDIS_DETACH_REFCNT_MASK ( 0x1 )
#define DBBE_REDIS_DETACH_DELETE_MASK ( 0x2 )

    switch( n )
    {
      case 0: // refcnt field
      {
        int refcnt = strtol( value, NULL, 10 );
        int refcnt_inc = hincrby->_data._integer;
        if(( refcnt > 0 ) || ( refcnt_inc > 0 ))
          to_delete &= ~DBBE_REDIS_DETACH_REFCNT_MASK; // unset bit 0 because we cannot delete
        else
          LOG( DBG_VERBOSE, stdout, "RefCnt hit 0. Ready to delete if marked accordingly\n" );
        break;
      }
      case 1: // flags field
      {
        int flags = strtol( value, NULL, 10 );
        if( ! ( flags & DBBE_REDIS_META_DELETE_FLAG ))
          to_delete &= ~DBBE_REDIS_DETACH_DELETE_MASK; // unset bit 1 because we cannot delete
        break;
      }
    }
  }
  return to_delete;
}


int dbBE_Redis_process_nsdetach( dbBE_Redis_request_t **in_out_request,
                                 dbBE_Redis_result_t *result,
                                 dbBE_Redis_s2r_queue_t *post_queue,
                                 dbBE_Redis_connection_mgr_t *conn_mgr,
                                 int remaining_responses )
{
  int rc = 0;

  if( in_out_request == NULL )
    return -EINVAL;

  dbBE_Redis_request_t *request = *in_out_request;

  switch( request->_step->_stage )
  {
    case DBBE_REDIS_NSDETACH_STAGE_DELCHECK:
      switch( remaining_responses )
      {
        case 3: // the OK from MULTI
          if(( result->_type == dbBE_REDIS_TYPE_CHAR ) && ( strncmp( result->_data._string._data, "OK", result->_data._string._size ) == 0 ) )
            return 0;
          return -EPROTO;

        case 2: // QUEUED from the 2 commands
        case 1:
          if(( result->_type == dbBE_REDIS_TYPE_CHAR ) && ( strncmp( result->_data._string._data, "QUEUED", result->_data._string._size ) == 0 ) )
            return 0;
          return -EPROTO;

        case 0: // the regular array response, just continue with processing
          rc = dbBE_Redis_process_general( request, result );
          break;
        default: // an invalid stage
          return -EINVAL;
      }
      if( rc != 0 )
        break;

      // check and split the response array for the 2 commands
      if(( result->_type != dbBE_REDIS_TYPE_ARRAY ) || ( result->_data._array._len != 2 ))
      {
        rc = return_error_clean_result( -EINVAL, result );
        break;
      }

      dbBE_Redis_result_t *hincrby_res = &result->_data._array._data[0];
      dbBE_Redis_result_t *hmget_res = &result->_data._array._data[1];

      if(( hincrby_res == NULL ) || ( hmget_res == NULL ) ||
          ( hincrby_res->_type != dbBE_REDIS_TYPE_INT ) || ( hmget_res->_type != dbBE_REDIS_TYPE_ARRAY ))
      {
        rc = return_error_clean_result( -EPROTO, result );
        break;
      }

      if( hincrby_res->_data._integer < 0 )
      {
        rc = return_error_clean_result( -EOVERFLOW, result );
        break;
      }

      // parse the result array check the refcount and the flags
      int to_delete = dbBE_Redis_process_nsdetach_check_delete( hincrby_res, hmget_res );

      if( to_delete == 0x3 )
      {
        LOG( DBG_VERBOSE, stdout, "RefCnt and DeleteMark apply: DELETING Namespace\n" );

        // allocate a memory area to count inflight deletes and scans to know when the request is complete
        request->_status.nsdetach.reference = dbBE_Refcounter_allocate();
        if( request->_status.nsdetach.reference == NULL )
        {
          rc = return_error_clean_result( -ENOMEM, result );
          break;
        }

        request->_status.nsdetach.to_delete = 1;
        dbBE_Redis_request_t *scan_list = dbBE_Redis_connection_mgr_request_each( conn_mgr, request );

        // if we created new requests, we need to destroy the old one
        if( scan_list != NULL )
        {
          dbBE_Redis_request_destroy( request );
          request = NULL;
        }
        else // TODO: if the scan list is empty, we need to complete (with error) (no connections available)
        {
          // whith no deletiong, no more need to do extra transitions here. it's handled in the transition fnct
          request->_status.nsdetach.to_delete = 0;
          rc = return_error_clean_result( -ENOTCONN, result );
          break;
        }

          // now iterate the new requests for each connection
        while( scan_list != NULL )
        {
          dbBE_Redis_request_t *scan = scan_list;
          scan_list = scan_list->_next;
          scan->_status.nsdetach.scankey = strdup( "0" );

          dbBE_Redis_request_stage_transition( scan ); // explicit transition of these new requests
          rc = dbBE_Redis_s2r_queue_push( post_queue, scan );
          if( rc != 0 )
          {
            // todo: clean up and complete only if no other scan request was started
            return_error_clean_result( rc, result );
            break;
          }
          dbBE_Refcounter_up( scan->_status.nsdetach.reference );
        }
        dbBE_Redis_result_cleanup( result, 0 );
        result->_type = dbBE_REDIS_TYPE_INT;
        result->_data._integer = 0;

        *in_out_request = request; // could be NULL if there was a scan list
      }
      else if ( to_delete < 0 )
      {
        rc = return_error_clean_result( -EINVAL, result );
        break;
      }
      else
      {
        // mark this request as 'detach-only' and transition to a final stage (so it can complete)
        request->_status.nsdetach.to_delete = 0;
        dbBE_Redis_request_stage_transition( request );
        dbBE_Redis_result_cleanup( result, 0 );
        result->_type = dbBE_REDIS_TYPE_INT;
        result->_data._integer = 0;
        rc = 0;
      }

      break;

    case DBBE_REDIS_NSDETACH_STAGE_SCAN:
      rc = dbBE_Redis_process_general( request, result );
      dbBE_Refcounter_down( request->_status.nsdetach.reference );  // decrease the inflight count

      // cleanup the scan key entry (if any) to prevent memleak
      if( request->_status.nsdetach.scankey )
      {
        free( request->_status.nsdetach.scankey );
        request->_status.nsdetach.scankey = NULL;
      }

      if( conn_mgr == NULL )
      {
        rc = return_error_clean_result( -EINVAL, result );
        break;
      }

      if(( result->_type != dbBE_REDIS_TYPE_ARRAY ) || ( result->_data._array._len != 2 ))
      {
        rc = return_error_clean_result( -EINVAL, result );
        break;
      }

      // parse the result array and create key delete requests
      dbBE_Redis_result_t *subresult = &result->_data._array._data[1];
      int n;
      for( n=0; n<subresult->_data._array._len; ++n )
      {
        if( subresult->_data._array._data[ n ]._data._string._data == NULL )
          continue;

        // place that user request into the deletion
        dbBE_Redis_request_t *delkey = dbBE_Redis_request_allocate( request->_user );
        if( ! delkey )
          continue;
        delkey->_location._type = request->_location._type;
        delkey->_location._data._conn_idx = request->_location._data._conn_idx;
        delkey->_next = request->_next;
        delkey->_step = request->_step;
        delkey->_status.nsdetach.reference = request->_status.nsdetach.reference;
        delkey->_status.nsdetach.scankey = strdup( subresult->_data._array._data[ n ]._data._string._data );

        dbBE_Redis_request_stage_transition( delkey );
        rc = dbBE_Redis_s2r_queue_push( post_queue, delkey );
        if( rc != 0 )
          continue;
        dbBE_Refcounter_up( request->_status.nsdetach.reference );
      }
      // if cursor is not "0", then create another match request
      subresult = &result->_data._array._data[0];
      if( subresult->_data._string._data[0] != '0' )
      {
        // it returned a valid cursor, so we have to send another scan request
        request->_status.nsdetach.scankey = strdup( subresult->_data._string._data );
        // do not transition - this request needs to repeat, just with a new cursor
        dbBE_Redis_s2r_queue_push( post_queue, request );
        dbBE_Refcounter_up( request->_status.nsdetach.reference );

        LOG( DBG_TRACE, stdout, "Creating next scan cursor %s for conn %d\n", request->_status.nsdetach.scankey, request->_location._data._conn_idx );
        // all new requests are pushed to s2r queue, we need to clean up the inbound request to prevent memleak
        *in_out_request = NULL;
      }
      else
      {
        // if there are other requests in flight, we can drop this one
        if( dbBE_Refcounter_get( request->_status.nsdetach.reference ) != 0 )
        {
          dbBE_Redis_request_destroy( request );
          *in_out_request = NULL;
        }
        else
        {
          // completed: time to clean up the allocated mem structures
          // transition 2x to get to DELNS stage (second time done by caller)
          dbBE_Redis_request_stage_transition( request );
        }
      }
      break;

    case DBBE_REDIS_NSDETACH_STAGE_DELKEYS:
    {
      rc = dbBE_Redis_process_general( request, result );
      uint64_t ref = dbBE_Refcounter_down( request->_status.nsdetach.reference );
      if( rc == 0 )
      {
        if( result->_data._integer != 1 )
          rc = -ENOENT;
        result->_data._integer = rc; // set the result/return code for upper layers
      }

      // cleanup the scan key entry (if any) to prevent memleak
      if( request->_status.nsdetach.scankey )
      {
        free( request->_status.nsdetach.scankey );
        request->_status.nsdetach.scankey = NULL;
      }

      // if there are other requests in flight, we can drop this one
      if( ref != 0 )
      {
        dbBE_Redis_request_destroy( request );
        *in_out_request = NULL;
      }
      break;
    }
    case DBBE_REDIS_NSDETACH_STAGE_DELNS:
    {
      rc = dbBE_Redis_process_general( request, result );
      uint64_t ref = dbBE_Refcounter_get( request->_status.nsdetach.reference );
      if( ref != 0 )
        // this is a bug: we must not reach this state with a refcount>0
        return -EFAULT;

      dbBE_Refcounter_destroy( request->_status.nsdetach.reference );
      request->_status.nsdetach.reference = NULL;

      if( rc == 0 )
      {
        if( result->_data._integer != 1 )
          rc = -ENOENT;
        result->_data._integer = rc; // set the result/return code for upper layers
      }
      break;
    }
    default:
      rc = -EPROTO;
      result->_data._integer = rc;
      break;
  }
  return rc;
}

int dbBE_Redis_process_nsdelete( dbBE_Redis_request_t *request,
                                 dbBE_Redis_result_t *result )
{
  int rc = 0;

  rc = dbBE_Redis_process_general( request, result );

  switch( request->_step->_stage )
  {
    case DBBE_REDIS_NSDELETE_STAGE_EXIST:
    {
      if( rc != 0 )
        break;
      if(  result->_data._array._len != 2 )
      {
        rc = return_error_clean_result( -EPROTO, result );
        break;
      }

      dbBE_Redis_result_t *refcnt_data = &result->_data._array._data[ 0 ];
      dbBE_Redis_result_t *flags_data = &result->_data._array._data[ 1 ];

      if( (( refcnt_data == NULL ) || ( refcnt_data->_type != dbBE_REDIS_TYPE_CHAR )) ||
          (( flags_data == NULL ) || ( flags_data->_type != dbBE_REDIS_TYPE_CHAR )) )
      {
        rc = return_error_clean_result( -ENOENT, result );
        break;
      }

      int refcnt = 0;
      if( refcnt_data->_data._string._data != NULL )
        refcnt = strtoll( refcnt_data->_data._string._data, NULL, 10 );

      // todo: check flag and skip the second stage
      // (optimization that's hard to do with early result stage)
//      int flags = strtoll( flags_data->_data._string._data, NULL, 10 );
//
//      if( ( flags & DBBE_REDIS_META_DELETE_FLAG) != 0 )
//      {
//        dbBE_Redis_result_cleanup( result, 0 );
//        result->_type = dbBE_REDIS_TYPE_INT;
//        result->_data._integer = 0;
//        break;
//      }
      if( refcnt > 1 )
      {
        return_error_clean_result( -EBUSY, result );
        rc = 0; // this error just needs to be in the result
      }
      else
      {
        dbBE_Redis_result_cleanup( result, 0 );
        result->_type = dbBE_REDIS_TYPE_INT;
        result->_data._integer = 0;
      }

      break;
    }

    case DBBE_REDIS_NSDELETE_STAGE_SETFLAG:
      if( rc != 0 )
        break;

      if( result->_data._integer != 0 )
      {
        LOG( DBG_ERR, stderr, "Namespace deletion detected a non-existing namespace. Possible data inconsistency.\n" );
        rc = return_error_clean_result( -ENOENT, result );
        break;
      }

      dbBE_Redis_result_cleanup( result, 0 );
      result->_type = dbBE_REDIS_TYPE_INT;
      result->_data._integer = 0;
      break;

    default:
      break;
  }

  return rc;
}
