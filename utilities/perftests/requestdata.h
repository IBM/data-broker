/*
 * Copyright © 2018-2020 IBM Corporation
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

#ifndef IBM_PERFTEST_REQUESTDATA_H_
#define IBM_PERFTEST_REQUESTDATA_H_

#include <sys/time.h>
#include <stdio.h>
#include "libdatabroker.h"
#include <algorithm>

namespace dbr {

struct requestdata {
  struct timeval *_start;
  DBR_Tag_t *_tags;
  DBR_Tuple_name_t *_names;
};

static inline
requestdata* InitializeRequest( config *cfg )
{
  requestdata *rd = new requestdata();
  rd->_start = new struct timeval[ cfg->_iterations ];
  rd->_tags = new DBR_Tag_t[ cfg->_iterations ];
  rd->_names = new DBR_Tuple_name_t[ cfg->_iterations ];
  for(int i = 0; i < cfg->_iterations; ++i)
       rd->_names[i] = NULL;
 return rd;
}

static
char* generateLongMsg( const uint64_t size )
{
  char *msg = new char[ size+1 ]; // +1 for terminating 0
  uint64_t i;
  for( i = 0; i < size; ++i )
    msg[ i ] = (char)( random() % 26 + 97);
  msg[ size ] = 0;
  return msg;
}

static
char* generateLongMsgValidate( const char *key,
                               const uint64_t keylen,
                               const char *data,
                               const uint64_t size )
{
  // size needs to be multiple of uint64_t length and at least keylen
  // we can skip the length checks because it's checked at program start

  char *msg = new char[ size+1 ];
  msg[ size+1 ] = '\0';
  memcpy( &msg[ sizeof( uint64_t ) ], key, keylen );
  memcpy( &msg[ keylen + sizeof( uint64_t ) ], data, size - keylen - sizeof( uint64_t ) );
  uint64_t chksum = 0;
  for( uint64_t i = size/sizeof(uint64_t)-1; i > 1; --i )
  {
    chksum ^= *(uint64_t*)(&msg[ i * sizeof(uint64_t) ]);
//    std::cerr << *(uint64_t*)(&msg[ i * sizeof(uint64_t) ]) << ":" << chksum << std::endl;
  }
  *((uint64_t*)msg) = chksum;
//  std::cerr << *(uint64_t*)&msg[ size/sizeof(uint64_t) ] << "|" << chksum << ":" << *((uint64_t*)msg) << std::endl;
  return msg;
}

static
bool validateMsg( const char *key,
                  const uint64_t keylen,
                  const char *data,
                  const uint64_t size )
{

  uint64_t chksum = 0;
  for( uint64_t i = size/sizeof(uint64_t)-1; i > 1; --i )
  {
    chksum ^= *(uint64_t*)(&data[ i * sizeof(uint64_t) ]);
//    std::cerr << *(uint64_t*)(&data[ i * sizeof(uint64_t) ]) << ":" << chksum << std::endl;
  }

  if( chksum != *((uint64_t*)data))
  {
    std::cerr << &data[ sizeof(uint64_t) ] << std::endl;
    std::cerr << "msg_chksum: " << *((uint64_t*)data) << "; actual: " << chksum << ":" << *(uint64_t*)&data[ size/sizeof(uint64_t) ] << std::endl;
  }
  return (chksum == *((uint64_t*)data));
}

static inline
int RandomizeData( requestdata *reqd, const size_t reqIdx, const size_t len )
{
  if( reqd->_names[ reqIdx ] != NULL )
    delete reqd->_names[ reqIdx ];
  reqd->_names[ reqIdx ] = generateLongMsg( len );
  return 0;
}

void DestroyRequest( requestdata *req )
{
  if( req != NULL )
  {
    if( req->_names != NULL )
      delete req->_names;
    if( req->_tags != NULL )
      delete req->_tags;
    if( req->_start != NULL )
      delete req->_start;

    delete req;
  }
}

}

#endif /* IBM_PERFTEST_REQUESTDATA_H_ */
