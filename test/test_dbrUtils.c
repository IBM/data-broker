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
#include <stddef.h>
#include <stdio.h>
#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>
#endif
#include <unistd.h>
#include <string.h>
#include <sys/time.h>

#include <libdatabroker.h>
#include "../src/libdatabroker_int.h"
#include "test_utils.h"


int dbrTag_get_test( dbrMain_context_t *mc )
{
  const int TAG_TEST_COUNT = 10000;
  int rc = 0;
  int n = 0;
  DBR_Tag_t tag;
  while(( tag = dbrTag_get( mc ) ) != DB_TAG_ERROR )
  {
    ++n;
    mc->_cs_wq[ tag ] = (dbrRequestContext_t *)malloc( sizeof (dbrRequestContext_t) );
    rc += TEST_NOT( mc->_cs_wq[ tag ], NULL );
    TEST_BREAK( rc, "Allocation failure" );
    mc->_cs_wq[ tag ]->_status = dbrSTATUS_PENDING;
    LOG( DBG_INFO, stdout, "Allocated tag: %lld\n", tag );
  }
  rc += TEST( n < dbrMAX_TAGS, 0 );

  LOG( DBG_INFO, stdout, "Allocated tag count: %d\n", n );
  rc += TEST( n, dbrMAX_TAGS );

  for( n = 0; n<TAG_TEST_COUNT; ++n )
  {
    int p = random() % dbrMAX_TAGS;
    mc->_cs_wq[ p ]->_status = dbrSTATUS_CLOSED;
    tag = dbrTag_get( mc );
    rc += TEST( tag, p );

    mc->_cs_wq[ tag ] = (dbrRequestContext_t *)malloc( sizeof (dbrRequestContext_t) );
    rc += TEST_NOT( mc->_cs_wq[ tag ], NULL );
    TEST_BREAK( rc, "Allocation failure" );
    mc->_cs_wq[ tag ]->_status = dbrSTATUS_PENDING;
    LOG( DBG_INFO, stdout, "Allocated tag: %lld\n", tag );

  }

  for( n=0; n<dbrMAX_TAGS; ++n )
  {
    if( mc->_cs_wq[ n ] != NULL )
      mc->_cs_wq[ n ]->_status = dbrSTATUS_CLOSED;
  }

  rc += TEST_NOT( dbrTag_get( mc ), DB_TAG_ERROR );

  return rc;
}

int main( int argc, char ** argv )
{
  int rc = 0;

  // Testing of the main context handling
  dbrMain_context_t *mc = dbrCheckCreateMainCTX();
  dbrMain_context_t *mc2 = dbrCheckCreateMainCTX();

  rc += TEST_NOT( mc, NULL );
  rc += TEST( mc, mc2 );

  rc += TEST( dbrMain_exit(), 0 );
  mc2 = dbrCheckCreateMainCTX();
  rc += TEST_NOT( mc2, NULL );
  rc += TEST( dbrMain_exit(), 0 );
  rc += TEST( dbrMain_exit(), 0 );

  // testing of dbrTag_get
  rc += TEST( dbrTag_get( NULL ), DB_TAG_ERROR );

  mc = dbrCheckCreateMainCTX();
  rc += dbrTag_get_test( mc );


  printf( "Test exiting with rc=%d\n", rc );
  return rc;
}

