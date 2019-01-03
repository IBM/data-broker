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

#include "util/lock_tools.h"
#include "libdatabroker.h"
#include "libdatabroker_int.h"

#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>
#include <stdlib.h>
#endif
#include <stdio.h>
#include <string.h>
#include <errno.h>

/*
 * check local existence
 *   - if exists: bump refcount
 *   - if not exists: create new
 *
 * contact/update global refcount
 *   - fail: name space not existent -> delete locally too
 *   - success:
 *      - if didn't exist: insert to local tracking
 */



DBR_Handle_t
libdbrAttach (DBR_Name_t db_name)
{
  if( db_name == NULL )
  {
    errno = EINVAL;
    return (DBR_Handle_t)NULL;
  }

  // retrieve/create global main context
  dbrMain_context_t *ctx = dbrCheckCreateMainCTX();
  if( ctx == NULL )
    return (DBR_Handle_t)NULL;

  BIGLOCK_LOCK( ctx );

  dbrName_space_t *cs = NULL;
  dbrName_space_t *cs_tmp = NULL;
  DBR_Errorcode_t local_result = DBR_SUCCESS;

  // check if this name is already tracked in the in-mem table
  uint32_t idx = dbrMain_find( ctx, db_name );
  if( idx != dbrERROR_INDEX )
  {
    cs = ctx->_cs_list[ idx ];
    if( cs == NULL )
    {
      errno = ENOENT;
      BIGLOCK_UNLOCKRETURN( ctx, (DBR_Handle_t)NULL );
    }
    ++cs->_ref_count;
  }
  else
  {
    errno = ENOENT;
    local_result = DBR_ERR_NSINVAL;
    cs_tmp = dbrMain_create_local( db_name );
    cs = cs_tmp;
  }

  DBR_Tag_t tag = dbrTag_get( cs->_reverse );
  if( tag == DB_TAG_ERROR )
    BIGLOCK_UNLOCKRETURN( ctx, (DBR_Handle_t)NULL );

  dbrRequestContext_t *rctx = dbrCreate_request_ctx( DBBE_OPCODE_NSATTACH,
                                                     cs,
                                                     DBR_GROUP_EMPTY,
                                                     NULL,
                                                     DBR_GROUP_EMPTY,
                                                     0,
                                                     NULL,
                                                     NULL,
                                                     NULL,
                                                     NULL,
                                                     tag );
  if( rctx == NULL )
    goto error;

  DBR_Tag_t rtag = dbrInsert_request( cs, rctx );
  if( rtag == DB_TAG_ERROR )
    goto error;

  DBR_Request_handle_t attach_handle = dbrPost_request( rctx );
  if( attach_handle == NULL )
    goto error;

  DBR_Errorcode_t attach_rc = dbrWait_request( cs, attach_handle, 0 );
  if( attach_rc != DBR_SUCCESS )
    goto error;

  if( dbrCheck_response( rctx ) != DBR_SUCCESS )
  {
    printf("name space doesn't exist: %s\n", db_name );
    goto error;
  }

  // namespace exists while local info is missing
  // create local info
  if( local_result == DBR_ERR_NSINVAL )
  {
    // create cs instance
    cs = cs_tmp;
    if( cs == NULL )
    {
      errno = ENOMEM;
      goto error;
    }

    // insert to global context table for ref-counting
    idx = dbrMain_insert( ctx, cs );
    if( cs->_idx == dbrERROR_INDEX )
    {
      errno = ETOOMANYREFS;
      goto error;
    }
    local_result = DBR_SUCCESS;

    int rc = dbrMain_attach( ctx, cs );
    if( rc != 0 )
    {
      errno = -rc;
      goto error;
    }
  }

  dbrRemove_request( cs, rctx );
  BIGLOCK_UNLOCKRETURN( ctx, (DBR_Handle_t)cs );

error:
  if( local_result == DBR_SUCCESS )
    --cs->_ref_count;   // restore the ref counter
  dbrRemove_request( cs, rctx );
  if( cs_tmp != NULL )
  {
    dbrMain_detach( ctx, cs_tmp );
    dbrMain_delete( ctx, cs_tmp );
  }
  BIGLOCK_UNLOCKRETURN( ctx, (DBR_Handle_t)NULL );
}
