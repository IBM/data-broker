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
#include <stdio.h>

#include "../common/dbbe_api.h"
#include "../common/data_transport.h"
#include "definitions.h"
#include "protocol.h"
#include "create.h"
#include "redis_cmds.h"

/*
 * insert a Redis integer representation to the buffer
 * return the length of the string
 */
int Redis_insert_integer( char *buf, const int64_t value )
{
  if( buf == NULL )
    return -1;

  int len = sprintf( buf, ":%"PRId64"\r\n", value );
  if( len < 4 )
    return -1;

  return len;
}

/*
 * insert a Redis bulk string to the buffer
 * return the length of the string
 */
int Redis_insert_bulk_string( char *buf, const char *string )
{
  if( buf == NULL )
    return -1;

  int len;
  int exp_len = strlen( string ) + 6; // covers only expected minimal length (with 2-digit length, the string could be longer)

  if( string == NULL )
  {
    len = sprintf( buf, "$-1\r\n");
    exp_len = 5;
  }
  else
    len = sprintf( buf, "$%ld\r\n%s\r\n", strlen( string ), string );

  if( len < exp_len )
    return -1;
  return len;
}

int Redis_insert_array_head( char *buf, const unsigned item_count )
{
  if( buf == NULL )
    return -1;

  int len = sprintf( buf, "*%d\r\n", item_count );
  if( len < 4 )
    return -1;
  return len;
}

int Redis_insert_bulk_string_head( char *buf, const size_t size )
{
  if( buf == NULL )
    return -1;
  int len = sprintf( buf, "$%ld\r\n", size);
  if( len < 4 )
    return -1;
  return len;
}

/*
 * use for short data only
 * large data should use the transports
 */
int Redis_insert_raw( char *buf, const char *data )
{
  if( buf == NULL )
    return -1;
  size_t len = sprintf( buf, "%s", data );
  if( len < strlen( data ) )
    return -1;
  return (int)len;
}


/*
 * converts the typed data into Redis representations
 */
int Redis_insert_to_sr_buffer( dbBE_Redis_sr_buffer_t *sr_buf, dbBE_REDIS_DATA_TYPE type, dbBE_Redis_data_t *data )
{
  char *writepos = dbBE_Redis_sr_buffer_get_processed_position( sr_buf );
  int data_len = 0;
  switch( type )
  {
    case dbBE_REDIS_TYPE_CHAR: ///< character/string
      data_len = Redis_insert_bulk_string( writepos, data->_string._data );
      break;
    case dbBE_REDIS_TYPE_STRING_HEAD:
      data_len = Redis_insert_bulk_string_head( writepos, data->_integer );
      break;
    case dbBE_REDIS_TYPE_RAW:
      data_len = Redis_insert_raw( writepos, data->_string._data );
      break;
    case dbBE_REDIS_TYPE_INT:  ///< integer
      data_len = Redis_insert_integer( writepos, data->_integer );
      break;
    case dbBE_REDIS_TYPE_ARRAY: ///< an array of results is parsed off the string
      data_len = Redis_insert_array_head( writepos, data->_integer );
      break;

    case dbBE_REDIS_TYPE_ERROR: // makes no sense to send errors
    case dbBE_REDIS_TYPE_REDIRECT: // not sending any redirects
    case dbBE_REDIS_TYPE_RELOCATE: // not sending any relocates

    case dbBE_REDIS_TYPE_UNSPECIFIED:
    case dbBE_REDIS_TYPE_INVALID:
    case dbBE_REDIS_TYPE_MAX:
    default:
      return 0;
  }
  if( data_len < 0 )
    return 0;

  dbBE_Redis_sr_buffer_add_data( sr_buf, data_len, 1 );
  return data_len;
}


/*
 * convert a redis request into a command string in sr_buf
 * returns the number of bytes placed into the buffer or negative error
 */
int dbBE_Redis_create_command( dbBE_Redis_request_t *request,
                               dbBE_Redis_sr_buffer_t *sr_buf,
                               dbBE_Data_transport_t *transport )
{
  int rc = 0;

  if( ( request == NULL ) || ( sr_buf == NULL ) || ( transport == NULL ) ||
      ( request->_step == NULL ) || ( transport->gather == NULL ) || (transport->scatter == NULL ))
    return -EINVAL;

  char *writepos = dbBE_Redis_sr_buffer_get_processed_position( sr_buf ); // store position to rewind on error
  dbBE_Redis_command_stage_spec_t *stage = request->_step;
  dbBE_Redis_data_t data;
  int len = 0;
  char keybuffer[ DBBE_REDIS_MAX_KEY_LEN ];

  switch( request->_user->_opcode )
  {
    case DBBE_OPCODE_PUT: // RPUSH ns_name%sep;t_name value
    {
      if( stage->_stage != 0 ) // Put is only a single stage request
        return -EINVAL;

      size_t vallen = dbBE_SGE_get_len( request->_user->_sge, request->_user->_sge_count );

      dbBE_Redis_create_key( request, keybuffer, DBBE_REDIS_MAX_KEY_LEN );
      dbBE_Redis_command_cmdandkey_only_create( stage, sr_buf, keybuffer, vallen );
      int64_t slen = dbBE_Redis_command_create_insert_value( sr_buf,
                                                             transport,
                                                             request->_user->_sge,
                                                             request->_user->_sge_count,
                                                             vallen );
      if( slen != (int64_t)vallen )
        rc = -1;

      dbBE_Redis_command_create_terminate( sr_buf );

      break;
    }
    case DBBE_OPCODE_GET: // LPOP ns_name%sep;t_name
    {
      if( stage->_stage != 0 ) // Get is only a single stage request
        return -EINVAL;

      dbBE_Redis_create_key( request, keybuffer, DBBE_REDIS_MAX_KEY_LEN );
      len = dbBE_Redis_command_lpop_create( stage, sr_buf, keybuffer );

      break;
    }
    case DBBE_OPCODE_READ: // LINDEX ns_name%sep;t_name 0
    {
      if( stage->_stage != 0 ) // Read is only a single stage request
        return -EINVAL;

      dbBE_Redis_create_key( request, keybuffer, DBBE_REDIS_MAX_KEY_LEN );
      len = dbBE_Redis_command_lindex_create( stage, sr_buf, keybuffer );

      break;
    }
    case DBBE_OPCODE_MOVE:
    {
      fprintf(stderr, "%s:%s(): ERROR: ToDo not implemented\n", __FILE__, __FUNCTION__);
      break;
    }
    case DBBE_OPCODE_DIRECTORY:
    {
      switch( stage->_stage )
      {
        case DBBE_REDIS_DIRECTORY_STAGE_META:
          len += dbBE_Redis_command_hmgetall_create( stage, sr_buf, request->_user->_ns_name );
          break;
        case DBBE_REDIS_DIRECTORY_STAGE_SCAN:
          snprintf( keybuffer, DBBE_REDIS_MAX_KEY_LEN,
                    "%s%s%s",
                    request->_user->_ns_name,
                    DBBE_REDIS_NAMESPACE_SEPARATOR,
                    request->_user->_match );
          len += dbBE_Redis_command_scan_create( stage,
                                                 sr_buf,
                                                 keybuffer,
                                                 request->_user->_key );
          break;
        default:
          dbBE_Redis_sr_buffer_rewind_available_to( sr_buf, writepos );
          return -EINVAL;
      }
      break;
    }
    case DBBE_OPCODE_NSCREATE:
    {
      switch( stage->_stage )
      {
        case 0: // HSETNX ns_name id ns_name
          len += dbBE_Redis_command_hsetnx_create( stage, sr_buf, request->_user->_ns_name );
          break;

        case 1: // HMSET ns_name refcnt 1 groups permissions
          dbBE_Redis_create_key( request, keybuffer, DBBE_REDIS_MAX_KEY_LEN );
          len += dbBE_Redis_command_hmset_create( stage, sr_buf, request, transport, keybuffer );
          break;

        default:
          dbBE_Redis_sr_buffer_rewind_available_to( sr_buf, writepos );
          return -EINVAL;
      }
      break;
    }
    case DBBE_OPCODE_NSQUERY:
    {
      len += dbBE_Redis_command_hmgetall_create( stage, sr_buf, request->_user->_ns_name );

      break;
    }
    case DBBE_OPCODE_NSDELETE:
    {
      switch( stage->_stage )
      {
        case DBBE_REDIS_NSDELETE_STAGE_DETACH: // HINCRBY ns_name refcnt -1
          dbBE_Redis_create_key( request, keybuffer, DBBE_REDIS_MAX_KEY_LEN );
          len += dbBE_Redis_command_hincrby_create( stage, sr_buf, keybuffer, -1 );
          break;

        case DBBE_REDIS_NSDELETE_STAGE_SCAN: // SCAN 0 MATCH ns_name%sep;*
          snprintf( keybuffer, DBBE_REDIS_MAX_KEY_LEN, "%s%s*", request->_user->_ns_name, DBBE_REDIS_NAMESPACE_SEPARATOR );
          len += dbBE_Redis_command_scan_create( stage,
                                                 sr_buf,
                                                 keybuffer,
                                                 request->_status.nsdelete.scankey );
          break;

        case DBBE_REDIS_NSDELETE_STAGE_DELKEYS: // DEL ns_name%sep;key
          if( request->_status.nsdelete.scankey == NULL )
            return -EINVAL;

          len += dbBE_Redis_command_microcmd_create( stage, sr_buf, &data );
          if( len < 0 )
            break;
          data._string._data = request->_status.nsdelete.scankey;
          data._string._size = strnlen( data._string._data, DBBE_REDIS_MAX_KEY_LEN );
          len += Redis_insert_to_sr_buffer( sr_buf, dbBE_REDIS_TYPE_CHAR, &data );
          break;

        case DBBE_REDIS_NSDELETE_STAGE_DELNS: // DEL ns_name
          len += dbBE_Redis_command_microcmd_create( stage, sr_buf, &data );
          if( len < 0 )
            break;
          data._string._data = request->_user->_ns_name;
          data._string._size = strnlen( data._string._data, DBBE_REDIS_MAX_KEY_LEN );
          len += Redis_insert_to_sr_buffer( sr_buf, dbBE_REDIS_TYPE_CHAR, &data );
          break;

        default:
          dbBE_Redis_sr_buffer_rewind_available_to( sr_buf, writepos );
          return -EINVAL;
      }
      break;
    }
    case DBBE_OPCODE_NSATTACH: // EXISTS ns_name; HINCRBY ns_name refcnt 1
    {
      switch( stage->_stage )
      {
        case 0:
          len += dbBE_Redis_command_exists_create( stage, sr_buf, request->_user->_ns_name );
          break;

        case 1:
          dbBE_Redis_create_key( request, keybuffer, DBBE_REDIS_MAX_KEY_LEN );
          len += dbBE_Redis_command_hincrby_create( stage, sr_buf, keybuffer, 1 );
          break;

        default:
          return -EINVAL;
      }
      break;
    }
    case DBBE_OPCODE_NSDETACH: // EXISTS ns_name; HINCRBY ns_name refcnt -1
    {
      switch( stage->_stage )
      {
        case 0:
          len += dbBE_Redis_command_exists_create( stage, sr_buf, request->_user->_ns_name );
          break;

        case 1:
          dbBE_Redis_create_key( request, keybuffer, DBBE_REDIS_MAX_KEY_LEN );
          len += dbBE_Redis_command_hincrby_create( stage, sr_buf, keybuffer, -1 );
          break;

        default:
          return -EINVAL;
      }
      break;
    }
    case DBBE_OPCODE_NSADDUNITS:
    case DBBE_OPCODE_NSREMOVEUNITS:
    case DBBE_OPCODE_REMOVE:
    case DBBE_OPCODE_UNSPEC:
    case DBBE_OPCODE_MAX:
    default:
      // wrong opcode
      return -ENOSYS;
  }

  if( len < 0 )
    return -EBADMSG;

  return rc;
}

/*
 * create the key, based on the command type
 */
char* dbBE_Redis_create_key( dbBE_Redis_request_t *request, char *keybuf, uint16_t size )
{
  if( keybuf == NULL )
  {
    errno = EINVAL;
    return NULL;
  }

  memset( keybuf, 0, size );
  switch( request->_user->_opcode )
  {
    case DBBE_OPCODE_PUT:
    case DBBE_OPCODE_GET:
    case DBBE_OPCODE_READ:
    {
      int len = snprintf( keybuf, size, "%s%s%s",
                          request->_user->_ns_name,
                          DBBE_REDIS_NAMESPACE_SEPARATOR,
                          request->_user->_key );
      if(( len < 0 ) || ( len >= size ))
      {
        errno = EMSGSIZE;
        return NULL;
      }
      break;
    }
    case DBBE_OPCODE_MOVE:
    {
      fprintf(stderr, "%s:%s(): ERROR: ToDo not implemented\n", __FILE__, __FUNCTION__);
      break;
    }
    case DBBE_OPCODE_DIRECTORY:
    case DBBE_OPCODE_NSCREATE:
    case DBBE_OPCODE_NSATTACH:
    case DBBE_OPCODE_NSDETACH:
    case DBBE_OPCODE_NSDELETE:
    case DBBE_OPCODE_NSQUERY:
    {
      int len = snprintf( keybuf, size, "%s", request->_user->_ns_name );
      if(( len < 0 ) || ( len >= size ))
      {
        errno = EMSGSIZE;
        return NULL;
      }
      break;
    }
    case DBBE_OPCODE_NSADDUNITS:
    case DBBE_OPCODE_NSREMOVEUNITS:
    case DBBE_OPCODE_CANCEL:
    case DBBE_OPCODE_REMOVE:
      errno = ENOSYS;
      return NULL;
    case DBBE_OPCODE_UNSPEC:
    case DBBE_OPCODE_MAX:
      errno = EINVAL;
      return NULL;
  }
  return keybuf;
}
