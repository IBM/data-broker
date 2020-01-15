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

#ifndef BACKEND_NETWORK_ADDRESS_H_
#define BACKEND_NETWORK_ADDRESS_H_

#include <string.h>
#include <netinet/in.h>

typedef struct
{
  struct sockaddr_in _address;
} dbBE_Redis_address_t;


/*
 * allocate and reset the memory for an address
 */
dbBE_Redis_address_t* dbBE_Redis_address_allocate();

/*
 * create a redis address from host and port
 */
dbBE_Redis_address_t* dbBE_Redis_address_create( const char *host,
                                                 const char *port );

/*
 * copy the content of a socket address into the Redis address
 */
dbBE_Redis_address_t* dbBE_Redis_address_copy( struct sockaddr *in_addr,
                                               int in_addr_len );


/*
 * convert a sockaddr into a string addr:port
 */
const char* dbBE_Redis_address_to_string( dbBE_Redis_address_t *addr, char *str, int strmaxlen );


/*
 * convert a string into a sockaddr
 */
dbBE_Redis_address_t* dbBE_Redis_address_from_string( const char *str );


/*
 * split a string of format ip:port into 2 strings
 * The returned string points to the port, the original string is terminated at ':' and points to the IP
 */
static inline
char* dbBE_Redis_address_split( char *input )
{
  if( input == NULL )
    return NULL;

  char *port = strchr( input, ':' );
  if( port == NULL )
    return NULL;

  *port = '\0';  // terminate the IP section
  port += 1;     // point to the port section

  return port;
}

/*
 * compare 2 input addresses and return 0 if equal; 1 otherwise
 * compare the IP address only
 */
int dbBE_Redis_address_compare_ip( struct sockaddr_in *a,
                                   struct sockaddr_in *b );
/* include the port in the comparison too */
int dbBE_Redis_address_compare( dbBE_Redis_address_t *a,
                                dbBE_Redis_address_t *b );

/*
 * destroy the address and clean up memory
 */
void dbBE_Redis_address_destroy( dbBE_Redis_address_t *addr );

#endif /* BACKEND_NETWORK_ADDRESS_H_ */
