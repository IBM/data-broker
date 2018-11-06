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

#include "../libdatabroker_int.h"
#include "../lib/sge.h"
#include "../../test/test_utils.h"

#define TEST_SGE_MAX (1024)


int main( int argc, char ** argv )
{
  int rc = 0;

  dbrRequestContext_t *rctx = NULL;

  dbrName_space_t *ns = dbrMain_create_local( "TestNameSpace" );

  rctx = dbrCreate_request_ctx( DBBE_OPCODE_GET,
                                NULL,
                                DBR_GROUP_LIST_EMPTY,
                                NULL,
                                DBR_GROUP_LIST_EMPTY,
                                0,
                                NULL,
                                NULL,
                                NULL,
                                NULL,
                                DB_TAG_ERROR );
  rc += TEST( rctx, NULL );


  rctx = dbrCreate_request_ctx( DBBE_OPCODE_GET,
                                ns,
                                DBR_GROUP_LIST_EMPTY,
                                NULL,
                                DBR_GROUP_LIST_EMPTY,
                                0,
                                NULL,
                                NULL,
                                NULL,
                                NULL,
                                DB_TAG_ERROR );
  rc += TEST_NOT( rctx, NULL );

  // trying to clean rctx with tag == TAG_ERROR, will not work
  rc += TEST( dbrRemove_request( ns, rctx ), DBR_ERR_TAGERROR );

  rctx = dbrCreate_request_ctx( DBBE_OPCODE_GET,
                                ns,
                                DBR_GROUP_LIST_EMPTY,
                                NULL,
                                DBR_GROUP_LIST_EMPTY,
                                0,
                                NULL,
                                NULL,
                                NULL,
                                NULL,
                                dbrTag_get( ns->_reverse ) );
  rc += TEST_NOT( rctx, NULL );

  // trying to clean rctx with tag == TAG_ERROR, will not work
  rc += TEST( dbrInsert_request( ns, rctx ), rctx->_tag );
  rc += TEST( dbrRemove_request( ns, rctx ), DBR_SUCCESS );




  printf( "Test exiting with rc=%d\n", rc );
  return rc;
}

