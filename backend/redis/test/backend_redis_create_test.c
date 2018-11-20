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
#include "../backend/redis/protocol.h"
#include "../backend/transports/memcopy.h"

#define DBBE_TEST_BUFFER_LEN ( 1024 )

int main( int argc, char ** argv )
{
  int rc = 0;
  size_t keylen, vallen;

  dbBE_Redis_sr_buffer_t *sr_buf = dbBE_Redis_sr_buffer_allocate( DBBE_TEST_BUFFER_LEN );
  dbBE_Redis_sr_buffer_t *data_buf = dbBE_Redis_sr_buffer_allocate( DBBE_TEST_BUFFER_LEN ); // misuse, just as a buffer
  dbBE_Redis_request_t *req;
  dbBE_Request_t* ureq = (dbBE_Request_t*) malloc (sizeof(dbBE_Request_t) + 2 * sizeof(dbBE_sge_t));

  dbBE_Redis_command_stage_spec_t *stage_specs = dbBE_Redis_command_stages_spec_init();
  rc += TEST_NOT( stage_specs, NULL );

  ureq->_sge_count = 2;
  ureq->_sge[ 0 ]._data = strdup("Hello World!");
  ureq->_sge[ 0 ].iov_len = 12;
  ureq->_sge[ 1 ]._data = strdup(" You're done.");
  ureq->_sge[ 1 ].iov_len = 13;
  ureq->_opcode = DBBE_OPCODE_PUT;
  ureq->_key = "bla";
  ureq->_ns_name = "TestNS";

  keylen = strlen( ureq->_key ) + strlen( ureq->_ns_name ) + 2;
  vallen = dbBE_SGE_get_len( ureq->_sge, ureq->_sge_count );
  rc += TEST( keylen, 11 );
  rc += TEST( vallen, 25 );

  req = dbBE_Redis_request_allocate( ureq );
  rc += TEST_NOT( req, NULL );

  // create a put
  rc += TEST( dbBE_Redis_create_command( req,
                                         sr_buf,
                                         &dbBE_Memcopy_transport ), 0 );
  rc += TEST( strcmp( "*3\r\n$5\r\nRPUSH\r\n$11\r\nTestNS::bla\r\n$25\r\nHello World! You're done.\r\n",
                      dbBE_Redis_sr_buffer_get_start( sr_buf ) ),
              0 );
  TEST_LOG( rc, dbBE_Redis_sr_buffer_get_start( sr_buf ) );
  dbBE_Redis_request_destroy( req );
  free( ureq->_sge[ 0 ]._data );
  free( ureq->_sge[ 1 ]._data );

  // create a get
  ureq->_opcode = DBBE_OPCODE_GET;
  ureq->_sge_count = 1;

  req = dbBE_Redis_request_allocate( ureq );
  rc += TEST_NOT( req, NULL );

  dbBE_Redis_sr_buffer_reset( sr_buf );
  rc += TEST( dbBE_Redis_create_command( req,
                                         sr_buf,
                                         &dbBE_Memcopy_transport ), 0 );
  rc += TEST( strcmp( "*2\r\n$4\r\nLPOP\r\n$11\r\nTestNS::bla\r\n",
                      dbBE_Redis_sr_buffer_get_start( sr_buf ) ),
              0 );

  TEST_LOG( rc, dbBE_Redis_sr_buffer_get_start( sr_buf ) );
  dbBE_Redis_request_destroy( req );

  // create a read
  ureq->_opcode = DBBE_OPCODE_READ;

  req = dbBE_Redis_request_allocate( ureq );
  rc += TEST_NOT( req, NULL );

  dbBE_Redis_sr_buffer_reset( sr_buf );
  rc += TEST( dbBE_Redis_create_command( req,
                                         sr_buf,
                                         &dbBE_Memcopy_transport ), 0 );
  rc += TEST( strcmp( "*3\r\n$6\r\nLINDEX\r\n$11\r\nTestNS::bla\r\n$1\r\n0\r\n",
                      dbBE_Redis_sr_buffer_get_start( sr_buf ) ),
              0 );

  TEST_LOG( rc, dbBE_Redis_sr_buffer_get_start( sr_buf ) );
  dbBE_Redis_request_destroy( req );

  // create a directory (meta stage)
  ureq->_opcode = DBBE_OPCODE_DIRECTORY;
  ureq->_sge_count = 1;
  ureq->_match = strdup( "*" );
  ureq->_key = NULL;

  req = dbBE_Redis_request_allocate( ureq );
  rc += TEST_NOT( req, NULL );

  dbBE_Redis_sr_buffer_reset( sr_buf );
  rc += TEST( dbBE_Redis_create_command( req,
                                         sr_buf,
                                         &dbBE_Memcopy_transport ), 0 );
  rc += TEST( strcmp( "*2\r\n$7\r\nHGETALL\r\n$6\r\nTestNS\r\n",
                      dbBE_Redis_sr_buffer_get_start( sr_buf ) ),
              0 );
  TEST_LOG( rc, dbBE_Redis_sr_buffer_get_start( sr_buf ) );

  // transition to scan stage
  rc += TEST( dbBE_Redis_request_stage_transition( req ), 0 );
  dbBE_Redis_sr_buffer_reset( sr_buf );

  rc += TEST( dbBE_Redis_create_command( req,
                                         sr_buf,
                                         &dbBE_Memcopy_transport ), 0 );
  rc += TEST( strcmp( "*6\r\n$4\r\nSCAN\r\n$1\r\n0\r\n$5\r\nMATCH\r\n$9\r\nTestNS::*\r\n$5\r\nCOUNT\r\n$4\r\n1000\r\n",
                      dbBE_Redis_sr_buffer_get_start( sr_buf ) ),
              0 );
  TEST_LOG( rc, dbBE_Redis_sr_buffer_get_start( sr_buf ) );

  free( ureq->_match ); ureq->_match = NULL;
  ureq->_key = "bla";
  dbBE_Redis_request_destroy( req );

  // create an nscreate
  ureq->_opcode = DBBE_OPCODE_NSCREATE;
  ureq->_sge_count = 1;
  ureq->_sge[0]._data = strdup("users, admins");
  ureq->_sge[0].iov_len = strlen( ureq->_sge[0]._data );

  req = dbBE_Redis_request_allocate( ureq );
  rc += TEST_NOT( req, NULL );

  dbBE_Redis_sr_buffer_reset( sr_buf );
  rc += TEST( dbBE_Redis_create_command( req,
                                         sr_buf,
                                         &dbBE_Memcopy_transport ), 0 );
  rc += TEST( strcmp( "*4\r\n$6\r\nHSETNX\r\n$6\r\nTestNS\r\n$2\r\nid\r\n$6\r\nTestNS\r\n",
                      dbBE_Redis_sr_buffer_get_start( sr_buf ) ),
              0 );

  TEST_LOG( rc, dbBE_Redis_sr_buffer_get_start( sr_buf ) );
  rc += TEST( dbBE_Redis_request_stage_transition( req ), 0 );
  dbBE_Redis_sr_buffer_reset( sr_buf );
  TEST_LOG( rc, "Create NSCREATE-HSETNX" );


  rc += TEST( dbBE_Redis_create_command( req,
                                         sr_buf,
                                         &dbBE_Memcopy_transport ), 0 );
  rc += TEST( strcmp( "*6\r\n$5\r\nHMSET\r\n$6\r\nTestNS\r\n$6\r\nrefcnt\r\n$1\r\n1\r\n$6\r\ngroups\r\n$13\r\nusers, admins\r\n",
                      dbBE_Redis_sr_buffer_get_start( sr_buf ) ),
              0 );

  TEST_LOG( rc, dbBE_Redis_sr_buffer_get_start( sr_buf ) );
  dbBE_Redis_request_destroy( req );
  free( ureq->_sge[ 0 ]._data );


  // create an nsquery
  ureq->_opcode = DBBE_OPCODE_NSQUERY;
  ureq->_sge[0]._data = dbBE_Redis_sr_buffer_get_start( data_buf );
  ureq->_sge[0].iov_len = dbBE_Redis_sr_buffer_get_size( data_buf );

  req = dbBE_Redis_request_allocate( ureq );
  rc += TEST_NOT( req, NULL );

  dbBE_Redis_sr_buffer_reset( sr_buf );
  rc += TEST( dbBE_Redis_create_command( req,
                                         sr_buf,
                                         &dbBE_Memcopy_transport ), 0 );
  rc += TEST( strcmp( "*2\r\n$7\r\nHGETALL\r\n$6\r\nTestNS\r\n",
                      dbBE_Redis_sr_buffer_get_start( sr_buf ) ),
              0 );
  TEST_LOG( rc, dbBE_Redis_sr_buffer_get_start( sr_buf ) );
  dbBE_Redis_request_destroy( req );


  // create an nsattach
  ureq->_opcode = DBBE_OPCODE_NSATTACH;
  ureq->_sge[0]._data = NULL;
  ureq->_sge[0].iov_len = 0;

  req = dbBE_Redis_request_allocate( ureq );
  rc += TEST_NOT( req, NULL );

  dbBE_Redis_sr_buffer_reset( sr_buf );
  rc += TEST( dbBE_Redis_create_command( req,
                                         sr_buf,
                                         &dbBE_Memcopy_transport ), 0 );
  rc += TEST( strcmp( "*2\r\n$6\r\nEXISTS\r\n$6\r\nTestNS\r\n",
                      dbBE_Redis_sr_buffer_get_start( sr_buf ) ),
              0 );

  TEST_LOG( rc, dbBE_Redis_sr_buffer_get_start( sr_buf ) );
  rc += TEST( dbBE_Redis_request_stage_transition( req ), 0 );
  dbBE_Redis_sr_buffer_reset( sr_buf );
  rc += TEST( dbBE_Redis_create_command( req,
                                         sr_buf,
                                         &dbBE_Memcopy_transport ), 0 );
  rc += TEST( strcmp( "*4\r\n$7\r\nHINCRBY\r\n$6\r\nTestNS\r\n$6\r\nrefcnt\r\n$1\r\n1\r\n",
                      dbBE_Redis_sr_buffer_get_start( sr_buf ) ),
              0 );
  rc += TEST( strlen( "*4\r\n$7\r\nHINCRBY\r\n$6\r\nTestNS\r\n$6\r\nrefcnt\r\n$1\r\n1\r\n" ), dbBE_Redis_sr_buffer_available( sr_buf ) );
  TEST_LOG( rc, dbBE_Redis_sr_buffer_get_start( sr_buf ) );
  dbBE_Redis_request_destroy( req );


  // create an nsdetach
  ureq->_opcode = DBBE_OPCODE_NSDETACH;
  ureq->_sge[0]._data = NULL;
  ureq->_sge[0].iov_len = 0;

  req = dbBE_Redis_request_allocate( ureq );
  rc += TEST_NOT( req, NULL );

  dbBE_Redis_sr_buffer_reset( sr_buf );
  rc += TEST( dbBE_Redis_create_command( req,
                                         sr_buf,
                                         &dbBE_Memcopy_transport ), 0 );
  rc += TEST( strcmp( "*2\r\n$6\r\nEXISTS\r\n$6\r\nTestNS\r\n",
                      dbBE_Redis_sr_buffer_get_start( sr_buf ) ),
              0 );

  TEST_LOG( rc, dbBE_Redis_sr_buffer_get_start( sr_buf ) );
  rc += TEST( dbBE_Redis_request_stage_transition( req ), 0 );
  dbBE_Redis_sr_buffer_reset( sr_buf );
  rc += TEST( dbBE_Redis_create_command( req,
                                         sr_buf,
                                         &dbBE_Memcopy_transport ), 0 );
  rc += TEST( strcmp( "*4\r\n$7\r\nHINCRBY\r\n$6\r\nTestNS\r\n$6\r\nrefcnt\r\n$2\r\n-1\r\n",
                      dbBE_Redis_sr_buffer_get_start( sr_buf ) ),
              0 );
  TEST_LOG( rc, dbBE_Redis_sr_buffer_get_start( sr_buf ) );
  dbBE_Redis_request_destroy( req );


  // create an nsdelete
  ureq->_opcode = DBBE_OPCODE_NSDELETE;
  ureq->_key = NULL;

  req = dbBE_Redis_request_allocate( ureq );
  req->_status.nsdelete.scankey = strdup( "0" );
  rc += TEST_NOT( req, NULL );

  rc += TEST( req->_step->_stage, DBBE_REDIS_NSDELETE_STAGE_DETACH );
  dbBE_Redis_sr_buffer_reset( sr_buf );
  rc += TEST( dbBE_Redis_create_command( req,
                                         sr_buf,
                                         &dbBE_Memcopy_transport ), 0 );
  rc += TEST( strcmp( "*4\r\n$7\r\nHINCRBY\r\n$6\r\nTestNS\r\n$6\r\nrefcnt\r\n$2\r\n-1\r\n",
                      dbBE_Redis_sr_buffer_get_start( sr_buf ) ),
              0 );

  TEST_LOG( rc, dbBE_Redis_sr_buffer_get_start( sr_buf ) );

  rc += TEST( dbBE_Redis_request_stage_transition( req ), 0 );
  rc += TEST( req->_step->_stage, DBBE_REDIS_NSDELETE_STAGE_SCAN );
  dbBE_Redis_sr_buffer_reset( sr_buf );
  rc += TEST( dbBE_Redis_create_command( req,
                                         sr_buf,
                                         &dbBE_Memcopy_transport ), 0 );
  rc += TEST( strcmp( "*6\r\n$4\r\nSCAN\r\n$1\r\n0\r\n$5\r\nMATCH\r\n$9\r\nTestNS::*\r\n$5\r\nCOUNT\r\n$4\r\n1000\r\n",
                      dbBE_Redis_sr_buffer_get_start( sr_buf ) ),
              0 );

  TEST_LOG( rc, dbBE_Redis_sr_buffer_get_start( sr_buf ) );

  if( req->_status.nsdelete.scankey != NULL )
    free( req->_status.nsdelete.scankey );
  req->_status.nsdelete.scankey = strdup( "bla" );

  rc += TEST( dbBE_Redis_request_stage_transition( req ), 0 );
  rc += TEST( req->_step->_stage, DBBE_REDIS_NSDELETE_STAGE_DELKEYS );
  dbBE_Redis_sr_buffer_reset( sr_buf );
  rc += TEST( dbBE_Redis_create_command( req,
                                         sr_buf,
                                         &dbBE_Memcopy_transport ), 0 );
  rc += TEST( strcmp( "*2\r\n$3\r\nDEL\r\n$3\r\nbla\r\n", // delkeys uses the key only (the prefix is already in the key, internally)
                      dbBE_Redis_sr_buffer_get_start( sr_buf ) ),
              0 );
  TEST_LOG( rc, dbBE_Redis_sr_buffer_get_start( sr_buf ) );

  if( req->_status.nsdelete.scankey != NULL )
    free( req->_status.nsdelete.scankey );
  req->_status.nsdelete.scankey = NULL;

  rc += TEST( dbBE_Redis_request_stage_transition( req ), 0 );
  rc += TEST( req->_step->_stage, DBBE_REDIS_NSDELETE_STAGE_DELNS );
  dbBE_Redis_sr_buffer_reset( sr_buf );
  rc += TEST( dbBE_Redis_create_command( req,
                                         sr_buf,
                                         &dbBE_Memcopy_transport ), 0 );
  rc += TEST( strcmp( "*2\r\n$3\r\nDEL\r\n$6\r\nTestNS\r\n",
                      dbBE_Redis_sr_buffer_get_start( sr_buf ) ),
              0 );
  TEST_LOG( rc, dbBE_Redis_sr_buffer_get_start( sr_buf ) );
  dbBE_Redis_request_destroy( req );



  dbBE_Redis_command_stages_spec_destroy( stage_specs );
  dbBE_Redis_sr_buffer_free( sr_buf );
  dbBE_Redis_sr_buffer_free( data_buf );

  free(ureq);

  printf( "Test exiting with rc=%d\n", rc );
  return rc;
}
