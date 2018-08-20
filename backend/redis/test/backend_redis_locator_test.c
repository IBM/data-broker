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
#include <errno.h>
#include <libdatabroker.h>

#include "../backend/redis/locator.h"
#include "test_utils.h"


int main( int argc, char ** argv )
{
  int rc = 0;

  dbBE_Redis_locator_index_t cidx1, cidx2;
  cidx1 = 1;
  cidx2 = 2;

  // create locator
  dbBE_Redis_locator_t *locator = dbBE_Redis_locator_create();
  rc += TEST_NOT( locator, NULL );

  // assign a first address
  rc += TEST( dbBE_Redis_locator_assign_conn_index( locator, cidx1, 1234), 0 );

  // try to assign address to invalid slot
  rc += TEST_NOT( dbBE_Redis_locator_assign_conn_index( locator, cidx1, 32151), 0 );

  // assign same address to a second slot
  rc += TEST( dbBE_Redis_locator_assign_conn_index( locator, cidx1, 1235), 0 );

  // error case: nullptr locator
  rc += TEST_NOT( dbBE_Redis_locator_assign_conn_index( NULL, cidx1, 1234), 0 );

  // assign NULL to second slot to remove the address assignment
  rc += TEST( dbBE_Redis_locator_assign_conn_index( locator, DBBE_REDIS_LOCATOR_INDEX_INVAL, 1235), 0 );
  TEST_LOG( rc, "dbBE_Redis_assign_conn_index" );

  // failure cases
  rc += TEST( dbBE_Redis_locator_get_conn_index( NULL, 1234 ), DBBE_REDIS_LOCATOR_INDEX_INVAL );
  rc += TEST( dbBE_Redis_locator_get_conn_index( locator, 32151 ), DBBE_REDIS_LOCATOR_INDEX_INVAL );

  // retrieve first address and check content
  dbBE_Redis_locator_index_t cidx_r = dbBE_Redis_locator_get_conn_index( locator, 1234 );
  rc += TEST( cidx_r, cidx1 );

  TEST_LOG( rc, "dbBE_Redis_get_conn_index1" );

  // reassociate address1 with address2, expect only one address to change
  rc += TEST( dbBE_Redis_locator_reassociate_conn_index( locator, cidx1, cidx2 ), 1 );
  cidx_r = dbBE_Redis_locator_get_conn_index( locator, 1234 );
  rc += TEST( cidx_r, cidx2 );
  TEST_LOG( rc, "dbBE_Redis_reassociate_conn_index2" );

  // reassociate with null-ptr locator
  rc += TEST_NOT( dbBE_Redis_locator_reassociate_conn_index( NULL, cidx2, cidx1 ), 0 );
  cidx_r = dbBE_Redis_locator_get_conn_index( locator, 1234 );

  // assign first address to new slot
  rc += TEST( dbBE_Redis_locator_assign_conn_index( locator, cidx1, 1236 ), 0 );
  TEST_LOG( rc, "dbBE_Redis_reassociate_conn_index" );

  // disassociate all addr2 entries
  rc += TEST( dbBE_Redis_locator_reassociate_conn_index( locator, cidx2, DBBE_REDIS_LOCATOR_INDEX_INVAL ), 1 );

  rc += TEST( dbBE_Redis_locator_get_conn_index( locator, 1234 ), DBBE_REDIS_LOCATOR_INDEX_INVAL );
  TEST_LOG( rc, "dbBE_Redis_disassociate" );

  // assign all remaining NULL slots with addr1
  rc += TEST( dbBE_Redis_locator_reassociate_conn_index( locator, DBBE_REDIS_LOCATOR_INDEX_INVAL, cidx1 ), DBBE_REDIS_HASH_SLOT_MAX - 1 );

  // destroy the locator
  rc += TEST( dbBE_Redis_locator_destroy( locator ), 0 );

  printf( "Test exiting with rc=%d\n", rc );
  return rc;
}
