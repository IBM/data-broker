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
#include "common/completion.h"

int test_serialize()
{
  int rc = 0;

  char *rdata = generateLongMsg( 150 );
  char *data = (char*)malloc( 1000 );
  char *user = (char*)calloc( 20, sizeof( char ) );
  rc += TEST( snprintf( user, 20, "%p\n", user ), (ssize_t)strnlen( user, 20 ) );

  // setup an SGE with response data
  dbBE_sge_t sge[3];
  sge[0].iov_base = rdata; sge[0].iov_len = 10;
  sge[1].iov_base = &rdata[5]; sge[1].iov_len = 20;
  sge[2].iov_base = &rdata[10]; sge[2].iov_len = 30;

  dbBE_Completion_t comp;
  comp._status = DBR_SUCCESS;
  comp._rc = 0;
  comp._next = NULL;
  comp._user = user;


  // using READ for fail-tests to include checks/handling of SGE parts
  rc += TEST( -EINVAL, dbBE_Completion_serialize( DBBE_OPCODE_MAX, NULL, NULL, 0, NULL, 0 ) );
  rc += TEST( -EINVAL, dbBE_Completion_serialize( DBBE_OPCODE_READ, NULL, NULL, 0, NULL, 0 ) );
  rc += TEST( -EINVAL, dbBE_Completion_serialize( DBBE_OPCODE_READ, &comp, NULL, 0, NULL, 0 ) );
  rc += TEST( -EINVAL, dbBE_Completion_serialize( DBBE_OPCODE_READ, &comp, sge, 0, NULL, 0 ) );
  rc += TEST( -EINVAL, dbBE_Completion_serialize( DBBE_OPCODE_READ, &comp, sge, 3, NULL, 0 ) );
  rc += TEST( -EINVAL, dbBE_Completion_serialize( DBBE_OPCODE_READ, &comp, sge, 3, data, 0 ) );

  rc += TEST( -EINVAL, dbBE_Completion_serialize( DBBE_OPCODE_MAX, &comp, sge, 3, data, 150 ) );
  rc += TEST( -EINVAL, dbBE_Completion_serialize( DBBE_OPCODE_READ, NULL, sge, 3, data, 150 ));
  rc += TEST( -EINVAL, dbBE_Completion_serialize( DBBE_OPCODE_READ, &comp, NULL, 3, data, 150 ));
  rc += TEST( -EINVAL, dbBE_Completion_serialize( DBBE_OPCODE_READ, &comp, sge, 0, data, 150 ));
  rc += TEST( -EINVAL, dbBE_Completion_serialize( DBBE_OPCODE_READ, &comp, sge, 3, NULL, 150 ));
  rc += TEST( -EINVAL, dbBE_Completion_serialize( DBBE_OPCODE_READ, &comp, sge, 3, data, 0 ));

  rc += TEST( -ENOSPC, dbBE_Completion_serialize( DBBE_OPCODE_READ, &comp, sge, 3, data, 12 ) ); // too small for base info
  rc += TEST( -ENOSPC, dbBE_Completion_serialize( DBBE_OPCODE_READ, &comp, sge, 3, data, 25 ) ); // too small for additional SGE data
  rc += TEST( -ENOSPC, dbBE_Completion_serialize( DBBE_OPCODE_READ, &comp, sge, 3, data, 50 ) ); // too small for additional SGE value data

  rc += TEST( dbBE_Completion_serialize( DBBE_OPCODE_READ, &comp, sge, 3, data, 150 ), (ssize_t)strnlen( data, 1000 ) );
  rc += TEST( strncmp( data, "3\n0\n", 4 ), 0 ); // find READ and rc=0
  rc += TEST_NOT( strstr( data, user ), NULL ); // need to find the user ptr
  rc += TEST_NOT( strstr( data, "60\n3\n10\n20\n30\n"), NULL ); // need to find the SGE

  rc += TEST( dbBE_Completion_serialize( DBBE_OPCODE_PUT, &comp, sge, 3, data, 150 ), (ssize_t)strnlen( data, 1000 ) );
  rc += TEST( strncmp( data, "1\n0\n", 4 ), 0 ); // find PUT and rc=0
  rc += TEST( strstr( data, "60\n3\n"), NULL ); // must not find the SGE


  free( user );
  free( data );
  free( rdata );
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
  data = build_data( data, "2\n0\n0\n(nil)\n(nil)\n60\n3\n10\n20\n30\nabcdeabcdedefghijklmdefghijklmklmnopqrstuvxyzklmnopqrstuvxyz\n" );
  dbBE_Completion_t *comp = NULL;
  dbBE_sge_t *sge = NULL;
  int sge_count;

  rc += TEST( -EINVAL, dbBE_Completion_deserialize( NULL, 150, &comp, &sge, &sge_count ) );
  rc += TEST( -EINVAL, dbBE_Completion_deserialize( data, 0, &comp, &sge, &sge_count ) );
  rc += TEST( -EINVAL, dbBE_Completion_deserialize( data, 150, NULL, &sge, &sge_count ) );
  rc += TEST( -EINVAL, dbBE_Completion_deserialize( data, 150, &comp, NULL, &sge_count ) );
  rc += TEST( -EINVAL, dbBE_Completion_deserialize( data, 150, &comp, &sge, 0 ) );

  rc += TEST( -EAGAIN, dbBE_Completion_deserialize( data, 1, &comp, &sge, &sge_count ) );
  rc += TEST( -EAGAIN, dbBE_Completion_deserialize( data, 4, &comp, &sge, &sge_count ) );
  rc += TEST( -EAGAIN, dbBE_Completion_deserialize( data, 10, &comp, &sge, &sge_count ) );
  rc += TEST( -EAGAIN, dbBE_Completion_deserialize( data, 18, &comp, &sge, &sge_count ) );
  rc += TEST( -EAGAIN, dbBE_Completion_deserialize( data, 38, &comp, &sge, &sge_count ) );

  sge = NULL; sge_count = 0;
  ssize_t slen = strlen( data );
  rc += TEST_RC( dbBE_Completion_deserialize( data, strlen( data ), &comp, &sge, &sge_count ), (ssize_t)strlen( data ) + 1, slen );
  rc += TEST( comp->_status, DBR_SUCCESS );
  rc += TEST( comp->_rc, 0 );
  rc += TEST( sge_count, 3 );
  rc += TEST( strncmp( sge[0].iov_base, "abcdeabcdedefghijklmdefghijklmklmnopqrstuvxyzklmnopqrstuvxyz", 61 ), 0 );
  rc += TEST( strncmp( sge[1].iov_base, "defghijklmdefghijklmklmnopqrstuvxyzklmnopqrstuvxyz", 51 ), 0 );
  rc += TEST( strncmp( sge[2].iov_base, "klmnopqrstuvxyzklmnopqrstuvxyz", 31 ), 0 );
  rc += TEST_NOT( sge, NULL );
  rc += TEST( dbBE_SGE_get_len( sge, sge_count ), 60 );
  free( sge );
  free( comp );

  sge = NULL; sge_count = 0;
  data = build_data( data, "1\n2\n3057\n0x35450673\n(nil)\n6\n3\n1\n2\n3\naxynmo\n" );
  rc += TEST( dbBE_Completion_deserialize( data, strlen( data ), &comp, &sge, &sge_count ), 26 );
  rc += TEST( comp->_status, DBR_ERR_INVALID );
  rc += TEST( comp->_rc, 3057 );
  rc += TEST( comp->_user, (void*)0x35450673ull );
  rc += TEST( sge, NULL );
  rc += TEST( sge_count, 0 );

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
