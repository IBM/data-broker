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

#include "sr_buffer.h"

#include <stddef.h>
#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>
#endif
#include <string.h>
#include <errno.h>


dbBE_Redis_sr_buffer_t* dbBE_Transport_sr_buffer_allocate( const size_t size )
{
  dbBE_Redis_sr_buffer_t *ret = (dbBE_Redis_sr_buffer_t*)malloc( sizeof(dbBE_Redis_sr_buffer_t) );
  if( ret == NULL )
    return NULL;

  if( dbBE_Transport_sr_buffer_initialize( ret, size, (char*)malloc( size ) ) != 0 )
  {
    free( ret );
    return NULL;
  }

  return ret;
}

/*
 * initialize existing sr_buffer with size and memory location
 */
int dbBE_Transport_sr_buffer_initialize( dbBE_Redis_sr_buffer_t *sr_buf,
                                         const size_t size,
                                         char *buffer )
{
  if( sr_buf == NULL )
    return -EINVAL;

  memset( sr_buf, 0, sizeof( dbBE_Redis_sr_buffer_t ) );

  sr_buf->_size = size;
  sr_buf->_available = 0;
  sr_buf->_processed = 0;
  sr_buf->_start = buffer;

  if( sr_buf->_start == NULL )
    return -ENOMEM;

  memset( sr_buf->_start, 0, size );

  return 0;
}


void dbBE_Transport_sr_buffer_reset( dbBE_Redis_sr_buffer_t *sr_buf )
{
  if( sr_buf == NULL )
    return;

  sr_buf->_available = 0;
  sr_buf->_processed = 0;
}

void dbBE_Transport_sr_buffer_free( dbBE_Redis_sr_buffer_t *sr_buf )
{
  if( sr_buf == NULL )
    return;

  if( sr_buf->_start != NULL )
  {
    memset( sr_buf->_start, 0, sr_buf->_size );
    free( sr_buf->_start );
  }

  memset( sr_buf, 0, sizeof( dbBE_Redis_sr_buffer_t ) );
  free( sr_buf );
}
