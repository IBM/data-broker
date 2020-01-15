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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/utility.h"
#include "network/connection.h"
#include "test_utils.h"

#define DBBE_TEST_BUFFER_LEN ( 1024 )

int main( int argc, char ** argv )
{
  int rc = 0;
  dbBE_Connection_t *conn = NULL;
  dbBE_Network_address_t *addr = NULL;
  dbBE_Network_address_t *addr2 = NULL;

  conn = dbBE_Connection_create( );
  rc += TEST_NOT( conn, NULL );
  rc += TEST( conn->_socket, -1 );
  rc += TEST( dbBE_Connection_get_status( conn ), DBBE_CONNECTION_STATUS_INITIALIZED );
  fprintf(stderr,"0. rc=%d\n", rc);


  char *url = dbBE_Extract_env( DBR_SERVER_HOST_ENV, DBR_SERVER_DEFAULT_HOST );
  rc += TEST_NOT( url, NULL );
  char *auth = dbBE_Extract_env( DBR_SERVER_AUTHFILE_ENV, DBR_SERVER_DEFAULT_AUTHFILE );
  rc += TEST_NOT( auth, NULL );

  fprintf(stderr,"url=%s, auth=%s\n", url, auth);

  // try unlink without being connected
  rc += TEST_NOT( dbBE_Connection_unlink( conn ), 0 );
  rc += TEST( dbBE_Connection_get_status( conn ), DBBE_CONNECTION_STATUS_INITIALIZED );

  rc += TEST_RC( dbBE_Connection_link( conn, "NON_EXSTHOST", auth ), NULL, addr );
  rc += TEST( dbBE_Connection_get_status( conn ), DBBE_CONNECTION_STATUS_INITIALIZED );

  rc += TEST_RC( dbBE_Connection_link( conn, url, "NON_EXISTFILE" ), NULL, addr );
  rc += TEST( dbBE_Connection_get_status( conn ), DBBE_CONNECTION_STATUS_DISCONNECTED );

  rc += TEST_RC( dbBE_Connection_link( conn, "sock://NON_EXSTHOST", auth ), NULL, addr );
  rc += TEST( dbBE_Connection_get_status( conn ), DBBE_CONNECTION_STATUS_DISCONNECTED );

  rc += TEST_RC( dbBE_Connection_link( conn, "://INVALIDURL", auth ), NULL, addr );
  rc += TEST( dbBE_Connection_get_status( conn ), DBBE_CONNECTION_STATUS_DISCONNECTED );

  rc += TEST_RC( dbBE_Connection_link( conn, "sock://localhost:INVPORT", auth ), NULL, addr );
  rc += TEST( dbBE_Connection_get_status( conn ), DBBE_CONNECTION_STATUS_DISCONNECTED );

  rc += TEST_RC( dbBE_Connection_link( conn, "sock://localhost:", auth ), NULL, addr );
  rc += TEST( dbBE_Connection_get_status( conn ), DBBE_CONNECTION_STATUS_DISCONNECTED );

  // now we should be able to link
  rc += TEST_NOT_RC( dbBE_Connection_link( conn, url, auth ), NULL, addr );
  rc += TEST( dbBE_Connection_get_status( conn ), DBBE_CONNECTION_STATUS_AUTHORIZED );

  // connecting twice should not work
  rc += TEST_RC( dbBE_Connection_link( conn, url, auth ), NULL, addr2 );
  rc += TEST( dbBE_Connection_get_status( conn ), DBBE_CONNECTION_STATUS_AUTHORIZED );

  rc += TEST( dbBE_Connection_unlink( conn ), 0 );
  rc += TEST( conn->_socket, -1 );
  rc += TEST_NOT( conn->_address, NULL ); // address is preserved after unlink
  rc += TEST( dbBE_Connection_get_status( conn ), DBBE_CONNECTION_STATUS_DISCONNECTED );
  rc += TEST( dbBE_Connection_RTS( conn ), 0 );
  rc += TEST( dbBE_Connection_RTR( conn ), 0 );

//  rc += TEST( dbBE_Connection_send( conn, NULL ), -EINVAL );
//  rc += TEST( dbBE_Connection_send( conn, sbuf ), -ENOTCONN );
//  rc += TEST( dbBE_Connection_recv( conn, rbuf ), -ENOTCONN );

  rc += TEST( dbBE_Connection_unlink( conn ), -EINVAL );
  rc += TEST( dbBE_Connection_get_status( conn ), DBBE_CONNECTION_STATUS_DISCONNECTED );

  // now we should be able to link again
  rc += TEST( dbBE_Connection_reconnect( conn ), 0 );
  rc += TEST( dbBE_Connection_get_status( conn ), DBBE_CONNECTION_STATUS_AUTHORIZED );

  free( auth );
  free( url );

  // disconnect
  rc += TEST( dbBE_Connection_unlink( conn ), 0 );
  rc += TEST( dbBE_Connection_get_status( conn ), DBBE_CONNECTION_STATUS_DISCONNECTED );

  // cleanup
  rc += TEST( dbBE_Connection_destroy( NULL ), -EINVAL );
  rc += TEST( dbBE_Connection_destroy( conn ), 0 );

  printf( "Test exiting with rc=%d\n", rc );
  return rc;
}
