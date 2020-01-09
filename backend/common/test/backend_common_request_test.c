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
  char *putdata = generateLongMsg(DBR_MAX_KEY_LEN);
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
  rc += TEST_NOT( strstr( data, "4\n1\n4\n"), NULL ); // contains the SGE header

  req->_opcode = DBBE_OPCODE_READ;
  rc += TEST( dbBE_Request_serialize( req, data, 1000 ), (ssize_t)strnlen( data, 1000 ) );
  rc += TEST( strstr( data, partdata ), NULL ); // read mustn't contain the SGE values
  rc += TEST_NOT( strstr( data, "4\n1\n4\n"), NULL ); // contains the SGE header

  req->_opcode = DBBE_OPCODE_NSQUERY;
  rc += TEST( dbBE_Request_serialize( req, data, 1000 ), (ssize_t)strnlen( data, 1000 ) );
  rc += TEST( strstr( data, partdata ), NULL ); // nsquery must only contain the sge header
  rc += TEST_NOT( strstr( data, "4\n1\n4\n"), NULL ); // contains the SGE header

  req->_opcode = DBBE_OPCODE_ITERATOR;
  req->_sge[0].iov_len = DBR_MAX_KEY_LEN;
  void *it = malloc(10);
  req->_key = (DBR_Tuple_name_t)it;
  rc += TEST( dbBE_Request_serialize( req, data, 1000 ), (ssize_t)strnlen( data, 1000 ) );
  rc += TEST( strstr( data, "1024\n\1\n\1024\n" ), NULL ); // iterator must contain an SGE with DBR_MAX_KEY_LEN single SGE


  free( it );
  free( req->_user );
  free( req->_ns_hdl );
  free( req );
  free( putdata );
  printf( "Serialize test exiting with rc=%d\n", rc );
  return rc;
}

char* build_data( char *data, const char *in )
{
  if( data != NULL )
    free( data );
  data = strdup( in );
  return data;
}

int test_deserialize()
{
  int rc = 0;

  char *data = NULL;
  data = build_data( data, "1\n(nil)\n(nil)\n" );
  dbBE_Request_t *req = NULL;

  rc += TEST( dbBE_Request_deserialize( NULL, 100, &req ), -EINVAL );
  rc += TEST( dbBE_Request_deserialize( "", 0, &req ), -EINVAL );
  rc += TEST( dbBE_Request_deserialize( "1\n", 8, NULL ), -EINVAL );

  rc += TEST( dbBE_Request_deserialize( data, strlen(data), NULL ), -EINVAL );

  // missing key+match (EAGAIN)
  data = build_data( data, "1\n(nil)\n(nil)\n(nil)\n(nil)\n5\n1\n0\n");
  rc += TEST( dbBE_Request_deserialize( data, strlen( data ), &req ), -EAGAIN );

  data = build_data( data, "1\n(nil)\n(nil)\n(nil)\n(nil)\n5\n1\n0\nHello*\n");
  rc += TEST( dbBE_Request_deserialize( data, strlen( data ), &req ), -EAGAIN );

  // a successfully serialized PUT
  data = build_data( data, "1\n0x0123456789\n(nil)\n(nil)\n(nil)\n5\n1\n0\nHello*\n6\n1\n6\nWorld!\n");
  rc += TEST( dbBE_Request_deserialize( data, strlen( data ), &req ), (ssize_t)strlen( data ) );
  rc += TEST( req->_opcode, DBBE_OPCODE_PUT );
  rc += TEST( req->_ns_hdl, (dbBE_NS_Handle_t)0x0123456789ull );
  rc += TEST( req->_user, NULL );
  rc += TEST( req->_sge_count, 1 );
  rc += TEST( strncmp( req->_key, "Hello", 6) , 0 );
  rc += TEST( req->_sge[0].iov_len, 6 );
  rc += TEST( strncmp( (char*)req->_sge[0].iov_base, "World!", req->_sge[0].iov_len ), 0 );

  free( data );
  printf( "Deserialize test exiting with rc=%d\n", rc );
  return rc;
}

int main( int argc, char *argv[] )
{
  int rc = 0;

  rc += test_serialize();
  rc += test_deserialize();

  printf( "Test exiting with rc=%d\n", rc );
  return rc;
}
