/*
 * Copyright © 2020 IBM Corporation
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

#include "test_utils.h"
#include "common/dbbe_api.h"


int test_serialize()
{
  int rc = 0;

  dbBE_Request_t *req = NULL;
  char *data = malloc(10000);
  char *putdata = generateLongMsg(64);
  char *partdata = malloc(10);
  strncpy(partdata, putdata, 4 );
  partdata[4] = '\0';
  size_t space = 10000;

  rc += TEST( -EINVAL, dbBE_Request_serialize( req, data, space ) );

  req = dbBE_Request_allocate(10);
  rc += TEST( -EINVAL, dbBE_Request_serialize( req, NULL, space ) );
  rc += TEST( -EINVAL, dbBE_Request_serialize( req, NULL, 0 ) );
  rc += TEST( -EINVAL, dbBE_Request_serialize( req, data, 0 ) );
  rc += TEST( -EINVAL, dbBE_Request_serialize( req, data, 5 ) );

  memset( req, 0, sizeof( dbBE_Request_t ) + 10 * sizeof(dbBE_sge_t) );
  rc += TEST( -EINVAL, dbBE_Request_serialize( req, data, 1000 ) );  // EINVAL because wrong opcode

  req->_opcode = DBBE_OPCODE_PUT; // EINVAL because uninitialized SGEs
  rc += TEST( -EINVAL, dbBE_Request_serialize( req, data, 1000 ) );

  req->_sge_count = 1; // EBADMSG because NULL SGEs
  rc += TEST( -EBADMSG, dbBE_Request_serialize( req, data, 1000 ) );

  req->_sge[0].iov_base = putdata;
  req->_sge[0].iov_len = 4;
  rc += TEST( dbBE_Request_serialize( req, data, 1000 ), (ssize_t)strnlen( data, 1000 ) );
  rc += TEST_NOT( strstr( data, partdata ), NULL );

  req->_sge_count = 2; // EBADMSG because NULL SGEs
  rc += TEST( -EBADMSG, dbBE_Request_serialize( req, data, 1000 ) );

  req->_sge_count = 1;
  req->_ns_hdl = malloc(10); // dummy namespace handle
  rc += TEST( dbBE_Request_serialize( req, data, 1000 ), (ssize_t)strnlen( data, 1000 ) );

  req->_user = malloc(10); // dummy user ptr
  rc += TEST( dbBE_Request_serialize( req, data, 1000 ), (ssize_t)strnlen( data, 1000 ) );

  req->_group = DBR_GROUP_EMPTY;
  rc += TEST( dbBE_Request_serialize( req, data, 1000 ), (ssize_t)strnlen( data, 1000 ) );

  req->_key = "HelloKey";
  rc += TEST( dbBE_Request_serialize( req, data, 1000 ), (ssize_t)strnlen( data, 1000 ) );
  rc += TEST_NOT( strstr( data, req->_key ), NULL );

  req->_match = "*";
  rc += TEST( dbBE_Request_serialize( req, data, 1000 ), (ssize_t)strnlen( data, 1000 ) );
  rc += TEST_NOT( strstr( data, "*" ), NULL );

  req->_opcode = DBBE_OPCODE_GET;
  rc += TEST( dbBE_Request_serialize( req, data, 1000 ), (ssize_t)strnlen( data, 1000 ) );
  rc += TEST( strstr( data, partdata ), NULL ); // get mustdn't contain the SGE values

  req->_opcode = DBBE_OPCODE_READ;
  rc += TEST( dbBE_Request_serialize( req, data, 1000 ), (ssize_t)strnlen( data, 1000 ) );
  rc += TEST( strstr( data, partdata ), NULL ); // read mustn't contain the SGE values

  free( req->_user );
  free( req->_ns_hdl );
  free( req );
  free( putdata );
  printf( "Serialize test exiting with rc=%d\n", rc );
  return rc;
}


int main( int argc, char *argv[] )
{
  int rc = 0;

  rc += test_serialize();

  printf( "Test exiting with rc=%d\n", rc );
  return rc;
}
