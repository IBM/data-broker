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
#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>
#endif
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>

#include "logutil.h"
#include "utility.h"
#include "definitions.h"
#include "connection.h"

/*
 * create a Redis connection object, initialize with default/uninitialized values
 * does not allocate the sr_buffers
 */
dbBE_Redis_connection_t *dbBE_Redis_connection_create( const uint64_t sr_buffer_size )
{
  dbBE_Redis_connection_t *conn = (dbBE_Redis_connection_t*)malloc( sizeof( dbBE_Redis_connection_t ) );
  if( conn == NULL )
  {
    return NULL;
  }
  memset( conn, 0, sizeof( dbBE_Redis_connection_t ) );


  dbBE_Redis_sr_buffer_t *sendb = dbBE_Redis_sr_buffer_allocate( sr_buffer_size );
  dbBE_Redis_sr_buffer_t *recvb = dbBE_Redis_sr_buffer_allocate( sr_buffer_size );

  if(( sendb == NULL ) || ( recvb == NULL ))
  {
    free( conn );
    dbBE_Redis_sr_buffer_free( sendb );
    dbBE_Redis_sr_buffer_free( recvb );
    errno = ENOMEM;
    return NULL;
  }

  conn->_sendbuf = sendb;
  conn->_recvbuf = recvb;
  conn->_index = -1;
  conn->_status = DBBE_CONNECTION_STATUS_INITIALIZED;
  if(( sendb == NULL ) || ( recvb == NULL ))
    conn->_status = DBBE_CONNECTION_STATUS_UNSPEC;

  conn->_posted_q = dbBE_Redis_s2r_queue_create( DBBE_REDIS_WORK_QUEUE_DEPTH );

  return conn;
}

/*
 * assign a send buffer to the connection
 */
int dbBE_Redis_connection_assign_sendbuf( dbBE_Redis_connection_t *conn,
                                          dbBE_Redis_sr_buffer_t *sendb )
{
  if( conn == NULL )
    return -EINVAL;

  conn->_sendbuf = sendb;

  // check the buffer status, if both buffers are available, it can transition to INITIALIZED
  if(( conn->_sendbuf == NULL ) || ( conn->_recvbuf == NULL ))
    conn->_status = DBBE_CONNECTION_STATUS_UNSPEC;
  else
    conn->_status = DBBE_CONNECTION_STATUS_INITIALIZED;

  return 0;
}

/*
 * assign a recv buffer to the connection
 */
int dbBE_Redis_connection_assign_recvbuf( dbBE_Redis_connection_t *conn,
                                          dbBE_Redis_sr_buffer_t *recvb )
{
  if( conn == NULL )
    return -EINVAL;

  conn->_recvbuf = recvb;

  // check the buffer status, if both buffers are available, it can transition to INITIALIZED
  if(( conn->_sendbuf == NULL ) || ( conn->_recvbuf == NULL ))
    conn->_status = DBBE_CONNECTION_STATUS_UNSPEC;
  else
    conn->_status = DBBE_CONNECTION_STATUS_INITIALIZED;

  return 0;
}

/*
 * connect to a Redis instance given by the address
 */
dbBE_Redis_address_t* dbBE_Redis_connection_link( dbBE_Redis_connection_t *conn,
                                                  const char *host,
                                                  const char *port,
                                                  const char *authfile )
{
  LOG( DBG_VERBOSE, stderr, "LINK: conn=%p, host=%s, port=%s, authfile=%s\n", conn, host, port, authfile );
  if(( conn == NULL ) ||
      (( conn->_status != DBBE_CONNECTION_STATUS_INITIALIZED ) &&
       ( conn->_status != DBBE_CONNECTION_STATUS_DISCONNECTED )) )
  {
    LOG( DBG_ERR, stderr, "connection_link: invalid arguments/status conn=%p, host=%s, port=%s, authfile=%s\n", conn, host, port, authfile );
    errno = EINVAL;
    return NULL;
  }

  int s;
  struct addrinfo hints, *addrs, *iface;
  memset( &hints, 0, sizeof( struct addrinfo ) );
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  LOG( DBG_VERBOSE, stdout, "Getting AddrInfo for host=%s port=%s\n", host, port );
  int rc = getaddrinfo( host, port,
                        &hints,
                        &addrs);
  if( rc != 0 )
  {
    return NULL;
  }

  iface = addrs;
  while( iface != NULL )
  {
    s = socket( iface->ai_family, iface->ai_socktype, iface->ai_protocol );
    if( s < 0 )
      continue;

    rc = connect( s, iface->ai_addr, iface->ai_addrlen );
    if( rc == 0 )
    {
      conn->_socket = s;
      conn->_address = dbBE_Redis_address_copy( iface->ai_addr, iface->ai_addrlen );
      conn->_status = DBBE_CONNECTION_STATUS_CONNECTED;
      LOG( DBG_VERBOSE, stdout, "Connected to %s\n", host );
      break;
    }

    LOG( DBG_VERBOSE, stdout, "Tried connecting to %s\n", iface->ai_canonname );
    close( s );
    s=-1;
    iface = iface->ai_next;
  }

  freeaddrinfo( addrs );

  if( conn->_status != DBBE_CONNECTION_STATUS_CONNECTED )
  {
    dbBE_Redis_address_destroy( conn->_address );
    errno = ENOTCONN;
    return NULL;
  }

  rc = dbBE_Redis_connection_auth( conn, authfile );
  if( rc != 0 )
  {
    dbBE_Redis_connection_unlink( conn );
    return NULL;
  }

#ifdef WITH_NON_BLOCKING_SOCKET
  struct timeval timeout;
  timeout.tv_sec = 10;
  timeout.tv_usec = 0;
  if( setsockopt( conn->_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof( timeout )) )
  {
    LOG( DBG_ERR, stderr, "Unable to set socket option SO_RCVTIMEO rc=\n", errno );
    return NULL;
  }

  if( setsockopt( conn->_socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof( timeout )) )
  {
    LOG( DBG_ERR, stderr, "Unable to set socket option SO_SNDTIMEO rc=\n", errno );
    return NULL;
  }
#endif

  return conn->_address;
}

int dbBE_Redis_connection_reconnect( dbBE_Redis_connection_t *conn )
{
  int rc = 0;
  if(( conn == NULL ) || ( conn->_status != DBBE_CONNECTION_STATUS_DISCONNECTED ))
    return -EINVAL;

  int s = socket( conn->_address->_address.sin_family, SOCK_STREAM, 0 );
  if( s < 0 )
    return -errno;

  rc = connect( s,
                (const struct sockaddr*)&(conn->_address->_address),
                sizeof( conn->_address->_address ) );
  if( rc == 0 )
  {
    conn->_status = DBBE_CONNECTION_STATUS_CONNECTED;
    LOG( DBG_VERBOSE, stdout, "Reconnected connection %d\n", conn->_index );
  }
  else
  {
    LOG( DBG_ERR, stderr, "Reconnection failed: %s\n", strerror( errno ) );
    return -errno;
  }

  conn->_socket = s;
  char *authfile = dbBE_Redis_extract_env( DBR_SERVER_AUTHFILE_ENV, DBR_SERVER_DEFAULT_AUTHFILE );
  rc = dbBE_Redis_connection_auth( conn, authfile );

  if( authfile != NULL )
    free( authfile );

  if( rc != 0 )
  {
    dbBE_Redis_connection_unlink( conn );
    return rc;
  }

  return rc;
}

static
ssize_t dbBE_Redis_connection_recv_base( dbBE_Redis_connection_t *conn )
{
  ssize_t rc = 0;
  int stored_errno=0;
  do
  {
    if( dbBE_Redis_sr_buffer_remaining( conn->_recvbuf ) <= 0 )
    {
      LOG( DBG_ERR, stderr, "Recv Buffer overrun. Protocol error.\n" );
      return -ENOBUFS;
    }
    errno = 0;
    rc = recv( conn->_socket,
               dbBE_Redis_sr_buffer_get_available_position( conn->_recvbuf ),
               dbBE_Redis_sr_buffer_remaining( conn->_recvbuf ),
               0 );
    stored_errno=errno;
    if( stored_errno == EINTR )
      LOG( DBG_INFO, stderr, "recv() got interrupted by a signal. will retry\n" );
    if( rc == 0 )
    {
      LOG( DBG_INFO, stderr, "recv() got no data, Redis server down?\n" );
      dbBE_Redis_connection_fail( conn );
      rc = -1;
      stored_errno = ENOTCONN;
      break;
    }
    if( rc > 0 )
      dbBE_Redis_sr_buffer_add_data( conn->_recvbuf, rc, 0 );

  } while( stored_errno == EINTR );

  if( rc >= 0 )
  {
    // disarm connection status if the received data was less than the max capacity
    // we've received all currently available data
    if( (size_t)rc <= dbBE_Redis_sr_buffer_get_size( conn->_recvbuf ) )
    {
      conn->_status = DBBE_CONNECTION_STATUS_AUTHORIZED;
    }
  }
  else
    rc = -stored_errno;

  return rc;
}


/*
 * receive data from a connection and place data into the attached sr_buffer
 */
ssize_t dbBE_Redis_connection_recv( dbBE_Redis_connection_t *conn )
{
  if( conn == NULL )
    return -EINVAL;
  if( ! dbBE_Redis_connection_RTR( conn ) )
    return -ENOTCONN;

  int empty = dbBE_Redis_sr_buffer_empty( conn->_recvbuf );
  if( ! empty )
    return -ENOTEMPTY;

  if( conn->_status != DBBE_CONNECTION_STATUS_PENDING_DATA )
  {
    return 0;
  }

  dbBE_Redis_sr_buffer_reset( conn->_recvbuf );
  ssize_t rc = dbBE_Redis_connection_recv_base( conn );

  LOG( DBG_TRACE, stdout, "RECV: conn=%d:%s", conn->_socket, dbBE_Redis_sr_buffer_get_start( conn->_recvbuf ) );

#ifdef DEBUG_REDIS_PROTOCOL
  if( dbBE_Redis_sr_buffer_available( conn->_recvbuf ) > 1000 )
  {
    char *logptr = dbBE_Redis_sr_buffer_get_start( conn->_recvbuf );
    logptr += dbBE_Redis_sr_buffer_available( conn->_recvbuf ) - 4;
    LOG( DBG_ALL, stderr, "RECV last bytes: %x %x %x %x (total:%"PRId64"\n",
         logptr[0], logptr[1], logptr[2], logptr[3], dbBE_Redis_sr_buffer_available( conn->_recvbuf ) );
  }
#endif
  return rc;
}

ssize_t dbBE_Redis_connection_recv_more( dbBE_Redis_connection_t *conn )
{
  if( conn == NULL )
    return -EINVAL;
  if( ! dbBE_Redis_connection_RTR( conn ) )
    return -ENOTCONN;

  ssize_t rc = dbBE_Redis_connection_recv_base( conn );

  LOG( DBG_VERBOSE, stdout, "recv_more: conn=%d; new=%"PRId64"; avail/rem=%"PRId64"/%"PRId64"\n",
       conn->_socket, rc, dbBE_Redis_sr_buffer_available( conn->_recvbuf ), dbBE_Redis_sr_buffer_remaining( conn->_recvbuf ) );
  LOG( DBG_TRACE, stdout, "recv_more: conn=%d:%s", conn->_socket, dbBE_Redis_sr_buffer_get_start( conn->_recvbuf ) );

#ifdef DEBUG_REDIS_PROTOCOL
  if( dbBE_Redis_sr_buffer_available( conn->_recvbuf ) > 1000 )
  {
    char *logptr = dbBE_Redis_sr_buffer_get_start( conn->_recvbuf );
    logptr += dbBE_Redis_sr_buffer_available( conn->_recvbuf ) - 4;
    LOG( DBG_ALL, stderr, "RECV last bytes: %x %x %x %x (total:%"PRId64"\n",
         logptr[0], logptr[1], logptr[2], logptr[3], dbBE_Redis_sr_buffer_available( conn->_recvbuf ) );
  }
#endif
  return rc;
}

/*
 * flush the send buffer by sending it to the connected Redis instance
 */
int dbBE_Redis_connection_send( dbBE_Redis_connection_t *conn )
{
  if( conn == NULL )
    return -EINVAL;
  if( ! dbBE_Redis_connection_RTS( conn ) )
    return -ENOTCONN;

#ifdef DEBUG_REDIS_PROTOCOL
  if( dbBE_Redis_sr_buffer_available( conn->_sendbuf ) > 1000 )
  {
    char *logptr = dbBE_Redis_sr_buffer_get_start( conn->_sendbuf );
    logptr += dbBE_Redis_sr_buffer_available( conn->_sendbuf ) - 4;
    LOG( DBG_ALL, stderr, "SEND last bytes: %x %x %x %x\n", logptr[0], logptr[1], logptr[2], logptr[3] );
  }
  else
    LOG( DBG_ALL, stderr, "SEND: conn=%d:%s", conn->_socket, dbBE_Redis_sr_buffer_get_start( conn->_sendbuf ) );

#endif
  ssize_t rc = send( conn->_socket,
                     dbBE_Redis_sr_buffer_get_start( conn->_sendbuf ),
                     dbBE_Redis_sr_buffer_available( conn->_sendbuf ),
                     MSG_WAITALL );
  if( rc == (ssize_t)dbBE_Redis_sr_buffer_available( conn->_sendbuf ))
    dbBE_Redis_sr_buffer_reset( conn->_sendbuf );
  else
    return -EBADMSG;
  return rc;
}

/*
 * disconnect from a Redis instance and destroy the address and socket
 */
int dbBE_Redis_connection_unlink( dbBE_Redis_connection_t *conn )
{
  if(( conn == NULL ) || ( ! dbBE_Redis_connection_RTS( conn ) && ( conn->_status != DBBE_CONNECTION_STATUS_FAILED )) )
    return -EINVAL;

  close( conn->_socket );
  conn->_socket = -1;
  conn->_status = DBBE_CONNECTION_STATUS_DISCONNECTED;
//  dbBE_Redis_address_destroy( conn->_address );
//  conn->_address = NULL;

  return 0;
}

int dbBE_Redis_connection_auth( dbBE_Redis_connection_t *conn, const char *authfile_name )
{
  int rc = 0;
  int auth_error = 0;
  struct stat auth_file_stat;

  // open the file
  int auth_fd = open( authfile_name, O_RDONLY );
  if( auth_fd < 0 )
  {
    perror( authfile_name );
    return -1;
  }

  size_t authbuf_size = 0;
  char *authbuf = NULL;

  // get the file size to determine the buffer size
  rc = fstat( auth_fd, &auth_file_stat );
  if( rc == 0 )
  {
    // allocate and reset the buffer
    authbuf_size = auth_file_stat.st_size + 128;

    if( dbBE_Redis_sr_buffer_get_size( conn->_sendbuf ) < authbuf_size )
    {
      LOG( DBG_ERR, stderr, "connection_auth: send/recv buffer too small for auth operation.\n" );
      close( auth_fd );
      return -1;
    }

    authbuf = (char*)malloc( authbuf_size );
    memset( authbuf, 0, authbuf_size );

    // read the file data
    ssize_t rlen = read( auth_fd, authbuf, authbuf_size );
    if( rlen <= 0 )
      perror( "Read authfile" );  // no exit needed because rc is 0
    else
    {
      rc = strcspn( authbuf, "\r\n" );  // strip the auth-string and update rc to enter the processing
      if( authbuf[ rc - 1 ] == ' ' )
      {
        LOG( DBG_WARN, stderr, "Warning, password in file ends with white space. Double check if you have authentication problems.\n" );
      }
    }
    if( rc > 0 )
    {
      authbuf[ rc ] = '\0'; // terminate the authbuf and remove and newlines
      int len = snprintf( dbBE_Redis_sr_buffer_get_start( conn->_sendbuf ),
                          dbBE_Redis_sr_buffer_remaining( conn->_sendbuf ),
                          "*2\r\n$4\r\nAUTH\r\n$%d\r\n%s\r\n", rc, authbuf );
      if( len > 0 )
      {
        dbBE_Redis_sr_buffer_add_data( conn->_sendbuf, len, 1 );

        conn->_status = DBBE_CONNECTION_STATUS_AUTHORIZED; // assume authorized for a brief moment to allow using the send/recv functions
        rc = dbBE_Redis_connection_send( conn );
        if( rc > 0 )
        {
          memset( authbuf, 0, authbuf_size );
          rc = 0;
          while( rc == 0 )
          {
            rc = recv( conn->_socket, authbuf, authbuf_size, 0 );
            if(( rc == -1 ) && ( errno == EAGAIN ))
              rc = 0;
          }
          if( rc > 0 )
          {
            if( strncmp( authbuf, "+OK\r\n", 5 ) != 0 )
            {
              conn->_status = DBBE_CONNECTION_STATUS_CONNECTED;
              auth_error = EPERM;
            }
          }
          else
            auth_error = errno;
        }
        else
          auth_error = errno;
      }
      else
        auth_error = errno;
    }
    else
      auth_error = errno;
  }
  else
    auth_error = errno;

  close( auth_fd );
  if( authbuf != NULL )
  {
    memset( authbuf, 0, authbuf_size );
    free( authbuf );
  }

  return auth_error;
}

/*
 * destroy a Redis connection object and free the
 * destroys the sr_buffers too
 */
void dbBE_Redis_connection_destroy( dbBE_Redis_connection_t *conn )
{
  if( conn == NULL )
    return;

  if( conn->_status == DBBE_CONNECTION_STATUS_UNSPEC )
  {
    LOG( DBG_ERR, stderr, "Attempting to clean a connection in unspecified state.\n" );
    return;
  }

  // disconnect if needed
  if(( conn->_status == DBBE_CONNECTION_STATUS_CONNECTED ) ||
     ( conn->_status == DBBE_CONNECTION_STATUS_AUTHORIZED ) ||
     ( conn->_status == DBBE_CONNECTION_STATUS_PENDING_DATA ))
    dbBE_Redis_connection_unlink( conn );

  dbBE_Redis_s2r_queue_destroy( conn->_posted_q );
  dbBE_Redis_sr_buffer_free( conn->_sendbuf );
  dbBE_Redis_sr_buffer_free( conn->_recvbuf );
  dbBE_Redis_address_destroy( conn->_address );

  // wipe memory
  memset( conn, 0, sizeof( dbBE_Redis_connection_t ) );

  free( conn );
}
