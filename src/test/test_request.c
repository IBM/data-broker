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

#include "libdatabroker.h"
#include "../libdatabroker_int.h"
#include "../lib/sge.h"
#include "../../test/test_utils.h"

#define TEST_SGE_MAX (1024)


int Request_create_test()
{
  int rc = 0;
  dbrRequestContext_t *rctx = NULL;
  dbBE_sge_t sge;
  sge._data = NULL;
  sge.iov_len = 0;

  dbrName_space_t *ns = dbrMain_create_local( "TestNameSpace" );

  rctx = dbrCreate_request_ctx( DBBE_OPCODE_GET, NULL, DBR_GROUP_LIST_EMPTY, NULL, DBR_GROUP_LIST_EMPTY, 0, NULL, NULL, NULL, NULL, DB_TAG_ERROR );
  rc += TEST( rctx, NULL );

  // request creation with error tag:
  rctx = dbrCreate_request_ctx( DBBE_OPCODE_GET, ns, DBR_GROUP_LIST_EMPTY, NULL, DBR_GROUP_LIST_EMPTY, 0, NULL, NULL, NULL, NULL, DB_TAG_ERROR );
  rc += TEST( rctx, NULL );

  // invalid sge input
  rctx = dbrCreate_request_ctx( DBBE_OPCODE_GET, ns, DBR_GROUP_LIST_EMPTY, NULL, DBR_GROUP_LIST_EMPTY, 1, NULL, NULL, NULL, NULL, dbrTag_get( ns->_reverse ) );
  rctx += TEST( rctx, NULL );

  rctx = dbrCreate_request_ctx( DBBE_OPCODE_GET, ns, DBR_GROUP_LIST_EMPTY, NULL, DBR_GROUP_LIST_EMPTY, 0, &sge, NULL, NULL, NULL, dbrTag_get( ns->_reverse ) );
  rctx += TEST( rctx, NULL );

  // try with invalid opcode
  rctx = dbrCreate_request_ctx( DBBE_OPCODE_MAX, ns, DBR_GROUP_LIST_EMPTY, NULL, DBR_GROUP_LIST_EMPTY, 0, &sge, NULL, NULL, NULL, dbrTag_get( ns->_reverse ) );
  rctx += TEST( rctx, NULL );


  // some successful creation
  rctx = dbrCreate_request_ctx( DBBE_OPCODE_GET, ns, DBR_GROUP_LIST_EMPTY, NULL, DBR_GROUP_LIST_EMPTY, 0, NULL, NULL, NULL, NULL, dbrTag_get( ns->_reverse ) );
  rc += TEST_NOT( rctx, NULL );

  // insert to remove
  rc += TEST( dbrInsert_request( ns, rctx ), rctx->_tag );
  rc += TEST( dbrRemove_request( ns, rctx ), DBR_SUCCESS );

  rc += TEST( dbrMain_delete( ns->_reverse, ns ), 0 );

  LOG( DBG_INFO, stdout, "Create request test: rc=%d\n", rc );
  return rc;
}

int Request_insert_test()
{
  int rc = 0;

  dbrRequestContext_t *rctx = NULL;
  dbBE_sge_t sge;
  sge._data = NULL;
  sge.iov_len = 0;

  dbrName_space_t *ns = dbrMain_create_local( "TestNameSpace" );

  // try to insert a NULL request
  rc += TEST( dbrInsert_request( ns, NULL ), DB_TAG_ERROR );

  // insert request with invalid tag
  rctx = dbrCreate_request_ctx( DBBE_OPCODE_GET, ns, DBR_GROUP_LIST_EMPTY, NULL, DBR_GROUP_LIST_EMPTY, 0, NULL, NULL, NULL, NULL, dbrTag_get( ns->_reverse ) );
  // change the tag to break the remove function
  DBR_Tag_t tmptag = rctx->_tag;
  rctx->_tag = DB_TAG_ERROR;

  rc += TEST( dbrInsert_request( ns, rctx ), DB_TAG_ERROR );

  // clean insert
  rctx->_tag = tmptag;
  rc += TEST( dbrInsert_request( ns, rctx ), tmptag );

  rc += TEST( dbrMain_delete( ns->_reverse, ns ), 0 );

  LOG( DBG_INFO, stdout, "Insert request test: rc=%d\n", rc );
  return rc;
}

int Request_remove_test()
{
  int rc = 0;

  dbrRequestContext_t *rctx = NULL;
  dbBE_sge_t sge;
  sge._data = NULL;
  sge.iov_len = 0;

  dbrName_space_t *ns = dbrMain_create_local( "TestNameSpace" );

  // trying to clean nullptr rctx
  rc += TEST( dbrRemove_request( ns, rctx ), DBR_ERR_INVALID );

  rctx = dbrCreate_request_ctx( DBBE_OPCODE_GET, ns, DBR_GROUP_LIST_EMPTY, NULL, DBR_GROUP_LIST_EMPTY, 0, NULL, NULL, NULL, NULL, dbrTag_get( ns->_reverse ) );

  // trying to remove a request that hasn't been inserted
  rc += TEST( dbrRemove_request( ns, rctx ), DBR_ERR_HANDLE );

  // cause invalid return by mis-initialized namespace
  ns->_reverse = NULL;
  rc += TEST( dbrRemove_request( ns, rctx ), DBR_ERR_INVALID );
  ns->_reverse = dbrCheckCreateMainCTX();

  // change the tag to break the remove function
  DBR_Tag_t tmptag = rctx->_tag;
  rctx->_tag = DB_TAG_ERROR;

  rc += TEST( dbrRemove_request( ns, rctx ), DBR_ERR_TAGERROR );

  rctx->_tag = tmptag;
  rc += TEST( dbrInsert_request( ns, rctx ), tmptag );
  rc += TEST( dbrRemove_request( ns, rctx ), DBR_SUCCESS );

  rc += TEST( dbrMain_delete( ns->_reverse, ns ), 0 );

  LOG( DBG_INFO, stdout, "Remove request test: rc=%d\n", rc );
  return rc;
}

int Request_post_test()
{
  int rc = 0;

  dbrRequestContext_t *rctx = NULL;
  dbBE_sge_t sge;
  sge._data = NULL;
  sge.iov_len = 0;

  dbrName_space_t *ns = dbrMain_create_local( "TestNameSpace" );

  // trying to post nullptr rctx
  rc += TEST( dbrPost_request( rctx ), NULL );

  rctx = dbrCreate_request_ctx( DBBE_OPCODE_GET, ns, DBR_GROUP_LIST_EMPTY, NULL, DBR_GROUP_LIST_EMPTY, 0, NULL, NULL, NULL, NULL, dbrTag_get( ns->_reverse ) );

  // trying to post a request that hasn't been inserted
  rc += TEST( dbrPost_request( rctx ), NULL );

  // cause invalid return by mis-initialized namespace
  ns->_reverse = NULL;
  rc += TEST( dbrRemove_request( ns, rctx ), DBR_ERR_INVALID );
  ns->_reverse = dbrCheckCreateMainCTX();

  // change the tag to break the post function
  DBR_Tag_t tmptag = rctx->_tag;
  rctx->_tag = DB_TAG_ERROR;

  rc += TEST( dbrPost_request( rctx ), NULL );

  rctx->_tag = tmptag;
  rc += TEST( dbrInsert_request( ns, rctx ), tmptag );
  rctx->_cpl._status = DBR_ERR_INPROGRESS;
  rc += TEST( dbrPost_request( rctx ), NULL );

  rc += TEST( dbrMain_delete( ns->_reverse, ns ), 0 );

  LOG( DBG_INFO, stdout, "Remove request test: rc=%d\n", rc );
  return rc;
}

int main( int argc, char ** argv )
{
  int rc = 0;

  rc += Request_create_test();
  rc += Request_insert_test();
  rc += Request_remove_test();
  rc += Request_post_test();

  printf( "Test exiting with rc=%d\n", rc );
  return rc;
}

