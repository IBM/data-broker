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

#include <errno.h>
#include <stdio.h>
#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>
#endif
#include <stddef.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include "address.h"

dbBE_Redis_address_t* dbBE_Redis_address_allocate()
{
  dbBE_Redis_address_t *addr = (dbBE_Redis_address_t*)malloc( sizeof( dbBE_Redis_address_t ) );
  if( addr == NULL )
  {
    errno = ENOMEM;
    return NULL;
  }

  memset( addr, 0, sizeof( dbBE_Redis_address_t ) );
  return addr;
}

dbBE_Redis_address_t* dbBE_Redis_address_create( const char *host,
                                                 const char *port )
{
  dbBE_Redis_address_t *addr = dbBE_Redis_address_allocate();
  if( addr != NULL )
  {
    struct addrinfo hints, *addrs;
    memset( &hints, 0, sizeof( struct addrinfo ) );
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    printf("Getting AddrInfo for host=%s port=%s\n", host, port );
    int rc = getaddrinfo( host, port,
                          &hints,
                          &addrs);
    if( rc != 0 )
      return NULL;

    addr = dbBE_Redis_address_copy( addrs->ai_addr, addrs->ai_addrlen );

    freeaddrinfo( addrs );
  }
  return addr;
}

dbBE_Redis_address_t* dbBE_Redis_address_copy( struct sockaddr *in_addr,
                                               int in_addr_len )
{
  dbBE_Redis_address_t *addr = dbBE_Redis_address_allocate();
  if( addr != NULL )
    memcpy( &addr->_address, in_addr, in_addr_len );

  return addr;
}

void dbBE_Redis_address_destroy( dbBE_Redis_address_t *addr )
{
  if( addr == NULL )
  {
    errno = EINVAL;
    return;
  }
  memset( addr, 0, sizeof( dbBE_Redis_address_t ) );
  free( addr );
}

const char* dbBE_Redis_address_to_string( dbBE_Redis_address_t *addr, char *str, int strmaxlen )
{
  if(( str == NULL ) || ( addr == NULL ))
    return NULL;

  char ip[ 32 ];
  if( inet_ntop( AF_INET, &(addr->_address.sin_addr.s_addr), ip, 32 ) == NULL )
    return NULL;

  sprintf( str, "%s:%d", ip, ntohs( addr->_address.sin_port ) );

  return str;
}


dbBE_Redis_address_t* dbBE_Redis_address_from_string( char *str )
{
  char *port = strchr( str, ':' );
  if( port == NULL )
    return NULL;

  *port = '\0';
  port++;

  return dbBE_Redis_address_create( str, port );
}
