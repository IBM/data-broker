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

#include <stdio.h>
#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>
#endif
#include <string.h>
#include <stdlib.h>

#include "test_utils.h"
#include "../backend/common/dbbe_api.h"

int main( int argc, char ** argv )
{
  int rc = 0;

  dbBE_Handle_t BE = g_dbBE.initialize();

  rc += TEST_NOT( BE, NULL );

  TEST_BREAK( rc, "Backend initialization failed" );

  char buf[128];
  memset( buf, 0, 128 );
  unsigned sge_count = 2;
  dbBE_Request_t *req = (dbBE_Request_t*) calloc ( 1, sizeof(dbBE_Request_t) + sge_count * sizeof(dbBE_sge_t) );
  req->_key = "HELLO";
  req->_next = NULL;
  req->_ns_hdl = "KEYSPACE";
  req->_opcode = DBBE_OPCODE_PUT;
  req->_user = req;
  req->_sge_count = 1;
  req->_sge[0].iov_base = (void*)buf;
  req->_sge[0].iov_len = 5;
  sprintf( req->_sge[0].iov_base, "WORLD" );

  // put data in
  dbBE_Request_handle_t *rhandle = g_dbBE.post( BE, req, 1 );
  rc += TEST_NOT( rhandle, NULL );
  if( rhandle != NULL )
  {
    dbBE_Completion_t *comp = NULL;
    while( comp == NULL )
      comp = g_dbBE.test_any( BE );
    rc += TEST_NOT( comp, NULL );
    if( comp != NULL )
    {
      rc += TEST( comp->_user, req->_user );
      free( comp );
    }
  }

  TEST_LOG( rc, "PUT:");

  req->_key = "HELLO";
  req->_next = NULL;
  req->_ns_hdl = "KEYSPACE";
  req->_opcode = DBBE_OPCODE_READ;
  req->_user = req;
  req->_sge_count = 1;
  memset( buf, 0, 128 );
  req->_sge[0].iov_base = (void*)buf;
  req->_sge[0].iov_len = 128;

  // get data out
  rhandle = g_dbBE.post( BE, req, 1 );
  rc += TEST_NOT( rhandle, NULL );
  if( rhandle != NULL )
  {
    dbBE_Completion_t *comp = NULL;
    while( comp == NULL )
      comp = g_dbBE.test_any( BE );
    rc += TEST_NOT( comp, NULL );
    if( comp != NULL )
    {
      rc += TEST( comp->_user, req->_user );
      rc += TEST( strncmp( buf, "WORLD", 6 ), 0 );
      rc += TEST( comp->_rc, 5 );
      free( comp );
    }
  }
  TEST_LOG( rc, "READ:");


  req->_key = "HELLO";
  req->_next = NULL;
  req->_ns_hdl = "KEYSPACE";
  req->_opcode = DBBE_OPCODE_MOVE;
  req->_user = req;
  req->_sge_count = 2;
  memset( buf, 0, 128 );
  snprintf( buf, 128, "NEWSPACE" );
  req->_sge[0].iov_base = (void*)buf;
  req->_sge[0].iov_len = 8;
  req->_sge[1].iov_base = DBR_GROUP_EMPTY;
  req->_sge[1].iov_len = sizeof( DBR_GROUP_EMPTY );

  rhandle = g_dbBE.post( BE, req, 1 );
  rc += TEST_NOT( rhandle, NULL );
  if( rhandle != NULL )
  {
    dbBE_Completion_t *comp = NULL;
    while( comp == NULL )
      comp = g_dbBE.test_any( BE );
    rc += TEST_NOT( comp, NULL );
    if( comp != NULL )
    {
      rc += TEST( comp->_user, req->_user );
      rc += TEST( comp->_rc, DBR_SUCCESS );
      free( comp );
    }
  }
  TEST_LOG( rc, "MOVE" );


  req->_key = "HELLO";
  req->_next = NULL;
  req->_ns_hdl = "NEWSPACE";
  req->_opcode = DBBE_OPCODE_GET;
  req->_user = req;
  req->_sge_count = 1;
  memset( buf, 0, 128 );
  req->_sge[0].iov_base = (void*)buf;
  req->_sge[0].iov_len = 128;

  // get data out
  rhandle = g_dbBE.post( BE, req, 1 );
  rc += TEST_NOT( rhandle, NULL );
  if( rhandle != NULL )
  {
    dbBE_Completion_t *comp = NULL;
    while( comp == NULL )
      comp = g_dbBE.test_any( BE );
    rc += TEST_NOT( comp, NULL );
    if( comp != NULL )
    {
      rc += TEST( comp->_user, req->_user );
      rc += TEST( strncmp( buf, "WORLD", 6 ), 0 );
      rc += TEST( comp->_rc, 5 );
      free( comp );
    }
  }

  TEST_LOG( rc, "GET:");

  memset( buf, 0, 128 );
  req->_key = "HELLO";
  req->_next = NULL;
  req->_ns_hdl = "KEYSPACE";
  req->_opcode = DBBE_OPCODE_PUT;
  req->_user = req;
  req->_sge_count = 1;
  req->_sge[0].iov_base = (void*)buf;
  req->_sge[0].iov_len = 5;
  sprintf( req->_sge[0].iov_base, "WORLD" );

  // put data in
  rhandle = g_dbBE.post( BE, req, 1 );
  rc += TEST_NOT( rhandle, NULL );
  if( rhandle != NULL )
  {
    dbBE_Completion_t *comp = NULL;
    while( comp == NULL )
      comp = g_dbBE.test_any( BE );
    rc += TEST_NOT( comp, NULL );
    if( comp != NULL )
    {
      rc += TEST( comp->_user, req->_user );
      rc += TEST( comp->_status, DBR_SUCCESS );
      free( comp );
    }
  }

  TEST_LOG( rc, "PUT2:");


  req->_key = "HELLO";
  req->_next = NULL;
  req->_ns_hdl = "KEYSPACE";
  req->_opcode = DBBE_OPCODE_REMOVE;
  req->_user = req;
  req->_sge_count = 1;
  memset( buf, 0, 128 );
  req->_sge[0].iov_base = NULL;
  req->_sge[0].iov_len = 0;

  // get data out
  rhandle = g_dbBE.post( BE, req, 1 );
  rc += TEST_NOT( rhandle, NULL );
  if( rhandle != NULL )
  {
    dbBE_Completion_t *comp = NULL;
    while( comp == NULL )
      comp = g_dbBE.test_any( BE );
    rc += TEST_NOT( comp, NULL );
    if( comp != NULL )
    {
      rc += TEST( comp->_user, req->_user );
      rc += TEST( comp->_status, DBR_SUCCESS );
      free( comp );
    }
  }

  TEST_LOG( rc, "REMOVE:");

  // create a namespace
  req->_key = "KEYSPACE";
  req->_next = NULL;
  req->_ns_hdl = NULL;
  req->_opcode = DBBE_OPCODE_NSCREATE;
  req->_user = req;
  req->_sge_count = 1;
  memset( buf, 0, 128 );
  snprintf( buf, 128, "users" );
  req->_sge[0].iov_base = (void*)buf;
  req->_sge[0].iov_len = 5;

  // get data out
  rhandle = g_dbBE.post( BE, req, 1 );
  rc += TEST_NOT( rhandle, NULL );
  if( rhandle != NULL )
  {
    dbBE_Completion_t *comp = NULL;
    while( comp == NULL )
      comp = g_dbBE.test_any( BE );
    rc += TEST_NOT( comp, NULL );
    if( comp != NULL )
    {
      rc += TEST( comp->_status, DBR_SUCCESS );
      rc += TEST( comp->_user, req->_user );
      rc += TEST_NOT( (void*)comp->_rc, NULL );
      rc += TEST( strcmp( (char*)comp->_rc, req->_key ), 0 );
      free( comp );
    }
  }

  free(req);

  sge_count = 0;
  req = (dbBE_Request_t*) calloc ( 1, sizeof(dbBE_Request_t) );

  TEST_LOG( rc, "NSCREATE:");


  // attach to namespace
  req->_key = "KEYSPACE";
  req->_next = NULL;
  req->_ns_hdl = NULL;
  req->_opcode = DBBE_OPCODE_NSATTACH;
  req->_user = req;
  req->_sge_count = 0;

  // get data out
  rhandle = g_dbBE.post( BE, req, 1 );
  rc += TEST_NOT( rhandle, NULL );
  if( rhandle != NULL )
  {
    dbBE_Completion_t *comp = NULL;
    while( comp == NULL )
      comp = g_dbBE.test_any( BE );
    rc += TEST_NOT( comp, NULL );
    if( comp != NULL )
    {
      rc += TEST( comp->_status, DBR_SUCCESS );
      rc += TEST( comp->_user, req->_user );
      rc += TEST_NOT( (void*)comp->_rc, NULL );
      rc += TEST( strcmp( (char*)comp->_rc, req->_key ), 0 );
      free( comp );
    }
  }

  TEST_LOG( rc, "NSATTACH:");


  // attach to namespace
  req->_key = NULL;
  req->_next = NULL;
  req->_ns_hdl = "KEYSPACE";
  req->_opcode = DBBE_OPCODE_NSDETACH;
  req->_user = req;
  req->_sge_count = 0;

  // get data out
  rhandle = g_dbBE.post( BE, req, 1 );
  rc += TEST_NOT( rhandle, NULL );
  if( rhandle != NULL )
  {
    dbBE_Completion_t *comp = NULL;
    while( comp == NULL )
      comp = g_dbBE.test_any( BE );
    rc += TEST_NOT( comp, NULL );
    if( comp != NULL )
    {
      rc += TEST( comp->_status, DBR_SUCCESS );
      rc += TEST( comp->_user, req->_user );
      rc += TEST( comp->_rc, 0 );
      free( comp );
    }
  }

  TEST_LOG( rc, "DETACH:");


  // delete namespace
  req->_key = NULL;
  req->_next = NULL;
  req->_ns_hdl = "KEYSPACE";
  req->_opcode = DBBE_OPCODE_NSDELETE;
  req->_user = req;
  req->_sge_count = 0;

  // get data out
  rhandle = g_dbBE.post( BE, req, 1 );
  rc += TEST_NOT( rhandle, NULL );
  if( rhandle != NULL )
  {
    dbBE_Completion_t *comp = NULL;
    while( comp == NULL )
      comp = g_dbBE.test_any( BE );
    rc += TEST_NOT( comp, NULL );
    if( comp != NULL )
    {
      rc += TEST( comp->_status, DBR_SUCCESS );
      rc += TEST( comp->_user, req->_user );
      free( comp );
    }
  }

  // deletion of namespace requires subsequent detach cmd:
  req->_key = NULL;
  req->_next = NULL;
  req->_ns_hdl = "KEYSPACE";
  req->_opcode = DBBE_OPCODE_NSDETACH;
  req->_user = req;
  req->_sge_count = 0;

  // get data out
  rhandle = g_dbBE.post( BE, req, 1 );
  rc += TEST_NOT( rhandle, NULL );
  if( rhandle != NULL )
  {
    dbBE_Completion_t *comp = NULL;
    while( comp == NULL )
      comp = g_dbBE.test_any( BE );
    rc += TEST_NOT( comp, NULL );
    if( comp != NULL )
    {
      rc += TEST( comp->_status, DBR_SUCCESS );
      rc += TEST( comp->_user, req->_user );
      free( comp );
    }
  }



  TEST_LOG( rc, "DELETE:");

  g_dbBE.exit( BE );

  printf( "Test exiting with rc=%d\n", rc );

  free(req);

  return rc;
}

