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

#ifndef BACKEND_REDIS_REDIS_CMDS_H_
#define BACKEND_REDIS_REDIS_CMDS_H_

#include <inttypes.h>
#include <string.h>

#include "sr_buffer.h"
#include "protocol.h"

/*
 * implementation of single redis command exec
 */

static inline
int Redis_insert_redis_terminator( char *buf )
{
  if( sprintf( buf, "\r\n" ) < 2 )
    return -1;
  return 2;
}

int Redis_insert_to_sr_buffer( dbBE_Redis_sr_buffer_t *sr_buf, dbBE_REDIS_DATA_TYPE type, dbBE_Redis_data_t *data );


// data argument is handed in to avoid another stack entry and reuse an existing memory location
static inline
int dbBE_Redis_command_microcmd_create( dbBE_Redis_command_stage_spec_t *stage,
                                        dbBE_Redis_sr_buffer_t *sr_buf,
                                        dbBE_Redis_data_t *data )
{
  int len = 0;
  data->_integer = stage->_array_len;
  len += Redis_insert_to_sr_buffer( sr_buf, dbBE_REDIS_TYPE_ARRAY, data );

  data->_string._data = stage->_command;
  data->_string._size = strnlen( stage->_command, DBBE_REDIS_COMMAND_LENGTH_MAX );
  len += Redis_insert_to_sr_buffer( sr_buf, dbBE_REDIS_TYPE_CHAR, data );

  return len;
}


int dbBE_Redis_command_cmdandkey_only_create( dbBE_Redis_command_stage_spec_t *stage,
                                              dbBE_Redis_sr_buffer_t *sr_buf,
                                              char *keybuffer,
                                              size_t vallen )
{
  int len = 0;
  dbBE_Redis_data_t data;
  len += dbBE_Redis_command_microcmd_create( stage, sr_buf, &data );

  data._string._data = keybuffer;
  data._string._size = strnlen( data._string._data, DBBE_REDIS_MAX_KEY_LEN );
  len += Redis_insert_to_sr_buffer( sr_buf, dbBE_REDIS_TYPE_CHAR, &data );

  data._integer = vallen;
  len += Redis_insert_to_sr_buffer( sr_buf, dbBE_REDIS_TYPE_STRING_HEAD, &data );

  return len;
}

int64_t dbBE_Redis_command_create_insert_value( dbBE_Redis_sr_buffer_t *sr_buf,
                                                dbBE_Data_transport_t *transport,
                                                dbBE_sge_t *sge,
                                                int sge_count,
                                                int64_t vallen )
{
  int64_t slen = transport->gather( (dbBE_Data_transport_device_t*)dbBE_Redis_sr_buffer_get_processed_position( sr_buf ),
                                    dbBE_Redis_sr_buffer_remaining( sr_buf ),
                                    sge_count,
                                    sge );
  return dbBE_Redis_sr_buffer_add_data( sr_buf, vallen, 1 );
}

static inline
int dbBE_Redis_command_create_terminate( dbBE_Redis_sr_buffer_t *sr_buf )
{
  return dbBE_Redis_sr_buffer_add_data( sr_buf,
                                        Redis_insert_redis_terminator( dbBE_Redis_sr_buffer_get_processed_position( sr_buf )),
                                        1 );
}

int dbBE_Redis_command_put_parse( dbBE_Redis_command_stage_spec_t spec,
                                  dbBE_Redis_result_t *result )
{
  return -ENOSYS;
}


int dbBE_Redis_command_lpop_create( dbBE_Redis_command_stage_spec_t *stage,
                                    dbBE_Redis_sr_buffer_t *sr_buf,
                                    char *keybuffer )
{
  int len = 0;
  dbBE_Redis_data_t data;
  len += dbBE_Redis_command_microcmd_create( stage, sr_buf, &data );

  data._string._data = keybuffer;
  data._string._size = strnlen( data._string._data, DBBE_REDIS_MAX_KEY_LEN );
  len += Redis_insert_to_sr_buffer( sr_buf, dbBE_REDIS_TYPE_CHAR, &data );

  return len;
}


int dbBE_Redis_command_lindex_create( dbBE_Redis_command_stage_spec_t *stage,
                                      dbBE_Redis_sr_buffer_t *sr_buf,
                                      char *keybuffer )
{
  int len = 0;
  dbBE_Redis_data_t data;
  len += dbBE_Redis_command_microcmd_create( stage, sr_buf, &data );

  data._string._data = keybuffer;
  data._string._size = strnlen( data._string._data, DBBE_REDIS_MAX_KEY_LEN );
  len += Redis_insert_to_sr_buffer( sr_buf, dbBE_REDIS_TYPE_CHAR, &data );

  data._string._data = "0"; // read list index 0
  data._string._size = 1;
  len += Redis_insert_to_sr_buffer( sr_buf, dbBE_REDIS_TYPE_CHAR, &data );

  return len;
}

int dbBE_Redis_command_hsetnx_create( dbBE_Redis_command_stage_spec_t *stage,
                                      dbBE_Redis_sr_buffer_t *sr_buf,
                                      char *name_space )
{
  int len = 0;
  dbBE_Redis_data_t data;
  const char *idbuf = "id";

  len += dbBE_Redis_command_microcmd_create( stage, sr_buf, &data );

  data._string._data = name_space;
  data._string._size = strnlen( data._string._data, DBBE_REDIS_MAX_KEY_LEN );
  len += Redis_insert_to_sr_buffer( sr_buf, dbBE_REDIS_TYPE_CHAR, &data );

  data._string._data = (char*)idbuf;
  data._string._size = strnlen( data._string._data, DBBE_REDIS_MAX_KEY_LEN );
  len += Redis_insert_to_sr_buffer( sr_buf, dbBE_REDIS_TYPE_CHAR, &data );

  data._string._data = name_space;
  data._string._size = strnlen( data._string._data, DBBE_REDIS_MAX_KEY_LEN );
  len += Redis_insert_to_sr_buffer( sr_buf, dbBE_REDIS_TYPE_CHAR, &data );

  return len;
}


int dbBE_Redis_command_hmset_create( dbBE_Redis_command_stage_spec_t *stage,
                                     dbBE_Redis_sr_buffer_t *sr_buf,
                                     dbBE_Redis_request_t *request,
                                     dbBE_Data_transport_t *transport,
                                     char *keybuffer )
{
  int len = 0;
  int rc = 1;
  dbBE_Redis_data_t data;
  size_t vallen = dbBE_SGE_get_len( request->_user->_sge, request->_user->_sge_count );

  len += dbBE_Redis_command_microcmd_create( stage, sr_buf, &data );

  data._string._data = keybuffer;
  data._string._size = strnlen( data._string._data, DBBE_REDIS_MAX_KEY_LEN );
  len += Redis_insert_to_sr_buffer( sr_buf, dbBE_REDIS_TYPE_CHAR, &data );

  data._string._data = keybuffer;
  sprintf( keybuffer, "refcnt" );
  data._string._size = strnlen( data._string._data, DBBE_REDIS_MAX_KEY_LEN );
  len += Redis_insert_to_sr_buffer( sr_buf, dbBE_REDIS_TYPE_CHAR, &data );

  data._string._data = keybuffer; // set refcnt to 1
  sprintf( keybuffer, "1" );
  data._string._size = strnlen( data._string._data, DBBE_REDIS_MAX_KEY_LEN );
  len += Redis_insert_to_sr_buffer( sr_buf, dbBE_REDIS_TYPE_CHAR, &data );

  data._string._data = keybuffer;
  sprintf( keybuffer, "groups" );
  data._string._size = strnlen( data._string._data, DBBE_REDIS_MAX_KEY_LEN );
  len += Redis_insert_to_sr_buffer( sr_buf, dbBE_REDIS_TYPE_CHAR, &data );

  data._integer = vallen; // grouplist collected from the value.
  len += Redis_insert_to_sr_buffer( sr_buf, dbBE_REDIS_TYPE_STRING_HEAD, &data );

  int64_t slen = dbBE_Redis_command_create_insert_value( sr_buf,
                                                         transport,
                                                         request->_user->_sge,
                                                         request->_user->_sge_count,
                                                         vallen );
  if( slen != (int64_t)vallen )
    rc = -1;
  len += dbBE_Redis_command_create_terminate( sr_buf );

  return len * rc;
}

int dbBE_Redis_command_hmgetall_create( dbBE_Redis_command_stage_spec_t *stage,
                                        dbBE_Redis_sr_buffer_t *sr_buf,
                                        char *name_space )
{
  int len = 0;
  dbBE_Redis_data_t data;

  dbBE_Redis_command_microcmd_create( stage, sr_buf, &data );

  data._string._data = name_space;
  data._string._size = strnlen( data._string._data, DBBE_REDIS_MAX_KEY_LEN );
  len += Redis_insert_to_sr_buffer( sr_buf, dbBE_REDIS_TYPE_CHAR, &data );

  return len;
}

int dbBE_Redis_command_exists_create( dbBE_Redis_command_stage_spec_t *stage,
                                      dbBE_Redis_sr_buffer_t *sr_buf,
                                      char *name_space )
{
  int len = 0;
  dbBE_Redis_data_t data;
  dbBE_Redis_command_microcmd_create( stage, sr_buf, &data );

  data._string._data = name_space;
  data._string._size = strnlen( data._string._data, DBBE_REDIS_MAX_KEY_LEN );
  len += Redis_insert_to_sr_buffer( sr_buf, dbBE_REDIS_TYPE_CHAR, &data );

  return len;
}

int dbBE_Redis_command_hincrby_create( dbBE_Redis_command_stage_spec_t *stage,
                                       dbBE_Redis_sr_buffer_t *sr_buf,
                                       char *keybuffer,
                                       int increment )
{
  int len = 0;
  dbBE_Redis_data_t data;

  dbBE_Redis_command_microcmd_create( stage, sr_buf, &data );
  data._string._data = keybuffer;
  data._string._size = strnlen( data._string._data, DBBE_REDIS_MAX_KEY_LEN );
  len += Redis_insert_to_sr_buffer( sr_buf, dbBE_REDIS_TYPE_CHAR, &data );

  data._string._data = keybuffer;
  sprintf( keybuffer, "refcnt" );
  data._string._size = strnlen( data._string._data, DBBE_REDIS_MAX_KEY_LEN );
  len += Redis_insert_to_sr_buffer( sr_buf, dbBE_REDIS_TYPE_CHAR, &data );

  data._string._data = keybuffer;
  sprintf( keybuffer, "%d", increment );
  data._string._size = strnlen( data._string._data, DBBE_REDIS_MAX_KEY_LEN );
  len += Redis_insert_to_sr_buffer( sr_buf, dbBE_REDIS_TYPE_CHAR, &data );

  return len;
}

int dbBE_Redis_command_scan_create( dbBE_Redis_command_stage_spec_t *stage,
                                    dbBE_Redis_sr_buffer_t *sr_buf,
                                    char *keybuffer,
                                    char *cursor )
{
  int len = 0;
  dbBE_Redis_data_t data;
  char *startcursor = "0";
  char *command = "MATCH";

  dbBE_Redis_command_microcmd_create( stage, sr_buf, &data );

  // create and insert the scan cursor
  data._string._data = startcursor;
  if( cursor != NULL )
    data._string._data = cursor;
  data._string._size = strnlen( data._string._data, DBBE_REDIS_MAX_KEY_LEN );
  len += Redis_insert_to_sr_buffer( sr_buf, dbBE_REDIS_TYPE_CHAR, &data );

  data._string._data = command;
  data._string._size = strnlen( data._string._data, DBBE_REDIS_MAX_KEY_LEN );
  len += Redis_insert_to_sr_buffer( sr_buf, dbBE_REDIS_TYPE_CHAR, &data );

  data._string._data = keybuffer;
  data._string._size = strnlen( data._string._data, DBBE_REDIS_MAX_KEY_LEN );
  len += Redis_insert_to_sr_buffer( sr_buf, dbBE_REDIS_TYPE_CHAR, &data );

  return len;
}

#endif /* BACKEND_REDIS_REDIS_CMDS_H_ */
