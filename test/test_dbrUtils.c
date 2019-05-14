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

  if( mc == NULL )
    return 1;
  while(( tag = dbrTag_get( mc ) ) != DB_TAG_ERROR )
  {
    ++n;
    dbrRequestContext_t *r = (dbrRequestContext_t *)malloc( sizeof (dbrRequestContext_t) + 5 * sizeof(dbBE_sge_t)  );
    memset( r, 0, sizeof( dbrRequestContext_t ) + 5 * sizeof(dbBE_sge_t) );
    mc->_cs_wq[ tag ] = r;

    rc += TEST_NOT( mc->_cs_wq[ tag ], NULL );
    LOG( DBG_INFO, stdout, "Freeing %p\n", mc->_cs_wq[ tag ] );
    TEST_BREAK( rc, "Allocation failure" );
    mc->_cs_wq[ tag ]->_status = dbrSTATUS_PENDING;
    LOG( DBG_INFO, stdout, "Allocated tag: %"PRId64"\n", tag );
  }
  rc += TEST( n < dbrMAX_TAGS, 0 );

  LOG( DBG_INFO, stdout, "Allocated tag count: %d\n", n );
  rc += TEST( n, dbrMAX_TAGS );

  for( n = 0; n<TAG_TEST_COUNT; ++n )
  {
    int p = random() % dbrMAX_TAGS;
    LOG( DBG_INFO, stdout, "Testing tag %d; wqe[]=%p\n", p, mc->_cs_wq[ p ] );
    rc += TEST( dbrValidateTag( NULL, p ), DBR_SUCCESS );
    if( mc->_cs_wq[ p ] != NULL )
      mc->_cs_wq[ p ]->_status = dbrSTATUS_CLOSED;
    tag = dbrTag_get( mc );
    rc += TEST( tag, p );

    dbrRequestContext_t *r = (dbrRequestContext_t *)malloc( sizeof (dbrRequestContext_t) + 5 * sizeof(dbBE_sge_t)  );
    memset( r, 0, sizeof( dbrRequestContext_t ) + 5 * sizeof(dbBE_sge_t) );
    mc->_cs_wq[ tag ] = r;
    rc += TEST_NOT( mc->_cs_wq[ tag ], NULL );
    TEST_BREAK( rc, "Allocation failure" );
    mc->_cs_wq[ tag ]->_status = dbrSTATUS_PENDING;
    LOG( DBG_INFO, stdout, "Allocated tag: %"PRId64"\n", tag );

  }

  for( n=0; n<dbrMAX_TAGS; ++n )
  {
    if( mc->_cs_wq[ n ] != NULL )
      mc->_cs_wq[ n ]->_status = dbrSTATUS_CLOSED;
  }

  rc += TEST_NOT( dbrTag_get( mc ), DB_TAG_ERROR );

  dbrRequestContext_t rctx;
  rc += TEST( dbrValidateTag( NULL, 0 ), DBR_SUCCESS );

#ifdef DBR_INTTAG
  rc += TEST( dbrValidateTag( &rctx, 0 ), DBR_SUCCESS );
  rc += TEST( dbrValidateTag( &rctx, DB_TAG_ERROR ), DBR_ERR_TAGERROR );
  rc += TEST( dbrValidateTag( &rctx, dbrMAX_TAGS - 1 ), DBR_SUCCESS );
  rc += TEST( dbrValidateTag( &rctx, dbrMAX_TAGS ), DBR_ERR_TAGERROR );
#else
  rc += TEST( dbrValidateTag( &rctx, NULL ), DBR_ERR_TAGERROR );
  rc += TEST( dbrValidateTag( &rctx, DB_TAG_ERROR ), DBR_ERR_TAGERROR );
#endif
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

