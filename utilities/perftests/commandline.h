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

#ifndef IBM_PERFTEST_COMMANDLINE_H_
#define IBM_PERFTEST_COMMANDLINE_H_

#include <string.h>
#include <unistd.h>

#include <algorithm>

namespace dbr {

struct config
{
  size_t _datasize;
  size_t _keylen;
  size_t _variable_key;
  size_t _iterations;
  size_t _inflight;
  size_t _timelimit;
  size_t _memlimit;
  bool _filldata;
  bool _validate;
  int _testcase;
};

enum TEST_CASE
{
  TEST_CASE_UNDEF = 0x0,
  TEST_CASE_PUT = 0x1,
  TEST_CASE_GET = 0x2,
  TEST_CASE_READ = 0x4,
  TEST_CASE_MAX = 0x8
};

static inline int
test_case_to_int( const char *test )
{
  int testcase = TEST_CASE_UNDEF;
  if( strstr( test, "PUT" ) != NULL )
    testcase += TEST_CASE_PUT;
  if( strstr( test, "GET" ) != NULL )
    testcase += TEST_CASE_GET;
  if( strstr( test, "READ" ) != NULL )
    testcase += TEST_CASE_READ;
  return testcase;
}

namespace par_single_common {

int extraParse( const int opt, dbr::config *cfg )
{
  switch( opt )
  {
    case 'p': // parallel/in-flight requests
      cfg->_inflight = std::strtol( optarg, NULL, 10 );
      break;
    case 't': // test case
      cfg->_testcase = test_case_to_int( optarg );
      if( cfg->_testcase == -1 )
      {
        std::cerr << "Unknown test case (available: PUT, GET, READ)" << std::endl;
        exit(1);
      }
      break;
    case 'v': // validation
      cfg->_validate = true;
      break;
    default:
      return -1;  // there are no extra options for this
  }
  return 0;
}

}

static inline void
PrintHelp( const std::string SpecialOptions = "" )
{
  std::cerr << "Usage:" << std::endl
      << "  -h              print this help" << std::endl
      << "  -d <datasize>   set the size of the value/tuple (64)" << std::endl
      << "  -k <keysize>    set the size of the keys (8)" << std::endl
      << "  -K              variable keysize, will randomize the keylen [rnd%keylen+keylen] (off)" << std::endl
      << "  -n <iterations> number of iterations (10000)" << std::endl
      << SpecialOptions;
}


static inline config*
ParseCommandline( int argc, char **argv,
                  const char* options,
                  int (*extraParse) (const int, config* ),
                  std::string extraHelp = "",
                  const bool single = true )
{
  config *cfg = new config();

  cfg->_datasize = 64;
  cfg->_keylen = 8;
  cfg->_variable_key = 0;
  cfg->_iterations = 10000;
  cfg->_inflight = 1;
  cfg->_testcase = test_case_to_int("PUT,READ,GET");
  cfg->_timelimit = 0; // no time limit
  cfg->_memlimit = 0; // no memory limit
  cfg->_filldata = false; // no floodfill of data
  cfg->_validate = false; // no validation (slows down the operation)

  int option;
  while(( option = getopt(argc, argv, options)) != -1 )
  {
    // locally check common options; callback for extra options
    switch( option )
    {
      case 'd': // datasize
        cfg->_datasize = std::strtol( optarg, NULL, 10 );
        break;
      case 'h': // help
        PrintHelp( extraHelp );
        exit(0);
        break;
      case 'k': // key length
        cfg->_keylen = std::strtol( optarg, NULL, 10 );
        break;
      case 'K': // variable keys?
        cfg->_variable_key = 1;
        break;
      case 'n': // iterations
        cfg->_iterations = std::strtol( optarg, NULL, 10 );
        break;
      default:
        // check if it's a special option
        if( extraParse( option, cfg ) != 0 )
        {
          PrintHelp( extraHelp );
          exit(1);
        }
        break;
    }
  }

  cfg->_iterations += (2 * cfg->_inflight);
  return cfg;
}

} // namespace dbr

#endif /* IBM_PERFTEST_COMMANDLINE_H_ */
