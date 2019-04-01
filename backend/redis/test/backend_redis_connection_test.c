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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../backend/redis/redis.h"
#include "../backend/redis/utility.h"
#include "../backend/redis/connection.h"
#include "test_utils.h"

#define DBBE_TEST_BUFFER_LEN ( 1024 )

int main( int argc, char ** argv )
{
  int rc = 0;
  dbBE_Redis_connection_t *conn = NULL;
  dbBE_Redis_address_t *addr = NULL;
  dbBE_Redis_address_t *addr2 = NULL;

  conn = dbBE_Redis_connection_create( DBBE_REDIS_SR_BUFFER_LEN );
  rc += TEST_NOT( conn, NULL );
  rc += TEST( dbBE_Redis_connection_get_status( conn ), DBBE_CONNECTION_STATUS_INITIALIZED );
  fprintf(stderr,"0. rc=%d\n", rc);


  char *url = dbBE_Redis_extract_env( DBR_SERVER_HOST_ENV, DBR_SERVER_DEFAULT_HOST );
  rc += TEST_NOT( url, NULL );
  char *auth = dbBE_Redis_extract_env( DBR_SERVER_AUTHFILE_ENV, DBR_SERVER_DEFAULT_AUTHFILE );
  rc += TEST_NOT( auth, NULL );

  fprintf(stderr,"url=%s, auth=%s\n", url, auth);

  // try unlink without being connected
  rc += TEST_NOT( dbBE_Redis_connection_unlink( conn ), 0 );
  rc += TEST( dbBE_Redis_connection_get_status( conn ), DBBE_CONNECTION_STATUS_INITIALIZED );
  fprintf(stderr,"1. rc=%d\n", rc);

  addr = dbBE_Redis_connection_link( conn, "NON_EXSTHOST", auth );
  rc += TEST( addr, NULL );
  rc += TEST( dbBE_Redis_connection_get_status( conn ), DBBE_CONNECTION_STATUS_INITIALIZED );
  fprintf(stderr,"2. rc=%d, url=%s, auth=%s\n", rc, url, auth);

  addr = dbBE_Redis_connection_link( conn, url, "NON_EXISTFILE" );
  rc += TEST( addr, NULL );
  rc += TEST( dbBE_Redis_connection_get_status( conn ), DBBE_CONNECTION_STATUS_DISCONNECTED );
  fprintf(stderr,"3.rc=%d, url=%s\n", rc, url);

  addr = dbBE_Redis_connection_link( conn, "sock://NON_EXSTHOST", auth );
  rc += TEST( addr, NULL );
  rc += TEST( dbBE_Redis_connection_get_status( conn ), DBBE_CONNECTION_STATUS_DISCONNECTED );

  addr = dbBE_Redis_connection_link( conn, "://INVALIDURL", auth );
  rc += TEST( addr, NULL );
  rc += TEST( dbBE_Redis_connection_get_status( conn ), DBBE_CONNECTION_STATUS_DISCONNECTED );

  addr = dbBE_Redis_connection_link( conn, "sock://localhost:INVPORT", auth );
  rc += TEST( addr, NULL );
  rc += TEST( dbBE_Redis_connection_get_status( conn ), DBBE_CONNECTION_STATUS_DISCONNECTED );

  addr = dbBE_Redis_connection_link( conn, "sock://localhost:", auth );
  rc += TEST( addr, NULL );
  rc += TEST( dbBE_Redis_connection_get_status( conn ), DBBE_CONNECTION_STATUS_DISCONNECTED );


  // now we should be able to link
  fprintf(stderr,"7.conn->_status = %u\n", conn->_status);
  addr = dbBE_Redis_connection_link( conn, url, auth );
  rc += TEST_NOT( addr, NULL );
  fprintf(stderr,"7a.rc=%d, url=%s, auth=%s\n", rc, url, auth);
  rc += TEST( dbBE_Redis_connection_get_status( conn ), DBBE_CONNECTION_STATUS_AUTHORIZED );
  fprintf(stderr,"7b.rc=%d, url=%s, auth=%s\n", rc, url, auth);

  // connecting twice should not work
  addr2 = dbBE_Redis_connection_link( conn, url, auth );
  rc += TEST( addr2, NULL );
  rc += TEST( dbBE_Redis_connection_get_status( conn ), DBBE_CONNECTION_STATUS_AUTHORIZED );
  fprintf(stderr,"8.rc=%d, url=%s, auth=%s\n", rc, url, auth);

  dbBE_Redis_connection_fail( conn );
  rc += TEST( dbBE_Redis_connection_get_status( conn ), DBBE_CONNECTION_STATUS_FAILED );
  rc += TEST( dbBE_Redis_connection_RTS( conn ), 0 );
  rc += TEST( dbBE_Redis_connection_RTR( conn ), 0 );

  rc += TEST( dbBE_Redis_connection_send( conn ), -ENOTCONN );
  rc += TEST( dbBE_Redis_connection_recv( conn ), -ENOTCONN );

  rc += TEST( dbBE_Redis_connection_unlink( conn ), 0 );
  rc += TEST( dbBE_Redis_connection_get_status( conn ), DBBE_CONNECTION_STATUS_DISCONNECTED );

  // now we should be able to link
  fprintf(stderr,"8.conn->_status = %u\n", conn->_status);
  addr = dbBE_Redis_connection_link( conn, url, auth );
  rc += TEST_NOT( addr, NULL );
  fprintf(stderr,"8a.rc=%d, url=%s, auth=%s\n", rc, url, auth);
  rc += TEST( dbBE_Redis_connection_get_status( conn ), DBBE_CONNECTION_STATUS_AUTHORIZED );
  fprintf(stderr,"8b.rc=%d, url=%s, auth=%s\n", rc, url, auth);

  free( auth );
  free( url );

  if( rc != 0 )
  {
    fprintf( stderr, "Connection creation and link+auth failed. Skipping send/recv part of test.\n" );
    goto connect_failed;
  }

  // send an insert command for a new list
  dbBE_Transport_sr_buffer_reset( conn->_sendbuf );
  int len = snprintf( dbBE_Transport_sr_buffer_get_processed_position( conn->_sendbuf ),
                      dbBE_Transport_sr_buffer_remaining( conn->_sendbuf ),
                      "*3\r\n$5\r\nRPUSH\r\n$11\r\nTestNS::bla\r\n$12\r\nHello World!\r\n");

  dbBE_Redis_sr_buffer_add_data( conn->_sendbuf, len, 1 );
  rc += TEST( dbBE_Redis_connection_send( conn ), len );

  // receive the Redis response
  len = dbBE_Redis_connection_recv( conn );
  if( len > 0 )
  {
    rc += TEST( dbBE_Redis_sr_buffer_available( conn->_recvbuf ), (unsigned)len );
    rc += TEST( dbBE_Redis_sr_buffer_advance( conn->_recvbuf, len ), (size_t)len );
  }

  // send a get command for the inserted item
  len = snprintf( dbBE_Transport_sr_buffer_get_processed_position( conn->_sendbuf ),
                  dbBE_Transport_sr_buffer_remaining( conn->_sendbuf ),
                  "*2\r\n$4\r\nLPOP\r\n$11\r\nTestNS::bla\r\n");

  dbBE_Redis_sr_buffer_add_data( conn->_sendbuf, len, 1 );
  rc += TEST( dbBE_Redis_connection_send( conn ), len );

  // receive the Redis response
  len = dbBE_Redis_connection_recv( conn );
  if( len > 0 )
  {
    rc += TEST( dbBE_Redis_sr_buffer_available( conn->_recvbuf ), (unsigned)len );
    rc += TEST( dbBE_Redis_sr_buffer_advance( conn->_recvbuf, len ), (size_t)len );
  }

connect_failed:

  // disconnect
  rc += TEST( dbBE_Redis_connection_unlink( conn ), 0 );
  rc += TEST( dbBE_Redis_connection_get_status( conn ), DBBE_CONNECTION_STATUS_DISCONNECTED );

  // cleanup
  dbBE_Redis_connection_destroy( conn );

  printf( "Test exiting with rc=%d\n", rc );
  return rc;
}
