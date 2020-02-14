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

#include <iostream>
#include <iomanip>

#include "timing.h"
#include "commandline.h"
#include "resultdata.h"
#include "requestdata.h"

#include "libdatabroker.h"
#include "benchmark.h"

#define MAX_TEST_MEMORY_USE ( 2 * 1024ull * 1024ull * 1024ull )


int main( int argc, char **argv )
{
  std::string extraHelp = "\
  -p <inflight>      number of requests that are kept in-flight at the same time (1)\n\
  -t <PUT|GET|READ>  comma separated list which command to test (PUT,READ,GET)\n\
  -v                 enable result validation (off)\n\
";

  dbr::config *config = dbr::ParseCommandline( argc, argv, "d:hk:Kn:p:t:v", dbr::par_single_common::extraParse, extraHelp, true );
  if( config == NULL )
  {
    std::cerr << "Failed to create configuration." << std::endl;
    return -1;
  }
  // sanity check
  if( config->_validate && ( config->_inflight > 1 ))
  {
    config->_validate = false;
    std::cout << "WARN: validation only works with -p 1. Disabling" << std::endl;
  }

  int rc = 0;

  dbr::test_start = dbr::myTime();

  dbr::resultdata *put_res = dbr::InitializeResult( config );
  dbr::resultdata *read_res = dbr::InitializeResult( config );
  dbr::resultdata *get_res = dbr::InitializeResult( config );

  dbr::requestdata *reqd = dbr::InitializeRequest( config );

  char *data = dbr::generateLongMsg( config->_datasize );

  size_t n = 0;
  if( config->_iterations * config->_keylen < MAX_TEST_MEMORY_USE )
  {
    for( n=0; n<config->_iterations; ++n )
      dbr::RandomizeData( reqd, n, (config->_variable_key * random() % config->_keylen ) + config->_keylen );
  }

  DBR_Handle_t h = dbrCreate((DBR_Name_t)TEST_NAMESPACE, DBR_PERST_VOLATILE_SIMPLE, DBR_GROUP_LIST_EMPTY );
  if( h == NULL )
  {
    std::cerr << "Failed to create namespace" << std::endl;
    exit( -1 );
  }

  // populate backend with data
  double put_actual_time = 0.0;
  if( config->_testcase & dbr::TEST_CASE_PUT )
  {
    std::cout << "."; std::flush( std::cout );
    put_actual_time = dbr::benchmark(config, dbr::TEST_CASE_PUT, put_res, reqd, h, data );
  }
  else
  {
    std::cout << "."; std::flush( std::cout );
    for( n=0; n<config->_iterations; ++n )
      dbrPut( h, data, config->_datasize, reqd->_names[n], DBR_GROUP_EMPTY );
  }

  std::cout << "."; std::flush( std::cout );

  double read_actual_time = 0.0;
  if( config->_testcase & dbr::TEST_CASE_READ )
  {
    std::cout << "."; std::flush( std::cout );
    read_actual_time = dbr::benchmark(config, dbr::TEST_CASE_READ, read_res, reqd, h, data );
    std::cout << "."; std::flush( std::cout );
  }
  // nothing else, if read is not part of the tests, just skip it

  double get_actual_time = 0.0;
  if( config->_testcase & dbr::TEST_CASE_GET )
  {
    std::cout << "."; std::flush( std::cout );
    get_actual_time = dbr::benchmark(config, dbr::TEST_CASE_GET, get_res, reqd, h, data );
    std::cout << "."; std::flush( std::cout );
  }
  else
  {
    std::cout << "."; std::flush( std::cout );
    int64_t size;
    for( n=0; n<config->_iterations; ++n )
    {
      size = config->_datasize;
      dbrGet( h, data, &size, reqd->_names[n], NULL, DBR_GROUP_EMPTY, DBR_FLAGS_NONE );
    }
  }

  std::cout << "." << std::endl;

  DBR_Errorcode_t res = dbrDelete( (DBR_Name_t)TEST_NAMESPACE );

  dbr::PrintHeader( config, argc, argv );

  dbr::PrintResultLine( config, dbr::TEST_CASE_PUT, put_res, put_actual_time );
  dbr::PrintResultLine( config, dbr::TEST_CASE_READ, read_res, read_actual_time );
  dbr::PrintResultLine( config, dbr::TEST_CASE_GET, get_res, get_actual_time );

  dbr::DestroyRequest( reqd );
  delete config;

  if( res != DBR_SUCCESS )
  {
    std::cerr << "There were errors. You might want to check for remaining data in the databroker." << std::endl;
  }


  return rc;
}
