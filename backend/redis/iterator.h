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

#ifndef BACKEND_REDIS_ITERATOR_H_
#define BACKEND_REDIS_ITERATOR_H_

#include "definitions.h"


/*
 * Iterator idea:
 * - a single API to cover creation, iteration, and (auto-)destruction
 * - start with a Redis SCAN command to a few or just one connection and cache the returned keys
 * - maintain a cache of received keys for subsequent calls to significantly reduce the amount of syscalls/network msgs
 * - prefetch a new chunk of keys whenever the number of cached keys drops below a threshold
 *
 * Challenges with iterator:
 * - how to make sure it's cleaned up properly?
 *   - avoid API for allocation and destruction of iterators
 *   - therefore BE has no idea when a cursor is no longer used (unless it's iterated to completion)
 *   - no allocations, just a fixed list of state
 *   - use free iterator slot
 *   - or destroy the 'coldest', i.e. the one that has not been in use for the longest time
 *   - THIS APPROACH LIMITS THE NESTING LEVEL OF ITERATORS TO THE SIZE OF THE FIXED ITERATOR LIST
 */


// maximum number of simultaneously active iterators
#define DBBE_REDIS_MAX_ITERATOR ( 10 )

// save SCAN cursors for N connections to feed the iterator cache
#define DBBE_REDIS_CONCURRENT_CURSORS ( 3 )

// number of cached entries per iterator
#define DBBE_REDIS_ITERATOR_CACHE_ENTRIES ( DBBE_REDIS_CONCURRENT_CURSORS * 20 )

#define DBBE_REDIS_MAX_CURSOR_LEN ( 64 )

struct dbBE_Redis_connection; // forward decl; we really only need the ptr here

typedef struct dbBE_Redis_iterator
{
  char _cursor[ DBBE_REDIS_MAX_CURSOR_LEN ];   // what Redis is returning/requiring
  struct dbBE_Redis_connection *_connection; // current/next connection to scan
  int _cache_count;      // number of currently cached items
  int _cache_head;      // head of cache (the next item to return to user)
  int _cache_tail;      // tail of cache (where to start prefetching)
  char *_cached_keys;  // locally cached key list
} dbBE_Redis_iterator_t;

typedef dbBE_Redis_iterator_t* dbBE_Redis_iterator_list_t;

static inline
int dbBE_Redis_iterator_reset( dbBE_Redis_iterator_t *it )
{
  if( it == NULL )
    return -EINVAL;

  memset( it->_cursor, 0, DBBE_REDIS_MAX_CURSOR_LEN );
  it->_cursor[0] = '0';
  it->_connection = NULL;
  it->_cache_count = 0;
  it->_cache_head = 0;
  it->_cache_tail = 0;
  memset( it->_cached_keys, 0, DBBE_REDIS_ITERATOR_CACHE_ENTRIES * DBR_MAX_KEY_LEN );
  return 0;
}

// return non-zero if no more Redis SCAN requests need to be sent
// i.e. all cursors have received the terminal 0
static inline
int dbBE_Redis_iterator_remote_complete( dbBE_Redis_iterator_t *it )
{
  return (( it->_connection == NULL ) && ( strncmp( it->_cursor, "0", DBBE_REDIS_MAX_CURSOR_LEN ) == 0 ));
}

// return non-zero if all entries have been consumed
static inline
int dbBE_Redis_iterator_complete( dbBE_Redis_iterator_t *it )
{
  if( it == NULL )
    return -1;
  return ( dbBE_Redis_iterator_remote_complete( it ) && ( it->_cache_count == 0 ));
}

static inline
char *dbBE_Redis_iterator_pop_cached_key( dbBE_Redis_iterator_t *it )
{
  if( it->_cache_count == 0 )
    return NULL;
  char *key = &(it->_cached_keys[ it->_cache_head * DBR_MAX_KEY_LEN ]);
  it->_cache_head = ( it->_cache_head + 1 ) % DBBE_REDIS_ITERATOR_CACHE_ENTRIES;
  --it->_cache_count;

  return key;
}

static inline
void dbBE_Redis_iterator_copy_key( dbBE_sge_t *sge, char* key )
{
  int copylen = strnlen( key, DBR_MAX_KEY_LEN );
  if( copylen > sge->iov_len )
    copylen = sge->iov_len;

  memcpy( sge->iov_base,
          key,
          copylen);
}









static inline
dbBE_Redis_iterator_list_t dbBE_Redis_iterator_list_allocate()
{
  int len = DBBE_REDIS_MAX_ITERATOR;
  dbBE_Redis_iterator_list_t it_list = (dbBE_Redis_iterator_list_t)malloc( sizeof( dbBE_Redis_iterator_t ) * len );
  int n;
  for( n = 0; n < len; ++n )
  {
    dbBE_Redis_iterator_t *it = &it_list[ n ];
    it->_cached_keys = (char*)malloc( DBBE_REDIS_ITERATOR_CACHE_ENTRIES * DBR_MAX_KEY_LEN );
    dbBE_Redis_iterator_reset( it );
  }
  return it_list;
}

static inline
dbBE_Redis_iterator_t* dbBE_Redis_iterator_new( dbBE_Redis_iterator_list_t it_list )
{
  int n;
  dbBE_Redis_iterator_t *it = NULL;

  if( it_list == NULL )
    return NULL;

  for( n = 0; n < DBBE_REDIS_MAX_ITERATOR; ++n )
  {
    if( it_list[ n ]._cache_count == 0 )
    {
      it = &it_list[n];
      dbBE_Redis_iterator_reset( it );
      break;
    }
  }

  return it;
}

static inline
int dbBE_Redis_iterator_free( dbBE_Redis_iterator_list_t it_list,
                              dbBE_Redis_iterator_t *it )
{
  return dbBE_Redis_iterator_reset( it );
}


static inline
int dbBE_Redis_iterator_list_destroy( dbBE_Redis_iterator_list_t it_list )
{
  if( it_list == NULL )
    return -EINVAL;

  int n;
  for( n = 0; n < DBBE_REDIS_MAX_ITERATOR; ++n )
  {
    dbBE_Redis_iterator_t *it = &it_list[ n ];
    dbBE_Redis_iterator_reset( it );
    if( it->_cached_keys != NULL )
    {
      free( it->_cached_keys );
      it->_cached_keys = NULL;
    }
  }
  return 0;
}

#endif /* BACKEND_REDIS_ITERATOR_H_ */
