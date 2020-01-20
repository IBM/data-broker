/*
 * Copyright © 2019,2020 IBM Corporation
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

#ifndef BACKEND_COMMON_RESOLVE_ADDR_H_
#define BACKEND_COMMON_RESOLVE_ADDR_H_

#include <string.h>  // memset
#include <stdlib.h> // free

#include <sys/types.h>  // getaddrinfo
#include <sys/socket.h>
#include <netdb.h>

#include "logutil.h" // LOG



#define DBBE_MAX_PROTO_CHAR ( 16 )

#define DBBE_PROTO_SOCKET "sock:"

static
struct addrinfo* dbBE_Common_resolve_address_socket( const char *server_string, const int passive )
{
  struct addrinfo hints, *addrs;
  memset( &hints, 0, sizeof( struct addrinfo ) );
  if( passive )
    hints.ai_flags = AI_PASSIVE;
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  char *host = (char*)calloc( strlen( server_string ), sizeof( char ) );
  if( host == NULL )
  {
    LOG( DBG_ERR, stderr, "Failed to allocate mem for hostname\n" );
    return NULL;
  }

  char *port = index( server_string, ':' );
  // check format:    no ':' or no host or no port
  if(( port == NULL ) || ( port == server_string ) || ( port[1] == '\0' ))
  {
    LOG( DBG_ERR, stderr, "DBR_SERVER requires SOCK://<host>:<port> formatting\n" );
    free( host );
    return NULL;
  }

  // split port and host
  char *host_end = memccpy( host, server_string, ':', DBBE_MAX_PROTO_CHAR );

  if( host_end == NULL )
  {
    LOG( DBG_ERR, stderr, "Destination spec has invalid format. Expected: <host>:<port>; found %s\n", server_string );
    free( host );
    return NULL;
  }
  host_end[-1] = '\0'; // terminate host and remove the ':'
  port += 1;

  LOG( DBG_VERBOSE, stdout, "Getting AddrInfo for host=%s port=%s\n", host, port );
  int rc = getaddrinfo( host, port,
                        &hints,
                        &addrs);

  free( host );

  if( rc != 0 )
  {
    return NULL;
  }

  return addrs;

}

static
struct addrinfo* dbBE_Common_resolve_address( const char *server_string, const int passive )
{
  if( server_string == NULL )
    return NULL;

  char proto[ DBBE_MAX_PROTO_CHAR ];

  char *destination = index( server_string, ':' );
  if(( destination == NULL ) || ( destination == server_string ) || ( destination[1] != '/' ) || ( destination[2] != '/' ))
  {
    LOG( DBG_ERR, stderr, "DBR_SERVER requires format <proto>://<destination>\n");
    return NULL;
  }

  char *proto_end = memccpy( proto, server_string, ':', DBBE_MAX_PROTO_CHAR );
  if( proto_end == NULL )
    return NULL;

  proto_end[0] = '\0'; // terminate proto
  destination += 3;

  int sock = strncmp( proto, DBBE_PROTO_SOCKET, DBBE_MAX_PROTO_CHAR );
  if( sock == 0 )
  {
    return dbBE_Common_resolve_address_socket( destination, passive );
  }
  // todo: additional address resolvers go here ...

  return NULL;
}

static
void dbBE_Common_release_addrinfo( struct addrinfo **addrs )
{
  if(( addrs ) && ( *addrs ))
    freeaddrinfo( *addrs );
  *addrs = NULL;
}


#endif /* BACKEND_COMMON_RESOLVE_ADDR_H_ */
