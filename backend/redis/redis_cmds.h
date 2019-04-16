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

#include "logutil.h"
#include "transports/sr_buffer.h"
#include "protocol.h"
#include "request.h"

#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

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

int dbBE_Redis_create_key_cmd( dbBE_Redis_request_t *request, char *keybuf, uint16_t size )
{
  if( keybuf == NULL )
    return -EINVAL;

  int len = 0;
  switch( request->_user->_opcode )
  {
    case DBBE_OPCODE_PUT:
    case DBBE_OPCODE_GET:
    case DBBE_OPCODE_READ:
    case DBBE_OPCODE_REMOVE:
    {
      int keylen = strnlen( request->_user->_ns_name, size ) + DBBE_REDIS_NAMESPACE_SEPARATOR_LEN + strnlen( request->_user->_key, size );
      len = snprintf( keybuf, size, "$%d\r\n%s%s%s\r\n",
                      keylen,
                      request->_user->_ns_name,
                      DBBE_REDIS_NAMESPACE_SEPARATOR,
                      request->_user->_key );
      if(( len < 0 ) || ( len >= size ))
        return -EMSGSIZE;
      break;
    }
    case DBBE_OPCODE_DIRECTORY:
    case DBBE_OPCODE_NSCREATE:
    case DBBE_OPCODE_NSQUERY:
    case DBBE_OPCODE_NSATTACH:
    case DBBE_OPCODE_NSDELETE:
    {
      int keylen = strnlen( request->_user->_ns_name, size );
      len = snprintf( keybuf, size, "$%d\r\n%s\r\n",
                      keylen,
                      request->_user->_ns_name );
      break;
    }

    case DBBE_OPCODE_NSDETACH:
    {
      switch( request->_step->_stage )
      {
        case DBBE_REDIS_NSDETACH_STAGE_DELCHECK: // HINCRBY ns_name refcnt -1; HMGET ns_name refcnt flags
        case DBBE_REDIS_NSDETACH_STAGE_DELNS: // DEL ns_name
        {
          int keylen = strnlen( request->_user->_ns_name, size );
          len = snprintf( keybuf, size, "$%d\r\n%s\r\n",
                          keylen,
                          request->_user->_ns_name );
          break;
        }
        case DBBE_REDIS_NSDETACH_STAGE_SCAN: // SCAN 0 MATCH ns_name%sep;*
          return -ENOSYS;
        case DBBE_REDIS_NSDETACH_STAGE_DELKEYS: // DEL ns_name%sep;key  (already complete key in nsdetach.scankey)
        {
          int keylen = strnlen( request->_status.nsdetach.scankey, size );
          len = snprintf( keybuf, size, "$%d\r\n%s\r\n",
                          keylen,
                          request->_status.nsdetach.scankey );
          if(( len < 0 ) || ( len >= size ))
            return -EMSGSIZE;
          break;
        }
        default:
          return -EPROTO;
      }
      break;
    }
    case DBBE_OPCODE_MOVE:
    {
      char *ns = NULL;
      switch( request->_step->_stage )
      {
        case DBBE_REDIS_MOVE_STAGE_RESTORE: // restore stage uses the new namespace for the key
          ns = (char*)request->_user->_sge[0].iov_base;
          break;
        default:
          ns = request->_user->_ns_name;
          break;
      }
      if( ns == NULL )
        return -EINVAL;

      int keylen = strnlen( ns, size ) + DBBE_REDIS_NAMESPACE_SEPARATOR_LEN + strnlen( request->_user->_key, size );
      len = snprintf( keybuf, size, "$%d\r\n%s%s%s\r\n",
                      keylen,
                      ns,
                      DBBE_REDIS_NAMESPACE_SEPARATOR,
                      request->_user->_key );
      if(( len < 0 ) || ( len >= size ))
        return -EMSGSIZE;
      break;
    }
    default:
      return -ENOSYS;
  }
  return len;
}


int64_t dbBE_Redis_command_create_insert_value( dbBE_Redis_sr_buffer_t *sr_buf,
                                                dbBE_Data_transport_t *transport,
                                                dbBE_sge_t *sge,
                                                int sge_count,
                                                int64_t vallen )
{
  int64_t slen = transport->gather( (dbBE_Data_transport_device_t*)dbBE_Transport_sr_buffer_get_processed_position( sr_buf ),
                                    dbBE_Transport_sr_buffer_remaining( sr_buf ),
                                    sge_count,
                                    sge );
  // data had to be truncated
  if( slen < 0 )
    return -slen;

  return dbBE_Transport_sr_buffer_add_data( sr_buf, vallen, 1 );
}

static inline
int dbBE_Redis_command_create_terminate( dbBE_Redis_sr_buffer_t *sr_buf )
{
  return dbBE_Transport_sr_buffer_add_data( sr_buf,
                                        Redis_insert_redis_terminator( dbBE_Transport_sr_buffer_get_processed_position( sr_buf )),
                                        1 );
}

static inline
int dbBE_Redis_command_create_sr_buffer_field( dbBE_Redis_sr_buffer_t *buf,
                                               char *field,
                                               size_t len,
                                               dbBE_sge_t *sge )
{
  // create and insert field entry
  char *fld = dbBE_Transport_sr_buffer_get_available_position( buf );
  int fldlen = 0;
  if( field == NULL )
    fldlen = snprintf( fld, dbBE_Transport_sr_buffer_remaining( buf ), "$0\r\n\r\n" ); // insert empty-string
  else
    fldlen = snprintf( fld, dbBE_Transport_sr_buffer_remaining( buf ), "$%"PRId64"\r\n%s\r\n", len, field );
  if( fldlen <= 0 )
    return -E2BIG;
  if( dbBE_Transport_sr_buffer_add_data( buf, fldlen, 1 ) != (size_t)fldlen )
    return -E2BIG;

  sge->iov_base = fld;
  sge->iov_len = fldlen;

  return 0;
}


int dbBE_Redis_command_put_parse( dbBE_Redis_command_stage_spec_t spec,
                                  dbBE_Redis_result_t *result )
{
  return -ENOSYS;
}


#define DBBE_REDIS_CMD_REWIND_BUF_AND_ERROR( err, sr_buf, position ) \
    { \
      dbBE_Transport_sr_buffer_rewind_processed_to( (sr_buf), (position) ); \
      return ( err ); \
    }

#define DBBE_REDIS_CMD_REWIND_CMD_AND_ERROR( err, cmd_idx ) \
    { \
      cmd_idx = 0; \
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
  char *initial = dbBE_Transport_sr_buffer_get_processed_position( sr_buf );
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
    int len = snprintf( dbBE_Transport_sr_buffer_get_processed_position( sr_buf ),
                        dbBE_Transport_sr_buffer_remaining( sr_buf ),
                        "%s", cmdptr );
    *loc = tmp;
    if( len < 0 ) // can be 0 if 2 args appear back-to-back
      DBBE_REDIS_CMD_REWIND_BUF_AND_ERROR( -ENOMEM, sr_buf, initial );

    rc += dbBE_Transport_sr_buffer_add_data( sr_buf, len, 1 );

    // insert args[n]
    len = snprintf( dbBE_Transport_sr_buffer_get_processed_position( sr_buf ),
                    dbBE_Transport_sr_buffer_remaining( sr_buf ),
                    "$%ld\r\n", args[ idx ].iov_len );
    if( len < 4 ) // fixed chars + single digit number + one-length argument
      DBBE_REDIS_CMD_REWIND_BUF_AND_ERROR( -ENOMEM, sr_buf, initial );

    rc += dbBE_Transport_sr_buffer_add_data( sr_buf, len, 1 );

    size_t maxlen = args[ idx ].iov_len;
    if( maxlen > dbBE_Transport_sr_buffer_remaining( sr_buf ) - 2 ) // -2 because of cmd terminator
      maxlen = dbBE_Transport_sr_buffer_remaining( sr_buf ) - 2;
    memcpy( dbBE_Transport_sr_buffer_get_processed_position( sr_buf ),
            args[ idx ].iov_base,
            maxlen );

    rc += dbBE_Transport_sr_buffer_add_data( sr_buf, maxlen, 1 );
    rc += dbBE_Redis_command_create_terminate( sr_buf );

    cmdptr = loc + 2;
    ++n;
  }

  // add any remaining/trailing cmd string data
  if( cmdptr < cmdend )
  {
    int len = snprintf( dbBE_Transport_sr_buffer_get_processed_position( sr_buf ),
                        dbBE_Transport_sr_buffer_remaining( sr_buf ),
                        "%s", cmdptr );
    if( len <= 0 ) // if equal to zero, then cmdptr == cmdend - can't be true here
      DBBE_REDIS_CMD_REWIND_BUF_AND_ERROR( -ENOMEM, sr_buf, initial );

    rc += dbBE_Transport_sr_buffer_add_data( sr_buf, len, 1 );
  }

  return rc;
}

static inline
int dbBE_Redis_command_create_sgeN_uncheck( dbBE_Redis_command_stage_spec_t *stage,
                                            dbBE_sge_t *args,
                                            dbBE_sge_t *cmd )
{
  char *cmdptr = stage->_command;
  char *cmdend = stage->_command + strlen( stage->_command );

  int cmd_idx = 0;

  while((cmdptr < cmdend ))
  {
    // get next location of parameter
    char *loc = index( cmdptr, '%' );

    // no more parameters. break and add the remaining cmd string
    if( loc == NULL )
      break;

    // index() did something funky...
    if(( loc > cmdend ) || ( *loc != '%' ))
      DBBE_REDIS_CMD_REWIND_CMD_AND_ERROR( -EBADMSG, cmd_idx );

    // insert chars from defined cmd string, if any
    if( loc != cmdptr )
    {
      cmd[ cmd_idx ].iov_base = cmdptr;
      cmd[ cmd_idx ].iov_len = (size_t)(loc - cmdptr);
      cmdptr += cmd[ cmd_idx ].iov_len;
      ++cmd_idx;
    }

    // what positional arg to insert?
    int idx = (int)loc[1] - 48;
    if(( idx < 0 )  ||  ( idx >= stage->_array_len ))
      DBBE_REDIS_CMD_REWIND_CMD_AND_ERROR( -EBADMSG, cmd_idx );

    if( args[ idx ].iov_base == NULL )
      break;

    // insert args[n]
    if( args[ idx ].iov_len > 0 )
    {
      cmd[ cmd_idx ].iov_base = args[ idx ].iov_base;
      cmd[ cmd_idx ].iov_len = args[ idx ].iov_len;
      ++cmd_idx;
    }
    cmdptr = loc + 2;
  }

  // add any remaining/trailing cmd string data
  if( cmdptr < cmdend )
  {
    cmd[ cmd_idx ].iov_base = cmdptr;
    cmd[ cmd_idx ].iov_len = (size_t)( cmdend - cmdptr );
    ++cmd_idx;
  }

  return cmd_idx;
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

int dbBE_Redis_command_lpop_create( dbBE_Redis_request_t *req,
                                    dbBE_Redis_sr_buffer_t *buf,
                                    dbBE_sge_t *cmd )
{
  char *key = dbBE_Transport_sr_buffer_get_available_position( buf );
  int keylen = dbBE_Redis_create_key_cmd( req, key,
                                          dbBE_Transport_sr_buffer_remaining( buf ) >= DBBE_REDIS_MAX_KEY_LEN ? DBBE_REDIS_MAX_KEY_LEN : dbBE_Transport_sr_buffer_remaining( buf ) );
  if( keylen < 0 )
    return keylen;
  if( dbBE_Transport_sr_buffer_add_data( buf, keylen, 1 ) != (size_t)keylen )
    return -E2BIG;

  dbBE_sge_t sge[ req->_step->_array_len + 1 ];
  sge[ req->_step->_array_len ].iov_base = NULL;
  sge[ req->_step->_array_len ].iov_len = 0;

  sge[0].iov_base = key;
  sge[0].iov_len = keylen;
  return dbBE_Redis_command_create_sgeN_uncheck( req->_step, sge, cmd );
}


int dbBE_Redis_command_lindex_create( dbBE_Redis_request_t *req,
                                      dbBE_Redis_sr_buffer_t *buf,
                                      dbBE_sge_t *cmd )
{
  char *key = dbBE_Transport_sr_buffer_get_available_position( buf );
  int keylen = dbBE_Redis_create_key_cmd( req, key,
                                          dbBE_Transport_sr_buffer_remaining( buf ) >= DBBE_REDIS_MAX_KEY_LEN ? DBBE_REDIS_MAX_KEY_LEN : dbBE_Transport_sr_buffer_remaining( buf ) );
  if( keylen < 0 )
    return keylen;
  if( dbBE_Transport_sr_buffer_add_data( buf, keylen, 1 ) != (size_t)keylen )
    return -E2BIG;

  dbBE_sge_t sge[ req->_step->_array_len ];
  sge[ req->_step->_array_len ].iov_base = NULL;
  sge[ req->_step->_array_len ].iov_len = 0;

  sge[0].iov_base = key;
  sge[0].iov_len = keylen;
  return dbBE_Redis_command_create_sgeN_uncheck( req->_step, sge, cmd );
}

int dbBE_Redis_command_del_create( dbBE_Redis_request_t *req,
                                   dbBE_Redis_sr_buffer_t *buf,
                                   dbBE_sge_t *cmd )
{
  char *key = dbBE_Transport_sr_buffer_get_available_position( buf );
  int keylen = dbBE_Redis_create_key_cmd( req, key,
                                          dbBE_Transport_sr_buffer_remaining( buf ) >= DBBE_REDIS_MAX_KEY_LEN ? DBBE_REDIS_MAX_KEY_LEN : dbBE_Transport_sr_buffer_remaining( buf ) );
  if( keylen < 0 )
    return keylen;
  if( dbBE_Transport_sr_buffer_add_data( buf, keylen, 1 ) != (size_t)keylen )
    return -E2BIG;

  dbBE_sge_t sge[ req->_step->_array_len + 1 ];
  sge[ req->_step->_array_len ].iov_base = NULL;
  sge[ req->_step->_array_len ].iov_len = 0;

  sge[0].iov_base = key;
  sge[0].iov_len = keylen;
  return dbBE_Redis_command_create_sgeN_uncheck( req->_step, sge, cmd );
}

int dbBE_Redis_command_hmgetall_create( dbBE_Redis_request_t *req,
                                        dbBE_Redis_sr_buffer_t *buf,
                                        dbBE_sge_t *cmd )
{
  char *key = dbBE_Transport_sr_buffer_get_available_position( buf );
  int keylen = dbBE_Redis_create_key_cmd( req, key,
                                          dbBE_Transport_sr_buffer_remaining( buf ) >= DBBE_REDIS_MAX_KEY_LEN ? DBBE_REDIS_MAX_KEY_LEN : dbBE_Transport_sr_buffer_remaining( buf ) );
  if( keylen < 0 )
    return keylen;
  if( dbBE_Transport_sr_buffer_add_data( buf, keylen, 1 ) != (size_t)keylen )
    return -E2BIG;

  dbBE_sge_t sge[ 1 ];
  sge[0].iov_base = key;
  sge[0].iov_len = keylen;

  return dbBE_Redis_command_create_sgeN_uncheck( req->_step, sge, cmd );
}

int dbBE_Redis_command_hmget_create( dbBE_Redis_request_t *req,
                                     dbBE_Redis_sr_buffer_t *buf,
                                     dbBE_sge_t *cmd )
{
  char *key = dbBE_Transport_sr_buffer_get_available_position( buf );
  int keylen = dbBE_Redis_create_key_cmd( req, key,
                                          dbBE_Transport_sr_buffer_remaining( buf ) >= DBBE_REDIS_MAX_KEY_LEN ? DBBE_REDIS_MAX_KEY_LEN : dbBE_Transport_sr_buffer_remaining( buf ) );
  if( keylen < 0 )
    return keylen;
  if( dbBE_Transport_sr_buffer_add_data( buf, keylen, 1 ) != (size_t)keylen )
    return -E2BIG;

  dbBE_sge_t sge[ req->_step->_array_len + 1 ];
  sge[ req->_step->_array_len ].iov_base = NULL;
  sge[ req->_step->_array_len ].iov_len = 0;

  sge[0].iov_base = key;
  sge[0].iov_len = keylen;

  return dbBE_Redis_command_create_sgeN_uncheck( req->_step, sge, cmd );
}

int dbBE_Redis_command_exists_create( dbBE_Redis_request_t *req,
                                      dbBE_Redis_sr_buffer_t *buf,
                                      dbBE_sge_t *cmd )
{
  dbBE_Redis_command_stage_spec_t *stage = req->_step;

  char *key = dbBE_Transport_sr_buffer_get_available_position( buf );
  int keylen = dbBE_Redis_create_key_cmd( req, key,
                                          dbBE_Transport_sr_buffer_remaining( buf ) >= DBBE_REDIS_MAX_KEY_LEN ? DBBE_REDIS_MAX_KEY_LEN : dbBE_Transport_sr_buffer_remaining( buf ) );
  if( keylen < 0 )
    return keylen;
  if( dbBE_Transport_sr_buffer_add_data( buf, keylen, 1 ) != (size_t)keylen )
    return -E2BIG;

  dbBE_sge_t sge[ stage->_array_len + 1 ];
  sge[ stage->_array_len ].iov_base = NULL;
  sge[ stage->_array_len ].iov_len = 0;

  sge[0].iov_base = key;
  sge[0].iov_len = keylen;

  return dbBE_Redis_command_create_sgeN_uncheck( stage, sge, cmd );
}

int dbBE_Redis_command_dump_create(  dbBE_Redis_request_t *req,
                                     dbBE_Redis_sr_buffer_t *buf,
                                     dbBE_sge_t *cmd )
{
 dbBE_Redis_command_stage_spec_t *stage = req->_step;

 char *key = dbBE_Transport_sr_buffer_get_available_position( buf );
 int keylen = dbBE_Redis_create_key_cmd( req, key,
                                         dbBE_Transport_sr_buffer_remaining( buf ) >= DBBE_REDIS_MAX_KEY_LEN ? DBBE_REDIS_MAX_KEY_LEN : dbBE_Transport_sr_buffer_remaining( buf ) );
 if( keylen < 0 )
   return keylen;
 if( dbBE_Transport_sr_buffer_add_data( buf, keylen, 1 ) != (size_t)keylen )
   return -E2BIG;

 dbBE_sge_t sge[ stage->_array_len + 1 ];
 sge[ stage->_array_len ].iov_base = NULL;
 sge[ stage->_array_len ].iov_len = 0;

 sge[0].iov_base = key;
 sge[0].iov_len = keylen;

 return dbBE_Redis_command_create_sgeN_uncheck( stage, sge, cmd );
}


int dbBE_Redis_command_restore_create( dbBE_Redis_request_t *req,
                                       dbBE_Redis_sr_buffer_t *buf,
                                       dbBE_sge_t *cmd )
{
  dbBE_Redis_command_stage_spec_t *stage = req->_step;
  dbBE_sge_t sge[ stage->_array_len + 1 ];
  sge[ stage->_array_len ].iov_base = NULL;
  sge[ stage->_array_len ].iov_len = 0;

  char *bstart = dbBE_Transport_sr_buffer_get_available_position( buf );
  char *key = bstart;
  int keylen = dbBE_Redis_create_key_cmd( req, key,
                                          dbBE_Transport_sr_buffer_remaining( buf ) >= DBBE_REDIS_MAX_KEY_LEN ? DBBE_REDIS_MAX_KEY_LEN : dbBE_Transport_sr_buffer_remaining( buf ) );
  if( keylen < 0 )
    return keylen;
  if( dbBE_Transport_sr_buffer_add_data( buf, keylen, 1 ) != (size_t)keylen )
    goto error;

  // todo: see note in protocol.c
  // create dump-value prefix
  char *prefix = dbBE_Transport_sr_buffer_get_available_position( buf );
  int prefix_len = snprintf( prefix, dbBE_Transport_sr_buffer_remaining( buf ),
                             "$%"PRId64"\r\n", req->_status.move.len );
  if(( prefix_len < 0 ) || ( ( dbBE_Transport_sr_buffer_add_data( buf, prefix_len, 1 ) != (size_t)prefix_len ) ))
    goto error;

  sge[0].iov_base = key;
  sge[0].iov_len = keylen;
  sge[1].iov_base = prefix;
  sge[1].iov_len = prefix_len;
  sge[2].iov_base = req->_status.move.dumped_value;
  sge[2].iov_len = req->_status.move.len;

  return dbBE_Redis_command_create_sgeN_uncheck( stage, sge, cmd );

error:
  dbBE_Transport_sr_buffer_rewind_available_to( buf, bstart );
  return -E2BIG;
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

int dbBE_Redis_command_hsetnx_create( dbBE_Redis_request_t *req,
                                      dbBE_Redis_sr_buffer_t *buf,
                                      dbBE_sge_t *cmd,
                                      char *field,
                                      char *value )
{
  dbBE_Redis_command_stage_spec_t *stage = req->_step;
  dbBE_sge_t sge[ stage->_array_len + 1 ];
  sge[ stage->_array_len ].iov_base = NULL;
  sge[ stage->_array_len ].iov_len = 0;

  // save start position for potential rewind
  char *bstart = dbBE_Transport_sr_buffer_get_available_position( buf );

  // create and insert key
  char *key = dbBE_Transport_sr_buffer_get_available_position( buf );
  int keylen = dbBE_Redis_create_key_cmd( req, key,
                                          dbBE_Transport_sr_buffer_remaining( buf ) >= DBBE_REDIS_MAX_KEY_LEN ? DBBE_REDIS_MAX_KEY_LEN : dbBE_Transport_sr_buffer_remaining( buf ) );
  if( keylen < 0 )
    return keylen;
  if( dbBE_Transport_sr_buffer_add_data( buf, keylen, 1 ) != (size_t)keylen )
    return -E2BIG;

  sge[0].iov_base = key;
  sge[0].iov_len = keylen;

  // create and insert field entry
  if( dbBE_Redis_command_create_sr_buffer_field( buf, field, strlen( field ), &sge[1] ) != 0 )
  {
    dbBE_Transport_sr_buffer_rewind_available_to( buf, bstart );
    return -E2BIG;
  }

  // create and insert value entry
  if( dbBE_Redis_command_create_sr_buffer_field( buf, value, strlen( value ), &sge[2] ) != 0 )
  {
    dbBE_Transport_sr_buffer_rewind_available_to( buf, bstart );
    return -E2BIG;
  }

  return dbBE_Redis_command_create_sgeN_uncheck( stage, sge, cmd );
}

int dbBE_Redis_command_hset_create( dbBE_Redis_request_t *req,
                                    dbBE_Redis_sr_buffer_t *buf,
                                    dbBE_sge_t *cmd,
                                    char *field,
                                    char *value )
{
  dbBE_Redis_command_stage_spec_t *stage = req->_step;

  dbBE_sge_t sge[ stage->_array_len + 1 ];
  sge[ stage->_array_len ].iov_base = NULL;
  sge[ stage->_array_len ].iov_len = 0;

  // create and insert key
  char *bstart = dbBE_Transport_sr_buffer_get_available_position( buf );
  char *key = bstart;
  int keylen = dbBE_Redis_create_key_cmd( req, key,
                                          dbBE_Transport_sr_buffer_remaining( buf ) >= DBBE_REDIS_MAX_KEY_LEN ? DBBE_REDIS_MAX_KEY_LEN : dbBE_Transport_sr_buffer_remaining( buf ) );
  if( keylen < 0 )
    return keylen;
  if( dbBE_Transport_sr_buffer_add_data( buf, keylen, 1 ) != (size_t)keylen )
    goto error;

  sge[0].iov_base = key;
  sge[0].iov_len = keylen;

  if( dbBE_Redis_command_create_sr_buffer_field( buf, field, strlen(field), &sge[1] ) != 0 )
    goto error;

  if( dbBE_Redis_command_create_sr_buffer_field( buf, value, strlen(value), &sge[2] ) != 0 )
    goto error;

  return dbBE_Redis_command_create_sgeN_uncheck( stage, sge, cmd );

error:
  dbBE_Transport_sr_buffer_rewind_available_to( buf, bstart );
  return -E2BIG;
}



int dbBE_Redis_command_hmset_create( dbBE_Redis_request_t *req,
                                     dbBE_Redis_sr_buffer_t *buf,
                                     dbBE_sge_t *cmd )
{
  dbBE_Redis_command_stage_spec_t *stage = req->_step;
  dbBE_sge_t sge[ stage->_array_len + 1 ];
  sge[ stage->_array_len ].iov_base = NULL;
  sge[ stage->_array_len ].iov_len = 0;

  // create and insert key
  char *bstart = dbBE_Transport_sr_buffer_get_available_position( buf );
  char *key = bstart;
  int keylen = dbBE_Redis_create_key_cmd( req, key,
                                          dbBE_Transport_sr_buffer_remaining( buf ) >= DBBE_REDIS_MAX_KEY_LEN ? DBBE_REDIS_MAX_KEY_LEN : dbBE_Transport_sr_buffer_remaining( buf ) );
  if( keylen < 0 )
    return keylen;
  if( dbBE_Transport_sr_buffer_add_data( buf, keylen, 1 ) != (size_t)keylen )
    return -E2BIG;

  sge[0].iov_base = key;
  sge[0].iov_len = keylen;

  if( dbBE_Redis_command_create_sr_buffer_field( buf, "refcnt", 6, &sge[1] ) != 0 )
    goto error;

  if( dbBE_Redis_command_create_sr_buffer_field( buf, "1", 1, &sge[2] ) != 0 )
    goto error;

  if( dbBE_Redis_command_create_sr_buffer_field( buf, "groups", 6, &sge[3] ) != 0 )
    goto error;

  // insert the groups list
  // todo: this currently only support the grouplist to reside in sge[0] of the request
  if( dbBE_Redis_command_create_sr_buffer_field( buf,
                                                 req->_user->_sge[0].iov_base,
                                                 req->_user->_sge[0].iov_len,
                                                 &sge[4] ) != 0 )
    goto error;

  if( dbBE_Redis_command_create_sr_buffer_field( buf, "flags", 5, &sge[5] ) != 0 )
    goto error;

  if( dbBE_Redis_command_create_sr_buffer_field( buf, "0", 1, &sge[6] ) != 0 )
    goto error;

  return dbBE_Redis_command_create_sgeN_uncheck( stage, sge, cmd );

error:
  dbBE_Transport_sr_buffer_rewind_available_to( buf, bstart );
  return -E2BIG;
}

int dbBE_Redis_command_hincrby_create( dbBE_Redis_request_t *req,
                                       dbBE_Redis_sr_buffer_t *buf,
                                       dbBE_sge_t *cmd,
                                       int increment )
{
  dbBE_sge_t sge[ req->_step->_array_len + 1 ];
  sge[ req->_step->_array_len ].iov_base = NULL;
  sge[ req->_step->_array_len ].iov_len = 0;

  // create and insert key
  char *bstart = dbBE_Transport_sr_buffer_get_available_position( buf );
  char *key = bstart;
  int keylen = dbBE_Redis_create_key_cmd( req, key,
                                          dbBE_Transport_sr_buffer_remaining( buf ) >= DBBE_REDIS_MAX_KEY_LEN ? DBBE_REDIS_MAX_KEY_LEN : dbBE_Transport_sr_buffer_remaining( buf ) );
  if( keylen < 0 )
    return keylen;
  if( dbBE_Transport_sr_buffer_add_data( buf, keylen, 1 ) != (size_t)keylen )
    goto error;

  sge[0].iov_base = key;
  sge[0].iov_len = keylen;

  char incbuf[ 32 ];
  int len = snprintf( incbuf, 31, "%d", increment );
  if( dbBE_Redis_command_create_sr_buffer_field( buf, incbuf, len, &sge[ 1 ] ) != 0 )
    goto error;

  return dbBE_Redis_command_create_sgeN_uncheck( req->_step, sge, cmd );

error:
  dbBE_Transport_sr_buffer_rewind_available_to( buf, bstart );
  return -E2BIG;
}

int dbBE_Redis_command_delcheck_create( dbBE_Redis_request_t *req,
                                        dbBE_Redis_sr_buffer_t *buf,
                                        dbBE_sge_t *cmd,
                                        int increment )
{
  dbBE_Redis_command_stage_spec_t *stage = req->_step;
  dbBE_sge_t sge[ stage->_array_len + 1 ];

  // create and insert key
  char *bstart = dbBE_Transport_sr_buffer_get_available_position( buf );
  char *key = bstart;
  int keylen = dbBE_Redis_create_key_cmd( req, key,
                                          dbBE_Transport_sr_buffer_remaining( buf ) >= DBBE_REDIS_MAX_KEY_LEN ? DBBE_REDIS_MAX_KEY_LEN : dbBE_Transport_sr_buffer_remaining( buf ) );
  if( keylen < 0 )
    return keylen;
  if( dbBE_Transport_sr_buffer_add_data( buf, keylen, 1 ) != (size_t)keylen )
    goto error;

  sge[0].iov_base = key;
  sge[0].iov_len = keylen;

  // create and insert decrement value
  char incbuf[ 32 ];
  int len = snprintf( incbuf, 31, "%d", increment );
  if( dbBE_Redis_command_create_sr_buffer_field( buf, incbuf, len, &sge[ 1 ] ) != 0 )
    goto error;

  return dbBE_Redis_command_create_sgeN_uncheck( stage, sge, cmd );

error:
  dbBE_Transport_sr_buffer_rewind_available_to( buf, bstart );
  return -E2BIG;
}

int dbBE_Redis_command_scan_create( dbBE_Redis_request_t *request,
                                    dbBE_Redis_sr_buffer_t *sr_buf,
                                    dbBE_sge_t *cmd,
                                    dbBE_sge_t *key,
                                    char *cursor )
{
  dbBE_Redis_command_stage_spec_t *stage = request->_step;
  dbBE_sge_t args[ stage->_array_len + 1 ];
  args[ stage->_array_len ].iov_base = NULL;
  args[ stage->_array_len ].iov_len = 0;

  // save start position for rewind after error
  char *bstart = dbBE_Transport_sr_buffer_get_available_position( sr_buf );

  // decide the scan cursor (start or not)
  int rc = 0;
  if( cursor == NULL )
    rc = dbBE_Redis_command_create_sr_buffer_field( sr_buf, "0", 1, &args[0] );
  else
    rc = dbBE_Redis_command_create_sr_buffer_field( sr_buf, cursor, strlen( cursor ), &args[0] );

  if( rc != 0 )
  {
    dbBE_Transport_sr_buffer_rewind_available_to( sr_buf, bstart );
    return -E2BIG;
  }

  // then the key
  args[ 1 ].iov_base = key->iov_base;
  args[ 1 ].iov_len = key->iov_len;

  return dbBE_Redis_command_create_sgeN_uncheck( stage, args, cmd );
}

int dbBE_Redis_command_rpush_create( dbBE_Redis_request_t *request,
                                     dbBE_Redis_sr_buffer_t *buf,
                                     dbBE_sge_t *cmd )
{
  int rc = 0;
  dbBE_Redis_command_stage_spec_t *stage = request->_step;

  if( stage->_stage != 0 ) // Put is only a single stage request
    return -EINVAL;

  // create key
  char *key = dbBE_Transport_sr_buffer_get_available_position( buf );
  int keylen = dbBE_Redis_create_key_cmd( request, key,
                                          dbBE_Transport_sr_buffer_remaining( buf ) >= DBBE_REDIS_MAX_KEY_LEN ? DBBE_REDIS_MAX_KEY_LEN : dbBE_Transport_sr_buffer_remaining( buf ) );
  dbBE_Transport_sr_buffer_add_data( buf, keylen, 1 );

  // insert key into cmd sge
  dbBE_sge_t args[2];
  args[0].iov_base = key;
  args[0].iov_len = keylen;
  args[1].iov_base = "";  // add empty dummy argument as the value
  args[1].iov_len = 0;

  rc = dbBE_Redis_command_create_sgeN_uncheck( stage, args, cmd );
  if( rc < 0 )
  {
    LOG( DBG_ERR, stderr, "Failed to create PUT cmd+key. rc=%d\n", rc );
    return rc;
  }

  // add value
  size_t vallen = dbBE_SGE_get_len( request->_user->_sge, request->_user->_sge_count );

  // insert lenth-prefix to buffer
  char *valpre = dbBE_Transport_sr_buffer_get_available_position( buf );
  if( dbBE_Transport_sr_buffer_remaining( buf ) <= vallen - 2 )
    return -E2BIG;

  int valprelen = snprintf( valpre, dbBE_Transport_sr_buffer_remaining( buf ), "$%"PRId64"\r\n", vallen );
  if( valprelen < 4 )
  {
    dbBE_Transport_sr_buffer_rewind_available_to( buf, key );
    return -E2BIG;
  }
  dbBE_Transport_sr_buffer_add_data( buf, valprelen, 1 );
  int idx = rc;

  // add sge in cmds sequence
  cmd[ idx ].iov_base = valpre;
  cmd[ idx ].iov_len = valprelen;
  ++idx;

  // insert value sge
  memcpy( &cmd[idx],
          request->_user->_sge,
          request->_user->_sge_count * sizeof( dbBE_sge_t ) );
  idx += request->_user->_sge_count;

  // terminate
  cmd[ idx ].iov_base = &stage->_command[2];
  cmd[ idx ].iov_len = 2;
  ++idx;

  return idx;
}

#endif /* BACKEND_REDIS_REDIS_CMDS_H_ */
