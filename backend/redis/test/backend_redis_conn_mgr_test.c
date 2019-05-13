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
#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>
#endif
#include <string.h>

#include <sys/time.h>      // rlimit
#include <sys/resource.h>  // rlimit

#include "../backend/redis/utility.h"
#include "../backend/redis/request.h"
#include "../backend/redis/conn_mgr.h"
#include "test_utils.h"

#define DBBE_TEST_BUFFER_LEN ( 1024 )


int test_request_each( dbBE_Redis_connection_mgr_t *conn_mgr )
{
  int rc = 0;
  dbBE_Request_t user;
  user._key = strdup( "Hello" );
  user._sge_count = 0;
  user._user = NULL;
  user._next = NULL;
  user._opcode = DBBE_OPCODE_NSDELETE;

  dbBE_Redis_request_t *rbase = dbBE_Redis_request_allocate( &user );
  if( rbase == NULL )
  {
    TEST_LOG( 1, "Request allocation failed" );
    free( user._key );
    return 1;
  }

  dbBE_Redis_request_t *req_queue = dbBE_Redis_connection_mgr_request_each( conn_mgr, rbase );

  rc += TEST_NOT( req_queue, NULL );
  int last_idx = -1;
  while( req_queue != NULL )
  {
    dbBE_Redis_request_t *req = req_queue;
    req_queue = req_queue->_next;

    // check whether the request's connection index changes from request to request
    rc += TEST_NOT( last_idx, req->_location._data._conn_idx );
    last_idx = req->_location._data._conn_idx;

    dbBE_Redis_request_destroy( req );
  }
  free( user._key );
  dbBE_Redis_request_destroy( rbase );

  return rc;
}


int main( int argc, char ** argv )
{
  int rc = 0;
  unsigned i;

  dbBE_Redis_connection_mgr_t *mgr = NULL;
  dbBE_Redis_connection_t *carray[ DBBE_REDIS_MAX_CONNECTIONS + 5 ];
  dbBE_Redis_locator_t *locator = NULL;
  dbBE_Redis_cluster_info_t *cluster = NULL;
  dbBE_Redis_result_t *result = NULL;

  char *host = dbBE_Redis_extract_env( DBR_SERVER_HOST_ENV, DBR_SERVER_DEFAULT_HOST );
  rc += TEST_NOT( host, NULL );
  TEST_BREAK( rc, "host env failed");

  char *auth = dbBE_Redis_extract_env( DBR_SERVER_AUTHFILE_ENV, DBR_SERVER_DEFAULT_AUTHFILE );
  rc += TEST_NOT( auth, NULL );
  if( rc != 0 )
  {
    free( host );
  }

  // get limit for open file descriptors
  int64_t flimit = DBBE_REDIS_MAX_CONNECTIONS;
  struct rlimit sh_limit;
  rc += TEST( getrlimit( RLIMIT_NOFILE, & sh_limit ), 0 );

  TEST_BREAK( rc, "auth/env/limits failed");

  // set the max number of connections for this test
  flimit = ( (int64_t)sh_limit.rlim_cur <= flimit + 10 ) ? (int64_t)sh_limit.rlim_cur - 10 : flimit;
  LOG( DBG_ALL, stdout, "Connections limited to #%"PRId64"\n", flimit );


  rc += TEST_NOT_RC( dbBE_Redis_locator_create(), NULL, locator );
  rc += TEST_NOT_RC( dbBE_Redis_connection_mgr_init(), NULL, mgr );

  rc += TEST( dbBE_Redis_connection_mgr_add( mgr, NULL ), -EINVAL );
  rc += TEST( dbBE_Redis_connection_mgr_rm( mgr, NULL ), -EINVAL );
  rc += TEST( dbBE_Redis_connection_mgr_conn_fail( mgr, NULL ), -EINVAL );
  rc += TEST( dbBE_Redis_connection_mgr_conn_recover( NULL, NULL, NULL ), -EINVAL );
  rc += TEST( dbBE_Redis_connection_mgr_conn_recover( mgr, NULL, NULL ), -EINVAL );
  rc += TEST( dbBE_Redis_connection_mgr_conn_recover( NULL, locator, NULL ), -EINVAL );

  dbBE_Redis_connection_t *conn = dbBE_Redis_connection_create( DBBE_REDIS_SR_BUFFER_LEN );
  rc += TEST_NOT( conn, NULL );
  // adding connection with socket = 0 should fail
  rc += TEST( dbBE_Redis_connection_mgr_add( mgr, conn ), -EINVAL );

  rc += TEST_NOT( dbBE_Redis_connection_link( conn, host, auth ), NULL );
  rc += TEST( dbBE_Redis_connection_mgr_add( mgr, conn ), 0 );
  rc += TEST_NOT_RC( dbBE_Redis_connection_mgr_retrieve_clusterinfo( mgr, conn, conn->_recvbuf ), NULL, result );
  rc += TEST_NOT_RC( dbBE_Redis_cluster_info_create( result ), NULL, cluster );
  rc += TEST( dbBE_Redis_result_cleanup( result, 1 ), 0 );

  // test the NULL-ptr exit path, expect no changes after the first added connection
  dbBE_Redis_connection_mgr_exit( NULL );
  rc += TEST( dbBE_Redis_connection_mgr_get_connections( mgr ), 1 );


  // remove the connection and expect an empty mgr
  rc += TEST( dbBE_Redis_connection_mgr_rm( mgr, conn ), 0 );
  rc += TEST( dbBE_Redis_connection_mgr_get_connections( mgr ), 0 );

  // fill the connmgr with connections
  for( i = 0; i < flimit; ++i )
  {
    LOG( DBG_ALL, stdout, "=== Connection #%d ===\n", i );
    carray[ i ] = dbBE_Redis_connection_create( 512 );
    rc += TEST_NOT( carray[ i ], NULL );
    rc += TEST_NOT( dbBE_Redis_connection_link( carray[ i ], host, auth ), NULL );
    rc += TEST( dbBE_Redis_connection_mgr_add( mgr, carray[ i ] ), 0 );
  }
  TEST_LOG( rc, "After adding connections" );

  rc += TEST( dbBE_Redis_connection_mgr_get_connections( mgr ), flimit );

  rc += test_request_each( mgr );

  // try to add one more and fail
  dbBE_Redis_connection_t *conn2 = dbBE_Redis_connection_create( DBBE_REDIS_SR_BUFFER_LEN );
  rc += TEST_NOT( conn2, NULL );

  // if the conn_mgr has more space than ulimit, adding another connection will succeed
  // so we have to branch the test here...
  if( flimit < DBBE_REDIS_MAX_CONNECTIONS )
  {
    rc += TEST_NOT( dbBE_Redis_connection_link( conn2, host, auth ), NULL );
    rc += TEST( dbBE_Redis_connection_mgr_add( mgr, conn2 ), 0 );
    rc += TEST( dbBE_Redis_connection_mgr_get_connections( mgr ), flimit + 1 );
    rc += TEST( dbBE_Redis_connection_unlink( conn2 ), 0 );
    rc += TEST( dbBE_Redis_connection_mgr_rm( mgr, conn2 ), 0 );
    dbBE_Redis_connection_destroy( conn2 );
  }
  else
  {
    rc += TEST_NOT( dbBE_Redis_connection_link( conn2, host, auth ), NULL );
    rc += TEST( dbBE_Redis_connection_mgr_add( mgr, conn2 ), -ENOMEM );
    rc += TEST( dbBE_Redis_connection_mgr_get_connections( mgr ), flimit );
    rc += TEST( dbBE_Redis_connection_unlink( conn2 ), 0 );
    dbBE_Redis_connection_destroy( conn2 );
  }

  // fail one connection and try to recover
  dbBE_Redis_connection_t *dconn = carray[ 10 ];
  rc += TEST( dbBE_Redis_connection_mgr_conn_fail( mgr, dconn ), 0 );
  rc += TEST( dbBE_Redis_connection_mgr_conn_recover( mgr, locator, cluster ), 1 );


  // remove all connections
  for( i = 0; i < flimit; ++i )
  {
    rc += TEST( dbBE_Redis_connection_mgr_rm( mgr, carray[ i ] ), 0 );
    rc += TEST( dbBE_Redis_connection_unlink( carray[ i ] ), 0 );
    dbBE_Redis_connection_destroy( carray[ i ] );
    carray[ i ] = NULL;
  }
  rc += TEST( dbBE_Redis_connection_mgr_get_connections( mgr ), 0 );

  // try to remove one more and fail
  rc += TEST( dbBE_Redis_connection_mgr_rm( mgr, conn ), -ENOENT );
  rc += TEST( dbBE_Redis_connection_mgr_get_connections( mgr ), 0 );
  rc += TEST( dbBE_Redis_connection_unlink( conn ), 0 );
  dbBE_Redis_connection_destroy( conn );

  // test the actual exit
  dbBE_Redis_connection_mgr_exit( mgr );

  // conn was added to conn_mgr and therefore it's destroyed by mgr_exit()
  // dbBE_Redis_connection_destroy( conn );

  free( auth );
  free( host );

  printf( "Test exiting with rc=%d\n", rc );
  return rc;
}
