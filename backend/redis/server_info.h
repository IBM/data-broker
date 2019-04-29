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

#ifndef BACKEND_REDIS_SERVER_INFO_H_
#define BACKEND_REDIS_SERVER_INFO_H_

#include "definitions.h"
#include "logutil.h"
#include "locator.h"

#include <errno.h>

/*
 * upper limit for number of replicas per master
 */
#define DBBE_REDIS_CLUSTER_MAX_REPLICA ( 8 )

/*
 * definition to indicate an invalid hash slot
 */
#define DBBE_REDIS_HASH_SLOT_INVAL ( -1 )


typedef struct
{
  int _first_slot;   ///< first hash slot covered by this server
  int _last_slot;    ///< last hash slot covered by this server
  int _server_count; ///< total number of servers for this hash range (master+replicas)
  char *_urls; ///< one single space to hold the replica address urls
  char *_servers[ DBBE_REDIS_CLUSTER_MAX_REPLICA ];  ///< list of servers that serve this slot range (url style for easier search)
  char *_master; ///< points to the current master address in the server list
} dbBE_Redis_server_info_t;

int dbBE_Redis_server_info_destroy( dbBE_Redis_server_info_t *si );

/*
 * create a server info entry from a result (needs to be a sub-result of cluster slots response)
 */
dbBE_Redis_server_info_t* dbBE_Redis_server_info_create( dbBE_Redis_result_t *sir )
{
  /* input sanity check:
   *  - array type
   *  - at least 3 entries (first-, last slot, master address)
   */

  if(( sir == NULL ) || ( sir->_type != dbBE_REDIS_TYPE_ARRAY ) || ( sir->_data._array._len < 3 ))
    return NULL;

  dbBE_Redis_server_info_t *si = (dbBE_Redis_server_info_t*)calloc( 1, sizeof( dbBE_Redis_server_info_t ) );
  if( si == NULL )
  {
    LOG( DBG_ERR, stderr, "Unable to allocate sufficient memory for server info\n" );
    return NULL;
  }

  // check and set number of replicas+master count
  int replica_count = sir->_data._array._len - 2; // -2: subtract the first/last slot entry count
  if(( replica_count < 0 ) || ( replica_count > DBBE_REDIS_CLUSTER_MAX_REPLICA ))
  {
    LOG( DBG_ERR, stderr, "Redis cluster replicas exceed pre-defined limit of %d\n", DBBE_REDIS_CLUSTER_MAX_REPLICA );
    goto error;
  }
  si->_server_count = replica_count;

  // check and set slot range
  if(( sir->_data._array._data[ 0 ]._type != dbBE_REDIS_TYPE_INT ) || ( sir->_data._array._data[ 1 ]._type != dbBE_REDIS_TYPE_INT ))
  {
    LOG( DBG_ERR, stderr, "Expected INT type for slots in CLUSTER SLOTS response\n" );
    goto error;
  }
  si->_first_slot = sir->_data._array._data[ 0 ]._data._integer;
  si->_last_slot = sir->_data._array._data[ 1 ]._data._integer;

  if(( si->_first_slot < 0 ) || ( si->_last_slot < 0 ) ||
      ( si->_first_slot >= DBBE_REDIS_HASH_SLOT_MAX ) || ( si->_last_slot >= DBBE_REDIS_HASH_SLOT_MAX ) ||
      ( si->_first_slot > si->_last_slot ))
  {
    LOG( DBG_ERR, stderr, "Invalid first/last slot settings found in response.\n" );
    goto error;
  }

  si->_urls = (char*)calloc( DBR_SERVER_URL_MAX_LENGTH * (DBBE_REDIS_CLUSTER_MAX_REPLICA + 1 ), sizeof( char ) );
  if( si->_urls == NULL )
  {
    LOG( DBG_ERR, stderr, "Unable to allocate sufficient memory to hold master+replica urls\n" );
    goto error;
  }

  // create master and replica address entries
  int replica;
  char *ip;
  for( replica=0; replica < replica_count; ++replica )
  {
    dbBE_Redis_result_t *node_info = &sir->_data._array._data[ replica + 2 ]; // +2 because server info is offset by first/last slot info
    if( node_info == NULL )
      continue;

    if( node_info->_type != dbBE_REDIS_TYPE_ARRAY )
    {
      LOG( DBG_ERR, stderr, "Expecting ARRAY type for server info in CLUSTER SLOTS response\n" );
      goto error;
    }
    if( node_info->_data._array._len < 3 )
    {
      LOG( DBG_ERR, stderr, "Expecting 3 entries for server info in CLUSTER SLOTS response. Got: %d\n", (int)node_info->_data._array._len );
      goto error;
    }

    if(( node_info->_data._array._data[ 0 ]._type != dbBE_REDIS_TYPE_CHAR ) ||
        ( node_info->_data._array._data[ 1 ]._type != dbBE_REDIS_TYPE_INT ))
    {
      LOG( DBG_ERR, stderr, "Unexpected type(s) for IP:PORT in CLUSTER SLOTS response.\n" );
      goto error;
    }
    ip = node_info->_data._array._data[ 0 ]._data._string._data;
    int64_t port = node_info->_data._array._data[ 1 ]._data._integer;

    if(( port > 65535 ) || ( port <= 0 ))
    {
      LOG( DBG_ERR, stderr, "Invalid port number.\n" );
      goto error;
    }
    si->_servers[ replica ] = &(si->_urls[ DBR_SERVER_URL_MAX_LENGTH * replica ]);
    if( snprintf( si->_servers[ replica ], DBR_SERVER_URL_MAX_LENGTH, "sock://%s:%"PRId64, ip, port ) < 0 )
    {
      LOG( DBG_ERR, stderr, "Unable to create server url for: %s:%d\n", ip, (int)port );
      goto error;
    }
    si->_servers[ replica ][DBR_SERVER_URL_MAX_LENGTH - 1 ] = '\0'; // assure 0-termination
  }

  si->_master = si->_servers[ 0 ];
  return si;

error:
  if( si != NULL )
    dbBE_Redis_server_info_destroy( si );
  return NULL;
}


int dbBE_Redis_server_info_destroy( dbBE_Redis_server_info_t *si )
{
  if( si == NULL )
    return -EINVAL;

  if( si->_urls != NULL )
  {
    memset( si->_urls, 0, DBR_SERVER_URL_MAX_LENGTH * ( DBBE_REDIS_CLUSTER_MAX_REPLICA + 1 ) );
    free( si->_urls );
  }

  memset( si, 0, sizeof( dbBE_Redis_server_info_t ) );

  // crazy assumption of use-after-free, but just in case...
  si->_first_slot = DBBE_REDIS_HASH_SLOT_INVAL;
  si->_last_slot = DBBE_REDIS_HASH_SLOT_INVAL;

  free( si );

  return 0;
}


#endif /* BACKEND_REDIS_SERVER_INFO_H_ */
