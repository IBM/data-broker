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

#ifndef BACKEND_TRANSPORTS_SGE_BUFFER_H_
#define BACKEND_TRANSPORTS_SGE_BUFFER_H_

#include "common/dbbe_api.h"

#include <stdlib.h> // calloc
#include <string.h> // memset
#include <limits.h> // UINT_MAX

#define DBBE_TRANSPORT_SGE_INDEX_INVAL ( UINT_MAX )

typedef struct {
  dbBE_sge_t _cmd[ DBBE_SGE_MAX ];
  unsigned _index;
} dbBE_Transport_sge_buffer_t;


#define dbBE_Transport_sge_get( cbuf ) ( cbuf->_cmd )
#define dbBE_Transport_sge_count( cbuf ) ( cbuf->_index )

static inline
dbBE_Transport_sge_buffer_t* dbBE_Transport_sge_buffer_create()
{
  dbBE_Transport_sge_buffer_t *buf = (dbBE_Transport_sge_buffer_t*)calloc( 1, sizeof( dbBE_Transport_sge_buffer_t ));
  if( buf == NULL )
    return NULL;

  return buf;
}

static inline
DBR_Errorcode_t dbBE_Transport_sge_buffer_destroy( dbBE_Transport_sge_buffer_t *cbuf )
{
  if( cbuf == NULL )
    return DBR_ERR_INVALID;

  memset( cbuf, 0, sizeof( dbBE_Transport_sge_buffer_t ) );
  free( cbuf );
  return DBR_SUCCESS;
}

static inline
dbBE_sge_t* dbBE_Transport_sge_buffer_get_current( dbBE_Transport_sge_buffer_t *cbuf )
{
  if( cbuf == NULL )
    return NULL;
  return &cbuf->_cmd[ cbuf->_index ];
}

static inline
unsigned dbBE_Transport_sge_buffer_remain( dbBE_Transport_sge_buffer_t *cbuf )
{
  if( cbuf == NULL )
    return DBBE_TRANSPORT_SGE_INDEX_INVAL;
  return DBBE_SGE_MAX - cbuf->_index;
}

static inline
unsigned dbBE_Transport_sge_buffer_add( dbBE_Transport_sge_buffer_t *cbuf, const unsigned addition )
{
  if( cbuf == NULL )
    return DBBE_TRANSPORT_SGE_INDEX_INVAL;
  if( cbuf->_index + addition > DBBE_SGE_MAX )
    return DBBE_TRANSPORT_SGE_INDEX_INVAL;
  cbuf->_index += addition;
  return cbuf->_index;
}

static inline
DBR_Errorcode_t dbBE_Transport_sge_buffer_reset( dbBE_Transport_sge_buffer_t *cbuf )
{
  if( cbuf == NULL )
    return DBR_ERR_INVALID;
  memset( cbuf, 0, sizeof( dbBE_Transport_sge_buffer_t ) );
  return DBR_SUCCESS;
}

static inline
size_t dbBE_Transport_sge_buffer_get_size( dbBE_Transport_sge_buffer_t *cbuf )
{
  if( cbuf == NULL )
    return 0;
  int i;
  size_t len = 0;
  for( i = cbuf->_index-1; i>=0; --i )
    len += cbuf->_cmd[ i ].iov_len;
  return len;
}

#endif /* BACKEND_TRANSPORTS_SGE_BUFFER_H_ */
