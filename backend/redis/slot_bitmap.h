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

#ifndef BACKEND_REDIS_SLOT_BITMAP_H_
#define BACKEND_REDIS_SLOT_BITMAP_H_

#include <stdlib.h>  // calloc
#include <errno.h>
#include <string.h>  // memset, ffsll

#define DBBE_REDIS_SLOT_BITMAP_TOP_SIZE (4)
#define DBBE_REDIS_SLOT_BITMAP_BOTTOM_SIZE (256)
#define DBBE_REDIS_SLOT_BITMAP_FULL ( 0xFFFFFFFFFFFFFFFFull )

typedef struct {
  uint64_t _top[4];
  uint64_t _bottom[256];
} dbBE_Redis_slot_bitmap_t;


static inline
dbBE_Redis_slot_bitmap_t* dbBE_Redis_slot_bitmap_create()
{
  dbBE_Redis_slot_bitmap_t *sb = (dbBE_Redis_slot_bitmap_t*)calloc( 1, sizeof( dbBE_Redis_slot_bitmap_t) );
  if( sb == NULL )
    return NULL;

  return sb;
}


static inline
int dbBE_Redis_slot_bitmap_destroy( dbBE_Redis_slot_bitmap_t *sb )
{
  if( sb == NULL )
    return -EINVAL;

  memset( sb, 0, sizeof( dbBE_Redis_slot_bitmap_t ) );
  free( sb );
  sb = NULL;

  return 0;
}


static inline
int dbBE_Redis_slot_bitmap_full( const dbBE_Redis_slot_bitmap_t *sb )
{
  if( sb == NULL )
    return 0;
  int n;
  uint64_t cover = DBBE_REDIS_SLOT_BITMAP_FULL;
  for( n = 0; (n < DBBE_REDIS_SLOT_BITMAP_TOP_SIZE) && (cover == DBBE_REDIS_SLOT_BITMAP_FULL); ++n )
    cover &= sb->_top[n];
  return (cover == DBBE_REDIS_SLOT_BITMAP_FULL ) ? 1 : 0;
}


#define DBBE_REDIS_SLOT_BITMAP_ITOP( s ) ((s) >> 12ull)       // divide by 4096 ( 64x64 )
#define DBBE_REDIS_SLOT_BITMAP_OTOP( s ) (((s) >> 6ull) % 64) // div by 256 and get the offset in the 64bit top value
#define DBBE_REDIS_SLOT_BITMAP_IBOT( s ) ((s) >> 6ull )       // divide by 64
#define DBBE_REDIS_SLOT_BITMAP_OBOT( s ) ((s) % 64ull )       // offset in the 64bit bottom value


static inline
int dbBE_Redis_slot_bitmap_set( dbBE_Redis_slot_bitmap_t *sb, int slot )
{
  if(( sb == NULL ) || ( slot < 0 ) || ( slot >= 16384 ))
    return -EINVAL;

  sb->_bottom[ DBBE_REDIS_SLOT_BITMAP_IBOT(slot) ] |= ( 1ull << DBBE_REDIS_SLOT_BITMAP_OBOT(slot) );

  // only enable the top-level, if all of the bottom level is set
  uint64_t all_set = ( sb->_bottom[ DBBE_REDIS_SLOT_BITMAP_IBOT(slot) ] == DBBE_REDIS_SLOT_BITMAP_FULL ) ? 1 : 0;
  sb->_top[ DBBE_REDIS_SLOT_BITMAP_ITOP(slot) ] |= ( all_set << DBBE_REDIS_SLOT_BITMAP_OTOP(slot) );

  return slot;
}

static inline
int dbBE_Redis_slot_bitmap_unset( dbBE_Redis_slot_bitmap_t *sb, int slot )
{
  if(( sb == NULL ) || ( slot < 0 ) || ( slot >= 16384 ))
    return -EINVAL;

  sb->_top[ DBBE_REDIS_SLOT_BITMAP_ITOP(slot) ] &= ~(1ull << DBBE_REDIS_SLOT_BITMAP_OTOP(slot) );
  sb->_bottom[ DBBE_REDIS_SLOT_BITMAP_IBOT(slot) ] &= ~(1ull << DBBE_REDIS_SLOT_BITMAP_OBOT(slot) );

  return slot;
}

static inline
int dbBE_Redis_slot_bitmap_get( dbBE_Redis_slot_bitmap_t *sb, int slot )
{
  if(( sb == NULL ) || ( slot < 0 ) || ( slot >= 16384 ))
    return -EINVAL;

  // just need to check the bottom, since the top only reflects the total
  if(sb->_bottom[ DBBE_REDIS_SLOT_BITMAP_IBOT(slot) & (1ull << DBBE_REDIS_SLOT_BITMAP_OBOT(slot)) ] != 0)
    return 1;
  else
    return 0;
}

static inline
int dbBE_Redis_slot_bitmap_get_first_unset( dbBE_Redis_slot_bitmap_t *sb )
{
  int top = 0;
  for( ;(top < DBBE_REDIS_SLOT_BITMAP_TOP_SIZE) && ( sb->_top[ top ] == DBBE_REDIS_SLOT_BITMAP_FULL ); ++top );
  if( top == DBBE_REDIS_SLOT_BITMAP_TOP_SIZE )
    return -1;

#ifdef Unhoueh_GNU_SOURCE
  int fstop = ffsll( ~sb->_top[ top ] ) - 1;
#else
  int fstop = __builtin_ffsll( ~sb->_top[ top ] ) - 1;
#endif
  if( fstop < 0 ) return -1;
  int bottom = 64 * top + fstop;
#ifdef uaeu_GNU_SOURCE
  int fsbottom = ffsll( ~sb->_bottom[ bottom ] ) - 1;
#else
  int fsbottom = __builtin_ffsll( ~sb->_bottom[ bottom ] ) - 1;
#endif
  if( fsbottom < 0 ) return -1;
  int empty = 64 * bottom + fsbottom;

  return empty;
}

#endif /* BACKEND_REDIS_SLOT_BITMAP_H_ */
