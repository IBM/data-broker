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

#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>
#endif

#include <stdio.h>
#include <string.h>

#include <libdatabroker.h>
#include "../backend/redis/result.h"
#include "../backend/redis/parse.h"
#include "../backend/redis/result.h"
#include "../backend/transports/double_buffer.h"
#include "test_utils.h"

#define DBBE_TEST_BUFFER_LEN ( 1024 )
#define DBBE_TEST_PRECV_LEN ( DBBE_TEST_BUFFER_LEN )
#define TEST_SGE_MAX (32)

int TestReset_buffer( char *buf, const char *content )
{
  memset( buf, 0, DBBE_TEST_BUFFER_LEN );
  snprintf( buf, DBBE_TEST_BUFFER_LEN, "%s", content );
  return strnlen( buf, DBBE_TEST_BUFFER_LEN );
}

int TestReset_sr_buffer( dbBE_Redis_sr_buffer_t *sr_buf, const char *content )
{
  dbBE_Transport_sr_buffer_reset( sr_buf );
  memset( dbBE_Transport_sr_buffer_get_processed_position( sr_buf ), 0, dbBE_Transport_sr_buffer_get_size( sr_buf ) );
  snprintf( dbBE_Transport_sr_buffer_get_processed_position( sr_buf ),
            dbBE_Transport_sr_buffer_get_size( sr_buf ),
	    "%s",
            content );
  dbBE_Transport_sr_buffer_set_fill( sr_buf,
                                 strnlen( dbBE_Transport_sr_buffer_get_processed_position( sr_buf ),
                                          dbBE_Transport_sr_buffer_get_size( sr_buf )
                                 )
  );
  return dbBE_Transport_sr_buffer_available( sr_buf );
}

int TestRedis_get_strlen()
{
  int rc = 0;
  size_t len;
  size_t parsed;

  char *buffer = (char*)malloc( DBBE_TEST_BUFFER_LEN );
  if( buffer == NULL )
    return 1;

  len = TestReset_buffer( buffer, "abcdefghijklmnopqrstuvwxyz\r\n" );
  parsed = 0;
  rc += TEST( dbBE_Redis_nul_terminate_string( buffer, &parsed, len ), (int64_t)len-2 );
  rc += TEST( parsed, len );

  // unterminated string should come back with: EAGAIN
  len = TestReset_buffer( buffer, "abcdefghijklmnopqrstuvwxyz" );
  parsed = 0;
  rc += TEST( dbBE_Redis_nul_terminate_string( buffer, &parsed, len ), -EAGAIN );
  rc += TEST( parsed, 0 );

  len = TestReset_buffer( buffer, "abcdefghijklmnopqrstuvwxyz\r\nab" );
  parsed = 0;
  rc += TEST( dbBE_Redis_nul_terminate_string( buffer, &parsed, len ), (int64_t)len-4 );
  rc += TEST( parsed, len-2 ); // extra chars after terminator won't be parsed

  rc += TEST( dbBE_Redis_nul_terminate_string( buffer, NULL, len ), 0 );
  free( buffer );

  buffer = NULL;
  rc += TEST( dbBE_Redis_nul_terminate_string( NULL, NULL, 0 ), 0 );
  rc += TEST( dbBE_Redis_nul_terminate_string( NULL, &parsed, 0 ), 0);

  printf( "TestRedis_get_strlen exiting with rc=%d\n", rc );
  return rc;
}

int TestRedis_extract_int()
{
  int rc = 0;
  size_t len;
  size_t parsed;

  char *buffer = (char*)malloc( DBBE_TEST_BUFFER_LEN );
  if( buffer == NULL )
    return 1;

  // test a small positive number
  len = TestReset_buffer( buffer, "135\r\n" );
  parsed = 0;
  rc += TEST( dbBE_Redis_extract_integer( buffer, &parsed, len ), 135 );
  rc += TEST( parsed, len );

  // test a long positive number prefixed with +
  len = TestReset_buffer( buffer, "+645161233522\r\n" );
  parsed = 0;
  int64_t result = dbBE_Redis_extract_integer( buffer, &parsed, len );
  rc += TEST( result, 645161233522 );
  rc += TEST( parsed, len );

  // test a negative number
  len = TestReset_buffer( buffer, "-9267203\r\n" );
  parsed = 0;
  rc += TEST( dbBE_Redis_extract_integer( buffer, &parsed, len ), -9267203 );
  rc += TEST( parsed, len );

  // test error cases
  len = TestReset_buffer( buffer, "-92faih\r\n" );
  parsed = 0;
  rc += TEST( dbBE_Redis_extract_integer( buffer, &parsed, len ), DBBE_REDIS_NAN );
  rc += TEST( parsed, len );

  len = TestReset_buffer( buffer, "+ua21054\r\n" );
  parsed = 0;
  rc += TEST( dbBE_Redis_extract_integer( buffer, &parsed, len ), DBBE_REDIS_NAN );
  rc += TEST( parsed, len );

  len = TestReset_buffer( buffer, "92\r\nABCD" );
  parsed = 0;
  rc += TEST( dbBE_Redis_extract_integer( buffer, &parsed, len ), 92 );
  rc += TEST( parsed, len-4 ); // extra chars after terminator won't be parsed

  rc += TEST( dbBE_Redis_extract_integer( buffer, NULL, len ), DBBE_REDIS_NAN );
  free( buffer );
  buffer = NULL;
  rc += TEST( dbBE_Redis_extract_integer( NULL, NULL, len ), DBBE_REDIS_NAN );
  rc += TEST( dbBE_Redis_extract_integer( NULL, &parsed, len ), DBBE_REDIS_NAN );

  printf( "TestRedis_extract_int exiting with rc=%d\n", rc );
  return rc;
}

int TestRedis_extract_bulk_string()
{
  int rc = 0;
  size_t len;
  size_t parsed;
  size_t total = 0;

  char *buffer = (char*)malloc( DBBE_TEST_BUFFER_LEN );
  if( buffer == NULL )
    return 1;

  char *string = buffer;

  // test a regular correct string
  len = TestReset_buffer( buffer, "5\r\nabcde\r\n" );
  string = buffer;
  parsed = 0;
  rc += TEST( dbBE_Redis_extract_bulk_string( &string, &parsed, len, NULL ), 5 );
  string[ 5 ] = '\0';
  rc += TEST( strncmp( string, "abcde", DBBE_TEST_BUFFER_LEN) , 0 );
  rc += TEST( parsed, len );

  // test fault tolerance of string-length value
  len = TestReset_buffer( buffer, "4\r\nabcde\r\n" );
  string = buffer;
  parsed = 0;
  rc += TEST( dbBE_Redis_extract_bulk_string( &string, &parsed, len, NULL ), 5 );
  string[ 5 ] = '\0';
  rc += TEST( strncmp( string, "abcde", DBBE_TEST_BUFFER_LEN) , 0 );
  rc += TEST( parsed, len );

  // test null ptr case
  len = TestReset_buffer( buffer, "-1\r\n" );
  string = buffer;
  parsed = 0;
  rc += TEST( dbBE_Redis_extract_bulk_string( &string, &parsed, len, NULL ), 0 );
  rc += TEST( string, NULL );
  rc += TEST( parsed, len );

  // test a negative length string
  len = TestReset_buffer( buffer, "-5\r\nabcde\r\n" );
  string = buffer;
  parsed = 0;
  rc += TEST( dbBE_Redis_extract_bulk_string( &string, &parsed, len, NULL ), -EPROTO );
  rc += TEST( string, buffer );
  rc += TEST( parsed, len );

  // test an unterminated string
  len = TestReset_buffer( buffer, "5\r\nabcdefgh" );
  string = buffer;
  parsed = 0;
  rc += TEST( dbBE_Redis_extract_bulk_string( &string, &parsed, len, NULL ), -EPROTO );
  rc += TEST( strncmp( string, "abcdefgh", DBBE_TEST_BUFFER_LEN) , 0 );
  rc += TEST( parsed, len );

  // test a malformatted length/string
  len = TestReset_buffer( buffer, "abcdefgh\r\n" );
  string = buffer;
  parsed = 0;
  rc += TEST( dbBE_Redis_extract_bulk_string( &string, &parsed, len, NULL ), -EPROTO );
  rc += TEST( string, buffer );
  rc += TEST( parsed, len );


  // test incomplete length strings
  len = TestReset_buffer( buffer, "10" );
  string = buffer;
  parsed = 0;
  rc += TEST( dbBE_Redis_extract_bulk_string( &string, &parsed, len, NULL ), -EAGAIN );
  rc += TEST( string, buffer );
  rc += TEST( parsed, 0 );

  len = TestReset_buffer( buffer, "10\r" );
  string = buffer;
  parsed = 0;
  rc += TEST( dbBE_Redis_extract_bulk_string( &string, &parsed, len, NULL ), -EAGAIN );
  rc += TEST( string, buffer );
  rc += TEST( parsed, 0 );

  // partial retrieve without expected total length
  len = TestReset_buffer( buffer, "10\r\n" );
  string = buffer;
  parsed = 0;
  rc += TEST( dbBE_Redis_extract_bulk_string( &string, &parsed, len, NULL ), -EAGAIN );
  rc += TEST( string, buffer );
  rc += TEST( parsed, 0 );

  len = TestReset_buffer( buffer, "10\r\nHello" );
  string = buffer;
  parsed = 0;
  rc += TEST( dbBE_Redis_extract_bulk_string( &string, &parsed, len, NULL ), -EAGAIN );
  rc += TEST( string, buffer );
  rc += TEST( parsed, 0 );

  len = TestReset_buffer( buffer, "10\r\nHelloWorld" );
  string = buffer;
  parsed = 0;
  rc += TEST( dbBE_Redis_extract_bulk_string( &string, &parsed, len, NULL ), -EAGAIN );
  rc += TEST( string, buffer );
  rc += TEST( parsed, 0 );

  len = TestReset_buffer( buffer, "10\r\nHelloWorld\r" );
  string = buffer;
  parsed = 0;
  rc += TEST( dbBE_Redis_extract_bulk_string( &string, &parsed, len, NULL ), -EAGAIN );
  rc += TEST( string, buffer );
  rc += TEST( parsed, 0 );

  len = TestReset_buffer( buffer, "10\r\nHelloWorldX\r\n" );
  string = buffer;
  parsed = 0;
  rc += TEST( dbBE_Redis_extract_bulk_string( &string, &parsed, len, NULL ), 11 );
  string[ 11 ] = '\0';
  rc += TEST( strncmp( string, "HelloWorldX", DBBE_TEST_BUFFER_LEN) , 0 );
  rc += TEST( parsed, len );


  // partial retrieve with expected total length
  len = TestReset_buffer( buffer, "10\r\n" );
  string = buffer;
  parsed = 0;
  total = 0;
  rc += TEST( dbBE_Redis_extract_bulk_string( &string, &parsed, len, &total ), -EOVERFLOW );
  rc += TEST( string, buffer+parsed );
  rc += TEST( parsed, 4 );
  rc += TEST( total, 10 );

  len = TestReset_buffer( buffer, "10\r\nHello" );
  string = buffer;
  parsed = 0;
  total = 0;
  rc += TEST( dbBE_Redis_extract_bulk_string( &string, &parsed, len, &total ), -EOVERFLOW );
  rc += TEST( string, buffer+4 );
  rc += TEST( parsed, 4 );
  rc += TEST( total, 10 );

  len = TestReset_buffer( buffer, "10\r\nHelloWorld" );
  string = buffer;
  parsed = 0;
  total = 0;
  rc += TEST( dbBE_Redis_extract_bulk_string( &string, &parsed, len, &total ), -EAGAIN );
  rc += TEST( string, buffer );
  rc += TEST( parsed, 0 );
  rc += TEST( total, 0 );

  len = TestReset_buffer( buffer, "10\r\nHelloWorld\r" );
  string = buffer;
  parsed = 0;
  total = 0;
  rc += TEST( dbBE_Redis_extract_bulk_string( &string, &parsed, len, &total ), -EAGAIN );
  rc += TEST( string, buffer );
  rc += TEST( parsed, 0 );
  rc += TEST( total, 0 );

  len = TestReset_buffer( buffer, "10\r\nHelloWorldX\r\n" );
  string = buffer;
  parsed = 0;
  total = 0;
  rc += TEST( dbBE_Redis_extract_bulk_string( &string, &parsed, len, &total ), 11 );
  string[ 11 ] = '\0';
  rc += TEST( strncmp( string, "HelloWorldX", DBBE_TEST_BUFFER_LEN) , 0 );
  rc += TEST( parsed, len );
  rc += TEST( total, 0 );

  // check if subsequent protocol items are ignored
  len = TestReset_buffer( buffer, "10\r\nHelloWorld\r\n:2\r\n" );
  string = buffer;
  parsed = 0;
  total = 0;
  rc += TEST( dbBE_Redis_extract_bulk_string( &string, &parsed, len, &total ), 10 );
  string[ 10 ] = '\0';
  rc += TEST( strncmp( string, "HelloWorld", DBBE_TEST_BUFFER_LEN) , 0 );
  rc += TEST( string, buffer+4 );
  rc += TEST( parsed, 16 );
  rc += TEST( total, 0 );



  rc += TEST( dbBE_Redis_extract_bulk_string( &buffer, NULL, len, NULL ), -EINVAL );
  free( buffer );
  buffer = NULL;
  rc += TEST( dbBE_Redis_extract_bulk_string( NULL, NULL, len, NULL ), -EINVAL );
  rc += TEST( dbBE_Redis_extract_bulk_string( NULL, &parsed, len, NULL ), -EINVAL );

  printf( "TestRedis_extract_bulk_string exiting with rc=%d\n", rc );
  return rc;
}


int TestRedis_parse_ctx_buffer()
{
  int rc = 0;
  size_t len;
  int err_code;

  dbBE_Redis_sr_buffer_t *sr_buf;
  dbBE_Redis_result_t result;

  sr_buf = dbBE_Transport_sr_buffer_allocate( DBBE_TEST_BUFFER_LEN );
  if( sr_buf == NULL )
    return 1;

  // parse an int
  len = TestReset_sr_buffer( sr_buf, ":1054\r\n" );
  err_code = dbBE_Redis_parse_sr_buffer( sr_buf, &result );
  rc += TEST( err_code, 0 );
  rc += TEST( result._type, dbBE_REDIS_TYPE_INT );
  rc += TEST( result._data._integer, 1054ll );
  rc += TEST( dbBE_Transport_sr_buffer_available( sr_buf ), len );
  rc += TEST( dbBE_Transport_sr_buffer_processed( sr_buf ), len );

  // continue parsing should return invalid because there's no more data to parse in the initialized string
  err_code = dbBE_Redis_parse_sr_buffer( sr_buf, &result );
  rc += TEST( err_code, -ENODATA );
  rc += TEST( result._type, dbBE_REDIS_TYPE_INVALID );
  rc += TEST( result._data._integer, -ENODATA );

  // parse an erroneous int, followed by a valid int
  len = TestReset_sr_buffer( sr_buf, ":10uo54\r\n:1054\r\n" );
  err_code = dbBE_Redis_parse_sr_buffer( sr_buf, &result );
  rc += TEST( err_code, 0 );
  rc += TEST( result._type, dbBE_REDIS_TYPE_INT );
  rc += TEST( result._data._integer, DBBE_REDIS_NAN );
  rc += TEST( dbBE_Transport_sr_buffer_available( sr_buf ), len );
  rc += TEST( dbBE_Transport_sr_buffer_processed( sr_buf ), 9 );

  // now keep parsing and find the valid int
  err_code = dbBE_Redis_parse_sr_buffer( sr_buf, &result );
  rc += TEST( err_code, 0 );
  rc += TEST( result._type, dbBE_REDIS_TYPE_INT );
  rc += TEST( result._data._integer, 1054 );
  rc += TEST( dbBE_Transport_sr_buffer_available( sr_buf ), len );
  rc += TEST( dbBE_Transport_sr_buffer_processed( sr_buf ), len );


  // parse a string
  len = TestReset_sr_buffer( sr_buf, "$10\r\nHelloWorld\r\n" );
  err_code = dbBE_Redis_parse_sr_buffer( sr_buf, &result );
  rc += TEST( err_code, 0 );
  rc += TEST( result._type, dbBE_REDIS_TYPE_CHAR );
  rc += TEST( result._data._string._size, 10 );
  rc += TEST( strncmp( result._data._string._data, "HelloWorld", 11), 0 );
  rc += TEST( dbBE_Transport_sr_buffer_available( sr_buf ), len );
  rc += TEST( dbBE_Transport_sr_buffer_processed( sr_buf ), len );

  // continue parsing should return invalid because there's no more data to parse in the initialized string
  err_code = dbBE_Redis_parse_sr_buffer( sr_buf, &result );
  rc += TEST( err_code, -ENODATA );
  rc += TEST( result._type, dbBE_REDIS_TYPE_INVALID );
  rc += TEST( result._data._integer, -ENODATA );

  // parse a string followed by an int
  len = TestReset_sr_buffer( sr_buf, "$12\r\nHello World!\r\n:-10354\r\n" );
  err_code = dbBE_Redis_parse_sr_buffer( sr_buf, &result );
  rc += TEST( err_code, 0 );
  rc += TEST( result._type, dbBE_REDIS_TYPE_CHAR );
  rc += TEST( strncmp( result._data._string._data, "Hello World!", 12), 0 );
  rc += TEST( result._data._string._size, 12 );
  rc += TEST( dbBE_Transport_sr_buffer_available( sr_buf ), len );
  rc += TEST( dbBE_Transport_sr_buffer_processed( sr_buf ), 19 );

  err_code = dbBE_Redis_parse_sr_buffer( sr_buf, &result );
  rc += TEST( err_code, 0 );
  rc += TEST( result._type, dbBE_REDIS_TYPE_INT );
  rc += TEST( result._data._integer, -10354 );
  rc += TEST( dbBE_Transport_sr_buffer_available( sr_buf ), len );
  rc += TEST( dbBE_Transport_sr_buffer_processed( sr_buf ), len );


  // continue parsing should return invalid because there's no more data to parse in the initialized string
  err_code = dbBE_Redis_parse_sr_buffer( sr_buf, &result );
  rc += TEST( err_code, -ENODATA );
  rc += TEST( result._type, dbBE_REDIS_TYPE_INVALID );
  rc += TEST( result._data._integer, -ENODATA );


  // parse an array:
  len = TestReset_sr_buffer( sr_buf, "*3\r\n$6\r\nHello \r\n$6\r\nWorld!\r\n:-10354\r\n" );
  err_code = dbBE_Redis_parse_sr_buffer( sr_buf, &result );
  rc += TEST( err_code, 0 );
  rc += TEST( result._type, dbBE_REDIS_TYPE_ARRAY );
  rc += TEST( result._data._array._len, 3 );
  rc += TEST( dbBE_Transport_sr_buffer_available( sr_buf ), len );
  rc += TEST( dbBE_Transport_sr_buffer_processed( sr_buf ), len );

  // parse the first entry
  rc += TEST( result._data._array._data[ 0 ]._type, dbBE_REDIS_TYPE_CHAR );
  rc += TEST( strncmp( result._data._array._data[ 0 ]._data._string._data, "Hello ", 7), 0 );
  rc += TEST( result._data._array._data[ 0 ]._data._string._size, 6 );

  // parse the second entry
  rc += TEST( result._data._array._data[ 1 ]._type, dbBE_REDIS_TYPE_CHAR );
  rc += TEST( strncmp( result._data._array._data[ 1 ]._data._string._data, "World!", 7), 0 );
  rc += TEST( result._data._array._data[ 1 ]._data._string._size, 6 );

  // parse the third entry
  rc += TEST( result._data._array._data[ 2 ]._type, dbBE_REDIS_TYPE_INT );
  rc += TEST( result._data._array._data[ 2 ]._data._integer, -10354 );
  dbBE_Redis_result_cleanup( &result, 0 );

  // parse an error response
  len = TestReset_sr_buffer( sr_buf, "-There was an Error in Redis\r\n" );
  err_code = dbBE_Redis_parse_sr_buffer( sr_buf, &result );
  rc += TEST( err_code, 0 );
  rc += TEST( result._type, dbBE_REDIS_TYPE_ERROR );
  rc += TEST( strncmp( result._data._string._data, "There was an Error in Redis", len), 0 );
  rc += TEST( result._data._string._size, (int64_t)len-3 );  // -1 for the '-' and -2 for the termination \r\n
  rc += TEST( dbBE_Transport_sr_buffer_available( sr_buf ), len );
  rc += TEST( dbBE_Transport_sr_buffer_processed( sr_buf ), len );


  // parse a MOVED response
  len = TestReset_sr_buffer( sr_buf, "-MOVED 1234 127.0.0.1:6381\r\n" );
  err_code = dbBE_Redis_parse_sr_buffer( sr_buf, &result );
  rc += TEST( err_code, 0 );
  rc += TEST( result._type, dbBE_REDIS_TYPE_RELOCATE );
  rc += TEST( result._data._location._hash, 1234 );
  rc += TEST( strncmp( result._data._location._address, "127.0.0.1:6381", 15), 0 );
  rc += TEST( dbBE_Transport_sr_buffer_available( sr_buf ), len );
  rc += TEST( dbBE_Transport_sr_buffer_processed( sr_buf ), len );

  // parse a ASK response
  len = TestReset_sr_buffer( sr_buf, "-ASK 1234 127.0.0.1:6381\r\n" );
  err_code = dbBE_Redis_parse_sr_buffer( sr_buf, &result );
  rc += TEST( err_code, 0 );
  rc += TEST( result._type, dbBE_REDIS_TYPE_REDIRECT );
  rc += TEST( result._data._location._hash, 1234 );
  rc += TEST( strncmp( result._data._location._address, "127.0.0.1:6381", 15), 0 );
  rc += TEST( dbBE_Transport_sr_buffer_available( sr_buf ), len );
  rc += TEST( dbBE_Transport_sr_buffer_processed( sr_buf ), len );

  // continue parsing should return invalid because there's no more data to parse in the initialized string
  err_code = dbBE_Redis_parse_sr_buffer( sr_buf, &result );
  rc += TEST( err_code, -ENODATA );
  rc += TEST( result._type, dbBE_REDIS_TYPE_INVALID );
  rc += TEST( result._data._integer, -ENODATA );

  dbBE_Transport_sr_buffer_free( sr_buf );
  sr_buf = NULL;
  rc += TEST( dbBE_Redis_parse_sr_buffer( NULL, NULL ), -EINVAL );
  rc += TEST( dbBE_Redis_parse_sr_buffer( NULL, &result ), -EINVAL );
  rc += TEST( result._type, dbBE_REDIS_TYPE_INVALID );
  printf( "TestRedis_parse_ctx_buffer exiting with rc=%d\n", rc );
  return rc;
}

int TestRedis_parse_ctx_buffer_errors()
{
  int rc = 0;
  size_t len;
  int err_code;

  dbBE_Redis_sr_buffer_t *sr_buf;
  dbBE_Redis_result_t result;

  sr_buf = dbBE_Transport_sr_buffer_allocate( DBBE_TEST_BUFFER_LEN );
  if( sr_buf == NULL )
    return 1;

  // parse an unterminated int
  len = TestReset_sr_buffer( sr_buf, ":1054" );
  err_code = dbBE_Redis_parse_sr_buffer( sr_buf, &result );
  rc += TEST( err_code, -EAGAIN );
  rc += TEST( dbBE_Transport_sr_buffer_get_start( sr_buf ), dbBE_Transport_sr_buffer_get_processed_position( sr_buf ) );
  rc += TEST( dbBE_Transport_sr_buffer_available( sr_buf ), len );
  rc += TEST( dbBE_Transport_sr_buffer_processed( sr_buf ), 0 );

  // parse an unterminated string
  len = TestReset_sr_buffer( sr_buf, "$40\r\nHello W" );
  err_code = dbBE_Redis_parse_sr_buffer( sr_buf, &result );
  rc += TEST( err_code, 0 );
  rc += TEST( dbBE_Transport_sr_buffer_get_start( sr_buf ) + len, dbBE_Transport_sr_buffer_get_processed_position( sr_buf ) );
  rc += TEST( dbBE_Transport_sr_buffer_available( sr_buf ), len );
  rc += TEST( dbBE_Transport_sr_buffer_processed( sr_buf ), len );


  // parse an incomplete error msg
  len = TestReset_sr_buffer( sr_buf, "-This is a half err" );
  err_code = dbBE_Redis_parse_sr_buffer( sr_buf, &result );
  rc += TEST( err_code, -EAGAIN );
  rc += TEST( dbBE_Transport_sr_buffer_get_start( sr_buf ), dbBE_Transport_sr_buffer_get_processed_position( sr_buf ) );
  rc += TEST( dbBE_Transport_sr_buffer_available( sr_buf ), len );
  rc += TEST( dbBE_Transport_sr_buffer_processed( sr_buf ), 0 );

  // parse an incomplete simple string
  len = TestReset_sr_buffer( sr_buf, "+This is a half stri" );
  err_code = dbBE_Redis_parse_sr_buffer( sr_buf, &result );
  rc += TEST( err_code, -EAGAIN );
  rc += TEST( dbBE_Transport_sr_buffer_get_start( sr_buf ), dbBE_Transport_sr_buffer_get_processed_position( sr_buf ) );
  rc += TEST( dbBE_Transport_sr_buffer_available( sr_buf ), len );
  rc += TEST( dbBE_Transport_sr_buffer_processed( sr_buf ), 0 );

  // parse an incomplete array with complete int entry
  len = TestReset_sr_buffer( sr_buf, "*2\r\n:3053\r\n" );
  err_code = dbBE_Redis_parse_sr_buffer( sr_buf, &result );
  rc += TEST( err_code, -EAGAIN );
  rc += TEST( dbBE_Transport_sr_buffer_get_start( sr_buf ), dbBE_Transport_sr_buffer_get_processed_position( sr_buf ) );
  rc += TEST( dbBE_Transport_sr_buffer_available( sr_buf ), len );
  rc += TEST( dbBE_Transport_sr_buffer_processed( sr_buf ), 0 );

  // parse an incomplete array with complete string entries
  len = TestReset_sr_buffer( sr_buf, "*2\r\n$10\r\nblafaseled\r\n" );
  err_code = dbBE_Redis_parse_sr_buffer( sr_buf, &result );
  rc += TEST( err_code, -EAGAIN );
  rc += TEST( dbBE_Transport_sr_buffer_get_start( sr_buf ), dbBE_Transport_sr_buffer_get_processed_position( sr_buf ) );
  rc += TEST( dbBE_Transport_sr_buffer_available( sr_buf ), len );
  rc += TEST( dbBE_Transport_sr_buffer_processed( sr_buf ), 0 );


  // parse an incomplete array with incomplete int
  len = TestReset_sr_buffer( sr_buf, "*2\r\n:3053\r\n:20153" );
  err_code = dbBE_Redis_parse_sr_buffer( sr_buf, &result );
  rc += TEST( err_code, -EAGAIN );
  rc += TEST( dbBE_Transport_sr_buffer_get_start( sr_buf ), dbBE_Transport_sr_buffer_get_processed_position( sr_buf ) );
  rc += TEST( dbBE_Transport_sr_buffer_available( sr_buf ), len );
  rc += TEST( dbBE_Transport_sr_buffer_processed( sr_buf ), 0 );

  // parse an incomplete array with incomplete string
  len = TestReset_sr_buffer( sr_buf, "*2\r\n:3053\r\n$10\r\nblafa" );
  err_code = dbBE_Redis_parse_sr_buffer( sr_buf, &result );
  rc += TEST( err_code, -EAGAIN );
  rc += TEST( dbBE_Transport_sr_buffer_get_start( sr_buf ), dbBE_Transport_sr_buffer_get_processed_position( sr_buf ) );
  rc += TEST( dbBE_Transport_sr_buffer_available( sr_buf ), len );
  rc += TEST( dbBE_Transport_sr_buffer_processed( sr_buf ), 0 );


  // parse an incomplete array
  len = TestReset_sr_buffer( sr_buf, "*2\r\n:3053\r\n$125" );
  err_code = dbBE_Redis_parse_sr_buffer( sr_buf, &result );
  rc += TEST( err_code, -EAGAIN );
  rc += TEST( dbBE_Transport_sr_buffer_get_start( sr_buf ), dbBE_Transport_sr_buffer_get_processed_position( sr_buf ) );
  rc += TEST( dbBE_Transport_sr_buffer_available( sr_buf ), len );
  rc += TEST( dbBE_Transport_sr_buffer_processed( sr_buf ), 0 );


  // parse an incomplete array with complete string and then add more data to check for reentrant parsing
  len = TestReset_sr_buffer( sr_buf, "*2\r\n$10\r\nblafaseled\r\n" );
  err_code = dbBE_Redis_parse_sr_buffer( sr_buf, &result );
  rc += TEST( err_code, -EAGAIN );
  rc += TEST( dbBE_Transport_sr_buffer_get_start( sr_buf ), dbBE_Transport_sr_buffer_get_processed_position( sr_buf ) );
  rc += TEST( dbBE_Transport_sr_buffer_available( sr_buf ), len );
  rc += TEST( dbBE_Transport_sr_buffer_processed( sr_buf ), 0 );

  // add a missing entry and see if we can correctly parse
  size_t added = snprintf( dbBE_Transport_sr_buffer_get_available_position( sr_buf ), 12, ":3253\r\n" );
  rc += TEST( added, 7 );
  rc += TEST( dbBE_Transport_sr_buffer_add_data( sr_buf, added, 0 ), added );
  rc += TEST( dbBE_Transport_sr_buffer_available( sr_buf ), len + 7 );
  err_code = dbBE_Redis_parse_sr_buffer( sr_buf, &result );
  rc += TEST( err_code, 0 );
  rc += TEST( dbBE_Transport_sr_buffer_empty( sr_buf ), 1 );
  rc += TEST( dbBE_Transport_sr_buffer_processed( sr_buf ), len + 7 );
  rc += TEST( result._type, dbBE_REDIS_TYPE_ARRAY );
  rc += TEST( result._data._array._len, 2 );
  rc += TEST( result._data._array._data[ 0 ]._type, dbBE_REDIS_TYPE_CHAR );
  rc += TEST( result._data._array._data[ 0 ]._data._string._size, 10 );
  rc += TEST( strncmp( result._data._array._data[ 0 ]._data._string._data, "blafaseled", 12 ), 0 );
  rc += TEST( result._data._array._data[ 1 ]._type, dbBE_REDIS_TYPE_INT );
  rc += TEST( result._data._array._data[ 1 ]._data._integer, 3253 );
  dbBE_Redis_result_cleanup( &result, 0 );

  dbBE_Transport_sr_buffer_free( sr_buf );
  sr_buf = NULL;
  rc += TEST( dbBE_Redis_parse_sr_buffer( NULL, NULL ), -EINVAL );
  rc += TEST( dbBE_Redis_parse_sr_buffer( NULL, &result ), -EINVAL );
  rc += TEST( result._type, dbBE_REDIS_TYPE_INVALID );
  printf( "TestRedis_parse_ctx_buffer_errors exiting with rc=%d\n", rc );
  return rc;
}

// define the function here because it's not exposed in header file
dbBE_Transport_sge_buffer_t* dbBE_Redis_parse_copy_assemble_sge( dbBE_Request_t *r,
                                                             dbBE_Redis_result_t *c,
                                                             dbBE_Transport_sge_buffer_t *sge_buf,
                                                             dbBE_Transport_dbuffer_t *dbuf,
                                                             dbBE_sge_t overflow );

#define MIN(x,y) ((x)<(y)?(x):(y))
#define MAX(x,y) ((x)>(y)?(x):(y))

int TestRedis_parse_create_random_sge( const int sge_max,
                                       const size_t size_max,
                                       char *base,
                                       dbBE_sge_t *sge )
{
  int sge_count = sge_max;
  size_t offset = 0;
  if( (size_t)sge_max > (size_max >> 1))  // make sure we can fit at least 2bytes per SGE into size_max
    return -EINVAL;

  // average SGE should be an Nth of the available size
  // with a spread of 1/4th of the average
  size_t spread = size_max / (sge_max << 2 );
  size_t min_len = size_max / sge_max - (spread >> 1 );
  size_t e_len;
  int n;
  for( n=0; n < sge_max; ++n )
  {
    if( n == sge_max - 1 ) // last SGE to fill
      e_len = size_max - offset;
    else
      e_len = (random() % (spread+1)) + min_len;
    if( offset + e_len > ( size_max - (min_len * ( sge_max - n - 1 ) )))
      e_len = min_len;
    sge[ n ].iov_base = base + offset;
    sge[ n ].iov_len = e_len;
    offset += e_len;
  }
  return sge_count;
}

int TestRedis_parse_find_first( dbBE_sge_t *sge, const int sge_count, const ssize_t len )
{
  if( len == 0 ) return 0;

  int n = 0;
  ssize_t remain = len;
  while(( remain > 0 ) && ( n < sge_count ))
  {
    remain -= sge[ n ].iov_len;
    ++n;
  }
  if( remain > 0 )  // when there's more data than space
    return n;

  return n - 1; // ++n overshoots by one
}


int TestSGEAssemble()
{
  int rc = 0;

  enum fuzz_cases {
      DBBE_TEST_NONE = 0x0,
      DBBE_TEST_R_lt_U = 0x01, // receive < user
      DBBE_TEST_R_gt_U = 0x02, // receive > user
      DBBE_TEST_R_eq_U = 0x04, // receive == user
      DBBE_TEST_O_lt_B = 0x08, // oversize < rbuffer
      DBBE_TEST_O_gt_B = 0x10, // oversize > rbuffer
      DBBE_TEST_O_eq_B = 0x20, // oversize == rbuffer
      DBBE_TEST_ALL = 0x3F
  };

  dbBE_Request_t *req;
  dbBE_Redis_result_t res;

  char *rbuf, *compbuf, *flat;
  dbBE_Transport_dbuffer_t *dbuf;
  dbBE_Transport_sge_buffer_t sge_buf, *r_sge_b;

  rc += TEST_NOT_RC( dbBE_Request_allocate( DBBE_SGE_MAX ), NULL, req );
  rc += TEST_NOT_RC( dbBE_Transport_dbuffer_allocate( DBBE_TEST_BUFFER_LEN ), NULL, dbuf );

  dbBE_Redis_sr_buffer_t *sr_buf = dbBE_Transport_dbuffer_get_active( dbuf );

  // create long random data as the user mem
  rc += TEST_NOT_RC( generateLongMsg( DBBE_TEST_BUFFER_LEN * 100 ), NULL, rbuf );
  rc += TEST_NOT_RC( (char*)calloc( 1, DBBE_TEST_BUFFER_LEN * 100 ), NULL, flat );
  rc += TEST_NOT_RC( (char*)calloc( 1, DBBE_TEST_BUFFER_LEN * 100 ), NULL, compbuf );

  // copy a chunk of the data into sr_buf as 'pre-received' data
  memcpy( dbBE_Transport_sr_buffer_get_start( sr_buf ), rbuf, DBBE_TEST_BUFFER_LEN );
  // copy into compbuf for data comparison/validation
  memcpy( compbuf, rbuf, DBBE_TEST_BUFFER_LEN * 100 );
  TEST_BREAK( rc, "Memory allocation failure\n" );

  size_t u_recv_len;


  // quick test the SGE randomizer
  for( u_recv_len = 2; u_recv_len < DBBE_TEST_BUFFER_LEN * 100; u_recv_len += (random() % DBBE_TEST_BUFFER_LEN) )
  {
    int u_sge_c;
    int u_max_sge = MIN( DBBE_SGE_MAX / 4, (u_recv_len >> 1) );
    for( u_sge_c = 1; u_sge_c <= u_max_sge; ++u_sge_c )
    {
      rc += TEST_RC( TestRedis_parse_create_random_sge( u_sge_c, u_recv_len, rbuf, req->_sge ), u_sge_c, req->_sge_count );
      rc += TEST( dbBE_SGE_get_len( req->_sge, req->_sge_count ), u_recv_len );
    }
  }


  dbBE_sge_t overflow;
  overflow.iov_len = DBBE_TEST_BUFFER_LEN * 99;
  rc += TEST_NOT_RC( malloc( overflow.iov_len ), NULL, overflow.iov_base );

  int64_t casecover = 0;

  int n;
  for( n=0; (n < 100) && (rc == 0); ++n )
  {
    u_recv_len= random() % (DBBE_TEST_BUFFER_LEN * 95) + 1;
//    size_t max_p_len = ( u_recv_len < DBBE_TEST_BUFFER_LEN ? u_recv_len : DBBE_TEST_BUFFER_LEN );
    size_t p_recv_len;


    for( p_recv_len=0; p_recv_len<DBBE_TEST_PRECV_LEN;  )
    {
      res._type = dbBE_REDIS_TYPE_STRING_PART;
      res._data._pstring._data = dbBE_Transport_sr_buffer_get_start( sr_buf );
      res._data._pstring._size = p_recv_len;
      res._data._pstring._total_size = u_recv_len + ( (n >> 5) * random() % (DBBE_TEST_PRECV_LEN * 2) ); // after 25% of n-loops, start adding increasing additional data

      if(( n == 30 ) &&( p_recv_len > ( DBBE_TEST_PRECV_LEN / 2 ) ))
        res._data._pstring._total_size = MAX( u_recv_len, p_recv_len ) + DBBE_TEST_BUFFER_LEN - 2;

      size_t copylen = MIN( p_recv_len, u_recv_len );
      int64_t oversize = res._data._pstring._total_size - MAX( u_recv_len, p_recv_len ); // what's too much

      if( p_recv_len < u_recv_len ) casecover |= DBBE_TEST_R_lt_U;
      if( p_recv_len > u_recv_len ) casecover |= DBBE_TEST_R_gt_U;
      if( p_recv_len == u_recv_len ) casecover |= DBBE_TEST_R_eq_U;
      if( oversize < DBBE_TEST_BUFFER_LEN - 2 ) casecover |= DBBE_TEST_O_lt_B;
      if( oversize > DBBE_TEST_BUFFER_LEN - 2 ) casecover |= DBBE_TEST_O_gt_B;
      if( oversize == DBBE_TEST_BUFFER_LEN - 2 ) casecover |= DBBE_TEST_O_eq_B;

      // build a user SGE with increasing length
      int u_sge_c;
      int u_max_sge = MIN( TEST_SGE_MAX, u_recv_len );
      for( u_sge_c = 1; u_sge_c < u_max_sge; ++u_sge_c )
      {
        rc += TEST_RC( TestRedis_parse_create_random_sge( u_sge_c, u_recv_len, rbuf, req->_sge ), u_sge_c, req->_sge_count );

        rc += TEST_NOT_RC( dbBE_Redis_parse_copy_assemble_sge( req, &res, &sge_buf, dbuf, overflow ), NULL, r_sge_b );
        int first_sge_idx = TestRedis_parse_find_first( req->_sge, req->_sge_count, p_recv_len );

        // number of sges depends on the amount of received vs. expected data (if too much, it needs to go to an extra dump buffer
        if( oversize > DBBE_TEST_BUFFER_LEN - 2) // -2 because of termination
          rc += TEST( (int)r_sge_b->_index, (p_recv_len > u_recv_len ? 0 : req->_sge_count - first_sge_idx)+2 );
        else
          rc += TEST( (int)r_sge_b->_index, (p_recv_len > u_recv_len ? 0 : req->_sge_count - first_sge_idx)+1 );

        int last = r_sge_b->_index - 1;
        if( p_recv_len <= u_recv_len )
        {
          // the first index in request sge where additional data will be received plus the r_sge_b count should match the user sge count (or +1 for overlapped SGE caused by partial fill
          rc += TEST( ((first_sge_idx + r_sge_b->_index - 1 == (unsigned)req->_sge_count ) || (first_sge_idx + r_sge_b->_index - 1 == (unsigned)req->_sge_count + 1)), 1 );
          rc += TEST( r_sge_b->_cmd[ 0 ].iov_base, rbuf + copylen );
          if( u_sge_c == 1 )
            rc += TEST( r_sge_b->_cmd[ 0 ].iov_len, MAX( (ssize_t)u_recv_len - (ssize_t)p_recv_len, 0) );
        }
        if( oversize > DBBE_TEST_BUFFER_LEN - 2)
        {
          rc += TEST( r_sge_b->_cmd[ last-1 ].iov_base, overflow.iov_base );
          rc += TEST( r_sge_b->_cmd[ last-1 ].iov_len, (size_t)oversize + 2 );
        }
        rc += TEST( r_sge_b->_cmd[last].iov_base, dbBE_Transport_sr_buffer_get_start( sr_buf ) );
        rc += TEST( r_sge_b->_cmd[last].iov_len, DBBE_TEST_BUFFER_LEN - (DBBE_TEST_BUFFER_LEN >> 3) );

        // assemble the data for verification
        memcpy( flat, dbBE_Transport_sr_buffer_get_start( sr_buf ), copylen );
        rc += TEST( Flatten_sge( r_sge_b->_cmd, r_sge_b->_index-1, flat+copylen ), 0 );
        size_t failloc;
        rc += TEST_RC( strncmp( flat, compbuf, u_recv_len ), 0, failloc );
        memset( flat, 0, u_recv_len + 1 );
      }

      size_t tmp = random() % ( DBBE_TEST_PRECV_LEN );
      if(( u_recv_len < DBBE_TEST_PRECV_LEN ) && ( p_recv_len < u_recv_len ))
        tmp = MAX( u_recv_len - p_recv_len, tmp );
      if( p_recv_len + tmp >= DBBE_TEST_PRECV_LEN )
        tmp = MAX( DBBE_TEST_PRECV_LEN - p_recv_len - 1, 1 );
      p_recv_len += tmp;
    }
  }
  if( casecover != DBBE_TEST_ALL )
  {
    rc += 1;
    LOG( DBG_ERR, stderr, "Fuzzing/random SGE test did not cover all cases: %x/%x\n", (unsigned)casecover, DBBE_TEST_ALL );
  }


  free( overflow.iov_base );
  free( compbuf );
  free( rbuf );
  free( flat );
  dbBE_Transport_sr_buffer_free( sr_buf );
  dbBE_Redis_result_cleanup( &res, 0 );
  free( req );

  printf( "TestSGEAssemble exiting with rc=%d\n", rc );
  return rc;
}

int main( int argc, char ** argv )
{
  int rc = 0;

  rc += TEST( sizeof( int64_t ), 8 );
  if( rc != 0 )
  {
    fprintf( stderr, "Something is wrong with compiler: sizeof( int64_t )=%ld\n", sizeof( int64_t ) );
    return -1;
  }

  rc += TestRedis_get_strlen();
  rc += TestRedis_extract_int();
  rc += TestRedis_extract_bulk_string();
  rc += TestRedis_parse_ctx_buffer();
  rc += TestRedis_parse_ctx_buffer_errors();
  rc += TestSGEAssemble();

  printf( "Test exiting with rc=%d\n", rc );
  return rc;
}
