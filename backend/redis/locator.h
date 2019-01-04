/*
 * Copyright © 2018 IBM Corporation
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

#ifndef BACKEND_REDIS_LOCATOR_H_
#define BACKEND_REDIS_LOCATOR_H_

#define DBBE_REDIS_HASH_SLOT_MAX ( 16384 )

/*
 * The locator is responsible to keep track of the mapping between
 * Redis hash slots and server addresses
 */

#include <inttypes.h>
#include <netinet/in.h>

#include "address.h"
#include "hash_cover.h"

typedef uint16_t dbBE_Redis_hash_slot_t;
typedef uint16_t dbBE_Redis_locator_index_t;


/*
 * this value signals an invalid index into the locator table
 */
#define DBBE_REDIS_LOCATOR_INDEX_INVAL ( (dbBE_Redis_locator_index_t)-1 )


typedef struct
{
  dbBE_Redis_locator_index_t _index[ DBBE_REDIS_HASH_SLOT_MAX ];
  dbBE_Redis_hash_cover_t *_hash_cover;
} dbBE_Redis_locator_t;

/*
 * create a locator instance and initialize
 */
dbBE_Redis_locator_t* dbBE_Redis_locator_create();

/*
 * destroy locator
 */
int dbBE_Redis_locator_destroy( dbBE_Redis_locator_t *locator );

/*
 * retrieve the address ptr for a given hash slot
 */
dbBE_Redis_locator_index_t dbBE_Redis_locator_get_conn_index( dbBE_Redis_locator_t *locator,
                                                              const dbBE_Redis_hash_slot_t hash_slot );

/*
 * assign or update an address to a hash slot
 */
int dbBE_Redis_locator_assign_conn_index( dbBE_Redis_locator_t *locator,
                                          dbBE_Redis_locator_index_t index,
                                          const dbBE_Redis_hash_slot_t hash_slot );

/*
 * remove an address-to-hashslot assignment
 */
int dbBE_Redis_locator_remove_conn_index( dbBE_Redis_locator_t *locator,
                                          const dbBE_Redis_hash_slot_t hash_slot );

/*
 * resassociate:migrate all hash slots from one address to another
 * this is an expensive O(n) operation
 * can also be used to disassociate after a disconnect
 */
int dbBE_Redis_locator_reassociate_conn_index( dbBE_Redis_locator_t *locator,
                                               const dbBE_Redis_locator_index_t from,
                                               dbBE_Redis_locator_index_t to );

/*
 * calculate crc16 of the key and return the redis hash slot
 */
dbBE_Redis_hash_slot_t dbBE_Redis_locator_hash( const char *key, const uint16_t size );


/*
 * return whether the hash range is covered with valid connections or not
 * todo: the hash_covered member should be restructured into a bitmap or
 *       other structure that includes location info of holes in the hash
 *       coverage to speed up the status updates
 */
int dbBE_Redis_locator_hash_covered( dbBE_Redis_locator_t *locator );


#endif /* BACKEND_REDIS_LOCATOR_H_ */
