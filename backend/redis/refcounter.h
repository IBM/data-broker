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

#ifndef BACKEND_REDIS_REFCOUNTER_H_
#define BACKEND_REDIS_REFCOUNTER_H_

#include <stddef.h>
#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>
#endif
#include <string.h>
#include <stdio.h>
#include <errno.h>

// todo: not enabled for multi-threaded access, since only used by parsing, we need to enable only if we start multi-threaded parsing

typedef struct dbBE_Refcounter
{
  volatile uint64_t _up;
  volatile uint64_t _down;
  //pthread_mutex_t *_mutex; // not in use yet, but will likely be needed later
} dbBE_Refcounter_t;

static inline
dbBE_Refcounter_t* dbBE_Refcounter_allocate()
{
  dbBE_Refcounter_t *ref = (dbBE_Refcounter_t*)malloc( sizeof( dbBE_Refcounter_t ));
  if( ref == NULL )
  {
    return NULL;
  }

  memset( ref, 0, sizeof( dbBE_Refcounter_t ) );
  return ref;
}

static inline
int dbBE_Refcounter_destroy( dbBE_Refcounter_t *ref )
{
  if( ref == NULL )
    return -EINVAL;

  memset( ref, 0, sizeof( dbBE_Refcounter_t ) );
  free( ref );
  return 0;
}

static inline
uint64_t dbBE_Refcounter_get( dbBE_Refcounter_t *ref )
{
  if( ref == NULL )
    return -1;
  return ( ref->_up - ref->_down );
}

static inline
uint64_t dbBE_Refcounter_up( dbBE_Refcounter_t *ref )
{
  ++ref->_up;
  return dbBE_Refcounter_get( ref );
}

static inline
uint64_t dbBE_Refcounter_down( dbBE_Refcounter_t *ref )
{
  ++ref->_down;
  return dbBE_Refcounter_get( ref );
}

#endif /* BACKEND_REDIS_REFCOUNTER_H_ */
