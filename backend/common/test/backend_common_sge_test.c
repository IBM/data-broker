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
#include "common/sge.h"

int main( int argc, char *argv[] )
{
  int rc = 0;
  int i;

  const size_t datalen = 16384;
  char *data = generateLongMsg( datalen );
  char *in_data, *serial;
  rc += TEST_NOT_RC( (char*)malloc( datalen * DBBE_SGE_MAX ), NULL, in_data );
  rc += TEST_NOT_RC( (char*)malloc( datalen * DBBE_SGE_MAX ), NULL, serial );
  TEST_BREAK( rc, "Failed to allocate memory. Stopping" );

  int sgelen;
  for( sgelen=1; sgelen<DBBE_SGE_MAX; ++sgelen)
  {
    dbBE_sge_t sge[ sgelen ];
    size_t total = 0;
    for( i=0; i<sgelen; ++i )
    {
      sge[i].iov_len = 1 + random() % (datalen>>1);
      sge[i].iov_base = &data[ 1 + random() % (datalen - sge[i].iov_len) ];
      memcpy( &in_data[ total ], sge[i].iov_base, sge[i].iov_len );
      total += sge[i].iov_len;
      in_data[ total ] = '\0';
    }

    rc += TEST( total, dbBE_SGE_get_len( sge, sgelen ) );
    rc += TEST( total, strnlen( in_data, datalen * DBBE_SGE_MAX ) );
    ssize_t serlen = dbBE_SGE_serialize( sge, sgelen, serial, datalen * DBBE_SGE_MAX );

    dbBE_sge_t sge2[DBBE_SGE_MAX];
    dbBE_SGE_deserialize( sge2, sgelen, serial, serlen );
    for( i=0; i<sgelen; ++i )
    {
      rc += TEST( total, dbBE_SGE_get_len( sge2, sgelen ) );
      rc += TEST( sge2[i].iov_len, sge[i].iov_len );
      rc += TEST( strncmp( (char*)sge2[i].iov_base, (char*)sge[i].iov_base, sge[i].iov_len ), 0 );
    }

  }

  printf( "Test exiting with rc=%d\n", rc );
  return rc;
}
