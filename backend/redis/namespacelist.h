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

#ifndef BACKEND_REDIS_NAMESPACELIST_H_
#define BACKEND_REDIS_NAMESPACELIST_H_

#include "namespace.h"

typedef struct dbBE_Redis_namespace_list
{
  struct dbBE_Redis_namespace_list *_n;
  struct dbBE_Redis_namespace_list *_p;
  dbBE_Redis_namespace_t *_ns;
} dbBE_Redis_namespace_list_t;


// retrieve namespace list ptr if namespace 'name' exists; otherwise NULL is returned
dbBE_Redis_namespace_list_t* dbBE_Redis_namespace_list_get( dbBE_Redis_namespace_list_t *s,
                                                            const char *name );

// insert namespace into sorted list; only insert if not exists
// attach/refcount needs to be done separately if needed
dbBE_Redis_namespace_list_t* dbBE_Redis_namespace_list_insert( dbBE_Redis_namespace_list_t *s,
                                                               dbBE_Redis_namespace_t * const ns );

// remove namespace from list in case the reference count hits 0 (in that case it's detached/cleaned up too)
// otherwise only detach, i.e. refcnt decrease
dbBE_Redis_namespace_list_t* dbBE_Redis_namespace_list_remove( dbBE_Redis_namespace_list_t *s,
                                                               const dbBE_Redis_namespace_t *ns );

// wipe the namespace list, regardless of content status
int dbBE_Redis_namespace_list_clean( dbBE_Redis_namespace_list_t *s );

#endif /* BACKEND_REDIS_NAMESPACELIST_H_ */
