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
#include "definitions.h"
#include "network/connection.h"
#include "common/resolve_addr.h"
#include "common/utility.h"

#include <stdlib.h> // calloc
#include <errno.h> // errno
#include <unistd.h> // close
#include <sys/types.h> // ..
#include <sys/stat.h> // ..
#include <fcntl.h> // ..open

dbBE_Connection_t *dbBE_Connection_create()
{
  dbBE_Connection_t *conn = (dbBE_Connection_t*)calloc( 1, sizeof( dbBE_Connection_t ) );
  if( conn == NULL )
    return NULL;

  conn->_socket = -1;
  conn->_status = DBBE_CONNECTION_STATUS_INITIALIZED;

  return conn;
}



dbBE_Network_address_t* dbBE_Connection_link( dbBE_Connection_t *conn,
                                            const char *url,
                                            const char *authfile )
{
  LOG( DBG_VERBOSE, stderr, "LINK: conn=%p, url=%s, authfile=%s\n", conn, url, authfile );
  if(( conn == NULL ) ||
      (( conn->_status != DBBE_CONNECTION_STATUS_INITIALIZED ) &&
       ( conn->_status != DBBE_CONNECTION_STATUS_DISCONNECTED )) )
  {
    LOG( DBG_ERR, stderr, "connection_link: invalid arguments/status conn=%p, url=%s, authfile=%s\n", conn, url, authfile );
    errno = EINVAL;
    return NULL;
  }

  int s;
  int rc = ENOTCONN;
  struct addrinfo *addrs = dbBE_Common_resolve_address( url, 0 );

  if( addrs == NULL )
  {
    LOG( DBG_ERR, stderr, "connection_link: unable to connect to: %s\n", url );
    return NULL;
  }
  struct addrinfo *iface = addrs;
  while( iface != NULL )
  {
    s = socket( iface->ai_family, iface->ai_socktype, iface->ai_protocol );
    if( s < 0 )
    {
      switch( errno )
      {
        case ENFILE:
        case EMFILE:
          LOG( DBG_ERR, stderr, "Open File Descriptor limit exceeded: %s\n", strerror( errno ) );
          return NULL;

        case ENOBUFS:
        case ENOMEM:
          LOG( DBG_ERR, stderr, "Ran out of memory\n" );
          return NULL;

        default:
          LOG( DBG_ERR, stderr, "socket creation error: %s\n", strerror( errno ) );
          iface = iface->ai_next;
          continue;
      }
    }

    rc = connect( s, iface->ai_addr, iface->ai_addrlen );
    if( rc == 0 )
    {
      conn->_socket = s;
      conn->_address = dbBE_Network_address_copy( iface->ai_addr, iface->ai_addrlen );
      conn->_status = DBBE_CONNECTION_STATUS_CONNECTED;
      dbBE_Network_address_to_string( conn->_address, conn->_url, DBBE_URL_MAX_LENGTH );
      LOG( DBG_VERBOSE, stdout, "Connected to %s\n", url );
      break;
    }

    LOG( DBG_VERBOSE, stdout, "Tried connecting to %s\n", iface->ai_canonname );
    close( s );
    s=-1;
    iface = iface->ai_next;
  }

  dbBE_Common_release_addrinfo( &addrs );

  if( conn->_status != DBBE_CONNECTION_STATUS_CONNECTED )
  {
    dbBE_Network_address_destroy( conn->_address );
    memset( conn->_url, 0, DBBE_URL_MAX_LENGTH );
    conn->_status = DBBE_CONNECTION_STATUS_DISCONNECTED;
    errno = ENOTCONN;
    return NULL;
  }

  rc = dbBE_Connection_auth( conn, authfile );
  if( rc != 0 )
  {
    dbBE_Connection_unlink( conn );
    dbBE_Network_address_destroy( conn->_address );
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

/*
 * return 0 if the connection is considered not recoverable
 * return 1 otherwise
 */
dbBE_Connection_recoverable_t dbBE_Connection_recoverable( dbBE_Connection_t *conn )
{
  // grab the timestamp or reconnection counter and decide based on that whether it's considered recoverable or not
  if( dbBE_Connection_RTR( conn ) )
    return DBBE_CONNECTION_RECOVERED;

  if( conn == NULL )
    return DBBE_CONNECTION_UNRECOVERABLE;

  struct timeval now;
  gettimeofday( &now, NULL );
  if(( now.tv_sec - conn->_last_alive.tv_sec ) < DBBE_RECONNECT_TIMEOUT )
    return DBBE_CONNECTION_RECOVERABLE;
  return DBBE_CONNECTION_UNRECOVERABLE;
}

int dbBE_Connection_reconnect( dbBE_Connection_t *conn )
{
  int rc = 0;
  if(( conn == NULL ) || ( conn->_status != DBBE_CONNECTION_STATUS_DISCONNECTED ))
    return -EINVAL;

  // make sure there's an address before dereferencing it
  if( conn->_address == NULL )
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
    LOG( DBG_VERBOSE, stdout, "Reconnected connection %s\n", conn->_url );
  }
  else
  {
    LOG( DBG_ERR, stderr, "Reconnection failed: %s\n", strerror( errno ) );
    close( s );
    return -errno;
  }

  conn->_socket = s;
  char *authfile = dbBE_Extract_env( DBR_SERVER_AUTHFILE_ENV, DBR_SERVER_DEFAULT_AUTHFILE );
  rc = dbBE_Connection_auth( conn, authfile );

  if( authfile != NULL )
    free( authfile );

  if( rc != 0 )
  {
    dbBE_Connection_unlink( conn );
    return rc;
  }

  return rc;
}

int dbBE_Connection_unlink( dbBE_Connection_t *conn )
{
  if( ! dbBE_Connection_RTS( conn ))
    return -EINVAL;

  close( conn->_socket );
  conn->_socket = -1;
  conn->_status = DBBE_CONNECTION_STATUS_DISCONNECTED;
//  don't touch the address, it can be reused during reconnect
//  dbBE_Address_destroy( conn->_address );
//  conn->_address = NULL;

  gettimeofday( &conn->_last_alive, NULL );
  return 0;
}

int dbBE_Connection_auth( dbBE_Connection_t *conn, const char *authfile )
{
  // skip authentication if authfile name is explicitly set to NONE
  if( strncmp( authfile, "NONE", 5 ) == 0 )
  {
    conn->_status = DBBE_CONNECTION_STATUS_AUTHORIZED;
    return 0;
  }

  // open the file
  int auth_fd = open( authfile, O_RDONLY );
  if( auth_fd < 0 )
  {
    perror( authfile );
    return -1;
  }

  close( auth_fd );

  conn->_status = DBBE_CONNECTION_STATUS_AUTHORIZED;
  return 0;
}


int dbBE_Connection_destroy( dbBE_Connection_t *conn )
{
  if( conn == NULL )
    return -EINVAL;

  dbBE_Connection_unlink( conn );

  if( conn->_address )
    dbBE_Network_address_destroy( conn->_address );

  free( conn );
  return 0;
}

int dbBE_Connection_noblock( dbBE_Connection_t *conn )
{
  return fcntl( conn->_socket, F_SETFL, O_NONBLOCK );
}
