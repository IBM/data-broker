/*
 * Copyright © 2018-2020 IBM Corporation
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
#include "definitions.h"

dbBE_Network_address_t* dbBE_Network_address_allocate()
{
  dbBE_Network_address_t *addr = (dbBE_Network_address_t*)malloc( sizeof( dbBE_Network_address_t ) );
  if( addr == NULL )
  {
    errno = ENOMEM;
    return NULL;
  }

  memset( addr, 0, sizeof( dbBE_Network_address_t ) );
  return addr;
}

dbBE_Network_address_t* dbBE_Network_address_create( const char *host,
                                                 const char *port )
{
  struct addrinfo hints, *addrs;
  memset( &hints, 0, sizeof( struct addrinfo ) );
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  int rc = getaddrinfo( host, port,
                        &hints,
                        &addrs);
  if( rc != 0 )
    return NULL;

  dbBE_Network_address_t *addr = dbBE_Network_address_copy( addrs->ai_addr, addrs->ai_addrlen );

  freeaddrinfo( addrs );

  return addr;
}

dbBE_Network_address_t* dbBE_Network_address_copy( struct sockaddr *in_addr,
                                               int in_addr_len )
{
  dbBE_Network_address_t *addr = dbBE_Network_address_allocate();
  if( addr != NULL )
    memcpy( &addr->_address, in_addr, in_addr_len );

  return addr;
}

void dbBE_Network_address_destroy( dbBE_Network_address_t *addr )
{
  if( addr == NULL )
  {
    errno = EINVAL;
    return;
  }
  memset( addr, 0, sizeof( dbBE_Network_address_t ) );
  free( addr );
}

const char* dbBE_Network_address_to_string( dbBE_Network_address_t *addr, char *str, int strmaxlen )
{
  if(( str == NULL ) || ( addr == NULL ))
    return NULL;

  char ip[ DBBE_URL_MAX_LENGTH ];
  if( inet_ntop( AF_INET, &(addr->_address.sin_addr.s_addr), ip, DBBE_URL_MAX_LENGTH ) == NULL )
    return NULL;

  // for now just use the sock prefix until there are more/other url types
  snprintf( str, strmaxlen, "sock://%s:%d", ip, ntohs( addr->_address.sin_port ) );

  return str;
}


dbBE_Network_address_t* dbBE_Network_address_from_string( const char *str )
{
  char *tmp = strdup( str );
  char *host = tmp;
  if( strchr( tmp, '/' ) != NULL )
  {
    host = strchr( tmp, ':');
    if( host == NULL )
    {
      free( tmp );
      return NULL;
    }
    host += 3;
  }
  char *port = strchr( host, ':' );
  if( port == NULL )
  {
    free( tmp );
    return NULL;
  }

  *port = '\0';
  port++;

  dbBE_Network_address_t *ret = dbBE_Network_address_create( host, port );
  free( tmp );
  return ret;
}

int dbBE_Network_address_compare_ip( struct sockaddr_in *a,
                                   struct sockaddr_in *b )
{
  if( (a == NULL) || (b == NULL) )
    return 1;
  int rc = (a->sin_family == b->sin_family );
  rc &= (a->sin_addr.s_addr == b->sin_addr.s_addr );
  return (rc == 0 );
}

int dbBE_Network_address_compare( dbBE_Network_address_t *a,
                                dbBE_Network_address_t *b )
{
  int rc = ( dbBE_Network_address_compare_ip( &a->_address, &b->_address ) == 0 );
  rc &= (a->_address.sin_port == b->_address.sin_port );
  return (rc == 0 );
}
