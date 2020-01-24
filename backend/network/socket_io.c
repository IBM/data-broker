/*
 * Copyright © 2020 IBM Corporation
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

#include "logutil.h"
#include "socket_io.h"

#include <sys/socket.h> // sendmsg


ssize_t dbBE_Socket_send( const int socket,
                          dbBE_Transport_sge_buffer_t *sge_buf )
{
  if(( socket < 0 ) || ( sge_buf == NULL ))
    return -EINVAL;

  if( sge_buf->_index == 0 )
    return 0;  // nothing to send

  dbBE_sge_t *cmd = sge_buf->_cmd;

  struct msghdr msg;
  memset( &msg, 0, sizeof( struct msghdr ) );
  msg.msg_iov = cmd;
  msg.msg_iovlen = sge_buf->_index;

  ssize_t total = dbBE_SGE_get_len( cmd, sge_buf->_index );
  ssize_t ssize = 0;
  ssize_t rc = 0;

  do
  {
    rc = sendmsg( socket, &msg, 0 );

    if( rc < 0 ) break;
    if(( rc > 0 ) && ( rc + ssize < total ))
    {
      ssize_t offset = rc;
      while(( sge_buf->_index > 0 ) && ( offset > 0 ))
      {
        if( (size_t)offset < cmd[0].iov_len )
        {
          LOG( DBG_TRACE, stderr, "SGE[0] reduce by %ld from %ld to %ld\n", offset, cmd[0].iov_len, cmd[0].iov_len - offset );
          cmd[0].iov_base = (char*)cmd[0].iov_base + offset;
          cmd[0].iov_len -= offset;
          offset = 0;
        }
        else
        {
          LOG( DBG_TRACE, stderr, "SGE shift remaining data reduced by %ld from %ld to %ld; remaining entries: %d\n",
               cmd[0].iov_len, offset, offset - cmd[0].iov_len, sge_buf->_index-1 );
          offset -= cmd[0].iov_len;
          memmove( cmd, &cmd[1], sizeof( cmd[0] ) * sge_buf->_index );
          --sge_buf->_index;
        }
      }
    }
    ssize += rc;
  } while (( rc >= 0 ) && ( ssize < total ));

#ifdef DEBUG_REDIS_PROTOCOL
  dbBE_Redis_sr_buffer_t *tmpbuffer = dbBE_Transport_sr_buffer_allocate( DBBE_REDIS_SR_BUFFER_LEN );
  ssize_t len = dbBE_Redis_connection_flatten_cmd( cmd, cmdlen, tmpbuffer );
  LOG( DBG_ALL, stdout, "SEND:%"PRId64"%s\n", len, dbBE_Transport_sr_buffer_get_start( tmpbuffer ) );
  if( len != (ssize_t)dbBE_Transport_sr_buffer_available( tmpbuffer ) )
    LOG( DBG_ERR, stderr, "SEND: Length error. accumulated %"PRId64" expected %"PRId64"\n", len, dbBE_Transport_sr_buffer_available( tmpbuffer ) );
  if( len != datalen )
    LOG( DBG_ERR, stderr, "SEND: Length error. accumulated %"PRId64" SGElen %"PRId64"\n", len, datalen );
  dbBE_Transport_sr_buffer_free( tmpbuffer );
#endif

  dbBE_Transport_sge_buffer_reset( sge_buf );

  return rc;
}

ssize_t dbBE_Socket_recv( const int socket,
                          dbBE_Redis_sr_buffer_t *buf )
{

  ssize_t rcvd = recv( socket,
                       dbBE_Transport_sr_buffer_get_available_position( buf ),
                       dbBE_Transport_sr_buffer_remaining( buf ),
                       0 );
  return rcvd;
}
