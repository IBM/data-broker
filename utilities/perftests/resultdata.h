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

#ifndef IBM_PERFTEST_RESULTDATA_H_
#define IBM_PERFTEST_RESULTDATA_H_

#include "timing.h"

namespace dbr {

struct resultdata {
  double *_latency;
  bool _validation_failed;
};

resultdata* InitializeResult( config *cfg )
{
  resultdata *rd = new resultdata();
  rd->_latency = new double[ cfg->_iterations ];
  rd->_validation_failed = false;
  return rd;
}

void DestroyResult( resultdata *res )
{
  if( res != NULL )
  {
    if( res->_latency != NULL )
      delete res->_latency;
    delete res;
  }
}

void PrintHeader( config *cfg, int argc, char **argv )
{
  std::cout << "CMD:";
  for( int n=0; n<argc; ++n )
    std::cout << " " << argv[n];
  std::cout << std::endl;
  if( cfg->_inflight > 1 )
    std::cout << "NOTE: When interpreting times per request (including min/max), take into account -p = " << cfg->_inflight << std::endl;
  if( cfg->_validate )
    std::cout << "NOTE: Result validation was active." << std::endl;

  std::cout << std::endl;

  std::cout << std::setw(10) << "datasize"
  << std::setw(12) << std::right << "time"
      << std::setw(12) << "requests"
      << std::setw(12) << "IOPS"
      << std::setw(12) << "BW"
      << std::setw(12) << "avg/req"
      << std::setw(12) << "min"
      << std::setw(12) << "max"
      << std::endl;
  std::cout << std::setw(10) << "[byte] "
      << std::setw(12) << "[ms] "
      << std::setw(12) << ""
      << std::setw(12) << "[/s] "
      << std::setw(12) << "[MB/s] "
      << std::setw(12) << "[ms] "
      << std::setw(12) << "[ms] "
      << std::setw(12) << "[ms] " << std::endl;
}

static std::string case_str[5] = { "UNDEF", "PUT", "GET", "", "READ" }; // mind the gap


void PrintResultLine( config *cfg,
                      int testcase,
                      resultdata *resd,
                      double actual_time )
{
  if( (cfg->_testcase & testcase) == 0 )
  {
    dbr::DestroyResult( resd );
    return;
  }

  double actual_req = cfg->_iterations - cfg->_inflight - cfg->_inflight;

  double minlat = 100000000000.;
  double maxlat = 0.0;
  double total = 0.0;
  size_t count = 0;
  for( int n=cfg->_inflight; n<cfg->_iterations-cfg->_inflight; ++n )
  {
    minlat = std::min( minlat, resd->_latency[ n ] );
    maxlat = std::max( maxlat, resd->_latency[ n ] );
    total += resd->_latency[ n ];
    ++count;
//    std::cout << " " << resd->_latency[ n ];
  }

  std::cout << std::setw(10) << cfg->_datasize
      << std::setw(12) << actual_time/1000.
      << std::setw(12) << actual_req
      << std::setw(12) << (actual_req)/(actual_time/1000000.)
      << std::setw(12) << (actual_req*cfg->_datasize)/actual_time
      << std::setw(12) << (actual_time/1000./actual_req)
      << std::setw(12) << minlat/1000.
      << std::setw(12) << maxlat/1000.
      << std::setw(6) << case_str[ testcase ]
      << (resd->_validation_failed ? " f" : "")
      << std::endl;

  dbr::DestroyResult( resd );
}


}





#endif /* IBM_PERFTEST_RESULTDATA_H_ */
