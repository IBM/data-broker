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


#include "logutil.h"
#include "test_utils.h"
#include "transports/double_buffer.h"

#define DBBE_TEST_BUFFER_LEN ( 1024 )

int main( int argc, char **argv )
{
  int rc = 0;

  dbBE_Transport_dbuffer_t *dbuf = NULL;
  rc += TEST( dbBE_Transport_dbuffer_allocate( -1 ), NULL );
  rc += TEST( dbBE_Transport_dbuffer_allocate( 0 ), NULL );

  // *2 because double buffer reports size of one half
  rc += TEST_NOT_RC( dbBE_Transport_dbuffer_allocate( DBBE_TEST_BUFFER_LEN ), NULL, dbuf );

  rc += TEST( dbBE_Transport_sr_buffer_get_size( &dbuf->_buf[0] ), DBBE_TEST_BUFFER_LEN );
  rc += TEST( dbBE_Transport_sr_buffer_get_size( &dbuf->_buf[1] ), DBBE_TEST_BUFFER_LEN );
  rc += TEST( dbBE_Transport_sr_buffer_get_start( &dbuf->_buf[0] ) + DBBE_TEST_BUFFER_LEN, dbuf->_buf[1]._start );

  rc += TEST( dbBE_Transport_dbuffer_get_active( dbuf ), &dbuf->_buf[0] );
  dbBE_Transport_dbuffer_toggle( dbuf );
  rc += TEST( dbBE_Transport_dbuffer_get_active( dbuf ), &dbuf->_buf[1] );
  dbBE_Transport_dbuffer_toggle( dbuf );
  rc += TEST( dbBE_Transport_dbuffer_get_active( dbuf ), &dbuf->_buf[0] );

  rc += TEST( dbBE_Transport_dbuffer_free( dbuf ), 0 );
  rc += TEST( dbBE_Transport_dbuffer_free( NULL ), -EINVAL );

  printf( "Test exiting with rc=%d\n", rc );
  return rc;
}
