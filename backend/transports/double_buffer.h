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


#ifndef BACKEND_TRANSPORTS_DOUBLE_BUFFER_H_
#define BACKEND_TRANSPORTS_DOUBLE_BUFFER_H_

#include "transports/sr_buffer.h"

#include <inttypes.h>
#include <errno.h>

typedef struct
{
  dbBE_Redis_sr_buffer_t _buf[2];
  int8_t _active;
} dbBE_Transport_dbuffer_t;


/*
 * allocate an initialize the buffer
 */
dbBE_Transport_dbuffer_t* dbBE_Transport_dbuffer_allocate( const size_t size );

/*
 * reset the stats of the buffer
 */
int dbBE_Transport_dbuffer_reset( dbBE_Transport_dbuffer_t *dbuf );

/*
 * deallocate and reset memory of buffer
 */
int dbBE_Transport_dbuffer_free( dbBE_Transport_dbuffer_t *dbuf );



/*
 * return the index of the active buffer
 */
static inline
dbBE_Redis_sr_buffer_t *dbBE_Transport_dbuffer_get_active( dbBE_Transport_dbuffer_t *dbuf )
{
  if( dbuf != NULL )
    return (&dbuf->_buf[ dbuf->_active ]);
  return NULL;
}

/*
 * toggle the active buffer index
 */
static inline
int dbBE_Transport_dbuffer_toggle( dbBE_Transport_dbuffer_t *dbuf )
{
  if( dbuf == NULL )
    return -EINVAL;
  dbuf->_active = ( ( dbuf->_active + 1 ) & 1 );
  return dbuf->_active;
}


#endif /* BACKEND_TRANSPORTS_DOUBLE_BUFFER_H_ */
