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

#ifndef BACKEND_REDIS_CMD_BUFFER_H_
#define BACKEND_REDIS_CMD_BUFFER_H_

#include "definitions.h"
#include "common/dbbe_api.h"

#include <stdlib.h> // calloc
#include <string.h> // memset
#include <limits.h> // UINT_MAX

#define DBBE_REDIS_CMD_INDEX_INVAL ( UINT_MAX )

typedef struct {
  dbBE_sge_t _cmd[ DBBE_SGE_MAX ];
  unsigned _index;
} dbBE_Redis_cmd_buffer_t;


dbBE_Redis_cmd_buffer_t* dbBE_Redis_cmd_buffer_create()
{
  dbBE_Redis_cmd_buffer_t *buf = (dbBE_Redis_cmd_buffer_t*)calloc( 1, sizeof( dbBE_Redis_cmd_buffer_t ));
  if( buf == NULL )
    return NULL;

  return buf;
}

DBR_Errorcode_t dbBE_Redis_cmd_buffer_destroy( dbBE_Redis_cmd_buffer_t *cbuf )
{
  if( cbuf == NULL )
    return DBR_ERR_INVALID;

  memset( cbuf, 0, sizeof( dbBE_Redis_cmd_buffer_t ) );
  free( cbuf );
  return DBR_SUCCESS;
}

dbBE_sge_t* dbBE_Redis_cmd_buffer_get_current( dbBE_Redis_cmd_buffer_t *cbuf )
{
  if( cbuf == NULL )
    return NULL;
  return &cbuf->_cmd[ cbuf->_index ];
}

unsigned dbBE_Redis_cmd_buffer_remain( dbBE_Redis_cmd_buffer_t *cbuf )
{
  if( cbuf == NULL )
    return DBBE_REDIS_CMD_INDEX_INVAL;
  return DBBE_SGE_MAX - cbuf->_index;
}

unsigned dbBE_Redis_cmd_buffer_add( dbBE_Redis_cmd_buffer_t *cbuf, const unsigned addition )
{
  if( cbuf == NULL )
    return DBBE_REDIS_CMD_INDEX_INVAL;
  if( cbuf->_index + addition > DBBE_SGE_MAX )
    return DBBE_REDIS_CMD_INDEX_INVAL;
  cbuf->_index += addition;
  return cbuf->_index;
}

DBR_Errorcode_t dbBE_Redis_cmd_buffer_reset( dbBE_Redis_cmd_buffer_t *cbuf )
{
  if( cbuf == NULL )
    return DBR_ERR_INVALID;
  memset( cbuf, 0, sizeof( dbBE_Redis_cmd_buffer_t ) );
  return DBR_SUCCESS;
}

#endif /* BACKEND_REDIS_CMD_BUFFER_H_ */
