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

#include <stddef.h>
#include <errno.h>
#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>
#endif
#include <string.h>

#include "libdatabroker_int.h"
#include "lib/backend.h"

uint32_t dbrMain_find( dbrMain_context_t *libctx, DBR_Name_t name )
{
  if( libctx == NULL )
    return dbrERROR_INDEX;

  uint32_t n;
  for( n=0; (n<dbrNUM_DB_MAX); ++n )
    if( ( libctx->_cs_list[ n ] != NULL ) &&
        ( libctx->_cs_list[ n ]->_db_name != NULL ) &&
        ( strncmp( libctx->_cs_list[ n ]->_db_name, name, DBR_MAX_KEY_LEN ) == 0 ))
      return n;

  return dbrERROR_INDEX;
}

dbrName_space_t* dbrMain_create_local( DBR_Name_t db_name )
{
  // allocate mem
  dbrName_space_t *cs = (dbrName_space_t*)calloc( 1, sizeof( dbrName_space_t ) );
  if( cs == NULL )
  {
    errno = ENOMEM;
    return (DBR_Handle_t)NULL;
  }

  // initialize the cs data
  cs->_db_name = strdup( db_name );
  cs->_ref_count = 1;
  cs->_be_ctx = dbrlib_backend_get_handle();
  cs->_reverse = dbrCheckCreateMainCTX();
  cs->_status = dbrNS_STATUS_CREATED;

  return cs;
}

// inserts cs into an empty slot regardless of whether an entry with the same name exists or not
uint32_t dbrMain_insert( dbrMain_context_t *libctx, dbrName_space_t *cs )
{
  if( libctx == NULL )
    return dbrERROR_INDEX;

  // find the first hole in the context list
  // todo: current search has O(N) complexity, improve that later!!!
  uint32_t idx = 0;
  while((idx<dbrNUM_DB_MAX) && ( libctx->_cs_list[ idx ] != NULL ))
    ++idx;

  // if we hit the end, then everything is occupied
  // todo: for now just return error, later maybe extend the list
  if( idx == dbrNUM_DB_MAX )
    return dbrERROR_INDEX;

  // insert
  libctx->_cs_list[ idx ] = cs;
  cs->_ref_count = 1;
  cs->_idx = idx;
  cs->_status = dbrNS_STATUS_REFERENCED;

  return idx;
}

int dbrMain_attach( dbrMain_context_t *libctx, dbrName_space_t *cs )
{
  if(( libctx == NULL ) || ( cs == NULL ))
    return -EINVAL;

  // find the first hole in the context list
  // todo: current search has O(N) complexity, improve that later!!!
  uint32_t idx = cs->_idx;
  if( libctx->_cs_list[ idx ] == NULL )
    return -ENOENT;

  // can only detach from referenced or deleted name spaces
  if(( cs->_status != dbrNS_STATUS_REFERENCED) &&
      ( cs->_status != dbrNS_STATUS_CREATED ))
    return -EINVAL;

  ++cs->_ref_count;

  return 0;
}

int dbrMain_detach( dbrMain_context_t *libctx, dbrName_space_t *cs )
{
  if(( libctx == NULL )||( cs == NULL ))
    return -EINVAL;

  // find the first hole in the context list
  // todo: current search has O(N) complexity, improve that later!!!
  uint32_t idx = cs->_idx;
  if( libctx->_cs_list[ idx ] == NULL )
    return -ENOENT;

  // can only detach from referenced or deleted name spaces
  if(( cs->_status != dbrNS_STATUS_REFERENCED) &&
      ( cs->_status != dbrNS_STATUS_DELETED ))
    return -EINVAL;

  // ref count checking
  if( cs->_ref_count == 1 )
  {
    if( cs->_status == dbrNS_STATUS_DELETED )
    {
      return dbrMain_delete( libctx, cs ); // redo the delete call, since we're the last one to detach
    }
    else
      return -EOVERFLOW;
  }
  else
    --cs->_ref_count;

  return 0;
}

int dbrMain_delete( dbrMain_context_t *libctx, dbrName_space_t *cs )
{
  if( cs == NULL )
    return -EINVAL;

  // only delete id there are no other references
  if( cs->_ref_count <= 1 )
  {
    int ref_cnt = cs->_ref_count; // store for use after cleaning cs

    uint32_t idx = cs->_idx;
    memset( cs->_db_name, 0, strlen( cs->_db_name ) );
    free( cs->_db_name );
    memset( cs, 0, sizeof( dbrName_space_t ) );
    free( cs );
    libctx->_cs_list[ idx ] = NULL;

    if( ref_cnt < 1 )
      return -EOVERFLOW;

  }
  else // if there are othere refs, then mark as deleted for the last detach to remove the name space
  {
    cs->_status = dbrNS_STATUS_DELETED;
  }

  return 0;
}
