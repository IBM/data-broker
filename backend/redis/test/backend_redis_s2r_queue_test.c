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
#include "../backend/redis/s2r_queue.h"
#include "test_utils.h"

int main( int argc, char ** argv )
{
  int rc = 0;
  int i;

  dbBE_Redis_s2r_queue_t *queue = dbBE_Redis_s2r_queue_create( 1 );
  dbBE_Redis_request_t req[ 10 ];
  dbBE_Redis_request_t *ret;

  for( i=0; i<10; ++i )
  {
    req[ i ]._location._type = DBBE_REDIS_REQUEST_LOCATION_TYPE_SLOT;
    req[ i ]._location._data._conn_idx = i;
  }

  rc += TEST_NOT( queue, NULL );
  rc += TEST( dbBE_Redis_s2r_queue_pop( queue ), NULL );

  // add 10 items to the queue
  for( i=0; i<10; ++i )
    rc += TEST( dbBE_Redis_s2r_queue_push( queue, &req[i] ), 0 );
  rc += TEST( dbBE_Redis_s2r_queue_len( queue ), 10 );

  // remove 4 items
  for( i=0; i<4; ++i )
  {
    ret = dbBE_Redis_s2r_queue_pop( queue );
    rc += TEST_NOT( ret, NULL );
    if( ret != NULL )
      rc += TEST( ret->_location._data._conn_idx, i );
  }
  rc += TEST( dbBE_Redis_s2r_queue_len( queue ), 6 );

  // add another 3
  for( i=0; i<3; ++i )
    rc += TEST( dbBE_Redis_s2r_queue_push( queue, &req[i] ), 0 );
  rc += TEST( dbBE_Redis_s2r_queue_len( queue ), 9 );

  // remove 6
  for( i=4; i<10; ++i )
  {
    ret = dbBE_Redis_s2r_queue_pop( queue );
    rc += TEST_NOT( ret, NULL );
    if( ret != NULL )
      rc += TEST( ret->_location._data._conn_idx, i );
  }
  rc += TEST( dbBE_Redis_s2r_queue_len( queue ), 3 );

  // remove the remaining 3
  for( i=0; i<3; ++i )
  {
    ret = dbBE_Redis_s2r_queue_pop( queue );
    rc += TEST_NOT( ret, NULL );
    if( ret != NULL )
      rc += TEST( ret->_location._data._conn_idx, i );
  }
  rc += TEST( dbBE_Redis_s2r_queue_len( queue ), 0 );

  // add a few items before destruction, to employ the wiping code path
  for( i=0; i<3; ++i )
    rc += TEST( dbBE_Redis_s2r_queue_push( queue, &req[i] ), 0 );

  rc += TEST( dbBE_Redis_s2r_queue_destroy( queue ), 0 );
  printf( "Test exiting with rc=%d\n", rc );
  return rc;
}
