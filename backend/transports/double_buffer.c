/*
 * Copyright © 2019 IBM Corporation
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

#include "double_buffer.h"
#include "logutil.h"

#include <errno.h>
#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h> // malloc
#endif
#include <string.h> // memset

/*
 * allocate an initialize the buffer
 */
dbBE_Transport_dbuffer_t* dbBE_Transport_dbuffer_allocate( const size_t size )
{
  if(( size % 2 ) || ( size < 1 ) || ((ssize_t)size < 0 ))
  {
    LOG( DBG_ERR, stderr, "dbuffer size is required to be an even number.\n" );
    errno = EINVAL;
    return NULL;
  }
  dbBE_Transport_dbuffer_t *ret = (dbBE_Transport_dbuffer_t*)calloc( 1, sizeof(dbBE_Transport_dbuffer_t) );
  if( ret == NULL )
    return NULL;

  char *buffer = (char*)malloc( size * 2 );
  if( buffer == NULL )
  {
    free( ret );
    return NULL;
  }
  dbBE_Transport_sr_buffer_initialize( &ret->_buf[0], size, buffer );
  dbBE_Transport_sr_buffer_initialize( &ret->_buf[1], size, &buffer[ size ] );

  ret->_active = 0;

  return ret;
}

/*
 * reset the stats of the buffer
 */
int dbBE_Transport_dbuffer_reset( dbBE_Transport_dbuffer_t *dbuf )
{
  if( dbuf == NULL )
    return -EINVAL;

  dbBE_Transport_sr_buffer_reset( &dbuf->_buf[0] );
  dbBE_Transport_sr_buffer_reset( &dbuf->_buf[1] );
  dbuf->_active = 0;

  return 0;
}

/*
 * deallocate and reset memory of buffer
 */
int dbBE_Transport_dbuffer_free( dbBE_Transport_dbuffer_t *dbuf )
{
  if( dbuf == NULL )
    return -EINVAL;

  dbBE_Transport_dbuffer_reset( dbuf );

  int rc = 0;
  if( dbuf->_buf[0]._start != NULL )
  {
    free( dbuf->_buf[0]._start );
    dbuf->_buf[0]._start = NULL;
    dbuf->_buf[1]._start = NULL;
  }
  else
    rc = EINVAL;

  free( dbuf );

  return rc;
}
