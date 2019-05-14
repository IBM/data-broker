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


static inline
int dbBE_Redis_server_info_getsize( dbBE_Redis_server_info_t *si )
{
  return ( si != NULL ) ? si->_server_count : 0;
}

static inline
int dbBE_Redis_server_info_get_first_slot( dbBE_Redis_server_info_t *si )
{
  return ( si != NULL ) ? si->_first_slot : DBBE_REDIS_HASH_SLOT_INVAL;
}

static inline
int dbBE_Redis_server_info_get_last_slot( dbBE_Redis_server_info_t *si )
{
  return ( si != NULL ) ? si->_last_slot : DBBE_REDIS_HASH_SLOT_INVAL;
}

static inline
char* dbBE_Redis_server_info_get_replica( dbBE_Redis_server_info_t *si, const int index )
{
  return ( si != NULL ) && ( index >= 0 ) && ( index < si->_server_count ) ? si->_servers[ index ] : NULL;
}

static inline
char* dbBE_Redis_server_info_get_master( dbBE_Redis_server_info_t *si )
{
  return ( si != NULL ) ? si->_master : NULL;
}

/*
 * update the ptr of the master to point to replica with index
 */
static inline
int dbBE_Redis_server_info_update_master( dbBE_Redis_server_info_t *si, const int index )
{
  if(( si == NULL ) || ( index < 0 ) || ( index >= si->_server_count ))
    return -EINVAL;

  si->_master = si->_servers[ index ];
  return 0;
}

/*
 * create a server info entry from a result (needs to be a sub-result of cluster slots response)
 */
dbBE_Redis_server_info_t* dbBE_Redis_server_info_create( dbBE_Redis_result_t *sir );

/*
 * create server info for a single-node Redis server
 */
dbBE_Redis_server_info_t* dbBE_Redis_server_info_create_single( char *url );

/*
 * cleanup and free the server info structure
 */
int dbBE_Redis_server_info_destroy( dbBE_Redis_server_info_t *si );


#endif /* BACKEND_REDIS_SERVER_INFO_H_ */
