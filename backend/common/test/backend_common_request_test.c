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

  req->_sge_count = 1; // EBADMSG because bad SGE entries
  req->_sge[0].iov_base = NULL;
  req->_sge[0].iov_len = 1;
  rc += TEST( -EBADMSG, dbBE_Request_serialize( req, data, 1000 ) );

  req->_sge[0].iov_base = partdata;
  req->_sge[0].iov_len = 0;
  rc += TEST( -EBADMSG, dbBE_Request_serialize( req, data, 1000 ) );

  req->_sge[0].iov_base = putdata;
  req->_sge[0].iov_len = 4;
  rc += TEST( dbBE_Request_serialize( req, data, 1000 ), (ssize_t)strnlen( data, 1000 ) );
  rc += TEST_NOT( strstr( data, partdata ), NULL );
  rc += TEST_NOT( strstr( data, "4\n1\n4\n"), NULL );

  req->_sge_count = 2; // EBADMSG because bad SGEs
  req->_sge[1].iov_base = NULL;
  req->_sge[1].iov_len = 1;
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

  rc += TEST( -ENOSPC, dbBE_Request_serialize( req, data, 25 ) ); // too short for req header
  rc += TEST( -ENOSPC, dbBE_Request_serialize( req, data, 50 ) ); // too short for SGE data

  req->_opcode = DBBE_OPCODE_GET;
  rc += TEST( dbBE_Request_serialize( req, data, 1000 ), (ssize_t)strnlen( data, 1000 ) );
  rc += TEST( strstr( data, partdata ), NULL ); // get mustdn't contain the SGE values
  rc += TEST_NOT( strstr( data, "4\n1\n4\n"), NULL ); // contains the SGE header

  req->_opcode = DBBE_OPCODE_READ;
  rc += TEST( dbBE_Request_serialize( req, data, 1000 ), (ssize_t)strnlen( data, 1000 ) );
  rc += TEST( strstr( data, partdata ), NULL ); // read mustn't contain the SGE values
  rc += TEST_NOT( strstr( data, "4\n1\n4\n"), NULL ); // contains the SGE header

  req->_opcode = DBBE_OPCODE_MOVE;
  req->_sge_count = 2;
  req->_sge[0].iov_base = (void*)0x0123456789; // dest hdl
  req->_sge[0].iov_len = 0;
  req->_sge[1].iov_base = NULL;  // dest group
  req->_sge[1].iov_len = 0;
  rc += TEST( dbBE_Request_serialize( req, data, 1000 ), (ssize_t)strnlen( data, 1000 ) );
  rc += TEST_NOT( strstr( data, "0x123456789\n(nil)\n"), NULL );  // needs to contain destination hdl/group

  rc += TEST( -ENOSPC, dbBE_Request_serialize( req, data, 50 ) );
  rc += TEST( -ENOSPC, dbBE_Request_serialize( req, data, 65 ) );
  rc += TEST( -ENOSPC, dbBE_Request_serialize( req, data, 66 ) );


  req->_opcode = DBBE_OPCODE_REMOVE;
  req->_sge_count = 0;
  rc += TEST( dbBE_Request_serialize( req, data, 1000 ), (ssize_t)strnlen( data, 1000 ) );

  req->_opcode = DBBE_OPCODE_CANCEL;
  rc += TEST( dbBE_Request_serialize( req, data, 1000 ), (ssize_t)strnlen( data, 1000 ) );

  req->_opcode = DBBE_OPCODE_CANCEL;
  rc += TEST( dbBE_Request_serialize( req, data, 1000 ), (ssize_t)strnlen( data, 1000 ) );


  req->_opcode = DBBE_OPCODE_NSQUERY;
  req->_sge_count = 1;
  req->_sge[0].iov_base = data; // is ignored, but set for testing here
  req->_sge[0].iov_len = 100;
  rc += TEST( dbBE_Request_serialize( req, data, 1000 ), (ssize_t)strnlen( data, 1000 ) );
  rc += TEST( strstr( data, partdata ), NULL ); // nsquery must only contain the sge header
  rc += TEST_NOT( strstr( data, "100\n1\n100\n"), NULL ); // contains the SGE header

  req->_opcode = DBBE_OPCODE_DIRECTORY;
  req->_sge_count = 1;
  req->_sge[0].iov_base = partdata;
  req->_sge[0].iov_len = 100;
  rc += TEST( dbBE_Request_serialize( req, data, 1000 ), (ssize_t)strnlen( data, 1000 ) );
  rc += TEST_NOT( strstr( data, "100\n1\n100\n" ), NULL ); // must contain SGE-header with length limit but no actual ptr

  req->_opcode = DBBE_OPCODE_NSCREATE;
  req->_sge_count = 1;
  req->_sge[0].iov_base = NULL;
  req->_sge[0].iov_len = 0;
  rc += TEST( dbBE_Request_serialize( req, data, 1000), (ssize_t)strnlen( data, 1000 ) );
  rc += TEST_NOT( strstr( data, "0\n1\n-1\n\n"), NULL ); // must contain serialized NULL ptr in SGE

  req->_opcode = DBBE_OPCODE_NSATTACH;
  req->_sge_count = 0;
  req->_sge[0].iov_base = NULL;
  req->_sge[0].iov_len = 0;
  rc += TEST( dbBE_Request_serialize( req, data, 1000), (ssize_t)strnlen( data, 1000 ) );

  req->_opcode = DBBE_OPCODE_NSDETACH;
  rc += TEST( dbBE_Request_serialize( req, data, 1000), (ssize_t)strnlen( data, 1000 ) );
  req->_opcode = DBBE_OPCODE_NSDELETE;
  rc += TEST( dbBE_Request_serialize( req, data, 1000), (ssize_t)strnlen( data, 1000 ) );


  req->_opcode = DBBE_OPCODE_ITERATOR;
  req->_sge_count = 1;
  req->_sge[0].iov_base = malloc(DBR_MAX_KEY_LEN);
  req->_sge[0].iov_len = DBR_MAX_KEY_LEN;
  void *it = malloc(10);
  req->_key = (DBR_Tuple_name_t)it;
  rc += TEST( dbBE_Request_serialize( req, data, 1000 ), (ssize_t)strnlen( data, 1000 ) );
  rc += TEST_NOT( strstr( data, "1023\n1\n1023\n" ), NULL ); // iterator must contain an SGE with DBR_MAX_KEY_LEN single SGE
  free( req->_sge[0].iov_base );

  free( it );
  free( req->_user );
  free( req->_ns_hdl );
  free( req );
  free( partdata );
  free( putdata );
  free( data );
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

  // a successfully serialized PUT
  data = build_data( data, "1\n(nil)\n(nil)\n(nil)\n(nil)\n5\n1\n0\n");
  rc += TEST( dbBE_Request_deserialize( data, strlen( data ), &req ), -EAGAIN );
  data = build_data( data, "1\n(nil)\n(nil)\n(nil)\n(nil)\n5\n1\n0\nHello*\n");
  rc += TEST( dbBE_Request_deserialize( data, strlen( data ), &req ), -EAGAIN );
  data = build_data( data, "1\n0x0123456789\n(nil)\n(nil)\n(nil)\n5\n1\n0\nHello*\n6\n1\n6\nWorld!\n");
  rc += TEST( dbBE_Request_deserialize( data, strlen( data ), &req ), (ssize_t)strlen( data ) );
  rc += TEST( req->_opcode, DBBE_OPCODE_PUT );
  rc += TEST( req->_ns_hdl, (dbBE_NS_Handle_t)0x0123456789ull );
  rc += TEST( req->_user, NULL );
  rc += TEST( req->_sge_count, 1 );
  rc += TEST( strncmp( req->_key, "Hello", 6) , 0 );
  rc += TEST( req->_sge[0].iov_len, 6 );
  rc += TEST( strncmp( (char*)req->_sge[0].iov_base, "World!", req->_sge[0].iov_len ), 0 );
  rc += TEST( dbBE_Request_free( req ), 0 );

  // serialized GET
  data = build_data( data, "2\n0x0123456789\n(nil)\n(nil)\n(nil)\n5\n1\n0\nHello*\n6\n1\n6\n");
  rc += TEST( dbBE_Request_deserialize( data, strlen( data ), &req ), (ssize_t)strlen( data ) );
  rc += TEST( req->_opcode, DBBE_OPCODE_GET );
  rc += TEST( req->_ns_hdl, (dbBE_NS_Handle_t)0x0123456789ull );
  rc += TEST( req->_user, NULL );
  rc += TEST( req->_sge_count, 1 );
  rc += TEST( strncmp( req->_key, "Hello", 6) , 0 );
  rc += TEST( req->_sge[0].iov_len, 6 );
  rc += TEST( req->_sge[0].iov_base, NULL );
  rc += TEST( dbBE_Request_free( req ), 0 );

  // serialized READ
  data = build_data( data, "3\n0x0123456789\n(nil)\n(nil)\n(nil)\n5\n1\n0\nHello*\n6\n1");
  rc += TEST( dbBE_Request_deserialize( data, strlen( data ), &req ), -EAGAIN );
  data = build_data( data, "3\n0x0123456789\n(nil)\n(nil)\n(nil)\n5\n1\n0\nHello*\n6\n1\n6\n");
  rc += TEST( dbBE_Request_deserialize( data, strlen( data ), &req ), (ssize_t)strlen( data ) );
  rc += TEST( req->_opcode, DBBE_OPCODE_READ );
  rc += TEST( req->_ns_hdl, (dbBE_NS_Handle_t)0x0123456789ull );
  rc += TEST( req->_user, NULL );
  rc += TEST( req->_sge_count, 1 );
  rc += TEST( strncmp( req->_key, "Hello", 6) , 0 );
  rc += TEST( req->_sge[0].iov_len, 6 );
  rc += TEST( req->_sge[0].iov_base, NULL );
  rc += TEST( dbBE_Request_free( req ), 0 );

  // serialized MOVE
  data = build_data( data, "4\n0x0123456789\n(nil)\n(nil)\n(nil)\n5\n1\n0\nHello*\n");
  rc += TEST( dbBE_Request_deserialize( data, strlen( data ), &req ), -EAGAIN );
  data = build_data( data, "4\n0x0123456789\n(nil)\n(nil)\n(nil)\n5\n1\n0\nHello*\n0x0987654321\n(ni");
  rc += TEST( dbBE_Request_deserialize( data, strlen( data ), &req ), -EAGAIN );
  data = build_data( data, "4\n0x0123456789\n(nil)\n(nil)\n(nil)\n5\n1\n0\nHello*\n0x0987654321\n(nil)\n");
  rc += TEST( dbBE_Request_deserialize( data, strlen( data ), &req ), (ssize_t)strlen( data ) );
  rc += TEST( req->_opcode, DBBE_OPCODE_MOVE );
  rc += TEST( req->_ns_hdl, (dbBE_NS_Handle_t)0x0123456789ull );
  rc += TEST( req->_user, NULL );
  rc += TEST( req->_sge_count, 2 );
  rc += TEST( strncmp( req->_key, "Hello", 6) , 0 );
  rc += TEST( req->_sge[0].iov_len, 0 );
  rc += TEST( req->_sge[0].iov_base, (void*)0x0987654321ull );
  rc += TEST( req->_sge[1].iov_len, 0 );
  rc += TEST( req->_sge[1].iov_base, NULL );
  rc += TEST( dbBE_Request_free( req ), 0 );

  // serialized REMOVE
  data = build_data( data, "5\n0x0123456789\n(nil)\n(nil)\n(nil)\n5\n1\n0\nHello*\n");
  rc += TEST( dbBE_Request_deserialize( data, strlen( data ), &req ), (ssize_t)strlen( data ) );
  rc += TEST( req->_opcode, DBBE_OPCODE_REMOVE );
  rc += TEST( req->_ns_hdl, (dbBE_NS_Handle_t)0x0123456789ull );
  rc += TEST( req->_user, NULL );
  rc += TEST( req->_sge_count, 0 );
  rc += TEST( strncmp( req->_key, "Hello", 6) , 0 );
  rc += TEST( dbBE_Request_free( req ), 0 );

  // serialized CANCEL
  data = build_data( data, "6\n0x0123456789\n(nil)\n(nil)\n(nil)\n3\n0\n0\n123\n");
  rc += TEST( dbBE_Request_deserialize( data, strlen( data ), &req ), (ssize_t)strlen( data ) );
  rc += TEST( req->_opcode, DBBE_OPCODE_CANCEL );
  rc += TEST( req->_ns_hdl, (dbBE_NS_Handle_t)0x0123456789ull );
  rc += TEST( req->_user, NULL );
  rc += TEST( req->_sge_count, 0 );
  rc += TEST( strncmp( req->_key, "123", 3) , 0 );
  rc += TEST( dbBE_Request_free( req ), 0 );

  // serialized DIRECTORY
  data = build_data( data, "7\n0x0123456789\n(nil)\n(nil)\n(nil)\n5\n1\n0\nHello*\n6\n1\n");
  rc += TEST( dbBE_Request_deserialize( data, strlen( data ), &req ), -EAGAIN );
  data = build_data( data, "7\n0x0123456789\n(nil)\n(nil)\n(nil)\n5\n1\n0\nHello*\n60\n1\n60\n");
  rc += TEST( dbBE_Request_deserialize( data, strlen( data ), &req ), (ssize_t)strlen( data ) );
  rc += TEST( req->_opcode, DBBE_OPCODE_DIRECTORY );
  rc += TEST( req->_ns_hdl, (dbBE_NS_Handle_t)0x0123456789ull );
  rc += TEST( req->_user, NULL );
  rc += TEST( req->_sge_count, 1 );
  rc += TEST( strncmp( req->_key, "Hello", 6) , 0 );
  rc += TEST( req->_sge[0].iov_len, 60 );
  rc += TEST( req->_sge[0].iov_base, NULL );
  rc += TEST( dbBE_Request_free( req ), 0 );

  // serialized NSCREATE
  data = build_data( data, "8\n(nil)\n(nil)\n(nil)\n(nil)\n9\n0\n0\nNamespa");
  rc += TEST( dbBE_Request_deserialize( data, strlen( data ), &req ), -EAGAIN );
  data = build_data( data, "8\n(nil)\n(nil)\n(nil)\n(nil)\n9\n0\n0\nNamespace\n");
  rc += TEST( dbBE_Request_deserialize( data, strlen( data ), &req ), -EAGAIN );
  data = build_data( data, "8\n(nil)\n(nil)\n(nil)\n(nil)\n9\n0\n0\nNamespace\n0\n1\n-1\n\n");
  rc += TEST( dbBE_Request_deserialize( data, strlen( data ), &req ), (ssize_t)strlen( data ) );
  rc += TEST( req->_opcode, DBBE_OPCODE_NSCREATE );
  rc += TEST( req->_ns_hdl, NULL );
  rc += TEST( req->_user, NULL );
  rc += TEST( req->_sge_count, 1 );
  rc += TEST( req->_sge[0].iov_base, NULL );
  rc += TEST( req->_sge[0].iov_len, 0 );
  rc += TEST( strncmp( req->_key, "Namespace", 9) , 0 );
  rc += TEST( dbBE_Request_free( req ), 0 );

  // serialized NSATTACH
  data = build_data( data, "9\n(nil)\n(nil)\n(nil)\n(nil)\n9\n0\n0");
  rc += TEST( dbBE_Request_deserialize( data, strlen( data ), &req ), -EAGAIN );
  data = build_data( data, "9\n(nil)\n(nil)\n(nil)\n(nil)\n9\n0\n0\nNamespace\n");
  rc += TEST( dbBE_Request_deserialize( data, strlen( data ), &req ), (ssize_t)strlen( data ) );
  rc += TEST( req->_opcode, DBBE_OPCODE_NSATTACH );
  rc += TEST( req->_ns_hdl, NULL );
  rc += TEST( req->_user, NULL );
  rc += TEST( req->_sge_count, 0 );
  rc += TEST( strncmp( req->_key, "Namespace", 9) , 0 );
  rc += TEST( dbBE_Request_free( req ), 0 );

  // serialized NSDETACH
  data = build_data( data, "10\n0x0123456789\n(nil)\n(nil)\n(nil)\n0\n0\n0\n");
  rc += TEST( dbBE_Request_deserialize( data, strlen( data ), &req ), (ssize_t)strlen( data ) );  // single trailing \n still valid if key and match are 0-length
  rc += TEST( dbBE_Request_free( req ), 0 );

  data = build_data( data, "10\n0x0123456789\n(nil)\n(nil)\n(nil)\n0\n0\n0\n\n");
  rc += TEST( dbBE_Request_deserialize( data, strlen( data ), &req ), (ssize_t)strlen( data ) );
  rc += TEST( req->_opcode, DBBE_OPCODE_NSDETACH );
  rc += TEST( req->_ns_hdl, (dbBE_NS_Handle_t)0x0123456789ull );
  rc += TEST( req->_user, NULL );
  rc += TEST( req->_sge_count, 0 );
  rc += TEST( req->_key, NULL );
  rc += TEST( dbBE_Request_free( req ), 0 );

  // serialized NSDELETE
  data = build_data( data, "11\n0x0123456789\n(nil)\n(nil)\n(nil)\n0\n0\n0");
  rc += TEST( dbBE_Request_deserialize( data, strlen( data ), &req ), -EAGAIN );  // single trailing \n still valid if key and match are 0-length
  data = build_data( data, "11\n0x0123456789\n(nil)\n(nil)\n(nil)\n0\n0\n0\n\n");
  rc += TEST( dbBE_Request_deserialize( data, strlen( data ), &req ), (ssize_t)strlen( data ) );
  rc += TEST( req->_opcode, DBBE_OPCODE_NSDELETE );
  rc += TEST( req->_ns_hdl, (dbBE_NS_Handle_t)0x0123456789ull );
  rc += TEST( req->_user, NULL );
  rc += TEST( req->_sge_count, 0 );
  rc += TEST( req->_key, NULL );
  rc += TEST( dbBE_Request_free( req ), 0 );

  // serialized NSQUERY
  data = build_data( data, "12\n0x0123456789\n(nil)\n(nil)\n(nil)\n5\n1\n0\n\n6\n1\n");
  rc += TEST( dbBE_Request_deserialize( data, strlen( data ), &req ), -EAGAIN );
  data = build_data( data, "12\n0x0123456789\n(nil)\n(nil)\n(nil)\n0\n0\n0\n\n60\n1\n60\n");
  rc += TEST( dbBE_Request_deserialize( data, strlen( data ), &req ), (ssize_t)strlen( data ) );
  rc += TEST( req->_opcode, DBBE_OPCODE_NSQUERY );
  rc += TEST( req->_ns_hdl, (dbBE_NS_Handle_t)0x0123456789ull );
  rc += TEST( req->_user, NULL );
  rc += TEST( req->_sge_count, 1 );
  rc += TEST( req->_key, NULL );
  rc += TEST( req->_sge[0].iov_len, 60 );
  rc += TEST( req->_sge[0].iov_base, NULL );
  rc += TEST( dbBE_Request_free( req ), 0 );

  data = build_data( data, "13\n0x0123456789\n(nil)\n(nil)\n(nil)\n14\n5\n0\n0x67447AFB3454bla.*\n1024\n1\n1024\n");
  rc += TEST( dbBE_Request_deserialize( data, strlen( data ), &req ), -ENOSYS );
  data = build_data( data, "14\n0x0123456789\n(nil)\n(nil)\n(nil)\n14\n5\n0\n0x67447AFB3454bla.*\n1024\n1\n1024\n");
  rc += TEST( dbBE_Request_deserialize( data, strlen( data ), &req ), -ENOSYS );

  // serialized ITERATOR
  data = build_data( data, "15\n0x0123456789\n(nil)\n(nil)\n(nil)\n14\n5\n0\n0x67447AFB3454bla.*\n1024\n1\n");
  rc += TEST( dbBE_Request_deserialize( data, strlen( data ), &req ), -EAGAIN );
  data = build_data( data, "15\n0x0123456789\n(nil)\n(nil)\n(nil)\n14\n0\n0\n0x67447AFB3454bla.*\n1024\n1\n1024\n"); // wrong match length
  rc += TEST( dbBE_Request_deserialize( data, strlen( data ), &req ), -EBADMSG );
  data = build_data( data, "15\n0x0123456789\n(nil)\n(nil)\n(nil)\n14\n5\n0\n0x67447AFB3454bla.*\n1024\n1\n1024\n");
  rc += TEST( dbBE_Request_deserialize( data, strlen( data ), &req ), (ssize_t)strlen( data ) );
  rc += TEST( req->_opcode, DBBE_OPCODE_ITERATOR );
  rc += TEST( req->_ns_hdl, (dbBE_NS_Handle_t)0x0123456789ull );
  rc += TEST( req->_user, NULL );
  rc += TEST( req->_sge_count, 1 );
  rc += TEST( req->_key, (DBR_Tuple_name_t)0x67447AFB3454ull );
  rc += TEST( strncmp( req->_match, "bla.*", 5 ), 0 );
  rc += TEST( req->_sge[0].iov_len, 1024 );
  rc += TEST( req->_sge[0].iov_base, NULL );
  req->_key = NULL; // key == iterator ptr was a fake, reset to NULL to avoid trouble in request_free()
  rc += TEST( dbBE_Request_free( req ), 0 );


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
