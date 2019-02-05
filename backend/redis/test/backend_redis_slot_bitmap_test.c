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
#include <errno.h>
#include <libdatabroker.h>

#include "../backend/redis/slot_bitmap.h"
#include "test_utils.h"


int main( int argc, char ** argv )
{
  int rc = 0;
  int n = 0;

  dbBE_Redis_slot_bitmap_t *sb = dbBE_Redis_slot_bitmap_create();
  rc += TEST_NOT( sb, NULL );

  // shouldn't be covered after initialization
  rc += TEST( dbBE_Redis_slot_bitmap_full( sb ), 0 );

  // fill the space
  for( n=0; n<16384; ++n )
    rc += TEST( dbBE_Redis_slot_bitmap_set( sb, n ), n );

  // should be covered now
  rc += TEST( dbBE_Redis_slot_bitmap_full( sb ), 1 );

  // set a bit that's already set and expect to be still covered
  rc += TEST( dbBE_Redis_slot_bitmap_set( sb, 5054 ), 5054 );
  rc += TEST( dbBE_Redis_slot_bitmap_full( sb ), 1 );


  int tb = 0;
  int cv;
  for( tb=0; tb<16384; ++tb )
  {
    // unset a bit and expect not to be covered
    rc += TEST( dbBE_Redis_slot_bitmap_unset( sb, tb ), tb );
    rc += TEST( dbBE_Redis_slot_bitmap_full( sb ), 0 );

    rc += TEST_RC( dbBE_Redis_slot_bitmap_get_first_unset( sb ), tb, cv );
//    fprintf( stdout, "First unset bit: %d == %d\n", tb, cv );

    // set this bit again and expect coverage
    rc += TEST( dbBE_Redis_slot_bitmap_set( sb, tb ), tb );
    rc += TEST( dbBE_Redis_slot_bitmap_full( sb ), 1 );
  }

  // destroy the hash cover
  rc += TEST( dbBE_Redis_slot_bitmap_destroy( sb ), 0 );
  rc += TEST( dbBE_Redis_slot_bitmap_destroy( NULL ), -EINVAL );

  printf( "Test exiting with rc=%d\n", rc );
  return rc;
}
