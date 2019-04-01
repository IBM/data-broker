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
#include <transports/sr_buffer.h>
#include "test_utils.h"

#define DBBE_TEST_BUFFER_LEN ( 1024 )

int main( int argc, char ** argv )
{
  int rc = 0;

  // test the regular use of the buffer
  dbBE_Redis_sr_buffer_t *buffer = dbBE_Transport_sr_buffer_allocate( DBBE_TEST_BUFFER_LEN );
  rc += TEST_NOT( buffer, NULL );
  rc += TEST( dbBE_Transport_sr_buffer_available( buffer ), 0 );
  rc += TEST( dbBE_Transport_sr_buffer_processed( buffer ), 0 );
  rc += TEST( dbBE_Transport_sr_buffer_empty( buffer ), 1 );
  rc += TEST( dbBE_Transport_sr_buffer_full( buffer, 0 ), 0 );

  rc += TEST( dbBE_Transport_sr_buffer_get_size( buffer ), DBBE_TEST_BUFFER_LEN );
  rc += TEST( dbBE_Transport_sr_buffer_get_processed_position( buffer ), dbBE_Transport_sr_buffer_get_start( buffer ));

  // test set_fill
  rc += TEST( dbBE_Transport_sr_buffer_set_fill( buffer, DBBE_TEST_BUFFER_LEN >> 1 ), DBBE_TEST_BUFFER_LEN >> 1 );
  rc += TEST( dbBE_Transport_sr_buffer_available( buffer ), DBBE_TEST_BUFFER_LEN >> 1 );
  rc += TEST( dbBE_Transport_sr_buffer_processed( buffer ), 0 );
  rc += TEST( dbBE_Transport_sr_buffer_empty( buffer ), 0 );
  rc += TEST( dbBE_Transport_sr_buffer_full( buffer, DBBE_TEST_BUFFER_LEN     ), 1 );
  rc += TEST( dbBE_Transport_sr_buffer_full( buffer, (DBBE_TEST_BUFFER_LEN >> 1)+1), 1 );
  rc += TEST( dbBE_Transport_sr_buffer_full( buffer, DBBE_TEST_BUFFER_LEN >> 1), 0 );
  rc += TEST( dbBE_Transport_sr_buffer_full( buffer, DBBE_TEST_BUFFER_LEN >> 2), 0 );
  rc += TEST( dbBE_Transport_sr_buffer_get_size( buffer ), DBBE_TEST_BUFFER_LEN );
  rc += TEST( dbBE_Transport_sr_buffer_get_processed_position( buffer ), dbBE_Transport_sr_buffer_get_start( buffer ));

  // test advancing
  rc += TEST( dbBE_Transport_sr_buffer_advance( buffer, DBBE_TEST_BUFFER_LEN >> 2 ), DBBE_TEST_BUFFER_LEN >> 2 );
  rc += TEST( dbBE_Transport_sr_buffer_available( buffer ), DBBE_TEST_BUFFER_LEN >> 1 );
  rc += TEST( dbBE_Transport_sr_buffer_processed( buffer ), DBBE_TEST_BUFFER_LEN >> 2 );
  rc += TEST( dbBE_Transport_sr_buffer_empty( buffer ), 0 );
  rc += TEST( dbBE_Transport_sr_buffer_get_size( buffer ), DBBE_TEST_BUFFER_LEN );
  rc += TEST( dbBE_Transport_sr_buffer_get_processed_position( buffer ), dbBE_Transport_sr_buffer_get_start( buffer ) + (DBBE_TEST_BUFFER_LEN >> 2) );

  // test advancing beyond the available data
  rc += TEST( dbBE_Transport_sr_buffer_advance( buffer, DBBE_TEST_BUFFER_LEN >> 1 ), DBBE_TEST_BUFFER_LEN >> 2 );
  rc += TEST( dbBE_Transport_sr_buffer_available( buffer ), DBBE_TEST_BUFFER_LEN >> 1 );
  rc += TEST( dbBE_Transport_sr_buffer_processed( buffer ), DBBE_TEST_BUFFER_LEN >> 1 );
  rc += TEST( dbBE_Transport_sr_buffer_empty( buffer ), 1 );
  rc += TEST( dbBE_Transport_sr_buffer_full( buffer, 0), 0 );
  rc += TEST( dbBE_Transport_sr_buffer_get_size( buffer ), DBBE_TEST_BUFFER_LEN );
  rc += TEST( dbBE_Transport_sr_buffer_get_processed_position( buffer ), dbBE_Transport_sr_buffer_get_start( buffer ) + (DBBE_TEST_BUFFER_LEN >> 1) );

  // resetting
  dbBE_Transport_sr_buffer_reset( buffer );
  rc += TEST( dbBE_Transport_sr_buffer_available( buffer ), 0 );
  rc += TEST( dbBE_Transport_sr_buffer_processed( buffer ), 0 );
  rc += TEST( dbBE_Transport_sr_buffer_empty( buffer ), 1 );
  rc += TEST( dbBE_Transport_sr_buffer_full( buffer, DBBE_TEST_BUFFER_LEN ), 0 );
  rc += TEST( dbBE_Transport_sr_buffer_get_processed_position( buffer ), dbBE_Transport_sr_buffer_get_start( buffer ));

  // test add data (read behavior)
  rc += TEST( dbBE_Transport_sr_buffer_add_data( buffer, DBBE_TEST_BUFFER_LEN >> 1, 0 ), DBBE_TEST_BUFFER_LEN >> 1 );
  rc += TEST( dbBE_Transport_sr_buffer_available( buffer ), DBBE_TEST_BUFFER_LEN >> 1 );
  rc += TEST( dbBE_Transport_sr_buffer_processed( buffer ), 0 );
  rc += TEST( dbBE_Transport_sr_buffer_empty( buffer ), 0 );
  rc += TEST( dbBE_Transport_sr_buffer_get_processed_position( buffer ), dbBE_Transport_sr_buffer_get_start( buffer ));

  // advance the processed position to see if the rewind is updating the processed too
  rc += TEST( dbBE_Transport_sr_buffer_advance( buffer, (DBBE_TEST_BUFFER_LEN >> 2) + 5 ), (DBBE_TEST_BUFFER_LEN >> 2) + 5 );
  rc += TEST( dbBE_Transport_sr_buffer_processed( buffer ), (DBBE_TEST_BUFFER_LEN >> 2) + 5 );
  rc += TEST( dbBE_Transport_sr_buffer_get_processed_position( buffer ), dbBE_Transport_sr_buffer_get_start( buffer ) + (DBBE_TEST_BUFFER_LEN >> 2) + 5);

  // test rewind by a number to half the current available size
  rc += TEST( dbBE_Transport_sr_buffer_rewind_available_by( buffer, DBBE_TEST_BUFFER_LEN >> 2 ), DBBE_TEST_BUFFER_LEN >> 2 );
  rc += TEST( dbBE_Transport_sr_buffer_available( buffer ), DBBE_TEST_BUFFER_LEN >> 2 );
  rc += TEST( dbBE_Transport_sr_buffer_processed( buffer ), DBBE_TEST_BUFFER_LEN >> 2 );
  rc += TEST( dbBE_Transport_sr_buffer_empty( buffer ), 1 );
  rc += TEST( dbBE_Transport_sr_buffer_get_processed_position( buffer ), dbBE_Transport_sr_buffer_get_start( buffer ) + (DBBE_TEST_BUFFER_LEN >> 2));

  // test add data (write behavior)
  dbBE_Transport_sr_buffer_reset( buffer );
  rc += TEST( dbBE_Transport_sr_buffer_add_data( buffer, DBBE_TEST_BUFFER_LEN >> 1, 1 ), DBBE_TEST_BUFFER_LEN >> 1 );
  rc += TEST( dbBE_Transport_sr_buffer_available( buffer ), DBBE_TEST_BUFFER_LEN >> 1 );
  rc += TEST( dbBE_Transport_sr_buffer_processed( buffer ), DBBE_TEST_BUFFER_LEN >> 1 );
  rc += TEST( dbBE_Transport_sr_buffer_get_processed_position( buffer ), dbBE_Transport_sr_buffer_get_start( buffer ) + (DBBE_TEST_BUFFER_LEN >> 1) );

  dbBE_Transport_sr_buffer_free( buffer );

  printf( "Test exiting with rc=%d\n", rc );
  return rc;
}
