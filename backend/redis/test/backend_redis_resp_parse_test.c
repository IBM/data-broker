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

  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );
  dbBE_Redis_sr_buffer_reset( sr_buf );

  len = snprintf( dbBE_Redis_sr_buffer_get_start( sr_buf ),
                  dbBE_Redis_sr_buffer_get_size( sr_buf ),
                  ":1\r\n");
  rc += TEST_NOT( len, -1 );
  rc += TEST( dbBE_Redis_sr_buffer_add_data( sr_buf, len, 0 ), (size_t)len );

  rc += TEST( dbBE_Redis_parse_sr_buffer( sr_buf, &result ), 0 );
  rc += TEST( dbBE_Redis_process_nscreate( req, &result ), 0 );


  // transition to next stage
  rc += TEST( dbBE_Redis_request_stage_transition( req ), 0 );

  // create return data for result testing of second stage (
  // returns OK when successful (simple string)
  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );
  dbBE_Redis_sr_buffer_reset( sr_buf );

  len = snprintf( dbBE_Redis_sr_buffer_get_start( sr_buf ),
                  dbBE_Redis_sr_buffer_get_size( sr_buf ),
                      "+OK\r\n");
  rc += TEST_NOT( len, -1 );
  rc += TEST( dbBE_Redis_sr_buffer_add_data( sr_buf, len, 0 ), (size_t)len );

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

  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );
  dbBE_Redis_sr_buffer_reset( sr_buf );

  len = snprintf( dbBE_Redis_sr_buffer_get_start( sr_buf ),
                  dbBE_Redis_sr_buffer_get_size( sr_buf ),
                  ":1\r\n");
  rc += TEST_NOT( len, -1 );
  rc += TEST( dbBE_Redis_sr_buffer_add_data( sr_buf, len, 0 ), (size_t)len );

  rc += TEST( dbBE_Redis_parse_sr_buffer( sr_buf, &result ), 0 );
  rc += TEST( dbBE_Redis_process_nsattach( req, &result ), 0 );


  // transition to next stage
  rc += TEST( dbBE_Redis_request_stage_transition( req ), 0 );

  // create return data to test result of stage 2 (HINCRBY)
  // returns new (increased) value (integer)
  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );
  dbBE_Redis_sr_buffer_reset( sr_buf );

  len = snprintf( dbBE_Redis_sr_buffer_get_start( sr_buf ),
                  dbBE_Redis_sr_buffer_get_size( sr_buf ),
                  ":5\r\n");
  rc += TEST_NOT( len, -1 );
  rc += TEST( dbBE_Redis_sr_buffer_add_data( sr_buf, len, 0 ), (size_t)len );

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

  // create return data struct to test result of stage one (EXISTS)
  // returns number of found keys (integer)
  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );
  dbBE_Redis_sr_buffer_reset( sr_buf );

  len = snprintf( dbBE_Redis_sr_buffer_get_start( sr_buf ),
                  dbBE_Redis_sr_buffer_get_size( sr_buf ),
                  ":1\r\n");
  rc += TEST_NOT( len, -1 );
  rc += TEST( dbBE_Redis_sr_buffer_add_data( sr_buf, len, 0 ), (size_t)len );

  rc += TEST( dbBE_Redis_parse_sr_buffer( sr_buf, &result ), 0 );
  rc += TEST( dbBE_Redis_process_nsdetach( req, &result ), 0 );

  // transition to next stage
  rc += TEST( dbBE_Redis_request_stage_transition( req ), 0 );

  // create return data to test result of stage 2 (HINCRBY)
  // returns new (increased) value (integer)
  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );
  dbBE_Redis_sr_buffer_reset( sr_buf );

  len = snprintf( dbBE_Redis_sr_buffer_get_start( sr_buf ),
                  dbBE_Redis_sr_buffer_get_size( sr_buf ),
                  ":5\r\n");
  rc += TEST_NOT( len, -1 );
  rc += TEST( dbBE_Redis_sr_buffer_add_data( sr_buf, len, 0 ), (size_t)len );

  rc += TEST( dbBE_Redis_parse_sr_buffer( sr_buf, &result ), 0 );
  rc += TEST( dbBE_Redis_process_nsdetach( req, &result ), 0 );

  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );

  return rc;
}


int TestNSDelete( const char *namespace,
                  dbBE_Redis_sr_buffer_t *sr_buf,
                  dbBE_Redis_request_t *req )
{
  int rc = 0;
  int len;
  rc += TEST_NOT( req, NULL );

  // make a copy of the request because delete processing messes around with the redis request
  dbBE_Redis_request_t *req_io = dbBE_Redis_request_allocate( req->_user );

  rc += TEST_NOT( req_io, NULL );
  TEST_BREAK( rc, "NULL-ptr request in TestNSCreate()." );
  dbBE_Redis_result_t result;

  // create a dummy connection mgr (for scan)
  dbBE_Redis_connection_mgr_t *cmr = dbBE_Redis_connection_mgr_init();

  // create a fake valid connection to allow the delete scan to start
  dbBE_Redis_connection_t *conn = dbBE_Redis_connection_create( 1024 );
  conn->_socket = socket( AF_INET, SOCK_STREAM, 0 );
  conn->_status = DBBE_CONNECTION_STATUS_AUTHORIZED;
  dbBE_Redis_connection_mgr_add( cmr, conn );

  // create a dummy request queue (for del)
  dbBE_Redis_s2r_queue_t *post_queue = dbBE_Redis_s2r_queue_create( 12 );


  // create return data to test result of stage 1 (HINCRBY)
  // return new (decreased) value (integer)
  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );
  dbBE_Redis_sr_buffer_reset( sr_buf );

  len = snprintf( dbBE_Redis_sr_buffer_get_start( sr_buf ),
                  dbBE_Redis_sr_buffer_get_size( sr_buf ),
                  ":5\r\n");
  rc += TEST_NOT( len, -1 );
  rc += TEST( dbBE_Redis_sr_buffer_add_data( sr_buf, len, 0 ), (size_t)len );

  rc += TEST( dbBE_Redis_parse_sr_buffer( sr_buf, &result ), 0 );
  rc += TEST( dbBE_Redis_process_nsdelete( &req_io, &result, post_queue, cmr ), 0 );

  req_io = dbBE_Redis_s2r_queue_pop( post_queue );
  rc += TEST_NOT( req_io, NULL );
  TEST_BREAK( rc, "no request in scan queue");

  // no need to transition the request because the request is prepared already
  //rc += TEST( dbBE_Redis_request_stage_transition( req_io ), 0 );

  // create return data to test result of stage 2 (SCAN)
  // returns array with cursor (integer) and array of keys (bulk strings)
  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );
  dbBE_Redis_sr_buffer_reset( sr_buf );

  len = snprintf( dbBE_Redis_sr_buffer_get_start( sr_buf ),
                  dbBE_Redis_sr_buffer_get_size( sr_buf ),
                  "*2\r\n$1\r\n0\r\n*5\r\n$3\r\nbla\r\n$2\r\nhi\r\n$5\r\nfasel\r\n$4\r\nfoob\r\n$6\r\ngnartz\r\n");
  rc += TEST_NOT( len, -1 );
  rc += TEST( dbBE_Redis_sr_buffer_add_data( sr_buf, len, 0 ), (size_t)len );

  rc += TEST( dbBE_Redis_parse_sr_buffer( sr_buf, &result ), 0 );
  rc += TEST( dbBE_Redis_process_nsdelete( &req_io, &result, post_queue, cmr ), 0 );

  rc += TEST( dbBE_Redis_s2r_queue_len( post_queue ), 5 );

  // pop the first request from the queue to get a prepared delete key request
  req_io = NULL;
  while( req_io == NULL )
  {
    req_io = dbBE_Redis_s2r_queue_pop( post_queue );
    rc += TEST_NOT( req_io, NULL );
    TEST_BREAK( rc, "no request in scan queue");

    // create return data to test result of stage 3 (DEL)
    // returns the number of deleted keys
    rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );
    dbBE_Redis_sr_buffer_reset( sr_buf );

    len = snprintf( dbBE_Redis_sr_buffer_get_start( sr_buf ),
                    dbBE_Redis_sr_buffer_get_size( sr_buf ),
                    ":1\r\n");
    rc += TEST_NOT( len, -1 );
    rc += TEST( dbBE_Redis_sr_buffer_add_data( sr_buf, len, 0 ), (size_t)len );

    rc += TEST( dbBE_Redis_parse_sr_buffer( sr_buf, &result ), 0 );
    rc += TEST( dbBE_Redis_process_nsdelete( &req_io, &result, post_queue, cmr ), 0 );

    if( dbBE_Redis_s2r_queue_len( post_queue ) != 0 )
      rc += TEST( req_io, NULL );
    else
      rc += TEST_NOT( req_io, NULL );
  }
  rc += TEST( dbBE_Redis_s2r_queue_len( post_queue ), 0 );

  rc += TEST( dbBE_Redis_request_stage_transition( req_io ), 0 );

  // create return data to test result of stage 4 (DEL)
  // returns the number of deleted keys
  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );
  dbBE_Redis_sr_buffer_reset( sr_buf );

  len = snprintf( dbBE_Redis_sr_buffer_get_start( sr_buf ),
                  dbBE_Redis_sr_buffer_get_size( sr_buf ),
                  ":1\r\n");
  rc += TEST_NOT( len, -1 );
  rc += TEST( dbBE_Redis_sr_buffer_add_data( sr_buf, len, 0 ), (size_t)len );

  rc += TEST( dbBE_Redis_parse_sr_buffer( sr_buf, &result ), 0 );
  rc += TEST( dbBE_Redis_process_nsdelete( &req_io, &result, post_queue, cmr ), 0 );


  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );

  dbBE_Redis_s2r_queue_destroy( post_queue );
  dbBE_Redis_connection_mgr_exit( cmr );
  dbBE_Redis_request_destroy( req_io );

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

  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );
  dbBE_Redis_sr_buffer_reset( sr_buf );

  len = snprintf( dbBE_Redis_sr_buffer_get_start( sr_buf ),
                  dbBE_Redis_sr_buffer_get_size( sr_buf ),
                  ":1\r\n");
  rc += TEST_NOT( len, -1 );
  rc += TEST( dbBE_Redis_sr_buffer_add_data( sr_buf, len, 0 ), (size_t)len );

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

  // create return data struct to test result (LINDEX/LPOP)
  // returns number of entries in list (integer)
  dbBE_Redis_result_t result;

  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );
  dbBE_Redis_sr_buffer_reset( sr_buf );

  len = snprintf( dbBE_Redis_sr_buffer_get_start( sr_buf ),
                  dbBE_Redis_sr_buffer_get_size( sr_buf ),
                  "$12\r\nReturnString\r\n");
  rc += TEST_NOT( len, -1 );
  rc += TEST( dbBE_Redis_sr_buffer_add_data( sr_buf, len, 0 ), (size_t)len );

  rc += TEST( dbBE_Redis_parse_sr_buffer( sr_buf, &result ), 0 );
  rc += TEST( dbBE_Redis_process_get( req, &result, transport ), 0 );

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

  // make a copy of the request because delete processing messes around with the redis request
  dbBE_Redis_request_t *req_io = dbBE_Redis_request_allocate( req->_user );

  // set a transport for the return of the data
  dbBE_Data_transport_t *transport = &dbBE_Memcopy_transport;

  // create a dummy connection mgr (for scan)
  dbBE_Redis_connection_mgr_t *cmr = dbBE_Redis_connection_mgr_init();

  // create a fake valid connection to allow the delete scan to start
  dbBE_Redis_connection_t *conn = dbBE_Redis_connection_create( 1024 );
  conn->_socket = socket( AF_INET, SOCK_STREAM, 0 );
  conn->_status = DBBE_CONNECTION_STATUS_AUTHORIZED;
  dbBE_Redis_connection_mgr_add( cmr, conn );

  // create a dummy request queue (for del)
  dbBE_Redis_s2r_queue_t *post_queue = dbBE_Redis_s2r_queue_create( 12 );

  // create return data to test result of stage 1 (HGETALL)
  // return meta data of namespace (array)
  rc += TEST( dbBE_Redis_result_cleanup( &result, 0 ), 0 );
  dbBE_Redis_sr_buffer_reset( sr_buf );
  len = snprintf( dbBE_Redis_sr_buffer_get_start( sr_buf ),
                  dbBE_Redis_sr_buffer_get_size( sr_buf ),
                  "*3\r\n$6\r\nTestNS\r\n$5\r\ncount\r\n$1\r\n7\r\n");
  rc += TEST_NOT( len, -1 );
  rc += TEST( dbBE_Redis_sr_buffer_add_data( sr_buf, len, 0 ), (size_t)len );

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
  dbBE_Redis_sr_buffer_reset( sr_buf );

  len = snprintf( dbBE_Redis_sr_buffer_get_start( sr_buf ),
                  dbBE_Redis_sr_buffer_get_size( sr_buf ),
                  "*2\r\n$1\r\n0\r\n*5\r\n$11\r\nTestNS::bla\r\n$10\r\nTestNS::hi\r\n$13\r\nTestNS::fasel\r\n$12\r\nTestNS::foob\r\n$14\r\nTestNS::gnartz\r\n");
  rc += TEST_NOT( len, -1 );
  rc += TEST( dbBE_Redis_sr_buffer_add_data( sr_buf, len, 0 ), (size_t)len );

  rc += TEST( dbBE_Redis_parse_sr_buffer( sr_buf, &result ), 0 );
  rc += TEST( dbBE_Redis_process_directory( &req_io, &result, transport, post_queue, cmr ), 0 );
  rc += TEST( strncmp( (char*)req_io->_user->_sge[0]._data, "bla\nhi\nfasel\nfoob\ngnartz", 1024 ), 0 );

  return rc;
}




int main( int argc, char ** argv )
{
  int rc = 0;
  size_t keylen, vallen;
  char buffer[1024];

  dbBE_Redis_sr_buffer_t *sr_buf = dbBE_Redis_sr_buffer_allocate( DBBE_TEST_BUFFER_LEN );
  dbBE_Redis_sr_buffer_t *data_buf = dbBE_Redis_sr_buffer_allocate( DBBE_TEST_BUFFER_LEN ); // misuse, just as a buffer
  dbBE_Redis_request_t *req;
  dbBE_Request_t* ureq = (dbBE_Request_t*) malloc (sizeof(dbBE_Request_t) + 2 * sizeof(dbBE_sge_t));

  dbBE_Redis_command_stage_spec_t *stage_specs = dbBE_Redis_command_stages_spec_init();
  rc += TEST_NOT( stage_specs, NULL );

  ureq->_opcode = DBBE_OPCODE_UNSPEC;
  ureq->_key = "bla";
  ureq->_ns_name = "TestNS";


  ureq->_opcode = DBBE_OPCODE_NSCREATE;
  ureq->_sge_count = 1;
  ureq->_sge[0]._data = strdup("users, admins");
  ureq->_sge[0]._size = strlen( ureq->_sge[0]._data );

  req = dbBE_Redis_request_allocate( ureq );
  rc += TestNSCreate( "TestNS", sr_buf, req );
  dbBE_Redis_request_destroy( req );



  ureq->_opcode = DBBE_OPCODE_NSATTACH;
  free( ureq->_sge[0]._data );
  ureq->_sge[0]._data = NULL;
  ureq->_sge[0]._size = 0;

  req = dbBE_Redis_request_allocate( ureq );
  rc += TestNSAttach( "TestNS", sr_buf, req );
  dbBE_Redis_request_destroy( req );



  ureq->_opcode = DBBE_OPCODE_NSDETACH;
  ureq->_sge[0]._data = NULL;
  ureq->_sge[0]._size = 0;

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
  ureq->_sge[ 0 ]._data = buffer;
  ureq->_sge[ 0 ]._size = 12;
  ureq->_sge[ 1 ]._data = &buffer[16];
  ureq->_sge[ 1 ]._size = 13;

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
  ureq->_sge[ 0 ]._data = buffer;
  ureq->_sge[ 0 ]._size = 1024;

  req = dbBE_Redis_request_allocate( ureq );
  rc += TestDirectory( "TestNS", sr_buf, req );
  dbBE_Redis_request_destroy( req );



  printf( "Test exiting with rc=%d\n", rc );
  return rc;
}
