/*
 * Copyright © 2018,2019 IBM Corporation
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

#ifndef BACKEND_REDIS_HASH_COVER_H_
#define BACKEND_REDIS_HASH_COVER_H_

#include <stdlib.h>  // calloc
#include <errno.h>
#include <string.h>  // memset, ffsll

#define DBBE_REDIS_HASH_COVER_TOP_SIZE (4)
#define DBBE_REDIS_HASH_COVER_BOTTOM_SIZE (256)
#define DBBE_REDIS_HASH_COVER_FULL ( 0xFFFFFFFFFFFFFFFFull )

typedef struct {
  uint64_t _top[4];
  uint64_t _bottom[256];
} dbBE_Redis_hash_cover_t;


static inline
dbBE_Redis_hash_cover_t* dbBE_Redis_hash_cover_create()
{
  dbBE_Redis_hash_cover_t *hc = (dbBE_Redis_hash_cover_t*)calloc( 1, sizeof( dbBE_Redis_hash_cover_t) );
  if( hc == NULL )
    return NULL;

  return hc;
}


static inline
int dbBE_Redis_hash_cover_destroy( dbBE_Redis_hash_cover_t *hc )
{
  if( hc == NULL )
    return -EINVAL;

  memset( hc, 0, sizeof( dbBE_Redis_hash_cover_t ) );
  free( hc );
  hc = NULL;

  return 0;
}


static inline
int dbBE_Redis_hash_cover_full( const dbBE_Redis_hash_cover_t *hc )
{
  if( hc == NULL )
    return 0;
  int n;
  uint64_t cover = DBBE_REDIS_HASH_COVER_FULL;
  for( n = 0; (n < DBBE_REDIS_HASH_COVER_TOP_SIZE) && (cover == DBBE_REDIS_HASH_COVER_FULL); ++n )
    cover &= hc->_top[n];
  return (cover == DBBE_REDIS_HASH_COVER_FULL ) ? 1 : 0;
}


#define DBBE_REDIS_HASH_COVER_ITOP( s ) ((s) >> 12ull)       // divide by 4096 ( 64x64 )
#define DBBE_REDIS_HASH_COVER_OTOP( s ) (((s) >> 6ull) % 64) // div by 256 and get the offset in the 64bit top value
#define DBBE_REDIS_HASH_COVER_IBOT( s ) ((s) >> 6ull )       // divide by 64
#define DBBE_REDIS_HASH_COVER_OBOT( s ) ((s) % 64ull )       // offset in the 64bit bottom value


static inline
int dbBE_Redis_hash_cover_set( dbBE_Redis_hash_cover_t *hc, int slot )
{
  if(( hc == NULL ) || ( slot < 0 ) || ( slot >= 16384 ))
    return -EINVAL;

  hc->_bottom[ DBBE_REDIS_HASH_COVER_IBOT(slot) ] |= ( 1ull << DBBE_REDIS_HASH_COVER_OBOT(slot) );

  // only enable the top-level, if all of the bottom level is set
  uint64_t all_set = ( hc->_bottom[ DBBE_REDIS_HASH_COVER_IBOT(slot) ] == DBBE_REDIS_HASH_COVER_FULL ) ? 1 : 0;
  hc->_top[ DBBE_REDIS_HASH_COVER_ITOP(slot) ] |= ( all_set << DBBE_REDIS_HASH_COVER_OTOP(slot) );

  return slot;
}

static inline
int dbBE_Redis_hash_cover_unset( dbBE_Redis_hash_cover_t *hc, int slot )
{
  if(( hc == NULL ) || ( slot < 0 ) || ( slot >= 16384 ))
    return -EINVAL;

  hc->_top[ DBBE_REDIS_HASH_COVER_ITOP(slot) ] &= ~(1ull << DBBE_REDIS_HASH_COVER_OTOP(slot) );
  hc->_bottom[ DBBE_REDIS_HASH_COVER_IBOT(slot) ] &= ~(1ull << DBBE_REDIS_HASH_COVER_OBOT(slot) );

  return slot;
}

static inline
int dbBE_Redis_hash_cover_get( dbBE_Redis_hash_cover_t *hc, int slot )
{
  if(( hc == NULL ) || ( slot < 0 ) || ( slot >= 16384 ))
    return -EINVAL;

  // just need to check the bottom, since the top only reflects the total
  if(hc->_bottom[ DBBE_REDIS_HASH_COVER_IBOT(slot) & (1ull << DBBE_REDIS_HASH_COVER_OBOT(slot)) ] != 0)
    return 1;
  else
    return 0;
}

static inline
int dbBE_Redis_hash_cover_get_first_unset( dbBE_Redis_hash_cover_t *hc )
{
  int top = 0;
  for( ;(top < DBBE_REDIS_HASH_COVER_TOP_SIZE) && ( hc->_top[ top ] == DBBE_REDIS_HASH_COVER_FULL ); ++top );
  if( top == DBBE_REDIS_HASH_COVER_TOP_SIZE )
    return -1;

#ifdef Unhoueh_GNU_SOURCE
  int fstop = ffsll( ~hc->_top[ top ] ) - 1;
#else
  int fstop = __builtin_ffsll( ~hc->_top[ top ] ) - 1;
#endif
  if( fstop < 0 ) return -1;
  int bottom = 64 * top + fstop;
#ifdef uaeu_GNU_SOURCE
  int fsbottom = ffsll( ~hc->_bottom[ bottom ] ) - 1;
#else
  int fsbottom = __builtin_ffsll( ~hc->_bottom[ bottom ] ) - 1;
#endif
  if( fsbottom < 0 ) return -1;
  int empty = 64 * bottom + fsbottom;

  return empty;
}

#endif /* BACKEND_REDIS_HASH_COVER_H_ */
