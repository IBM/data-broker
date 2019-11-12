/*
 * Copyright © 2019 IBM Corporation
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

#ifndef SRC_UTIL_MEMUTIL_H_
#define SRC_UTIL_MEMUTIL_H_

#include <string.h>
#include <inttypes.h>

static inline
int memsum( const void *p, size_t len )
{
  uint64_t sum = 0;
  uint64_t *r = (uint64_t*)p;

  size_t i = 0;
  while(( sum == 0 ) && ( i < (len / sizeof( uint64_t )) ))
    sum += r[ i++ ];

  if( len % sizeof( uint64_t ) )
  {
    unsigned char *c = (unsigned char*)r;
    i = len / sizeof( uint64_t );
    while(( sum == 0 ) && ( i < len ))
      sum += c[ i++ ];
  }
  return (sum != 0 );
}

volatile int* memzero( void *buf, int val, size_t s );

#endif /* SRC_UTIL_MEMUTIL_H_ */
