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

#include <mpi.h>

#include <iostream>
#include <iomanip>

#include "timing.h"
#include "commandline.h"
#include "resultdata.h"
#include "requestdata.h"

#include "libdatabroker.h"
#include "benchmark.h"

#define MAX_TEST_MEMORY_USE ( 2 * 1024ull * 1024ull * 1024ull )

void PrintParallelResultLine( dbr::config *cfg,
                              int testcase,
                              dbr::resultdata *resd,
                              double actual_time,
                              MPI_Comm comm )
{
  if( (cfg->_testcase & testcase) == 0 )
  {
    dbr::DestroyResult( resd );
    return;
  }

  int pid = 0;
  int np = 0;
  MPI_Comm_size( comm, &np );
  MPI_Comm_rank( comm, &pid );

  double total_req = np * ( cfg->_iterations - cfg->_inflight - cfg->_inflight );
  double total_time = 0.0;
  MPI_Allreduce( &actual_time, &total_time, 1, MPI_DOUBLE, MPI_SUM, comm );

  total_time = total_time/np;

  double minlat = 100000000000.;
  double maxlat = 0.0;
  double total = 0.0;
  size_t count = 0;
  for( size_t n=cfg->_inflight; n<cfg->_iterations-cfg->_inflight; ++n )
  {
    minlat = std::min( minlat, resd->_latency[ n ] );
    maxlat = std::max( maxlat, resd->_latency[ n ] );
    total += resd->_latency[ n ];
    ++count;
//    std::cout << " " << resd->_latency[ n ];
  }

  double glob_min, glob_max;

  MPI_Allreduce( &minlat, &glob_min, 1, MPI_DOUBLE, MPI_MIN, comm );
  MPI_Allreduce( &maxlat, &glob_max, 1, MPI_DOUBLE, MPI_MAX, comm );

  if( pid == 0 )
  {
    std::cout << std::setw(10) << cfg->_datasize
        << std::setw(12) << total_time/1000.
        << std::setw(12) << total_req
        << std::setw(12) << (total_req)/(total_time/1000000.)
        << std::setw(12) << (total_req*cfg->_datasize)/total_time
        << std::setw(12) << (total_time*np/1000./total_req)
        << std::setw(12) << glob_min/1000.
        << std::setw(12) << glob_max/1000.
        << std::setw(6) << dbr::case_str[ testcase ]
        << std::endl;
  }
  MPI_Barrier( comm );
  dbr::DestroyResult( resd );
}


int main( int argc, char **argv )
{
  int np,pid;
  MPI_Comm comm = MPI_COMM_WORLD;
  MPI_Init(&argc,&argv);
  MPI_Comm_size(comm,&np);
  MPI_Comm_rank(comm,&pid);

  std::string extraHelp = "\
  -p <inflight>      number of requests that are kept in-flight at the same time (1)\n\
  -t <PUT|GET|READ>  comma separated list which command to test (PUT,READ,GET)\n\
";

  dbr::config *config = dbr::ParseCommandline( argc, argv, "d:hk:Kn:p:t:", dbr::par_single_common::extraParse, extraHelp, true );
  if( config == NULL )
  {
    std::cerr << "Failed to create configuration." << std::endl;
    return -1;
  }

  int rc = 0;

  dbr::test_start = dbr::myTime();

  dbr::resultdata *put_res = dbr::InitializeResult( config );
  dbr::resultdata *read_res = dbr::InitializeResult( config );
  dbr::resultdata *get_res = dbr::InitializeResult( config );

  dbr::requestdata *reqd = dbr::InitializeRequest( config );

  char *data = dbr::generateLongMsg( config->_datasize );

  if( config->_iterations * config->_keylen < MAX_TEST_MEMORY_USE )
  {
    for( size_t n=0; n<config->_iterations; ++n )
      dbr::RandomizeData( reqd, n, (config->_variable_key * random() % config->_keylen ) + config->_keylen );
  }

  DBR_Handle_t h;
  if( pid == 0 )
  {
    h = dbrCreate((DBR_Name_t)TEST_NAMESPACE, DBR_PERST_VOLATILE_SIMPLE, DBR_GROUP_LIST_EMPTY );
    MPI_Barrier(comm);
  }
  else
  {
    MPI_Barrier(comm);
    h = dbrAttach( (DBR_Name_t)TEST_NAMESPACE );
  }
  if( h == NULL )
  {
    std::cerr << "Failed to create namespace" << std::endl;
    exit( -1 );
  }


  // populate backend with data
  double put_actual_time = 0.0;
  if( config->_testcase & dbr::TEST_CASE_PUT )
  {
    if( pid == 0 ) { std::cout << "."; std::flush( std::cout ); }
    put_actual_time = dbr::benchmark(config, dbr::TEST_CASE_PUT, put_res, reqd, h, data );
  }
  else
  {
    if( pid == 0 ) { std::cout << "."; std::flush( std::cout ); }
    size_t n;
    for( n=0; n<config->_iterations; ++n )
      dbrPut( h, data, config->_datasize, reqd->_names[n], DBR_GROUP_EMPTY );
  }
  if( pid == 0 ) { std::cout << "."; std::flush( std::cout ); }


  double read_actual_time = 0.0;
  if( config->_testcase & dbr::TEST_CASE_READ )
  {
    if( pid == 0 ) { std::cout << "."; std::flush( std::cout ); }
    MPI_Barrier(comm);
    read_actual_time = dbr::benchmark(config, dbr::TEST_CASE_READ, read_res, reqd, h, data );
    if( pid == 0 ) { std::cout << "."; std::flush( std::cout ); }
  }
  // nothing else, if read is not part of the tests, just skip it
  MPI_Barrier(comm);

  // cleanup any generated data
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
    for( size_t n=0; n<config->_iterations; ++n )
    {
      size = config->_datasize;
      dbrGet( h, data, &size, reqd->_names[n], NULL, DBR_GROUP_EMPTY, DBR_FLAGS_NONE );
    }
  }
  if( pid == 0 )
  {
    MPI_Barrier(comm);
    dbrDelete( (DBR_Name_t)TEST_NAMESPACE );
  }
  else
  {
    dbrDetach( h );
    MPI_Barrier(comm);
  }
  if( pid == 0 ) std::cout << "Done." << std::endl;


  if( pid == 0 )
    dbr::PrintHeader( config, argc, argv );

  PrintParallelResultLine( config, dbr::TEST_CASE_PUT, put_res, put_actual_time, comm );
  PrintParallelResultLine( config, dbr::TEST_CASE_READ, read_res, read_actual_time, comm );
  PrintParallelResultLine( config, dbr::TEST_CASE_GET, get_res, get_actual_time, comm );

  dbr::DestroyRequest( reqd );
  delete config;

  MPI_Finalize();
  return rc;
}
