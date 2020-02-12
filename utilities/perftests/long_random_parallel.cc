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
#include <errno.h>

//#include "timing.h"
#include "commandline.h"
//#include "resultdata.h"
#include "requestdata.h"

#include "libdatabroker.h"
//#include "benchmark.h"

#define MAX_TEST_MEMORY_USE ( 2ull * 1024ull * 1024ull * 1024ull )

// number of pre-generated keys
#define LRP_KEYSTORE_COUNT ( 8ull * 1024ull * 1024ull )

// number of key ptrs that point into the key store
#define LRP_MAX_KEY_PTRS ( 1ull * 1024ull * 1024ull * 1024ull )

// iteration granularity when using time limit
#define LRP_ITER_GRANULARITY ( 1000 )

// name of the namespace to be used for this test
static const char* TEST_NAMESPACE = "lrp";

inline int get_operation()
{
  return random() % 3;
}

int terminate( int pid, DBR_Handle_t h, DBR_Errorcode_t rc, const bool cleanup = true )
{
  std::cout << "Rank: " << pid << (rc==DBR_SUCCESS?" exiting":" stopping") << " with rc=" << rc << std::endl;
  if(( pid == 0 ) && ( cleanup ) )
  {
    MPI_Barrier( MPI_COMM_WORLD );
    dbrDelete( (DBR_Name_t)TEST_NAMESPACE );
//    dbrDetach( h );
  }
  else
  {
    dbrDetach( h );
    MPI_Barrier( MPI_COMM_WORLD );
  }
  MPI_Finalize();
  exit(0);
}


int extraParse( const int opt, dbr::config *cfg )
{
  switch( opt )
  {
    case 'f':
      cfg->_filldata = true;
      break;
    case 'T': // time limit
      cfg->_timelimit = std::strtol( optarg, NULL, 10 );
      break;
    case 'M':
      cfg->_memlimit = std::strtol( optarg, NULL, 10 );
      break;
    case 'v':
      cfg->_validate = true;
      break;
    default:
      return -1;
  }
  return 0;
}

int main( int argc, char **argv )
{
  int np,pid;
  MPI_Comm comm = MPI_COMM_WORLD;
  MPI_Init(&argc,&argv);
  MPI_Comm_size(comm,&np);
  MPI_Comm_rank(comm,&pid);

  std::string extraHelp = "\
  -f              run a fill-data test that just writes random data\n\
  -M <memory>     limit iteration by amount of data given in bytes\n\
  -T <seconds>    instead of iterations, run for a given amount of time\n\
  -v              validate data to check for correctness\n\
";

  dbr::config *config = dbr::ParseCommandline( argc, argv, "d:fhk:KM:n:p:t:T:v", extraParse, extraHelp, true );
  if( config == NULL )
  {
    std::cerr << "Failed to create configuration." << std::endl;
    return -1;
  }

  if(( config->_validate ) && ( config->_datasize < sizeof( uint64_t ) + config->_keylen ) )
  {
    std::cerr << "ERROR: Data size for validation needs to be more than keysize + " << sizeof( uint64_t ) << std::endl;
    return -1;
  }
  if(( config->_validate ) && ( config->_datasize % sizeof( uint64_t ) != 0 ))
  {
    std::cerr << "ERROR: Data size for validation needs to be multiple of " << sizeof( uint64_t ) << std::endl;
    return -1;
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
    MPI_Abort(MPI_COMM_WORLD, EEXIST );
    exit( -1 );
  }

  srand( pid );

  int rc = 0;
  // generate key data
  char *keydata = dbr::generateLongMsg( (LRP_KEYSTORE_COUNT+1) * config->_keylen );
  // nul-terminate the keys
  for( uint64_t n=0; n<LRP_KEYSTORE_COUNT; ++n )
    keydata[ n * config->_keylen + config->_keylen - 1 ] = 0;

  // generate some random data to insert
  char *database = dbr::generateLongMsg( config->_datasize * 100 );

  // pre-allocate memory for data retrieval
  char *dataspace = (char*)malloc( config->_datasize );


  size_t key_ptr_count = config->_filldata ? 10 : LRP_MAX_KEY_PTRS;
  char **keys = (char**)calloc( key_ptr_count, sizeof( char* ) );
  uint64_t khead = 0;
  uint64_t ktail = 0;

  // some stats:
  uint64_t read_count = 0;
  size_t current_written=0;
  size_t current_read=0;
  size_t current_gotten=0;


  DBR_Errorcode_t errcode;

  int64_t data_size;
  char *data;
  char *key;
  uint64_t key_index;

  size_t this_time, end_time;
  struct timeval curtim;
  gettimeofday( &curtim, NULL );
  end_time = curtim.tv_sec + config->_timelimit;
  if( config->_timelimit > 0 )
    config->_iterations = LRP_ITER_GRANULARITY * 2; // iterations don't matter, but need to be at least TIME_GRANULARITY

  for( int64_t i=config->_iterations; i>0; --i )
  {
    int op = config->_filldata ? 0 : get_operation();

    switch( op )
    {
      case 0: // put
        if(( !config->_filldata ) && ( khead - ktail >= (key_ptr_count - 1) ))
        {
          std::cerr << "WARNING: Keys exhausted. Need a few get() to free up space. " << khead << "-" << ktail << "=" << khead-ktail << "; limit=" << LRP_MAX_KEY_PTRS << std::endl;
          break;
        }

        if( config->_validate )
          data_size = config->_datasize;
        else
          data_size = random() % config->_datasize + 1;
        key = &(keydata[ (random() % LRP_KEYSTORE_COUNT) * config->_keylen ]);

        data = &(database[ random() % ( config->_datasize * 99 ) ]);
        if( config->_validate )
          data = dbr::generateLongMsgValidate( key, config->_keylen - 1, data, data_size );
        errcode = dbrPut( h, data, data_size, key, DBR_GROUP_LIST_EMPTY );

//        std::cout << "PUT(" << khead << "|" << key << ") = " << errcode << std::endl;
        if( config->_validate )
          delete data;
        if( errcode == DBR_SUCCESS )
        {
          keys[ khead  % key_ptr_count ] = key;
          ++khead;
          current_written += data_size;
        }
        else
        {
          std::cerr << "ERROR: in dbrPut[ " << khead+1 << " ]; key=" << key << "; rc=" << dbrGet_error( errcode ) << std::endl;
          terminate( pid, h, errcode );
        }
        break;
      case 1: // read
        if( khead == ktail ) break;
        data_size = config->_datasize;
        data = dataspace;
        if( config->_validate )
          memset( data, 0, data_size+1 );
        key_index = (random() % (khead - ktail) + ktail) % key_ptr_count;
        key = keys[ key_index ];
        errcode = dbrRead( h, data, &data_size, key, NULL, DBR_GROUP_LIST_EMPTY, DBR_FLAGS_NOWAIT );

//        std::cout << "READ(" << key_index << "|" << key << ") == " << errcode << std::endl;
        if( errcode == DBR_SUCCESS )
        {
          ++read_count;
          current_read += data_size;

          if(( config->_validate ) && ( ! dbr::validateMsg( key, config->_keylen - 1, data, data_size )) )
          {
            std::cerr << "ERROR: dbrRead returned corrupted data. for key: " << key << std::endl;
            terminate( pid, h, DBR_ERR_GENERIC );
          }
        }
        else
        {
          std::cerr << "ERROR: in dbrRead[ " << read_count << " ]; key=" << key << "; rc=" << dbrGet_error( errcode ) << std::endl;
          terminate( pid, h, errcode );
        }
        break;
      case 2: // get
        if( khead == ktail ) break;
        data_size = config->_datasize;
        data = dataspace;
        if( config->_validate )
          memset( data, 0, data_size+1 );
        key_index = ktail % key_ptr_count;
        key = keys[ key_index ];
        errcode = dbrGet( h, data, &data_size, key, NULL, DBR_GROUP_LIST_EMPTY, DBR_FLAGS_NOWAIT );

//        std::cout << "GET(" << ktail << "|" << key << ") == " << errcode << std::endl;
        if( errcode == DBR_SUCCESS )
        {
          ++ktail;
          current_gotten += data_size;

          if(( config->_validate ) && ( ! dbr::validateMsg( key, config->_keylen - 1, data, data_size )) )
          {
            std::cerr << "ERROR: dbrRead returned corrupted data. for key: " << key << std::endl;
            terminate( pid, h, DBR_ERR_GENERIC );
          }
        }
        else
        {
          std::cerr << "ERROR: in dbrGet[ " << ktail << " ]; key=" << key << "; rc=" << dbrGet_error( errcode ) << std::endl;
          terminate( pid, h, errcode );
        }
        break;
      default:
        exit(1);
    }

    if(( i % LRP_ITER_GRANULARITY ) && ( config->_memlimit > 0 ))
    {
      if( current_written > config->_memlimit )
        break;
      else
        i += (LRP_ITER_GRANULARITY - 1 );
    }

    if(( i % LRP_ITER_GRANULARITY ) && (config->_timelimit > 0))
    {
      gettimeofday( &curtim, NULL );
      this_time = curtim.tv_sec;

      if( this_time < end_time )
        i += (LRP_ITER_GRANULARITY - 1);
      else
        break;
    }
  }

  current_written /= 1000000;
  current_read /= 1000000;
  current_gotten /= 1000000;
  std::cout << "Completed: " << khead << " x PUT; " << read_count << " x READ; " << ktail << " x GET" << std::endl;
  std::cout << "      written: " << current_written << " MB; read: " << current_read << " MB; gotten: " << current_gotten << " MB" << std::endl;
  terminate( pid, h, DBR_SUCCESS, ! config->_filldata );
  return rc;
}
