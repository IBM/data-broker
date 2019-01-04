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

#include <stddef.h>  // NULL
#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>  // malloc
#endif
#include <errno.h>   // error values
#include <string.h>  // memset
#include <inttypes.h>

#include "locator.h"
#include "crc16.h"

#define DBBE_REDIS_HASH_SLOT_MASK ( 0x3FFF )

/*
 * create a locator instance and initialize
 */
dbBE_Redis_locator_t* dbBE_Redis_locator_create()
{
  dbBE_Redis_locator_t *locator = (dbBE_Redis_locator_t*)malloc( sizeof( dbBE_Redis_locator_t ) );
  if( locator == NULL )
    return NULL;

  dbBE_Redis_hash_slot_t i;
  for( i = 0; i < DBBE_REDIS_HASH_SLOT_MAX; ++i )
  {
    locator->_index[ i ] = DBBE_REDIS_LOCATOR_INDEX_INVAL;
  }
  locator->_hash_cover = dbBE_Redis_hash_cover_create();
  return locator;
}

/*
 * destroy locator
 */
int dbBE_Redis_locator_destroy( dbBE_Redis_locator_t *locator )
{
  if( locator == NULL )
    return -EINVAL;

  int rc = 0;
  rc = dbBE_Redis_hash_cover_destroy( locator->_hash_cover );

  memset( locator, 0, sizeof( dbBE_Redis_locator_t ) );
  free( locator );
  locator = NULL;
  return rc;
}

/*
 * retrieve the address ptr for a given hash slot
 */
dbBE_Redis_locator_index_t dbBE_Redis_locator_get_conn_index( dbBE_Redis_locator_t *locator,
                                                              const dbBE_Redis_hash_slot_t hash_slot )
{
  if(( locator != NULL ) && ( hash_slot == (hash_slot & DBBE_REDIS_HASH_SLOT_MASK) ))
    return locator->_index[ hash_slot ];
  else
    return DBBE_REDIS_LOCATOR_INDEX_INVAL;
}

/*
 * assign an address to a hash slot
 */
int dbBE_Redis_locator_assign_conn_index( dbBE_Redis_locator_t *locator,
                                          dbBE_Redis_locator_index_t index,
                                          const dbBE_Redis_hash_slot_t hash_slot )
{
  if(( locator != NULL ) && ( hash_slot == (hash_slot & DBBE_REDIS_HASH_SLOT_MASK) ))
  {
    locator->_index[ hash_slot ] = index;
    if( index == DBBE_REDIS_LOCATOR_INDEX_INVAL )
      dbBE_Redis_hash_cover_unset( locator->_hash_cover, hash_slot );
    else
      dbBE_Redis_hash_cover_set( locator->_hash_cover, hash_slot );
    return 0;
  }
  else
    return -EINVAL;
}

/*
 * remove an address-to-hashslot assignment
 */
int dbBE_Redis_locator_remove_conn_index( dbBE_Redis_locator_t *locator,
                                          const dbBE_Redis_hash_slot_t hash_slot )
{
  if(( locator != NULL ) && ( hash_slot == (hash_slot & DBBE_REDIS_HASH_SLOT_MASK) ))
  {
    locator->_index[ hash_slot ] = DBBE_REDIS_LOCATOR_INDEX_INVAL;
    dbBE_Redis_hash_cover_unset( locator->_hash_cover, hash_slot );
    return 0;
  }
  else
    return -EINVAL;
}

/*
 * resassociate:migrate all hash slots from one address to another
 * this is an expensive O(n) operation
 * can also be used to disassociate after a disconnect
 * also allows 'from' to be NULL to assign all empty slots to one address
 */
int dbBE_Redis_locator_reassociate_conn_index( dbBE_Redis_locator_t *locator,
                                               const dbBE_Redis_locator_index_t from,
                                               dbBE_Redis_locator_index_t to )
{
  if( locator == NULL )
    return -EINVAL;
  int updated = 0;
  dbBE_Redis_hash_slot_t i;
  for( i = 0; i < DBBE_REDIS_HASH_SLOT_MAX; ++i )
  {
    if( locator->_index[ i ] == from )
    {
      locator->_index[ i ] = to;
      if( to == DBBE_REDIS_LOCATOR_INDEX_INVAL )
        dbBE_Redis_hash_cover_unset( locator->_hash_cover, i );
      else
        dbBE_Redis_hash_cover_set( locator->_hash_cover, i );
      ++updated;
    }
  }
  return updated;
}

dbBE_Redis_hash_slot_t dbBE_Redis_locator_hash( const char *key, const uint16_t size )
{
  return (dbBE_Redis_hash_slot_t)crcremainder( key, size ) & DBBE_REDIS_HASH_SLOT_MASK;
}


int dbBE_Redis_locator_hash_covered( dbBE_Redis_locator_t *locator )
{
  if( locator == NULL )
    return 0;

  return dbBE_Redis_hash_cover_full( locator->_hash_cover );
}
