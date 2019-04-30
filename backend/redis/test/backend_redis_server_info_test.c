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

#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>
#endif

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <libdatabroker.h>

#include "../backend/redis/server_info.h"
#include "../backend/redis/cluster_info.h"
#include "../backend/redis/result.h"
#include "test_utils.h"


int server_info_test()
{
  int rc = 0;

  // NULLptr input
  rc += TEST( dbBE_Redis_server_info_create( NULL ), NULL );

  // invalid result type
  dbBE_Redis_result_t result;
  result._type = dbBE_REDIS_TYPE_CHAR;
  result._data._string._data = NULL;
  result._data._string._size = 0;
  rc += TEST( dbBE_Redis_server_info_create( &result ), NULL );

  // result array too short
  result._type = dbBE_REDIS_TYPE_ARRAY;
  result._data._array._len = 2;
  result._data._array._data = NULL;  // make it an invalid result
  rc += TEST( dbBE_Redis_server_info_create( &result ), NULL );

  // create a number of proper entries
  result._data._array._len = 3;
  result._data._array._data = (dbBE_Redis_result_t*)malloc( sizeof (dbBE_Redis_result_t ) * result._data._array._len );
  memset( result._data._array._data, 0, sizeof (dbBE_Redis_result_t ) * result._data._array._len );

  // with wrong type entry
  result._data._array._data[0]._type = dbBE_REDIS_TYPE_ERROR;
  result._data._array._data[1]._type = dbBE_REDIS_TYPE_INT;
  result._data._array._data[1]._data._integer = 14053;
  rc += TEST( dbBE_Redis_server_info_create( &result ), NULL );

  result._data._array._data[0]._type = dbBE_REDIS_TYPE_INT;
  result._data._array._data[0]._data._integer = 1450;
  result._data._array._data[1]._type = dbBE_REDIS_TYPE_ERROR;
  rc += TEST( dbBE_Redis_server_info_create( &result ), NULL );

  // invalid slot ranges
  result._data._array._data[1]._type = dbBE_REDIS_TYPE_INT;
  result._data._array._data[0]._data._integer = -1;
  result._data._array._data[1]._data._integer = 1000;
  rc += TEST( dbBE_Redis_server_info_create( &result ), NULL );

  result._data._array._data[0]._data._integer = 0;
  result._data._array._data[1]._data._integer = 16384;
  rc += TEST( dbBE_Redis_server_info_create( &result ), NULL );

  result._data._array._data[0]._data._integer = 1000;
  result._data._array._data[1]._data._integer = 100;
  rc += TEST( dbBE_Redis_server_info_create( &result ), NULL );

  // correct range, invalid master info
  result._data._array._data[0]._data._integer = 0;
  result._data._array._data[1]._data._integer = 16383;
  result._data._array._data[2]._type = dbBE_REDIS_TYPE_ERROR;
  rc += TEST( dbBE_Redis_server_info_create( &result ), NULL );

  result._data._array._data[2]._type = dbBE_REDIS_TYPE_ARRAY;
  result._data._array._data[2]._data._array._len = 3;
  result._data._array._data[2]._data._array._data = (dbBE_Redis_result_t*)malloc( sizeof (dbBE_Redis_result_t ) * result._data._array._len );

  result._data._array._data[2]._data._array._data[0]._type = dbBE_REDIS_TYPE_INT;
  rc += TEST( dbBE_Redis_server_info_create( &result ), NULL );

  result._data._array._data[2]._data._array._data[0]._type = dbBE_REDIS_TYPE_CHAR;
  result._data._array._data[2]._data._array._data[0]._data._string._data = strdup("127.0.0.1");
  result._data._array._data[2]._data._array._data[0]._data._string._size = 9;

  result._data._array._data[2]._data._array._data[1]._type = dbBE_REDIS_TYPE_CHAR;
  rc += TEST( dbBE_Redis_server_info_create( &result ), NULL );

  result._data._array._data[2]._data._array._data[1]._type = dbBE_REDIS_TYPE_INT;
  result._data._array._data[2]._data._array._data[1]._data._integer = 540354;
  rc += TEST( dbBE_Redis_server_info_create( &result ), NULL );

  result._data._array._data[2]._data._array._data[1]._data._integer = 6300;
  dbBE_Redis_server_info_t *si;
  rc += TEST_NOT_RC( dbBE_Redis_server_info_create( &result ), NULL, si );

  rc += TEST( si->_first_slot, 0 );
  rc += TEST( si->_last_slot, 16383 );
  rc += TEST( si->_server_count, 1 );
  rc += TEST( strncmp( si->_servers[ 0 ], "sock://127.0.0.1:6300", DBR_SERVER_URL_MAX_LENGTH ), 0 );
  rc += TEST( si->_servers[ 1 ], NULL );
  rc += TEST( si->_master, si->_servers[ 0 ] );

  rc += TEST( dbBE_Redis_server_info_destroy( si ), 0 );

  free( result._data._array._data[2]._data._array._data[0]._data._string._data );
  free( result._data._array._data[2]._data._array._data );
  free( result._data._array._data );

  printf( "Server_info_test exiting with rc=%d\n", rc );
  return rc;
}

int cluster_info_test_create_server( dbBE_Redis_result_t *res, int idx, int scnt )
{
  res->_type = dbBE_REDIS_TYPE_ARRAY;
  res->_data._array._len = 2+scnt;
  res->_data._array._data = (dbBE_Redis_result_t*)malloc( sizeof (dbBE_Redis_result_t ) * res->_data._array._len );

  res->_data._array._data[0]._type = dbBE_REDIS_TYPE_INT;
  res->_data._array._data[0]._data._integer = 0 + idx * 1000;
  res->_data._array._data[1]._type = dbBE_REDIS_TYPE_INT;
  res->_data._array._data[1]._data._integer = 999 + idx * 1000;

  int n;
  for( n = 2; n < res->_data._array._len; ++n )
  {
    res->_data._array._data[n]._type = dbBE_REDIS_TYPE_ARRAY;
    res->_data._array._data[n]._data._array._len = 3;
    res->_data._array._data[n]._data._array._data = (dbBE_Redis_result_t*)malloc( sizeof (dbBE_Redis_result_t ) * res->_data._array._data[n]._data._array._len );

    res->_data._array._data[n]._data._array._data[0]._type = dbBE_REDIS_TYPE_CHAR;
    res->_data._array._data[n]._data._array._data[0]._data._string._data = strdup("127.0.0.1");
    res->_data._array._data[n]._data._array._data[0]._data._string._data[8] += n;
    res->_data._array._data[n]._data._array._data[0]._data._string._size = 9;

    res->_data._array._data[n]._data._array._data[1]._type = dbBE_REDIS_TYPE_INT;
    res->_data._array._data[n]._data._array._data[1]._data._integer = 6300 + idx;

    res->_data._array._data[n]._data._array._data[2]._type = dbBE_REDIS_TYPE_CHAR;
    res->_data._array._data[n]._data._array._data[2]._data._string._data = strdup("DEADBEEF");
    res->_data._array._data[n]._data._array._data[2]._data._string._size = 8;
  }

  return 0;
}

int cluster_info_test()
{
  int rc = 0;

  rc += TEST( dbBE_Redis_cluster_info_create( NULL ), NULL );

  dbBE_Redis_result_t *result = (dbBE_Redis_result_t*)calloc( 1, sizeof( dbBE_Redis_result_t ) );
  rc += TEST_NOT( result, NULL );
  TEST_BREAK( rc, "Result allocation failed" );

  result->_type = dbBE_REDIS_TYPE_CHAR;
  result->_data._string._data = NULL;
  result->_data._string._size = 0;
  rc += TEST( dbBE_Redis_cluster_info_create( result ), NULL );

  result->_type = dbBE_REDIS_TYPE_ARRAY;
  result->_data._array._len = 0;
  rc += TEST( dbBE_Redis_cluster_info_create( result ), NULL );

  result->_data._array._len = DBBE_REDIS_CLUSTER_MAX_SIZE + 1;
  rc += TEST( dbBE_Redis_cluster_info_create( result ), NULL );

  result->_data._array._len = 3;
  result->_data._array._data = (dbBE_Redis_result_t*)malloc( sizeof (dbBE_Redis_result_t ) * result->_data._array._len );
  result->_data._array._data[0]._type = dbBE_REDIS_TYPE_CHAR;
  result->_data._array._data[1]._type = dbBE_REDIS_TYPE_CHAR;
  result->_data._array._data[2]._type = dbBE_REDIS_TYPE_CHAR;
  rc += TEST( dbBE_Redis_cluster_info_create( result ), NULL );

  result->_data._array._data[0]._type = dbBE_REDIS_TYPE_ARRAY;
  result->_data._array._data[0]._data._array._len = 0;
  rc += TEST( dbBE_Redis_cluster_info_create( result ), NULL );

  cluster_info_test_create_server( &result->_data._array._data[0], 0, 1 );
  cluster_info_test_create_server( &result->_data._array._data[1], 1, 2 );
  cluster_info_test_create_server( &result->_data._array._data[2], 2, 4 );

  dbBE_Redis_cluster_info_t *ci;
  rc += TEST_NOT_RC( dbBE_Redis_cluster_info_create( result ), NULL, ci );

  rc += TEST( ci->_cluster_size, 3 );
  rc += TEST( ci->_nodes[ 0 ]->_first_slot, 0 );
  rc += TEST( ci->_nodes[ 1 ]->_first_slot, 1000 );
  rc += TEST( ci->_nodes[ 2 ]->_first_slot, 2000 );
  rc += TEST( ci->_nodes[ 0 ]->_last_slot, 999 );
  rc += TEST( ci->_nodes[ 1 ]->_last_slot, 1999 );
  rc += TEST( ci->_nodes[ 2 ]->_last_slot, 2999 );

  rc += TEST( ci->_nodes[ 0 ]->_server_count, 1 );
  rc += TEST( ci->_nodes[ 1 ]->_server_count, 2 );
  rc += TEST( ci->_nodes[ 2 ]->_server_count, 4 );

  // result cleanup
  // free up the strdup-allocated entries in the result since they are not cleaned up by result_cleanup()
  int node, srv;
  for( node=0; node < ci->_cluster_size; ++node )
    for( srv=0; srv < ci->_nodes[ node ]->_server_count; ++srv )
    {
      free( result->_data._array._data[ node ]._data._array._data[ srv+2]._data._array._data[0]._data._string._data );
      free( result->_data._array._data[ node ]._data._array._data[ srv+2]._data._array._data[2]._data._string._data );
    }
  dbBE_Redis_result_cleanup( result, 1 );

  rc += TEST( dbBE_Redis_cluster_info_destroy( ci ), 0 );

  printf( "Cluster_info_test exiting with rc=%d\n", rc );
  return rc;
}

int single_node_test()
{
  int rc = 0;

  rc += TEST( dbBE_Redis_server_info_create_single( NULL ), NULL );
  rc += TEST( dbBE_Redis_cluster_info_create_single( NULL ), NULL );

  dbBE_Redis_server_info_t *si;
  dbBE_Redis_cluster_info_t *ci;

  // test single node server info-creation from url
  rc += TEST_NOT_RC( dbBE_Redis_server_info_create_single( "sock://localhost:6300" ), NULL, si );

  TEST_BREAK( rc, "NULL-ptr server-info" );
  rc += TEST( si->_first_slot, 0 );
  rc += TEST( si->_last_slot, DBBE_REDIS_HASH_SLOT_MAX - 1 );
  rc += TEST( strncmp( si->_master, "sock://localhost:6300", DBR_SERVER_URL_MAX_LENGTH ), 0 );
  rc += TEST( strncmp( si->_servers[ 0 ], "sock://localhost:6300", DBR_SERVER_URL_MAX_LENGTH ), 0 );
  rc += TEST( si->_server_count, 1 );

  rc += TEST( dbBE_Redis_server_info_destroy( si ), 0 );
  rc += TEST( dbBE_Redis_server_info_destroy( NULL ), -EINVAL );

  // test single node cluster info creation from url
  rc += TEST_NOT_RC( dbBE_Redis_cluster_info_create_single( "sock://localhost:6300" ), NULL, ci );

  TEST_BREAK( rc, "NULL-ptr server-info" );
  rc += TEST( ci->_cluster_size, 1 );
  rc += TEST_NOT( ci->_nodes[0], NULL );
  rc += TEST( ci->_nodes[1], NULL );
  rc += TEST( ci->_nodes[0]->_first_slot, 0 );
  rc += TEST( ci->_nodes[0]->_last_slot, DBBE_REDIS_HASH_SLOT_MAX - 1 );
  rc += TEST( strncmp( ci->_nodes[0]->_master, "sock://localhost:6300", DBR_SERVER_URL_MAX_LENGTH ), 0 );
  rc += TEST( strncmp( ci->_nodes[0]->_servers[0], "sock://localhost:6300", DBR_SERVER_URL_MAX_LENGTH ), 0 );
  rc += TEST( ci->_nodes[0]->_server_count, 1 );

  rc += TEST( dbBE_Redis_cluster_info_destroy( ci ), 0 );
  rc += TEST( dbBE_Redis_cluster_info_destroy( NULL ), -EINVAL );

  printf( "Single_node_test exiting with rc=%d\n", rc );
  return rc;
}


int main( int argc, char ** argv )
{
  int rc = 0;

  rc += server_info_test();
  rc += cluster_info_test();
  rc += single_node_test();

  printf( "Test exiting with rc=%d\n", rc );
  return rc;
}
