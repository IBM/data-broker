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

#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "logutil.h"
#include "result.h"

// deep delete result structure (especially taking care of arrays)
int dbBE_Redis_result_cleanup( dbBE_Redis_result_t *result, int dealloc )
{
  if( result == NULL )
    return -EINVAL;

  switch( result->_type )
  {
    case dbBE_REDIS_TYPE_ARRAY:
    {
      int n;
      // cleanup and free array - the array creation mechanism allocates new memory
      // so we have to clean it up here
      if( result->_data._array._data != NULL )
      {
        for( n = 0; n < result->_data._array._len; ++n )
          dbBE_Redis_result_cleanup( & result->_data._array._data[ n ], 0 );
        free( result->_data._array._data );
      }
      break;
    }
    case dbBE_REDIS_TYPE_CHAR:
    case dbBE_REDIS_TYPE_ERROR:
    default:
      break;
  }

  // resetting mem
  memset( &result->_data, 0, sizeof( result->_data ) );
  result->_type = dbBE_REDIS_TYPE_UNSPECIFIED;
  if( dealloc )
    free( result );

  return 0;
}

int dbBE_Redis_result_terminate_strings( dbBE_Redis_result_t *result )
{
  if( result == NULL )
    return -EINVAL;

  int rc = 0;

  switch( result->_type )
  {
    case dbBE_REDIS_TYPE_INT:
      break;
    case dbBE_REDIS_TYPE_CHAR:
    case dbBE_REDIS_TYPE_ERROR:
      if( result->_data._string._data != NULL )
        result->_data._string._data[ result->_data._string._size ] = '\0';
      break;
    case dbBE_REDIS_TYPE_REDIRECT:
    case dbBE_REDIS_TYPE_RELOCATE:
    {
      if( result->_data._location._address != NULL )
      {
        char *term = strstr( result->_data._location._address, "\r\n" );
        int addrlen = (int)( term - result->_data._location._address );
        result->_data._location._address[ addrlen ] = '\0';
      }
      break;
    }
    case dbBE_REDIS_TYPE_ARRAY:
    {
      int n;
      int array_len = result->_data._array._len;
      for( n = 0; (n < array_len) && ( rc == 0 ); ++n )
        rc = dbBE_Redis_result_terminate_strings( &result->_data._array._data[ n ] );
      break;
    }
    default:
      LOG( DBG_ERR, stderr, "Unexpected entry in result: type=%d\n", result->_type );
      return -EBADMSG;
  }
  return rc;
}
