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
#include <string.h>
#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>
#endif

#include "test_utils.h"

#include "../backend/common/dbbe_api.h"
#include "../backend/common/data_transport.h"
#include "../backend/redis/parse.h"


#include "../backend/redis/create.h"
#include "../backend/redis/parse.h"
#include "../backend/redis/protocol.h"
#include "../backend/redis/result.h"
#include "../backend/transports/memcopy.h"

#define DBBE_TEST_BUFFER_LEN ( 1024 )

int TestNSCreate( const char *namespace,
                  dbBE_Redis_sr_buffer_t *sr_buf,
                  dbBE_Redis_request_t *req )
{
  int rc = 0;
  int len;
  rc += TEST_NOT( req, NULL );
  TEST_BREAK( rc, "NULL-ptr request in TestNSCreate()." );

  // create return data struct to test result of stage one (HSETNX)
  // returns number of created keys (integer)
  dbBE_Redis_result_t result;
  memset( &result, 0, sizeof( dbBE_Redis_result_t ) );

  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );
  dbBE_Transport_sr_buffer_reset( sr_buf );

  len = snprintf( dbBE_Transport_sr_buffer_get_start( sr_buf ),
                  dbBE_Transport_sr_buffer_get_size( sr_buf ),
                  ":1\r\n");
  rc += TEST_NOT( len, -1 );
  rc += TEST( dbBE_Transport_sr_buffer_add_data( sr_buf, len, 0 ), (size_t)len );

  rc += TEST( dbBE_Redis_parse_sr_buffer( sr_buf, &result ), 0 );
  rc += TEST( dbBE_Redis_process_nscreate( req, &result ), 0 );


  // transition to next stage
  rc += TEST( dbBE_Redis_request_stage_transition( req ), 0 );

  // create return data for result testing of second stage (
  // returns OK when successful (simple string)
  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );
  dbBE_Transport_sr_buffer_reset( sr_buf );

  len = snprintf( dbBE_Transport_sr_buffer_get_start( sr_buf ),
                  dbBE_Transport_sr_buffer_get_size( sr_buf ),
                      "+OK\r\n");
  rc += TEST_NOT( len, -1 );
  rc += TEST( dbBE_Transport_sr_buffer_add_data( sr_buf, len, 0 ), (size_t)len );

  rc += TEST( dbBE_Redis_parse_sr_buffer( sr_buf, &result ), 0 );
  rc += TEST( dbBE_Redis_process_nscreate( req, &result ), 0 );

  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );

  return rc;
}



int TestNSAttach( const char *namespace,
                  dbBE_Redis_sr_buffer_t *sr_buf,
                  dbBE_Redis_request_t *req )
{
  int rc = 0;
  int len;
  rc += TEST_NOT( req, NULL );
  TEST_BREAK( rc, "NULL-ptr request in TestNSCreate()." );

  // create return data struct to test result of stage one (EXISTS)
  // returns number of found keys (integer)
  dbBE_Redis_result_t result;
  memset( &result, 0, sizeof( dbBE_Redis_result_t ) );

  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );
  dbBE_Transport_sr_buffer_reset( sr_buf );

  len = snprintf( dbBE_Transport_sr_buffer_get_start( sr_buf ),
                  dbBE_Transport_sr_buffer_get_size( sr_buf ),
                  ":1\r\n");
  rc += TEST_NOT( len, -1 );
  rc += TEST( dbBE_Transport_sr_buffer_add_data( sr_buf, len, 0 ), (size_t)len );

  rc += TEST( dbBE_Redis_parse_sr_buffer( sr_buf, &result ), 0 );
  rc += TEST( dbBE_Redis_process_nsattach( req, &result ), 0 );


  // transition to next stage
  rc += TEST( dbBE_Redis_request_stage_transition( req ), 0 );

  // create return data to test result of stage 2 (HINCRBY)
  // returns new (increased) value (integer)
  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );
  dbBE_Transport_sr_buffer_reset( sr_buf );

  len = snprintf( dbBE_Transport_sr_buffer_get_start( sr_buf ),
                  dbBE_Transport_sr_buffer_get_size( sr_buf ),
                  ":5\r\n");
  rc += TEST_NOT( len, -1 );
  rc += TEST( dbBE_Transport_sr_buffer_add_data( sr_buf, len, 0 ), (size_t)len );

  rc += TEST( dbBE_Redis_parse_sr_buffer( sr_buf, &result ), 0 );
  rc += TEST( dbBE_Redis_process_nsattach( req, &result ), 0 );

  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );

  return rc;
}


/*
 * Even though Detach() uses the same redis commands as Attach(),
 * the post-processing is different and therefore needs to be tested
 * separately
 */
int TestNSDetach( const char *namespace,
                  dbBE_Redis_sr_buffer_t *sr_buf,
                  dbBE_Redis_request_t *req )
{
  int rc = 0;
  int len;
  rc += TEST_NOT( req, NULL );
  TEST_BREAK( rc, "NULL-ptr request in TestNSCreate()." );
  dbBE_Redis_result_t result;
  memset( &result, 0, sizeof( dbBE_Redis_result_t ) );

  // make a copy of the request because delete processing messes around with the redis request
  dbBE_Redis_request_t *req_io = dbBE_Redis_request_allocate( req->_user );

  // create a dummy connection mgr (for scan)
  dbBE_Redis_connection_mgr_t *cmr;
  dbBE_Redis_conn_mgr_config_t config;
  config._rbuf_len = 1024;
  config._sbuf_len = 1024;
  rc += TEST_NOT_RC( dbBE_Redis_connection_mgr_init( &config ), NULL, cmr );

  // create a fake valid connection to allow the delete scan to start
  dbBE_Redis_connection_t *conn;
  rc += TEST_NOT_RC( dbBE_Redis_connection_create( 1024 ), NULL, conn );
  conn->_socket = socket( AF_INET, SOCK_STREAM, 0 );
  conn->_status = DBBE_CONNECTION_STATUS_AUTHORIZED;
  rc += TEST( dbBE_Redis_connection_mgr_add( cmr, conn ), 0 );
  TEST_BREAK( rc, "Conn/ConnMgr setup already failed, can't continue\n" );

  // create a dummy request queue (for del)
  dbBE_Redis_s2r_queue_t *post_queue = dbBE_Redis_s2r_queue_create( 12 );


  // create return data struct to test result of stage one (HMGET refcnt flags)
  // returns array: 1 1
  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );
  dbBE_Transport_sr_buffer_reset( sr_buf );

  // create a response that would trigger the delete path: refcnt=1 flags=1
  len = snprintf( dbBE_Transport_sr_buffer_get_start( sr_buf ),
                  dbBE_Transport_sr_buffer_get_size( sr_buf ),
                  "+OK\r\n+QUEUED\r\n+QUEUED\r\n*2\r\n:0\r\n*2\r\n$1\r\n0\r\n$1\r\n1\r\n");
  rc += TEST_NOT( len, -1 );
  rc += TEST( dbBE_Transport_sr_buffer_add_data( sr_buf, len, 0 ), (size_t)len );

  rc += TEST( dbBE_Redis_parse_sr_buffer( sr_buf, &result ), 0 );
  rc += TEST( dbBE_Redis_process_nsdetach( &req_io, &result, post_queue, cmr, 3 ), 0 );
  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );

  rc += TEST( dbBE_Redis_parse_sr_buffer( sr_buf, &result ), 0 );
  rc += TEST( dbBE_Redis_process_nsdetach( &req_io, &result, post_queue, cmr, 2 ), 0 );
  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );

  rc += TEST( dbBE_Redis_parse_sr_buffer( sr_buf, &result ), 0 );
  rc += TEST( dbBE_Redis_process_nsdetach( &req_io, &result, post_queue, cmr, 1 ), 0 );
  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );

  rc += TEST( dbBE_Redis_parse_sr_buffer( sr_buf, &result ), 0 );
  rc += TEST( dbBE_Redis_process_nsdetach( &req_io, &result, post_queue, cmr, 0 ), 0 );

  rc += TEST( req_io, NULL ); // the returned request should be NULL because new SCAN requests had been pushed to the s2r queue

  // no transition, because a bunch of new requests have been pushed to s2r queue
  // rc += TEST( dbBE_Redis_request_stage_transition( req ), 0 );
  req_io = dbBE_Redis_s2r_queue_pop( post_queue );
  rc += TEST_NOT( req_io, NULL );
  rc += TEST( req_io->_step->_stage, DBBE_REDIS_NSDETACH_STAGE_SCAN );
  TEST_BREAK( rc, "no request in scan queue");

  // create return data to test result of stage 2 (SCAN)
  // returns array with cursor (integer) and array of keys (bulk strings)
  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );
  dbBE_Transport_sr_buffer_reset( sr_buf );

  len = snprintf( dbBE_Transport_sr_buffer_get_start( sr_buf ),
                  dbBE_Transport_sr_buffer_get_size( sr_buf ),
                  "*2\r\n$1\r\n0\r\n*5\r\n$3\r\nbla\r\n$2\r\nhi\r\n$5\r\nfasel\r\n$4\r\nfoob\r\n$6\r\ngnartz\r\n");
  rc += TEST_NOT( len, -1 );
  rc += TEST( dbBE_Transport_sr_buffer_add_data( sr_buf, len, 0 ), (size_t)len );

  rc += TEST( dbBE_Redis_parse_sr_buffer( sr_buf, &result ), 0 );
  rc += TEST( dbBE_Redis_process_nsdetach( &req_io, &result, post_queue, cmr, 0 ), 0 );

  rc += TEST( dbBE_Redis_s2r_queue_len( post_queue ), 5 );

  // pop the first request from the queue to get a prepared delete key request
  req_io = NULL;
  while( req_io == NULL )
  {
    req_io = dbBE_Redis_s2r_queue_pop( post_queue );
    rc += TEST_NOT( req_io, NULL );
    TEST_BREAK( rc, "no request in scan queue");

    rc += TEST( req_io->_step->_stage, DBBE_REDIS_NSDETACH_STAGE_DELKEYS );

    // create return data to test result of stage 3 (DEL)
    // returns the number of deleted keys
    rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );
    dbBE_Transport_sr_buffer_reset( sr_buf );

    len = snprintf( dbBE_Transport_sr_buffer_get_start( sr_buf ),
                    dbBE_Transport_sr_buffer_get_size( sr_buf ),
                    ":1\r\n");
    rc += TEST_NOT( len, -1 );
    rc += TEST( dbBE_Transport_sr_buffer_add_data( sr_buf, len, 0 ), (size_t)len );

    rc += TEST( dbBE_Redis_parse_sr_buffer( sr_buf, &result ), 0 );
    rc += TEST( dbBE_Redis_process_nsdetach( &req_io, &result, post_queue, cmr, 0 ), 0 );

    if( dbBE_Redis_s2r_queue_len( post_queue ) != 0 )
      rc += TEST( req_io, NULL );
    else
      rc += TEST_NOT( req_io, NULL );
  }
  rc += TEST( dbBE_Redis_s2r_queue_len( post_queue ), 0 );

  rc += TEST( dbBE_Redis_request_stage_transition( req_io ), 0 );
  rc += TEST( req_io->_step->_stage, DBBE_REDIS_NSDETACH_STAGE_DELNS );

  // create return data to test result of stage 4 (DEL)
  // returns the number of deleted keys
  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );
  dbBE_Transport_sr_buffer_reset( sr_buf );

  len = snprintf( dbBE_Transport_sr_buffer_get_start( sr_buf ),
                  dbBE_Transport_sr_buffer_get_size( sr_buf ),
                  ":1\r\n");
  rc += TEST_NOT( len, -1 );
  rc += TEST( dbBE_Transport_sr_buffer_add_data( sr_buf, len, 0 ), (size_t)len );

  rc += TEST( dbBE_Redis_parse_sr_buffer( sr_buf, &result ), 0 );
  rc += TEST( dbBE_Redis_process_nsdetach( &req_io, &result, post_queue, cmr, 0 ), 0 );

  // using only req below here
  dbBE_Redis_request_destroy( req_io );
  req_io = NULL;

  // reset the request stage
  req->_step = &gRedis_command_spec[ DBBE_OPCODE_NSDETACH * DBBE_REDIS_COMMAND_STAGE_MAX + 0 ];
  rc += TEST( req->_step->_stage, DBBE_REDIS_NSDETACH_STAGE_DELCHECK );
  // Set the first stage response to go the detach-path
  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );
  dbBE_Transport_sr_buffer_reset( sr_buf );

  // just stage 3 output (skipping multi and queued response)
  len = snprintf( dbBE_Transport_sr_buffer_get_start( sr_buf ),
                  dbBE_Transport_sr_buffer_get_size( sr_buf ),
                  "*2\r\n:3\r\n*2\r\n$1\r\n3\r\n$1\r\n1\r\n");
  rc += TEST_NOT( len, -1 );
  rc += TEST( dbBE_Transport_sr_buffer_add_data( sr_buf, len, 0 ), (size_t)len );

  rc += TEST( dbBE_Redis_parse_sr_buffer( sr_buf, &result ), 0 );
  rc += TEST( dbBE_Redis_process_nsdetach( &req, &result, post_queue, cmr, 0 ), 0 );

  // detach only needs to come back with final stage (which has no command at all
  rc += TEST( req->_step->_stage, DBBE_REDIS_NSDETACH_STAGE_DELNS );

  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );
  dbBE_Transport_sr_buffer_reset( sr_buf );

  dbBE_Redis_s2r_queue_destroy( post_queue );
  dbBE_Redis_connection_mgr_exit( cmr );

  return rc;
}


int TestNSDelete( const char *namespace,
                  dbBE_Redis_sr_buffer_t *sr_buf,
                  dbBE_Redis_request_t *req )
{
  int rc = 0;
  int len;
  rc += TEST_NOT( req, NULL );

  dbBE_Redis_result_t result;
  memset( &result, 0, sizeof( dbBE_Redis_result_t ) );

  rc += TEST( req->_step->_stage, DBBE_REDIS_NSDELETE_STAGE_EXIST );

  // create return data to test result of stage 0 (HMGET refcnt flags)
  // return needs to be <=1 and 0 (or it's an error)
  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );
  dbBE_Transport_sr_buffer_reset( sr_buf );

  len = snprintf( dbBE_Transport_sr_buffer_get_start( sr_buf ),
                  dbBE_Transport_sr_buffer_get_size( sr_buf ),
                  "*2\r\n$1\r\n1\r\n$1\r\n0\r\n");
  rc += TEST_NOT( len, -1 );
  rc += TEST( dbBE_Transport_sr_buffer_add_data( sr_buf, len, 0 ), (size_t)len );

  rc += TEST( dbBE_Redis_parse_sr_buffer( sr_buf, &result ), 0 );
  rc += TEST( dbBE_Redis_process_nsdelete( req, &result ), 0 );


  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );
  dbBE_Transport_sr_buffer_reset( sr_buf );

  // create BUSY error situation with refcnt > 1
  len = snprintf( dbBE_Transport_sr_buffer_get_start( sr_buf ),
                  dbBE_Transport_sr_buffer_get_size( sr_buf ),
                  "*2\r\n$1\r\n2\r\n$1\r\n0\r\n");
  rc += TEST_NOT( len, -1 );
  rc += TEST( dbBE_Transport_sr_buffer_add_data( sr_buf, len, 0 ), (size_t)len );

  rc += TEST( dbBE_Redis_parse_sr_buffer( sr_buf, &result ), 0 );
  rc += TEST( dbBE_Redis_process_nsdelete( req, &result ), EBUSY );
  rc += TEST( result._type, dbBE_REDIS_TYPE_INT );
  rc += TEST( result._data._integer, 1 );

  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );
  dbBE_Transport_sr_buffer_reset( sr_buf );

  // create situation with refcnt = 1; flags set
  len = snprintf( dbBE_Transport_sr_buffer_get_start( sr_buf ),
                  dbBE_Transport_sr_buffer_get_size( sr_buf ),
                  "*2\r\n$1\r\n1\r\n$1\r\n1\r\n");
  rc += TEST_NOT( len, -1 );
  rc += TEST( dbBE_Transport_sr_buffer_add_data( sr_buf, len, 0 ), (size_t)len );

  rc += TEST( dbBE_Redis_parse_sr_buffer( sr_buf, &result ), 0 );
  rc += TEST( dbBE_Redis_process_nsdelete( req, &result ), 0 );
  rc += TEST( result._type, dbBE_REDIS_TYPE_INT );
  rc += TEST( result._data._integer, 0 );


  // create return data to test result of stage 1 (HSET)
  // return needs to be 0 (or it's an error)
  rc += TEST( dbBE_Redis_request_stage_transition( req ), 0 );
  rc += TEST( req->_step->_stage, DBBE_REDIS_NSDELETE_STAGE_SETFLAG );

  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );
  dbBE_Transport_sr_buffer_reset( sr_buf );

  len = snprintf( dbBE_Transport_sr_buffer_get_start( sr_buf ),
                  dbBE_Transport_sr_buffer_get_size( sr_buf ),
                  ":0\r\n");
  rc += TEST_NOT( len, -1 );
  rc += TEST( dbBE_Transport_sr_buffer_add_data( sr_buf, len, 0 ), (size_t)len );

  rc += TEST( dbBE_Redis_parse_sr_buffer( sr_buf, &result ), 0 );
  rc += TEST( dbBE_Redis_process_nsdelete( req, &result ), 0 );
  rc += TEST( result._type, dbBE_REDIS_TYPE_INT );
  rc += TEST( result._data._integer, 0 );

  // create return data to test non-existing namespace in stage 1 (HSET)
  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );
  dbBE_Transport_sr_buffer_reset( sr_buf );

  LOG( DBG_ALL, stderr, "Testing non-existing namespace failure:\n" );
  len = snprintf( dbBE_Transport_sr_buffer_get_start( sr_buf ),
                  dbBE_Transport_sr_buffer_get_size( sr_buf ),
                  ":1\r\n");
  rc += TEST_NOT( len, -1 );
  rc += TEST( dbBE_Transport_sr_buffer_add_data( sr_buf, len, 0 ), (size_t)len );

  rc += TEST( dbBE_Redis_parse_sr_buffer( sr_buf, &result ), 0 );
  rc += TEST( dbBE_Redis_process_nsdelete( req, &result ), -ENOENT );
  rc += TEST( result._type, dbBE_REDIS_TYPE_INT );
  rc += TEST( result._data._integer, 0 );

  return rc;
}

int TestPut( const char *namespace,
             dbBE_Redis_sr_buffer_t *sr_buf,
             dbBE_Redis_request_t *req )
{
  int rc = 0;
  int len;
  rc += TEST_NOT( req, NULL );
  TEST_BREAK( rc, "NULL-ptr request in TestNSCreate()." );

  // create return data struct to test result (RPUSH)
  // returns number of entries in list (integer)
  dbBE_Redis_result_t result;
  memset( &result, 0, sizeof( dbBE_Redis_result_t ) );

  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );
  dbBE_Transport_sr_buffer_reset( sr_buf );

  len = snprintf( dbBE_Transport_sr_buffer_get_start( sr_buf ),
                  dbBE_Transport_sr_buffer_get_size( sr_buf ),
                  ":1\r\n");
  rc += TEST_NOT( len, -1 );
  rc += TEST( dbBE_Transport_sr_buffer_add_data( sr_buf, len, 0 ), (size_t)len );

  rc += TEST( dbBE_Redis_parse_sr_buffer( sr_buf, &result ), 0 );
  rc += TEST( dbBE_Redis_process_put( req, &result ), 0 );

  return rc;
}


int TestRead( const char *namespace,
              dbBE_Redis_sr_buffer_t *sr_buf,
              dbBE_Redis_request_t *req )
{
  int rc = 0;
  int len;
  rc += TEST_NOT( req, NULL );
  TEST_BREAK( rc, "NULL-ptr request in TestNSCreate()." );

  dbBE_Data_transport_t *transport = &dbBE_Memcopy_transport;
  dbBE_Redis_connection_t *connection = NULL;
  rc += TEST_NOT_RC( dbBE_Redis_connection_create(1024), NULL, connection );

  // create return data struct to test result (LINDEX/LPOP)
  // returns number of entries in list (integer)
  dbBE_Redis_result_t result;
  memset( &result, 0, sizeof( dbBE_Redis_result_t ) );

  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );
  dbBE_Transport_sr_buffer_reset( sr_buf );

  len = snprintf( dbBE_Transport_sr_buffer_get_start( sr_buf ),
                  dbBE_Transport_sr_buffer_get_size( sr_buf ),
                  "$12\r\nReturnString\r\n");
  rc += TEST_NOT( len, -1 );
  rc += TEST( dbBE_Transport_sr_buffer_add_data( sr_buf, len, 0 ), (size_t)len );

  rc += TEST( dbBE_Redis_parse_sr_buffer( sr_buf, &result ), 0 );
  rc += TEST( dbBE_Redis_process_get( req, &result, transport, connection ), 0 );


  dbBE_Redis_connection_destroy( connection );
  return rc;
}


int TestDirectory( const char *namespace,
                   dbBE_Redis_sr_buffer_t *sr_buf,
                   dbBE_Redis_request_t *req )
{
  int rc = 0;
  int len;
  rc += TEST_NOT( req, NULL );
  TEST_BREAK( rc, "NULL-ptr request in TestNSCreate()." );

  dbBE_Redis_result_t result;
  memset( &result, 0, sizeof( dbBE_Redis_result_t ) );

  // make a copy of the request because delete processing messes around with the redis request
  dbBE_Redis_request_t *req_io = dbBE_Redis_request_allocate( req->_user );

  // set a transport for the return of the data
  dbBE_Data_transport_t *transport = &dbBE_Memcopy_transport;

  // create a dummy connection mgr (for scan)
  dbBE_Redis_connection_mgr_t *cmr;
  dbBE_Redis_conn_mgr_config_t config;
  config._rbuf_len = 1024;
  config._sbuf_len = 1024;
  rc += TEST_NOT_RC( dbBE_Redis_connection_mgr_init( &config ), NULL, cmr );

  // create a fake valid connection to allow the delete scan to start
  dbBE_Redis_connection_t *conn;
  rc += TEST_NOT_RC( dbBE_Redis_connection_create( 1024 ), NULL, conn );
  conn->_socket = socket( AF_INET, SOCK_STREAM, 0 );
  conn->_status = DBBE_CONNECTION_STATUS_AUTHORIZED;
  rc += TEST( dbBE_Redis_connection_mgr_add( cmr, conn ), 0 );
  TEST_BREAK( rc, "Conn/ConnMgr init already failed. Can't continue\n" );

  // create a dummy request queue (for del)
  dbBE_Redis_s2r_queue_t *post_queue = dbBE_Redis_s2r_queue_create( 12 );

  // create return data to test result of stage 1 (HGETALL)
  // return meta data of namespace (array)
  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );
  dbBE_Transport_sr_buffer_reset( sr_buf );
  len = snprintf( dbBE_Transport_sr_buffer_get_start( sr_buf ),
                  dbBE_Transport_sr_buffer_get_size( sr_buf ),
                  "*3\r\n$6\r\nTestNS\r\n$5\r\ncount\r\n$1\r\n7\r\n");
  rc += TEST_NOT( len, -1 );
  rc += TEST( dbBE_Transport_sr_buffer_add_data( sr_buf, len, 0 ), (size_t)len );

  rc += TEST( dbBE_Redis_parse_sr_buffer( sr_buf, &result ), 0 );
  rc += TEST( dbBE_Redis_process_directory( &req_io, &result, transport, post_queue, cmr ), 0 );

  req_io = dbBE_Redis_s2r_queue_pop( post_queue );
  rc += TEST_NOT( req_io, NULL );
  TEST_BREAK( rc, "no request in scan queue");

  // no need to transition the request because the request is prepared already
  //rc += TEST( dbBE_Redis_request_stage_transition( req_io ), 0 );

  // create return data struct to test result (SCAN)
  // returns number of entries in list (integer)
  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );
  dbBE_Transport_sr_buffer_reset( sr_buf );

  len = snprintf( dbBE_Transport_sr_buffer_get_start( sr_buf ),
                  dbBE_Transport_sr_buffer_get_size( sr_buf ),
                  "*2\r\n$1\r\n0\r\n*5\r\n$11\r\nTestNS::bla\r\n$10\r\nTestNS::hi\r\n$13\r\nTestNS::fasel\r\n$12\r\nTestNS::foob\r\n$14\r\nTestNS::gnartz\r\n");
  rc += TEST_NOT( len, -1 );
  rc += TEST( dbBE_Transport_sr_buffer_add_data( sr_buf, len, 0 ), (size_t)len );

  rc += TEST( dbBE_Redis_parse_sr_buffer( sr_buf, &result ), 0 );
  rc += TEST( dbBE_Redis_process_directory( &req_io, &result, transport, post_queue, cmr ), 0 );
  rc += TEST( strncmp( (char*)req_io->_user->_sge[0].iov_base, "bla\nhi\nfasel\nfoob\ngnartz", 1024 ), 0 );


  dbBE_Redis_s2r_queue_destroy( post_queue );


  // cleanup the connections
  dbBE_Redis_connection_mgr_rm( cmr, conn );
  dbBE_Redis_connection_destroy( conn );
  dbBE_Redis_connection_mgr_exit( cmr );
  dbBE_Redis_request_destroy( req_io );

  return rc;
}

int TestRemove( const char *namespace,
                dbBE_Redis_sr_buffer_t *sr_buf,
                dbBE_Redis_request_t *req )
{
  int rc = 0;
  dbBE_Redis_result_t result;
  memset( &result, 0, sizeof( dbBE_Redis_result_t ) );
  int len = 0;

  // successful remove
  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );
  dbBE_Transport_sr_buffer_reset( sr_buf );

  len = snprintf( dbBE_Transport_sr_buffer_get_start( sr_buf ),
                  dbBE_Transport_sr_buffer_get_size( sr_buf ),
                  ":1\r\n");
  rc += TEST_NOT( len, -1 );
  rc += TEST( dbBE_Transport_sr_buffer_add_data( sr_buf, len, 0 ), (size_t)len );

  rc += TEST( dbBE_Redis_parse_sr_buffer( sr_buf, &result ), 0 );
  rc += TEST( dbBE_Redis_process_remove( req, &result ), 0 );

  rc += TEST( result._type, dbBE_REDIS_TYPE_INT );
  rc += TEST( result._data._integer, 0 );

  // failed remove (not exist)
  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );
  dbBE_Transport_sr_buffer_reset( sr_buf );

  len = snprintf( dbBE_Transport_sr_buffer_get_start( sr_buf ),
                  dbBE_Transport_sr_buffer_get_size( sr_buf ),
                  ":0\r\n");
  rc += TEST_NOT( len, -1 );
  rc += TEST( dbBE_Transport_sr_buffer_add_data( sr_buf, len, 0 ), (size_t)len );

  rc += TEST( dbBE_Redis_parse_sr_buffer( sr_buf, &result ), 0 );
  rc += TEST( dbBE_Redis_process_remove( req, &result ), -ENOENT );

  rc += TEST( result._type, dbBE_REDIS_TYPE_INT );
  rc += TEST( result._data._integer, 0 );

  // protocol error response
  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );
  dbBE_Transport_sr_buffer_reset( sr_buf );

  len = snprintf( dbBE_Transport_sr_buffer_get_start( sr_buf ),
                  dbBE_Transport_sr_buffer_get_size( sr_buf ),
                  "-Error in Protocol\r\n");
  rc += TEST_NOT( len, -1 );
  rc += TEST( dbBE_Transport_sr_buffer_add_data( sr_buf, len, 0 ), (size_t)len );

  rc += TEST( dbBE_Redis_parse_sr_buffer( sr_buf, &result ), 0 );
  rc += TEST( dbBE_Redis_process_remove( req, &result ), -EBADMSG );

  rc += TEST( result._type, dbBE_REDIS_TYPE_INT ); // result still regular int, rc carries error code
  rc += TEST( result._data._integer, 0 );


  return rc;
}

int TestMove( const char *namespace,
              dbBE_Redis_sr_buffer_t *sr_buf,
              dbBE_Redis_request_t *req )
{
  int rc = 0;
  int len = 0;

  dbBE_Redis_connection_t *connection = NULL;
  rc += TEST_NOT_RC( dbBE_Redis_connection_create(1024), NULL, connection );

  dbBE_Redis_result_t result;
  memset( &result, 0, sizeof( dbBE_Redis_result_t ) );

  // stage: DUMP
  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );
  dbBE_Transport_sr_buffer_reset( sr_buf );

  len = snprintf( dbBE_Transport_sr_buffer_get_start( sr_buf ),
                  dbBE_Transport_sr_buffer_get_size( sr_buf ),
                  "$10\r\nDumpedData\r\n");
  rc += TEST_NOT( len, -1 );
  rc += TEST( dbBE_Transport_sr_buffer_add_data( sr_buf, len, 0 ), (size_t)len );

  rc += TEST( dbBE_Redis_parse_sr_buffer( sr_buf, &result ), 0 );
  rc += TEST( dbBE_Redis_process_move( req, &result, connection ), 0 );

  rc += TEST( strncmp( (char*)result._data._string._data, "DumpedData", 10 ), 0 );
  rc += TEST( result._data._string._size, 10 );
  rc += TEST_NOT( req->_status.move.dumped_value, NULL );
  rc += TEST( req->_status.move.len, 10 );

  // stage: RESTORE
  rc += TEST( dbBE_Redis_request_stage_transition( req ), 0 );

  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );
  dbBE_Transport_sr_buffer_reset( sr_buf );

  len = snprintf( dbBE_Transport_sr_buffer_get_start( sr_buf ),
                  dbBE_Transport_sr_buffer_get_size( sr_buf ),
                  "$2\r\nOK\r\n");
  rc += TEST_NOT( len, -1 );
  rc += TEST( dbBE_Transport_sr_buffer_add_data( sr_buf, len, 0 ), (size_t)len );

  rc += TEST( dbBE_Redis_parse_sr_buffer( sr_buf, &result ), 0 );
  rc += TEST( dbBE_Redis_process_move( req, &result, connection ), 0 );

  rc += TEST( strncmp( (char*)result._data._string._data, "OK", 2 ), 0 );
  rc += TEST( result._data._string._size, 2 );
  rc += TEST( req->_status.move.dumped_value, NULL );
  rc += TEST( req->_status.move.len, 0 );


  // stage: DEL
  rc += TEST( dbBE_Redis_request_stage_transition( req ), 0 );

  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );
  dbBE_Transport_sr_buffer_reset( sr_buf );

  len = snprintf( dbBE_Transport_sr_buffer_get_start( sr_buf ),
                  dbBE_Transport_sr_buffer_get_size( sr_buf ),
                  ":1\r\n");
  rc += TEST_NOT( len, -1 );
  rc += TEST( dbBE_Transport_sr_buffer_add_data( sr_buf, len, 0 ), (size_t)len );

  rc += TEST( dbBE_Redis_parse_sr_buffer( sr_buf, &result ), 0 );
  rc += TEST( dbBE_Redis_process_move( req, &result, connection ), 0 );

  rc += TEST( result._data._integer, 1 );
  rc += TEST( req->_status.move.dumped_value, NULL );
  rc += TEST( req->_status.move.len, 0 );

  dbBE_Redis_connection_destroy( connection );
  return rc;
}


int main( int argc, char ** argv )
{
  int rc = 0;
  char buffer[1024];

  dbBE_Redis_sr_buffer_t *sr_buf = dbBE_Transport_sr_buffer_allocate( DBBE_TEST_BUFFER_LEN );
  dbBE_Redis_sr_buffer_t *data_buf = dbBE_Transport_sr_buffer_allocate( DBBE_TEST_BUFFER_LEN ); // misuse, just as a buffer
  dbBE_Redis_request_t *req;
  dbBE_Request_t* ureq = (dbBE_Request_t*) malloc (sizeof(dbBE_Request_t) + 2 * sizeof(dbBE_sge_t));

  dbBE_Redis_command_stage_spec_t *stage_specs = dbBE_Redis_command_stages_spec_init();
  rc += TEST_NOT( stage_specs, NULL );

  ureq->_opcode = DBBE_OPCODE_UNSPEC;
  ureq->_key = "bla";


  ureq->_opcode = DBBE_OPCODE_NSCREATE;
  ureq->_sge_count = 1;
  ureq->_sge[0].iov_base = strdup("users, admins");
  ureq->_sge[0].iov_len = strlen( ureq->_sge[0].iov_base );
  ureq->_group = DBR_GROUP_EMPTY;

  req = dbBE_Redis_request_allocate( ureq );
  rc += TestNSCreate( "TestNS", sr_buf, req );
  dbBE_Redis_request_destroy( req );



  ureq->_opcode = DBBE_OPCODE_NSATTACH;
  free( ureq->_sge[0].iov_base );
  ureq->_sge[0].iov_base = NULL;
  ureq->_sge[0].iov_len = 0;

  req = dbBE_Redis_request_allocate( ureq );
  rc += TestNSAttach( "TestNS", sr_buf, req );
  dbBE_Redis_request_destroy( req );



  ureq->_opcode = DBBE_OPCODE_NSDETACH;
  ureq->_sge[0].iov_base = NULL;
  ureq->_sge[0].iov_len = 0;

  req = dbBE_Redis_request_allocate( ureq );
  rc += TestNSDetach( "TestNS", sr_buf, req );
  dbBE_Redis_request_destroy( req );



  ureq->_opcode = DBBE_OPCODE_NSDELETE;
  ureq->_key = NULL;

  req = dbBE_Redis_request_allocate( ureq );
  rc += TestNSDelete( "TestNS", sr_buf, req );
  dbBE_Redis_request_destroy( req );


  ureq->_opcode = DBBE_OPCODE_PUT;
  ureq->_key = strdup("blafasel");
  ureq->_sge_count = 2;
  ureq->_sge[ 0 ].iov_base = buffer;
  ureq->_sge[ 0 ].iov_len = 12;
  ureq->_sge[ 1 ].iov_base = &buffer[16];
  ureq->_sge[ 1 ].iov_len = 13;

  snprintf( buffer, 13, "Hello World" );
  snprintf( &buffer[16], 14, " You're done." );

  req = dbBE_Redis_request_allocate( ureq );
  rc += TestPut( "TestNS", sr_buf, req );
  dbBE_Redis_request_destroy( req );

  memset( buffer, 0, 1024 );

  ureq->_opcode = DBBE_OPCODE_READ;
  req = dbBE_Redis_request_allocate( ureq );
  rc += TestRead( "TestNS", sr_buf, req );
  dbBE_Redis_request_destroy( req );

  memset( buffer, 0, 1024 );

  ureq->_opcode = DBBE_OPCODE_GET;
  req = dbBE_Redis_request_allocate( ureq );
  rc += TestRead( "TestNS", sr_buf, req );
  dbBE_Redis_request_destroy( req );

  memset( buffer, 0, 1024 );

  ureq->_opcode = DBBE_OPCODE_DIRECTORY;
  ureq->_sge_count = 1;
  ureq->_sge[ 0 ].iov_base = buffer;
  ureq->_sge[ 0 ].iov_len = 1024;

  req = dbBE_Redis_request_allocate( ureq );
  rc += TestDirectory( "TestNS", sr_buf, req );
  dbBE_Redis_request_destroy( req );

  memset( buffer, 0, 1024 );

  ureq->_opcode = DBBE_OPCODE_REMOVE;
  req = dbBE_Redis_request_allocate( ureq );
  rc += TestRemove( "TestNS", sr_buf, req );
  dbBE_Redis_request_destroy( req );


  memset( buffer, 0, 1024 );
  snprintf( buffer, 1024, "Target" );

  ureq->_opcode = DBBE_OPCODE_MOVE;
  ureq->_sge_count = 2;
  ureq->_sge[ 0 ].iov_base = buffer;
  ureq->_sge[ 0 ].iov_len = 6;
  ureq->_sge[ 1 ].iov_base = DBR_GROUP_EMPTY;
  ureq->_sge[ 1 ].iov_len = sizeof( DBR_Group_t );

  req = dbBE_Redis_request_allocate( ureq );
  rc += TestMove( "TestNS", sr_buf, req );
  dbBE_Redis_request_destroy( req );

  free( ureq->_key );
  free( ureq );

  dbBE_Transport_sr_buffer_free( sr_buf );
  dbBE_Transport_sr_buffer_free( data_buf );

  dbBE_Redis_command_stages_spec_destroy( stage_specs );

  printf( "Test exiting with rc=%d\n", rc );
  return rc;
}
