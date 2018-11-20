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

  rc += TEST( dbrSGE_extract_size( NULL ), 0 );

  char data[100];

  dbBE_Request_t *req = (dbBE_Request_t*)calloc( 1, sizeof( dbBE_Request_t ) + TEST_SGE_MAX * sizeof( dbBE_sge_t ) );
  rc += TEST_NOT( req, NULL );
  TEST_BREAK( rc, "Failed memory allocation" );

  rc += TEST( dbrSGE_extract_size( req ), 0 );

  req->_sge_count = 1;
  req->_sge[0].iov_base = NULL;
  req->_sge[0].iov_len = 0;
  rc += TEST( dbrSGE_extract_size( req ), 0 );

  req->_sge_count = 1;
  req->_sge[0].iov_base = NULL;
  req->_sge[0].iov_len = 10;
  rc += TEST( dbrSGE_extract_size( req ), 0 );


  int n;
  int64_t data_size = 0;

  req->_sge_count = TEST_SGE_MAX;

  for( n=0; n<req->_sge_count; ++n)
  {
    req->_sge[n].iov_base = data;
    req->_sge[n].iov_len = random() % 100;
    data_size += req->_sge[n].iov_len;
  }
  rc += TEST( dbrSGE_extract_size( req ), data_size );

  req->_sge[ random() % TEST_SGE_MAX ].iov_base = NULL;
  rc += TEST( dbrSGE_extract_size( req ), 0 );


  free( req );
  printf( "Test exiting with rc=%d\n", rc );
  return rc;
}

