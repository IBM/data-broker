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

#ifndef BACKEND_REDIS_NAMESPACE_H_
#define BACKEND_REDIS_NAMESPACE_H_

#include "libdatabroker.h"

#include <inttypes.h> // int64_t
#include <stddef.h> // NULL
#include <errno.h> // errno values
#include <string.h> // strncpy and more
#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>
#endif

typedef struct dbBE_Redis_namespace
{
  int64_t _chksum; // a simple checksum to allow some validity checks; e.g. for use-after-free cases
  uint32_t _refcnt;     // local reference counting
  uint32_t _len;        // length of the namespace string to speed up length calculation
  char _name[0];   // space holder for the actual namespace string
} dbBE_Redis_namespace_t;


#define dbBE_Redis_namespace_get_name( ns ) ( (ns)->_name )
#define dbBE_Redis_namespace_get_len( ns ) ( (ns)->_len )
#define dbBE_Redis_namespace_get_refcnt( ns ) ( (ns)->_refcnt )

dbBE_Redis_namespace_t* dbBE_Redis_namespace_create( const char *name );
int dbBE_Redis_namespace_destroy( dbBE_Redis_namespace_t *ns );
int dbBE_Redis_namespace_attach( dbBE_Redis_namespace_t *ns );
int dbBE_Redis_namespace_detach( dbBE_Redis_namespace_t *ns );

#endif /* BACKEND_REDIS_NAMESPACE_H_ */
