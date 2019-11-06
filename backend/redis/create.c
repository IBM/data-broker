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

#include "../common/dbbe_api.h"
#include "../common/data_transport.h"
#include "definitions.h"
#include "protocol.h"
#include "create.h"
#include "redis_cmds.h"
#include "namespace.h"

#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

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
int Redis_insert_raw( char *buf, const char *data, const size_t size )
{
  if( buf == NULL )
    return -1;
  memcpy( buf, data, size );
  return (int)size;
}


/*
 * converts the typed data into Redis representations
 */
int Redis_insert_to_sr_buffer( dbBE_Redis_sr_buffer_t *sr_buf, dbBE_REDIS_DATA_TYPE type, dbBE_Redis_data_t *data )
{
  char *writepos = dbBE_Transport_sr_buffer_get_processed_position( sr_buf );
  int data_len = 0;
  switch( type )
  {
    case dbBE_REDIS_TYPE_CHAR: ///< character/string
      data_len = Redis_insert_bulk_string( writepos, data->_string._data );
      break;
    case dbBE_REDIS_TYPE_STRING_PART:
      data_len = Redis_insert_bulk_string_head( writepos, data->_pstring._total_size );
      break;
    case dbBE_REDIS_TYPE_RAW:
      data_len = Redis_insert_raw( writepos, data->_string._data, data->_string._size );
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

  dbBE_Transport_sr_buffer_add_data( sr_buf, data_len, 1 );
  return data_len;
}

/*
 * create the key, based on the command type
 */
int dbBE_Redis_create_key( dbBE_Redis_request_t *request, char *keybuf, uint16_t size )
{
  if( keybuf == NULL )
    return -EINVAL;

  int len = 0;
  dbBE_Redis_namespace_t *ns = (dbBE_Redis_namespace_t*)request->_user->_ns_hdl;
  memset( keybuf, 0, size );
  switch( request->_user->_opcode )
  {
    case DBBE_OPCODE_PUT:
    case DBBE_OPCODE_GET:
    case DBBE_OPCODE_READ:
    case DBBE_OPCODE_REMOVE:
    {
      len = snprintf( keybuf, size, "%s%s%s",
                          dbBE_Redis_namespace_get_name( ns ),
                          DBBE_REDIS_NAMESPACE_SEPARATOR,
                          request->_user->_key );
      if(( len < 0 ) || ( len >= size ))
        return -EMSGSIZE;
      break;
    }

    case DBBE_OPCODE_MOVE:
    {
      len = 0;
      switch( request->_step->_stage )
      {
        case DBBE_REDIS_MOVE_STAGE_RESTORE: // restore stage uses the new namespace for the key
          ns = (dbBE_Redis_namespace_t*)request->_user->_sge[0].iov_base;  // destination namespace is in the first SGE arg
          len = snprintf( keybuf, size, "%s%s%s",
                          dbBE_Redis_namespace_get_name( ns ),
                          DBBE_REDIS_NAMESPACE_SEPARATOR,
                          request->_user->_key );
          break;
        default:
          len = snprintf( keybuf, size, "%s%s%s",
                          dbBE_Redis_namespace_get_name( ns ),
                          DBBE_REDIS_NAMESPACE_SEPARATOR,
                          request->_user->_key );
          break;
      }
      if(( len < 0 ) || ( len >= size ))
        return -EMSGSIZE;
      break;
    }
    case DBBE_OPCODE_NSCREATE:
    case DBBE_OPCODE_NSATTACH:
    {
      len = snprintf( keybuf, size, "%s", request->_user->_key );
      if(( len < 0 ) || ( len >= size ))
        return -EMSGSIZE;
      break;
    }
    case DBBE_OPCODE_NSDETACH:
    case DBBE_OPCODE_NSDELETE:
    {
      len = snprintf( keybuf, size, "%s", dbBE_Redis_namespace_get_name( ns ) );
      if(( len < 0 ) || ( len >= size ))
        return -EMSGSIZE;
      break;
    }
    case DBBE_OPCODE_ITERATOR:
    case DBBE_OPCODE_DIRECTORY:
    case DBBE_OPCODE_NSQUERY:
    {
      len = snprintf( keybuf, size, "%s", dbBE_Redis_namespace_get_name( ns ) );
      if(( len < 0 ) || ( len >= size ))
        return -EMSGSIZE;
      break;
    }
    case DBBE_OPCODE_NSADDUNITS:
    case DBBE_OPCODE_NSREMOVEUNITS:
    case DBBE_OPCODE_CANCEL:
      return -ENOSYS;
    case DBBE_OPCODE_UNSPEC:
    case DBBE_OPCODE_MAX:
      return -EINVAL;
  }
  return len;
}

#ifdef DBR_DEBUG_PROTOCOL
static int Flatten_cmd_b( dbBE_sge_t *cmd, int cmdlen, dbBE_Redis_sr_buffer_t *dest )
{
  if(( cmd == NULL ) || ( cmdlen > DBBE_SGE_MAX ) || (cmdlen <= 0 ) || ( dest == NULL ))
    return -EINVAL;

  int n = 0;
  dbBE_Transport_sr_buffer_reset( dest );
  for( n = 0; n < cmdlen; ++n )
  {
    if( cmd[ n ].iov_base == NULL )
      return -EBADMSG;
    memcpy( dbBE_Transport_sr_buffer_get_available_position( dest ),
            cmd[ n ].iov_base,
            cmd[ n ].iov_len );
    dbBE_Transport_sr_buffer_add_data( dest, cmd[ n ].iov_len, 1 );
    *(dbBE_Transport_sr_buffer_get_available_position( dest )) = '\0'; // string termination (for debugging purposes only)
  }
  return 0;
}
#endif

int dbBE_Redis_create_scan_key( dbBE_Redis_request_t *request,
                                dbBE_Redis_sr_buffer_t *buf,
                                char *user_match,
                                dbBE_sge_t *keysge )
{
  char *key = dbBE_Transport_sr_buffer_get_available_position( buf );
  char *namespace = dbBE_Redis_namespace_get_name( (dbBE_Redis_namespace_t*)(request->_user->_ns_hdl) );

  char *match_all = "*";
  char *match = user_match;
  if(( user_match == NULL ) || ( user_match[0] == '\0'))
    match = match_all;
  size_t keylen = strnlen( namespace,
                        DBBE_REDIS_MAX_KEY_LEN ) + DBBE_REDIS_NAMESPACE_SEPARATOR_LEN + strnlen( match, DBBE_REDIS_MAX_KEY_LEN );
  if( keylen > dbBE_Transport_sr_buffer_remaining( buf ) )
    return -ENOMEM;

  int len = snprintf( key,
                      DBBE_REDIS_MAX_KEY_LEN,
                      "$%ld\r\n%s%s%s\r\n",
                      keylen,
                      namespace,
                      DBBE_REDIS_NAMESPACE_SEPARATOR,
                      match );
  if( len < 0 )
    return -EPROTO;
  dbBE_Transport_sr_buffer_add_data( buf, len, 1 );

  keysge->iov_base = key;
  keysge->iov_len = len;
  return 0;
}

int dbBE_Redis_create_command_sge( dbBE_Redis_request_t *request,
                                   dbBE_Redis_sr_buffer_t *buf,
                                   dbBE_sge_t *cmd )
{
  int rc = 0;

  dbBE_Redis_command_stage_spec_t *stage = request->_step;

  switch( request->_user->_opcode )
  {
    case DBBE_OPCODE_PUT: // RPUSH ns_name%sep;t_name value
      if( stage->_stage != 0 )
        return -EPROTO;
      rc = dbBE_Redis_command_rpush_create( request, buf, cmd );
      break;

    case DBBE_OPCODE_GET: // LPOP ns_name%sep;t_name
      if( stage->_stage != 0 )
        return -EPROTO;
      rc = dbBE_Redis_command_lpop_create( request, buf, cmd );
      break;

    case DBBE_OPCODE_READ:
      if( stage->_stage != 0 )
        return -EPROTO;
      rc = dbBE_Redis_command_lindex_create( request, buf, cmd );
      break;

    case DBBE_OPCODE_DIRECTORY:
    {
      switch( stage->_stage )
      {
        case DBBE_REDIS_DIRECTORY_STAGE_META:
          rc = dbBE_Redis_command_hmgetall_create( request, buf, cmd );
          break;
        case DBBE_REDIS_DIRECTORY_STAGE_SCAN:
        {
          dbBE_sge_t keysge;
          if( ( rc = dbBE_Redis_create_scan_key( request, buf, request->_user->_match, &keysge )) != 0 )
            break;

          rc = dbBE_Redis_command_scan_create( request,
                                               buf,
                                               cmd,
                                               &keysge,
                                               request->_status.directory.scankey );
#ifdef DBR_DEBUG_PROTOCOL
          dbBE_Redis_sr_buffer_t *cbuf = dbBE_Transport_sr_buffer_allocate( 1024 );
          Flatten_cmd_b( cmd, rc, cbuf );
          LOG( DBG_ALL, stderr, "SCANCMD:%s\n", dbBE_Transport_sr_buffer_get_start( cbuf ) );
          dbBE_Transport_sr_buffer_free( cbuf );
#endif
          break;
        }
        default:
          return -EINVAL;
      }
      break;
    }

    case DBBE_OPCODE_NSCREATE:
      switch( stage->_stage )
      {
        case 0: // HSETNX ns_name id ns_name
          rc = dbBE_Redis_command_hsetnx_create( request, buf, cmd, "id", request->_user->_key );
          break;

        case 1: // HMSET ns_name refcnt 1 groups permissions
          rc = dbBE_Redis_command_hmset_create( request, buf, cmd );
          break;

        default:
          return -EINVAL;
      }
      break;

    case DBBE_OPCODE_NSQUERY:
      rc = dbBE_Redis_command_hmgetall_create( request, buf, cmd );
      break;

    case DBBE_OPCODE_NSATTACH: // EXISTS ns_name; HINCRBY ns_name refcnt 1
    {
      switch( stage->_stage )
      {
        case 0:
          rc = dbBE_Redis_command_hmget_create( request, buf, cmd );
          break;

        case 1:
          rc = dbBE_Redis_command_hincrby_create( request, buf, cmd, 1 );
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
        case DBBE_REDIS_NSDETACH_STAGE_DELCHECK:
          rc = dbBE_Redis_command_delcheck_create( request, buf, cmd, -1 );
          break;

        case DBBE_REDIS_NSDETACH_STAGE_SCAN: // SCAN 0 MATCH ns_name%sep;*
        {
          dbBE_sge_t keysge;
          if( ( rc = dbBE_Redis_create_scan_key( request, buf, "*", &keysge )) != 0 )
            break;

          rc = dbBE_Redis_command_scan_create( request,
                                               buf,
                                               cmd,
                                               &keysge,
                                               request->_status.nsdetach.scankey );
          break;
        }
        case DBBE_REDIS_NSDETACH_STAGE_DELKEYS: // DEL ns_name%sep;key
          if( request->_status.nsdetach.scankey == NULL )
            return -EINVAL;

          rc = dbBE_Redis_command_del_create( request, buf, cmd );
          break;

        case DBBE_REDIS_NSDETACH_STAGE_DELNS: // DEL ns_name
          rc = dbBE_Redis_command_del_create( request, buf, cmd );
          break;

        default:
          return -EINVAL;
      }
      break;
    }

    case DBBE_OPCODE_NSDELETE:
    {
      switch( stage->_stage )
      {
        case DBBE_REDIS_NSDELETE_STAGE_EXIST:
          rc = dbBE_Redis_command_hmget_create( request, buf, cmd );
          break;

        case DBBE_REDIS_NSDELETE_STAGE_SETFLAG: // HSET flags 1
          rc = dbBE_Redis_command_hset_create( request, buf, cmd, "flags", "1" );
          break;

        default:
          return -EINVAL;
      }
      break;
    }

    case DBBE_OPCODE_REMOVE:
    {
      if( stage->_stage != 0 ) // Read is only a single stage request
        return -EINVAL;

      rc = dbBE_Redis_command_del_create( request, buf, cmd );
      break;
    }

    case DBBE_OPCODE_MOVE:
    {
      switch( stage->_stage )
      {
        case DBBE_REDIS_MOVE_STAGE_DUMP:
          rc = dbBE_Redis_command_dump_create( request, buf, cmd );
          break;

        case DBBE_REDIS_MOVE_STAGE_RESTORE:
          rc = dbBE_Redis_command_restore_create( request, buf, cmd );
          break;

        case DBBE_REDIS_MOVE_STAGE_DEL:
          rc = dbBE_Redis_command_del_create( request, buf, cmd );
          break;

        default:
          return -EINVAL;
      }
      break;
    }

    case DBBE_OPCODE_ITERATOR:
    {
      dbBE_sge_t keysge;
      if( ( rc = dbBE_Redis_create_scan_key( request, buf, request->_user->_match, &keysge )) != 0 )
        break;

      rc = dbBE_Redis_command_scan_create( request,
                                           buf,
                                           cmd,
                                           &keysge,
                                           request->_status.iterator._it->_cursor );
      break;
    }
    default:
      return -ENOSYS;
  }

  return rc;
}
