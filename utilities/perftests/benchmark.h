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

#ifndef IBM_PERFTEST_BENCHMARK_H_
#define IBM_PERFTEST_BENCHMARK_H_

#ifdef OLDDBR
#define DBR_GROUP_LIST_EMPTY (NULL)
#endif

static const char* TEST_NAMESPACE = "pn";

#include <inttypes.h>
#include <libdatabroker.h>
#include "commandline.h"
#include "requestdata.h"
#include "resultdata.h"

namespace dbr {

static
double benchmark( config *config,
                  int testcase,
                  resultdata *resd,
                  requestdata *reqd,
                  DBR_Handle_t h,
                  char *data )
{
  size_t n;
  int64_t retsize[ config->_inflight ];
  char match[] = "";

  char *base_data = strdup( data );
  bool validation_needed = ( config->_validate &&( testcase & dbr::TEST_CASE_PUT ));

  // warmup
  for( n=0; n<config->_inflight; ++n )
  {
//    std::cout << "Creating: " << n << ":" << reqd->_names[n] << std::endl;
    resd->_latency[n] = -dbr::myTime();

    switch( testcase )
    {
      case dbr::TEST_CASE_PUT:
        reqd->_tags[n] = dbrPutA( h, data, config->_datasize, reqd->_names[n], DBR_GROUP_EMPTY );
        break;
      case dbr::TEST_CASE_GET:
        retsize[n % config->_inflight ] = config->_datasize;
        reqd->_tags[n] = dbrGetA( h, data, &retsize[n % config->_inflight ], reqd->_names[n], match, DBR_GROUP_EMPTY, DBR_FLAGS_NONE );
        break;
      case dbr::TEST_CASE_READ:
        retsize[n % config->_inflight ] = config->_datasize;
        reqd->_tags[n] = dbrReadA( h, data, &retsize[n % config->_inflight ], reqd->_names[n], match, DBR_GROUP_EMPTY, DBR_FLAGS_NONE );
        break;
      default:
        std::cerr << "Undefined test case: " << testcase << std::endl;
        exit(1);
        break;
    }
    if( reqd->_tags[n] == DB_TAG_ERROR )
    {
      std::cerr << "(Warmup) Failed to request: " << n << " (key=" << reqd->_names[ n ] << ")" << std::endl;
      return -1;
    }
  }

  // measuring phase
  double start = dbr::myTime();
  //  std::cout << "StartStamp: " << start << std::endl;

  for( ; n<config->_iterations; ++n )
  {
    int waitidx = n - config->_inflight;
  //    std::cout << "Waiting : " << waitidx << ":" << reqd->_names[ waitidx ] << std::endl;
    while( dbrTest( reqd->_tags[ waitidx ] ) == DBR_ERR_INPROGRESS );
    resd->_latency[ waitidx ] += dbr::myTime();

    if(( validation_needed ) && ( strncmp( data, base_data, config->_datasize ) != 0 ))
      resd->_validation_failed = true;
    if( validation_needed )
      memset( data, 0, config->_datasize );

//    std::cout << "Creating: " << n << ":" << reqd->_names[n] << std::endl;
    resd->_latency[n] = -dbr::myTime();
    switch( testcase )
    {
      case dbr::TEST_CASE_PUT:
        reqd->_tags[n] = dbrPutA( h, data, config->_datasize, reqd->_names[n], DBR_GROUP_EMPTY );
        break;
      case dbr::TEST_CASE_GET:
        retsize[n % config->_inflight ] = config->_datasize;
        reqd->_tags[n] = dbrGetA( h, data, &retsize[n % config->_inflight ], reqd->_names[n], match, DBR_GROUP_EMPTY, DBR_FLAGS_NONE );
        break;
      case dbr::TEST_CASE_READ:
        retsize[n % config->_inflight ] = config->_datasize;
        reqd->_tags[n] = dbrReadA( h, data, &retsize[n % config->_inflight ], reqd->_names[n], match, DBR_GROUP_EMPTY, DBR_FLAGS_NONE );
        break;
      default:
        std::cerr << "Undefined test case: " << testcase << std::endl;
        exit(1);
        break;
    }
    if( reqd->_tags[n] == DB_TAG_ERROR )
    {
      std::cerr << "(Measure) Failed to request: " << n << " (key=" << reqd->_names[ n ] << ")" << std::endl;
      return -1;
    }
//    std::cout << "Created: " << n << ":" << reqd->_names[n] << " tag:" << reqd->_tags[n] << std::endl;
  }

  double end = dbr::myTime();
  //  std::cout << "EndStamp: " << end << std::endl;

  // cooldown
  for( ; n<config->_iterations+config->_inflight; ++n )
  {
    int waitidx = n - config->_inflight;
  //    std::cout << "Waiting : " << waitidx << ":" << reqd->_names[ waitidx ] << std::endl;
    while( dbrTest( reqd->_tags[ waitidx ] ) == DBR_ERR_INPROGRESS );
    resd->_latency[ waitidx ] += dbr::myTime();

    if(( config->_validate ) && ( strncmp( data, base_data, config->_datasize ) != 0 ))
      resd->_validation_failed = true;
  }

  return end-start;
}

}

#endif /* IBM_PERFTEST_BENCHMARK_H_ */
