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

int test_header_extract()
{
  int rc = 0;


  dbBE_sge_t *sge = NULL;
  dbBE_sge_t ssge;
  char *data = "150\n\5\n\10\n\20\n\30\n\40\n\50\n";
  size_t parsed;
  rc += TEST( -EINVAL, dbBE_SGE_extract_header( NULL, 0, NULL, 0, NULL, NULL ));
  rc += TEST( -EINVAL, dbBE_SGE_extract_header( &ssge, 0, NULL, 0, NULL, NULL ));
  rc += TEST( -EINVAL, dbBE_SGE_extract_header( &ssge, 1, NULL, 0, NULL, NULL ));
  rc += TEST( -EINVAL, dbBE_SGE_extract_header( &ssge, 1, data, 0, NULL, NULL ));
  rc += TEST( -EINVAL, dbBE_SGE_extract_header( &ssge, 1, data, strlen(data), NULL, NULL ));
  rc += TEST( -EINVAL, dbBE_SGE_extract_header( &ssge, 1, data, strlen(data), &sge, NULL ));
  rc += TEST( -EINVAL, dbBE_SGE_extract_header( NULL, 1, data, strlen(data), &sge, &parsed ));
  rc += TEST( -EINVAL, dbBE_SGE_extract_header( &ssge, 0, data, strlen(data), &sge, &parsed ));
  rc += TEST( -EINVAL, dbBE_SGE_extract_header( &ssge, 1, NULL, strlen(data), &sge, &parsed ));
  rc += TEST( -EINVAL, dbBE_SGE_extract_header( &ssge, 1, data, 3, &sge, &parsed ));
  rc += TEST( -EINVAL, dbBE_SGE_extract_header( &ssge, 1, data, strlen(data), NULL, &parsed ));
  rc += TEST( -EINVAL, dbBE_SGE_extract_header( &ssge, 1, data, strlen(data), &sge, NULL ));

  rc += TEST( -EBADMSG, dbBE_SGE_extract_header( &ssge, 1, "05E\n", 4, &sge, &parsed ));
  rc += TEST( -EBADMSG, dbBE_SGE_extract_header( &ssge, 1, "-40\n", 4, &sge, &parsed ));
  rc += TEST( -EBADMSG, dbBE_SGE_extract_header( &ssge, 1, "405\n", 4, &sge, &parsed ));
  rc += TEST( -EBADMSG, dbBE_SGE_extract_header( &ssge, 1, "4\nUO\n", 5, &sge, &parsed ));
  rc += TEST( -EBADMSG, dbBE_SGE_extract_header( &ssge, 1, "4\n-1\n", 5, &sge, &parsed ));
  rc += TEST( -E2BIG, dbBE_SGE_extract_header( &ssge, 1, "4\n3\n12345", 9, &sge, &parsed ));
  rc += TEST( -EAGAIN, dbBE_SGE_extract_header( &ssge, 1, "4\n1\n", 4, &sge, &parsed ));
  rc += TEST( -EBADMSG, dbBE_SGE_extract_header( &ssge, 1, "4\n1\nno", 6, &sge, &parsed ));
  rc += TEST( -EAGAIN, dbBE_SGE_extract_header( &ssge, 1, "4\n1\n12", 6, &sge, &parsed ));
  rc += TEST( -EBADMSG, dbBE_SGE_extract_header( &ssge, 1, "4\n1\n12265\n", 6, &sge, &parsed ));


  rc += TEST( 1, dbBE_SGE_extract_header( &ssge, 1, "4\n1\n12\n", 7, &sge, &parsed ));
  rc += TEST( parsed, 7 );
  rc += TEST( sge[0].iov_len, 12 );

  return rc;
}


#if 0
int test_deserialize()
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
  return rc;
}
#endif

int main( int argc, char *argv[] )
{
  int rc = 0;

  rc += test_header_extract();
//  rc += test_deserialize();

  printf( "Test exiting with rc=%d\n", rc );
  return rc;
}
