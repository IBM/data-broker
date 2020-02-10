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

#include <common/utility.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h> // for open
#include <unistd.h> // for close

#include "logutil.h"
#include "test_utils.h"

#include "common/utility.h"
#include "../backend/redis/event_mgr.h"

#define timeout 1



int main( int argc, char **argv )
{
  int rc = 0;

  dbBE_Redis_event_mgr_t *mgr = dbBE_Redis_event_mgr_init( timeout );
  rc += TEST_NOT( mgr, NULL );
  TEST_BREAK( rc, "event mgr create failed" );

  dbBE_Redis_connection_t *conn = dbBE_Redis_connection_create( DBBE_REDIS_SR_BUFFER_LEN );
  rc += TEST_NOT( conn, NULL );

  char *host = dbBE_Extract_env( DBR_SERVER_HOST_ENV, DBR_SERVER_DEFAULT_HOST );
  rc += TEST_NOT( host, NULL );
  char *auth = dbBE_Extract_env( DBR_SERVER_AUTHFILE_ENV, DBR_SERVER_DEFAULT_AUTHFILE );
  rc += TEST_NOT( auth, NULL );

  TEST_BREAK( rc, "connection create failed");

  /////////////////////////////////////////////////////////
  //  testing add()
  // testing inval argument cases for _add()
  rc += TEST( dbBE_Redis_event_mgr_add( NULL, NULL ), -EINVAL );
  rc += TEST( dbBE_Redis_event_mgr_add( mgr, NULL ), -EINVAL );
  rc += TEST( dbBE_Redis_event_mgr_add( NULL, conn ), -EINVAL );
  TEST_LOG( rc, "invalid arg testing" );

  // testing with socket == 0; should get us bad file descriptor error
  rc += TEST( dbBE_Redis_event_mgr_add( mgr, conn ), -EBADF );

  // testing with valid socket but invalid index
  conn->_socket = socket( AF_INET, SOCK_STREAM, 0 );
  rc += TEST_NOT( conn->_socket, -1 );
  rc += TEST( dbBE_Redis_event_mgr_add( mgr, conn ), -ERANGE );

  // do a correct insert
  conn->_index = 0;
  rc += TEST( dbBE_Redis_event_mgr_add( mgr, conn ), 0 );

  // testing to add with different index but same socket (should fail the eventbase add)
  conn->_index = 1;
  rc += TEST( dbBE_Redis_event_mgr_add( mgr, conn ), -EEXIST );
  conn->_index = 0;

  /////////////////////////////////////////////////////////
  //  testing rearm


  /////////////////////////////////////////////////////////
  //  testing rm()
  // testing _rm with invalid args
  rc += TEST( dbBE_Redis_event_mgr_rm( NULL, NULL ), -EINVAL );
  rc += TEST( dbBE_Redis_event_mgr_rm( mgr, NULL ), -EINVAL );
  rc += TEST( dbBE_Redis_event_mgr_rm( NULL, conn ), -EINVAL );


  rc += TEST( dbBE_Redis_event_mgr_rm( mgr, conn ), 0 );


  rc += TEST( dbBE_Redis_event_mgr_exit( NULL ), -EINVAL );
  rc += TEST( dbBE_Redis_event_mgr_exit( mgr ), 0 );

  close( conn->_socket );
  dbBE_Redis_connection_destroy( conn );

  TEST_BREAK( rc, "Error injection tests failed." );

  /////////////////////////////////////////////////////////
  //  testing the good path
  // connect to Redis and testing with correct parameters
  mgr = dbBE_Redis_event_mgr_init( timeout );
  rc += TEST_NOT( mgr, NULL );
  conn = dbBE_Redis_connection_create( DBBE_REDIS_SR_BUFFER_LEN );
  rc += TEST_NOT( conn, NULL );
  dbBE_Network_address_t *addr = dbBE_Redis_connection_link( conn, host, auth );
  rc += TEST_NOT( addr, NULL );

  conn->_index = 0;
  rc += TEST( dbBE_Redis_event_mgr_add( mgr, conn ), 0 );
  TEST_LOG( rc, "add testing" );

  dbBE_Redis_connection_t *active = NULL;
  int64_t retry = 10000000;
  while((--retry > 0) && ( active == NULL ))
    active = dbBE_Redis_event_mgr_next( mgr );

  // there should only be a timeout - i.e. no data pending status
  rc += TEST( dbBE_Redis_connection_get_status( active ), DBBE_CONNECTION_STATUS_AUTHORIZED );


  rc += TEST( dbBE_Redis_event_mgr_rm( mgr, conn ), 0 );














  rc += TEST( dbBE_Redis_event_mgr_exit( mgr ), 0 );

  rc += TEST( dbBE_Redis_connection_unlink( conn ), 0 );
  dbBE_Redis_connection_destroy( conn );

  free( auth );
  free( host );

  printf( "Test exiting with rc=%d\n", rc );
  return rc;
}
