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

#include "logutil.h"
#include "backend.h"
#include "common/utility.h"

#include <stddef.h>
#include <errno.h>
#include <dlfcn.h>

#ifndef DEFAULT_BE
#define DEFAULT_BE_LIB "libdbbe_redis.so"
#endif


static dbrBackend_t *gBE = NULL;

dbrBackend_t* dbrlib_backend_get_handle(void)
{
  // check backend context and initialize
  dbrBackend_t *be = NULL;
  if( gBE == NULL )
  {
    char *to_str = dbBE_Extract_env( DBR_BACKEND_ENV, DEFAULT_BE_LIB );
    if( to_str == NULL )
    {
      LOG( DBG_ERR, stderr, "libdatabroker: failed to get backend environment variable.\n" );
      goto error;
    }

    be = (dbrBackend_t*)calloc( 1, sizeof( dbrBackend_t ));
    if( be == NULL )
      return NULL;

    if( (be->_library = dlopen( to_str, RTLD_LAZY )) == NULL )
    {
      LOG( DBG_ERR, stderr, "libdatabroker: failed to load Backend Library %s. Looked for in %s\n", to_str, getenv("LD_LIBRARY_PATH") );
      goto error;
    }
    dlerror();
    if( (be->_api = dlsym( be->_library, "dbBE" )) == NULL )
    {
      LOG( DBG_ERR, stderr, "libdatabroker: symbol 'dbBE' not defined in %s\n", to_str );
      goto error;
    }

    be->_context = be->_api->initialize( );
    gBE = be;
  }
  return gBE;

error:
  if( be != NULL )
  {
    if( be->_api != NULL ) be->_api = NULL;
    if( be->_library != NULL )
    {
      dlclose( be->_library );
      be->_library = NULL;
    }
    free( be );
    be = NULL;
    gBE = NULL;
  }
  return (dbBE_Handle_t)be;
}

int dbrlib_backend_delete( dbrBackend_t *be )
{
  if( be == NULL )
    return -EINVAL;

  int rc = 0;
  if( be->_api != NULL )
    rc = be->_api->exit( be->_context );

  if( be->_library != NULL )
    rc = dlclose( be->_library );

  free( be );
  gBE = NULL;
  return rc;
}
