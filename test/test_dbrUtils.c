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

#include <libdatabroker.h>
#include "../src/libdatabroker_int.h"
#include "test_utils.h"


int main( int argc, char ** argv )
{
  int rc = 0;

  // Testing of the main context handling
  dbrMain_context_t *mc = dbrCheckCreateMainCTX();
  dbrMain_context_t *mc2 = dbrCheckCreateMainCTX();

  rc += TEST_NOT( mc, NULL );
  rc += TEST( mc, mc2 );

  rc += TEST( dbrMain_exit(), 0 );
  mc2 = dbrCheckCreateMainCTX();
  rc += TEST_NOT( mc2, NULL );
  rc += TEST( dbrMain_exit(), 0 );
  rc += TEST( dbrMain_exit(), 0 );

  // testing of dbrTag_get
  rc += TEST( dbrTag_get( NULL ), DB_TAG_ERROR );

  mc = dbrCheckCreateMainCTX();
  rc += TEST_NOT( dbrTag_get( mc ), DB_TAG_ERROR );


  printf( "Test exiting with rc=%d\n", rc );
  return rc;
}

