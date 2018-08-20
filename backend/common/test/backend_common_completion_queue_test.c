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

#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>
#endif
#include <stdio.h>
#include <string.h>

#include <libdatabroker.h>
#include "../completion_queue.h"
#include "test_utils.h"

int RemoveTest( dbBE_Completion_queue_t *queue, dbBE_Completion_t *compl )
{
  int rc = 0;
  int i;

  rc += TEST( dbBE_Completion_queue_delete( NULL, &compl[0] ), EINVAL );
  rc += TEST( dbBE_Completion_queue_delete( queue, NULL ), EINVAL );

  for( i=0; i<10; ++i )
    rc += TEST( dbBE_Completion_queue_push( queue, &compl[i] ), 0 );
  rc += TEST( dbBE_Completion_queue_len( queue ), 10 );
  TEST_LOG( rc, "Queue Delete Test." );

  // delete the first (head access)
  rc += TEST( dbBE_Completion_queue_delete( queue, &compl[0] ), 0 );

  // delete the last (tail access)
  rc += TEST( dbBE_Completion_queue_delete( queue, &compl[9] ), 0 );

  // delete one from the middle
  rc += TEST( dbBE_Completion_queue_delete( queue, &compl[3] ), 0 );

  rc += TEST( dbBE_Completion_queue_len( queue ), 7 );

  // remove all but one
  for( i=0; i<6; ++i )
    rc += TEST_NOT( dbBE_Completion_queue_pop( queue ), NULL );
  rc += TEST( dbBE_Completion_queue_len( queue ), 1 );

  // remove the last entry (head and tail access)
  rc += TEST( dbBE_Completion_queue_delete( queue, &compl[8] ), 0 );

  TEST_LOG( rc, "Queue Delete Test." );
  return rc;
}

int main( int argc, char ** argv )
{
  int rc = 0;
  unsigned i;

  dbBE_Completion_queue_t *queue = dbBE_Completion_queue_create( 1 );
  dbBE_Completion_t compl[ 10 ];
  dbBE_Completion_t *ret;

  for( i=0; i<10; ++i )
    compl[ i ]._status = i;

  rc += TEST_NOT( queue, NULL );
  rc += TEST( dbBE_Completion_queue_pop( queue ), NULL );

  // add 10 items to the queue
  for( i=0; i<10; ++i )
    rc += TEST( dbBE_Completion_queue_push( queue, &compl[i] ), 0 );
  rc += TEST( dbBE_Completion_queue_len( queue ), 10 );

  // remove 4 items
  for( i=0; i<4; ++i )
  {
    ret = dbBE_Completion_queue_pop( queue );
    rc += TEST_NOT( ret, NULL );
    if( ret != NULL )
      rc += TEST( ret->_status, i );
  }
  rc += TEST( dbBE_Completion_queue_len( queue ), 6 );

  // add another 3
  for( i=0; i<3; ++i )
    rc += TEST( dbBE_Completion_queue_push( queue, &compl[i] ), 0 );
  rc += TEST( dbBE_Completion_queue_len( queue ), 9 );

  // remove 6
  for( i=4; i<10; ++i )
  {
    ret = dbBE_Completion_queue_pop( queue );
    rc += TEST_NOT( ret, NULL );
    if( ret != NULL )
      rc += TEST( ret->_status, i );
  }
  rc += TEST( dbBE_Completion_queue_len( queue ), 3 );

  // remove the remaining 3
  for( i=0; i<3; ++i )
  {
    ret = dbBE_Completion_queue_pop( queue );
    rc += TEST_NOT( ret, NULL );
    if( ret != NULL )
      rc += TEST( ret->_status, i );
  }
  rc += TEST( dbBE_Completion_queue_len( queue ), 0 );

  rc += RemoveTest( queue, compl );

  // add a few items before destruction, to employ the wiping code path
  for( i=0; i<3; ++i )
    rc += TEST( dbBE_Completion_queue_push( queue, &compl[i] ), 0 );

  rc += TEST( dbBE_Completion_queue_destroy( queue ), 0 );
  printf( "Test exiting with rc=%d\n", rc );
  return rc;
}
