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

#ifndef BACKEND_COMMON_REQUEST_SET_H_
#define BACKEND_COMMON_REQUEST_SET_H_

#include <inttypes.h>
#include <stddef.h>
#ifndef __APPLE__
#include <malloc.h>  // malloc
#endif
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <pthread.h>

#include "../common/dbbe_api.h"

typedef struct
{
  dbBE_Request_t **_set;  // holding the data
  size_t _size;             // amount of available space
  size_t _fill;             // actually occupied space
  pthread_mutex_t _mutex;  // for any multithreaded locking
} dbBE_Request_set_t;

/*
 * create and initialize the set
 */
static inline
dbBE_Request_set_t* dbBE_Request_set_create( const size_t size )
{
  dbBE_Request_set_t *set = (dbBE_Request_set_t*)calloc( sizeof( dbBE_Request_set_t ), 1 );
  if( set == NULL )
    return NULL;

  pthread_mutex_init( &set->_mutex, NULL );

  set->_set = (dbBE_Request_t**)calloc( sizeof( dbBE_Request_t* ), size );
  if( set->_set == NULL )
  {
    free( set );
    return NULL;
  }

  set->_size = size;
  set->_fill = 0;
  return set;
}

/*
 * returns 0 if there are no requests in the set
 * returns 1 if there are requests in the set (or set == NULL )
 */
static inline
size_t dbBE_Request_set_empty( const dbBE_Request_set_t *set )
{
  return (set == NULL ) || ( set->_fill == 0 );
}

static inline
size_t dbBE_Request_set_get_len( const dbBE_Request_set_t *set )
{
  if( set != NULL )
    return set->_fill;
  return 0;
}

/*
 * clean all content from the set
 */
static inline
int dbBE_Request_set_clear( dbBE_Request_set_t *set )
{
  if( set == NULL )
    return EINVAL;

  pthread_mutex_lock( &set->_mutex );
  size_t slot;
  for( slot = 0; slot < set->_size; ++slot )
  {
    set->_set[ slot ] = NULL;
    --set->_fill;
  }
  pthread_mutex_unlock( &set->_mutex );
  return 0;
}

/*
 * cleanup and destroy the set
 */
static inline
int dbBE_Request_set_destroy( dbBE_Request_set_t *set )
{
  if( set == NULL )
    return EINVAL;

  // drain the set
  dbBE_Request_set_clear( set );
  pthread_mutex_destroy( &set->_mutex );

  free( set->_set );
  memset( set, 0, sizeof( dbBE_Request_set_t ) );
  free( set );
  return 0;
}

/*
 * inserts request into set
 * returns 0 on success
 * returns ENOSPC if no free slot is available
 */
static inline
int dbBE_Request_set_insert( dbBE_Request_set_t *set,
                             dbBE_Request_t *request )
{
  if(( set == NULL ) || ( request == NULL ))
    return EINVAL;

  size_t slot;
  for( slot = 0; slot < set->_size; ++slot )
    if( set->_set[ slot ] == NULL )
    {
      set->_set[ slot ] = request;
      ++set->_fill;
      return 0;
    }
  return ENOSPC;
}

/*
 * tries to find the request in the set
 * returns 1 if found
 * returns 0 if not found
 */
static inline
int dbBE_Request_set_find( dbBE_Request_set_t *set,
                           dbBE_Request_t *request )
{
  if(( set == NULL ) || ( request == NULL ))
    return 0;
  size_t slot;
  for( slot = 0; slot < set->_size; ++slot )
    if( set->_set[ slot ] == request )
      return 1;

  return 0;
}

/*
 * deletes the request from set
 * returns 0 if nothing got deleted
 * returns 1 if the entry was deleted
 */
static inline
int dbBE_Request_set_delete( dbBE_Request_set_t *set,
                             dbBE_Request_t *request )
{
  if(( set == NULL ) || ( request == NULL ))
    return 0;

  if( dbBE_Request_set_empty( set ) )
    return 0;

  size_t slot;
  for( slot = 0; slot < set->_size; ++slot )
    if( set->_set[ slot ] == request )
    {
      set->_set[ slot ] = NULL;
      --set->_fill;
      return 1;
    }


  return 0;
}

static inline
dbBE_Request_t* dbBE_Request_set_pop( dbBE_Request_set_t *set )
{
  if( set == NULL )
    return NULL;

  size_t slot;
  for( slot = 0; slot < set->_size; ++slot )
    if( set->_set[ slot ] != NULL )
    {
      dbBE_Request_t *request = set->_set[ slot ];
      set->_set[ slot ] = NULL;
      --set->_fill;
      return request;
    }

  return NULL;
}

#endif /* BACKEND_COMMON_REQUEST_SET_H_ */
