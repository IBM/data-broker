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
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>

#include "logutil.h"

#ifndef TEST_TEST_UTILS_H_
#define TEST_TEST_UTILS_H_

#define TEST( function, expect ) ( (function)==(expect)? 0 : 1 )
#define TEST_RC( function, expect, returned ) ( ((returned)=(function))==(expect)? 0 : 1 )
#define TEST_NOT( function, expect ) ( 1 - TEST( function, expect ) )

#define TEST_LOG( rc, text ) { if( (rc) != 0 ) printf("%s; rc=%d\n", (text), (rc) ); }

#define TEST_BREAK( rc, text ) { if( (rc) != 0 ) { printf("%s; rc=%d. Exiting early...\n", (text), (rc) ); return (rc); } }

static inline
char* generateLongMsg( const uint64_t size )
{
  char *msg = (char*)calloc( 1, size+1 ); // +1 for terminating 0
  uint64_t i;
  for( i = 0; i < size; ++i )
    msg[ i ] = (char)( random() % 26 + 97);

  LOG( DBG_TRACE, stdout, "%s\n", msg );
  return msg;
}

#endif /* TEST_TEST_UTILS_H_ */
