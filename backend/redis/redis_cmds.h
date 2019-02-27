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

#ifndef BACKEND_REDIS_REDIS_CMDS_H_
#define BACKEND_REDIS_REDIS_CMDS_H_

#include <inttypes.h>
#include <string.h>
#include <errno.h>

#include "sr_buffer.h"
#include "protocol.h"
#include "request.h"

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
  // data had to be truncated
  if( slen < 0 )
    return -slen;

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


#define DBBE_REDIS_CMD_REWIND_BUF_AND_ERROR( err, sr_buf, position ) \
    { \
      dbBE_Redis_sr_buffer_rewind_processed_to( (sr_buf), (position) ); \
      return ( err ); \
    }


static inline
int dbBE_Redis_command_create_sgeN( dbBE_Redis_command_stage_spec_t *stage,
                                    dbBE_Redis_sr_buffer_t *sr_buf,
                                    dbBE_sge_t *args )
{
  int rc = 0;
  int n = 0;
  char *cmdptr = stage->_command;
  char *initial = dbBE_Redis_sr_buffer_get_processed_position( sr_buf );
  char *cmdend = stage->_command + strlen( stage->_command );

  while((cmdptr < cmdend ))
  {
    // get next location of parameter
    char *loc = index( cmdptr, '%' );

    // no more parameters. break and add the remaining cmd string
    if( loc == NULL )
      break;

    // index() did something funky...
    if(( loc > cmdend ) || ( *loc != '%' ))
      DBBE_REDIS_CMD_REWIND_BUF_AND_ERROR( -EBADMSG, sr_buf, initial );

    // what positional arg to insert?
    int idx = (int)loc[1] - 48;
    if(( idx < 0 )  ||  ( idx >= stage->_array_len )  || ( args[ idx ].iov_base == NULL ))
      DBBE_REDIS_CMD_REWIND_BUF_AND_ERROR( -EBADMSG, sr_buf, initial );

    // insert cmd string up to position
    char tmp = *loc;
    /*
     *  todo: note this changes the cmd buffer and thus is not thread-safe
     *        in case there are multiple threads creating commands
     */
    *loc = '\0';
    int len = snprintf( dbBE_Redis_sr_buffer_get_processed_position( sr_buf ),
                        dbBE_Redis_sr_buffer_remaining( sr_buf ),
                        "%s", cmdptr );
    *loc = tmp;
    if( len < 0 ) // can be 0 if 2 args appear back-to-back
      DBBE_REDIS_CMD_REWIND_BUF_AND_ERROR( -ENOMEM, sr_buf, initial );

    rc += dbBE_Redis_sr_buffer_add_data( sr_buf, len, 1 );

    // insert args[n]
    len = snprintf( dbBE_Redis_sr_buffer_get_processed_position( sr_buf ),
                    dbBE_Redis_sr_buffer_remaining( sr_buf ),
                    "$%ld\r\n", args[ idx ].iov_len );
    if( len < 4 ) // fixed chars + single digit number + one-length argument
      DBBE_REDIS_CMD_REWIND_BUF_AND_ERROR( -ENOMEM, sr_buf, initial );

    rc += dbBE_Redis_sr_buffer_add_data( sr_buf, len, 1 );

    size_t maxlen = args[ idx ].iov_len;
    if( maxlen > dbBE_Redis_sr_buffer_remaining( sr_buf ) - 2 ) // -2 because of cmd terminator
      maxlen = dbBE_Redis_sr_buffer_remaining( sr_buf ) - 2;
    memcpy( dbBE_Redis_sr_buffer_get_processed_position( sr_buf ),
            args[ idx ].iov_base,
            maxlen );

    rc += dbBE_Redis_sr_buffer_add_data( sr_buf, maxlen, 1 );
    rc += dbBE_Redis_command_create_terminate( sr_buf );

    cmdptr = loc + 2;
    ++n;
  }

  // add any remaining/trailing cmd string data
  if( cmdptr < cmdend )
  {
    int len = snprintf( dbBE_Redis_sr_buffer_get_processed_position( sr_buf ),
                        dbBE_Redis_sr_buffer_remaining( sr_buf ),
                        "%s", cmdptr );
    if( len <= 0 ) // if equal to zero, then cmdptr == cmdend - can't be true here
      DBBE_REDIS_CMD_REWIND_BUF_AND_ERROR( -ENOMEM, sr_buf, initial );

    rc += dbBE_Redis_sr_buffer_add_data( sr_buf, len, 1 );
  }

  return rc;
}

static inline
int dbBE_Redis_command_create_str1( dbBE_Redis_command_stage_spec_t *stage,
                                    dbBE_Redis_sr_buffer_t *sr_buf,
                                    char *buffer )
{
  if( stage->_array_len != 1 )
    return -EINVAL;

  dbBE_sge_t sge[ 2 ];
  sge[0].iov_base = buffer;
  sge[0].iov_len = strlen( buffer );
  sge[1].iov_base = NULL;
  sge[1].iov_len = 0;

  return dbBE_Redis_command_create_sgeN( stage, sr_buf, sge );
}

int dbBE_Redis_command_lpop_create( dbBE_Redis_command_stage_spec_t *stage,
                                    dbBE_Redis_sr_buffer_t *sr_buf,
                                    char *keybuffer )
{
  return dbBE_Redis_command_create_str1( stage, sr_buf, keybuffer );
}


int dbBE_Redis_command_lindex_create( dbBE_Redis_command_stage_spec_t *stage,
                                      dbBE_Redis_sr_buffer_t *sr_buf,
                                      char *keybuffer )
{
  return dbBE_Redis_command_create_str1( stage, sr_buf, keybuffer );
}

int dbBE_Redis_command_del_create( dbBE_Redis_command_stage_spec_t *stage,
                                   dbBE_Redis_sr_buffer_t *sr_buf,
                                   char *keybuffer )
{
  return dbBE_Redis_command_create_str1( stage, sr_buf, keybuffer );
}

int dbBE_Redis_command_hmgetall_create( dbBE_Redis_command_stage_spec_t *stage,
                                        dbBE_Redis_sr_buffer_t *sr_buf,
                                        char *name_space )
{
  return dbBE_Redis_command_create_str1( stage, sr_buf, name_space );
}

int dbBE_Redis_command_hmget_create( dbBE_Redis_command_stage_spec_t *stage,
                                     dbBE_Redis_sr_buffer_t *sr_buf,
                                     char *name_space )
{
  return dbBE_Redis_command_create_str1( stage, sr_buf, name_space );
}

int dbBE_Redis_command_exists_create( dbBE_Redis_command_stage_spec_t *stage,
                                      dbBE_Redis_sr_buffer_t *sr_buf,
                                      char *name_space )
{
  return dbBE_Redis_command_create_str1( stage, sr_buf, name_space );
}

int dbBE_Redis_command_dump_create( dbBE_Redis_command_stage_spec_t *stage,
                                    dbBE_Redis_sr_buffer_t *sr_buf,
                                    char *keybuffer )
{
  return dbBE_Redis_command_create_str1( stage, sr_buf, keybuffer );
}


static inline
int dbBE_Redis_command_create_str2( dbBE_Redis_command_stage_spec_t *stage,
                                    dbBE_Redis_sr_buffer_t *sr_buf,
                                    char *arg1,
                                    char *arg2 )
{
  dbBE_sge_t args[ stage->_array_len + 1 ];
  args[ 0 ].iov_base = arg1;
  args[ 0 ].iov_len = strlen( arg1 );
  args[ 1 ].iov_base = arg2;
  args[ 1 ].iov_len = strlen( arg2 );
  args[ stage->_array_len ].iov_base = NULL;
  args[ stage->_array_len ].iov_len = 0;

  return dbBE_Redis_command_create_sgeN( stage, sr_buf, args );
}

static inline
int dbBE_Redis_command_create_str3( dbBE_Redis_command_stage_spec_t *stage,
                                    dbBE_Redis_sr_buffer_t *sr_buf,
                                    char *arg1,
                                    char *arg2,
                                    char *arg3 )
{
  dbBE_sge_t args[ stage->_array_len + 1 ];
  args[ 0 ].iov_base = arg1;
  args[ 0 ].iov_len = strlen( arg1 );
  args[ 1 ].iov_base = arg2;
  args[ 1 ].iov_len = strlen( arg2 );
  args[ 2 ].iov_base = arg3;
  args[ 2 ].iov_len = strlen( arg3 );
  args[ stage->_array_len ].iov_base = NULL;
  args[ stage->_array_len ].iov_len = 0;

  return dbBE_Redis_command_create_sgeN( stage, sr_buf, args );
}

int dbBE_Redis_command_hsetnx_create( dbBE_Redis_command_stage_spec_t *stage,
                                      dbBE_Redis_sr_buffer_t *sr_buf,
                                      char *name_space,
                                      char *field,
                                      char *value )
{
  return dbBE_Redis_command_create_str3( stage, sr_buf, name_space, field, value );
}

int dbBE_Redis_command_hset_create( dbBE_Redis_command_stage_spec_t *stage,
                                    dbBE_Redis_sr_buffer_t *sr_buf,
                                    char *name_space,
                                    char *field,
                                    char *value )
{
  return dbBE_Redis_command_create_str3( stage, sr_buf, name_space, field, value );
}



int dbBE_Redis_command_hmset_create( dbBE_Redis_command_stage_spec_t *stage,
                                     dbBE_Redis_sr_buffer_t *sr_buf,
                                     dbBE_Redis_request_t *request,
                                     dbBE_Data_transport_t *transport,
                                     char *keybuffer )
{
  int len = 0;
  int rc = 1; // used as multiplier to len to indicate error/success
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

  data._string._data = keybuffer;
  sprintf( keybuffer, "flags" );
  data._string._size = strnlen( data._string._data, DBBE_REDIS_MAX_KEY_LEN );
  len += Redis_insert_to_sr_buffer( sr_buf, dbBE_REDIS_TYPE_CHAR, &data );

  data._string._data = keybuffer;
  sprintf( keybuffer, "0" );
  data._string._size = strnlen( data._string._data, DBBE_REDIS_MAX_KEY_LEN );
  len += Redis_insert_to_sr_buffer( sr_buf, dbBE_REDIS_TYPE_CHAR, &data );

  return len * rc;
}

int dbBE_Redis_command_hincrby_create( dbBE_Redis_command_stage_spec_t *stage,
                                       dbBE_Redis_sr_buffer_t *sr_buf,
                                       char *name_space,
                                       int increment )
{
  char incbuf[32];
  dbBE_sge_t args[ stage->_array_len + 1 ];
  args[ 0 ].iov_base = name_space;
  args[ 0 ].iov_len = strlen( name_space );
  args[ 1 ].iov_base = incbuf;
  args[ 1 ].iov_len = (size_t)snprintf( incbuf, 31, "%d", increment );
  if( (int)args[ 1 ].iov_len < 0 )
    return -E2BIG;
  args[ stage->_array_len ].iov_base = NULL;
  args[ stage->_array_len ].iov_len = 0;

  return dbBE_Redis_command_create_sgeN( stage, sr_buf, args );
}

int dbBE_Redis_command_scan_create( dbBE_Redis_command_stage_spec_t *stage,
                                    dbBE_Redis_sr_buffer_t *sr_buf,
                                    char *keybuffer,
                                    char *cursor )
{
  char *startcursor = "0";
  dbBE_sge_t args[ stage->_array_len + 1 ];

  // decide the scan cursor (start or not)
  args[ 0 ].iov_base = startcursor;
  if( cursor != NULL )
    args[ 0 ].iov_base = cursor;

  args[ 0 ].iov_len = strlen( (char*)args[ 0 ].iov_base );

  // then the key
  args[ 1 ].iov_base = keybuffer;
  args[ 1 ].iov_len = strlen( keybuffer );
  args[ stage->_array_len ].iov_base = NULL;
  args[ stage->_array_len ].iov_len = 0;

  return dbBE_Redis_command_create_sgeN( stage, sr_buf, args );
}

int dbBE_Redis_command_restore_create( dbBE_Redis_command_stage_spec_t *stage,
                                       dbBE_Redis_sr_buffer_t *sr_buf,
                                       char *keybuffer,
                                       dbBE_Redis_intern_move_data_t dump_data )
{
  dbBE_sge_t args[ stage->_array_len + 1 ];

  args[0].iov_base = keybuffer;
  args[0].iov_len = strnlen( keybuffer, DBR_MAX_KEY_LEN );
  args[1].iov_base = dump_data.dumped_value;
  args[1].iov_len = dump_data.len;
  args[ stage->_array_len ].iov_base = NULL;
  args[ stage->_array_len ].iov_len = 0;

  return dbBE_Redis_command_create_sgeN( stage, sr_buf, args );
}

#endif /* BACKEND_REDIS_REDIS_CMDS_H_ */
