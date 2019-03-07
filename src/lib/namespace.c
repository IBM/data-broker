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

#include "logutil.h"
#include "libdatabroker_int.h"
#include "lib/backend.h"

#include <stddef.h>
#include <errno.h>
#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>
#endif
#include <string.h>


/* crazy work-around for memset_s being pretty much un-portable */
static
volatile void* memzero( void *buf, int val, size_t s )
{
  char *b = (char*)buf;
  memset( b, val, s );
  size_t n;
  uint64_t sum = 0;
  for( n=0; n < s; ++n )
    sum += (uint64_t)b[n];
  if(  sum != 0 )
  {
    LOG( DBG_ERR, stderr, "Failed to zero-out memory\n" );
    return NULL;
  }
  return buf;
}

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
  cs->_reverse = dbrCheckCreateMainCTX();
  cs->_be_ctx = dbrlib_backend_get_handle();
  cs->_status = dbrNS_STATUS_CREATED;
  cs->_idx = dbrERROR_INDEX;

  if( dbrMain_insert( cs->_reverse, cs ) == dbrERROR_INDEX )
  {
    LOG( DBG_ERR, stderr, "Reference count error detected while creating namespace.\n" );
    memset( cs, 0, sizeof( dbrName_space_t ) );
    free(cs);
    cs = NULL;
  }
  return cs;
}

// inserts cs into an empty slot (idempotent if already inserted before)
uint32_t dbrMain_insert( dbrMain_context_t *libctx, dbrName_space_t *cs )
{
  if(( libctx == NULL ) || ( cs == NULL ))
    return dbrERROR_INDEX;

  uint32_t idx = 0;
  if( cs->_idx == dbrERROR_INDEX ) // not yet inserted?
  {
    // find the first hole in the context list
    // todo: current search has O(N) complexity, improve that later!!!
    while((idx<dbrNUM_DB_MAX) && ( libctx->_cs_list[ idx ] != NULL ))
      ++idx;

    // if we hit the end, then everything is occupied
    // todo: for now just return error, later maybe extend the list
    if( idx == dbrNUM_DB_MAX )
      return dbrERROR_INDEX;
  }
  else
    idx = cs->_idx;

  // insert
  if(( libctx->_cs_list[ idx ] != NULL ) && ( libctx->_cs_list[ idx ] != cs ))
  {
    LOG( DBG_ERR, stderr, "Inconsistent namespace table.\n" );
    return dbrERROR_INDEX;
  }

  // do nothing if already inserted
  if( libctx->_cs_list[ idx ] == cs )
    return idx;

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

  // check consistency
  uint32_t idx = cs->_idx;
  if(( libctx->_cs_list[ idx ] == NULL ) || ( cs != libctx->_cs_list[ idx ] ))
  {
    LOG( DBG_ERR, stderr, "Inconsistent Namespace table.\n" );
    return -ENOENT;
  }

  // can only detach from referenced or deleted name spaces
  if(( cs->_status != dbrNS_STATUS_REFERENCED) &&
      ( cs->_status != dbrNS_STATUS_CREATED ))
    return -EINVAL;

  ++cs->_ref_count;

  return 0;
}


static int
dbrMain_delete_local( dbrMain_context_t *libctx, dbrName_space_t *cs )
{
  int rc = 0;
  if(( cs == NULL ) || ( libctx == NULL ))
    return -EINVAL;

  if( cs->_status != dbrNS_STATUS_DELETED )
  {
    LOG( DBG_ERR, stderr, "Reference count error: expected namespace status DELETED.\n" );
  }

  uint32_t idx = cs->_idx;
  memzero( cs->_db_name, 0, strlen( cs->_db_name ) );
  if( cs->_db_name[0] != 0 )
    rc = -EFAULT;

  free( cs->_db_name );
  memzero( cs, 0, sizeof( dbrName_space_t ) );
  if(( cs->_ref_count != 0 )||( cs->_idx != 0 )||(cs->_status != dbrNS_STATUS_UNDEFINED))
    rc = -EFAULT;

  free( cs );
  libctx->_cs_list[ idx ] = NULL;

  return rc;
}

int dbrMain_detach( dbrMain_context_t *libctx, dbrName_space_t *cs )
{
  if(( libctx == NULL )||( cs == NULL ))
    return -EINVAL;

  // check consistency
  uint32_t idx = cs->_idx;
  if(( libctx->_cs_list[ idx ] == NULL ) || ( cs != libctx->_cs_list[ idx ] ))
  {
    LOG( DBG_ERR, stderr, "Inconsistent Namespace table.\n" );
    return -ENOENT;
  }

  // can only detach from referenced or deleted name spaces
  if(( cs->_status != dbrNS_STATUS_REFERENCED) &&
      ( cs->_status != dbrNS_STATUS_DELETED ))
    return -EINVAL;

  // ref count + check
  --cs->_ref_count;
  if( cs->_ref_count < 1 )
  {
    // remove from namespace list
    cs->_status = dbrNS_STATUS_DELETED;
    return dbrMain_delete_local( libctx, cs );
  }

  return 0;
}

int dbrMain_delete( dbrMain_context_t *libctx, dbrName_space_t *cs )
{
  if(( cs == NULL ) || ( libctx == NULL ))
    return -EINVAL;

  if( cs->_status == dbrNS_STATUS_DELETED )
    return -EBADF;

  cs->_status = dbrNS_STATUS_DELETED;
  return 0;
}
