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

#include "util/lock_tools.h"
#include "libdatabroker.h"
#include "libdatabroker_int.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>


DBR_Handle_t
libdbrCreate (DBR_Name_t db_name,
              DBR_Tuple_persist_level_t level,
              unsigned int sgec,
              dbBE_sge_t* sgev)
{
  if( db_name == NULL )
  {
    errno = EINVAL;
    return (DBR_Handle_t)NULL;
  }

  DBR_Errorcode_t local_result = DBR_ERR_GENERIC;
  dbrRequestContext_t *rctx = NULL;

  // retrieve/create global main context
  dbrMain_context_t *ctx = dbrCheckCreateMainCTX();
  if( ctx == NULL )
    return NULL;

  // check if back end context was successful
  if( ctx->_be_ctx == NULL )
    return NULL;

  BIGLOCK_LOCK( ctx );
  dbrName_space_t *cs = NULL;

  // check if this name is already tracked in the in-mem table
  uint32_t idx = dbrMain_find( ctx, db_name );
  if( idx != dbrERROR_INDEX )
  {
    // already existing CS is an error - but check the global state to be sure!!
    local_result = DBR_ERR_EXISTS;

    cs = ctx->_cs_list[ idx ];
    if( cs == NULL )
    {
      errno = ENOENT;
      BIGLOCK_UNLOCKRETURN( ctx, (DBR_Handle_t)NULL );
    }
  }
  else
  {
    // create cs instance
    cs = dbrMain_create_local( db_name );
    if( cs == NULL )
    {
      errno = ENOMEM;
      BIGLOCK_UNLOCKRETURN( ctx, (DBR_Handle_t)NULL );
    }

    local_result = DBR_SUCCESS;
  }

  DBR_Tag_t tag = dbrTag_get( cs->_reverse );
  if( tag == DB_TAG_ERROR )
    BIGLOCK_UNLOCKRETURN( ctx, (DBR_Handle_t)NULL );

  rctx = dbrCreate_request_ctx( DBBE_OPCODE_NSCREATE,
                                cs,
                                DBR_GROUP_EMPTY,
                                NULL,
                                DBR_GROUP_EMPTY,
                                sgec,
                                sgev,
                                NULL,
                                NULL,
                                NULL,
                                tag );
  if( rctx == NULL )
    goto error;

  DBR_Tag_t rtag = dbrInsert_request( cs, rctx );
  if( rtag == DB_TAG_ERROR )
    goto error;

  DBR_Request_handle_t create_handle = dbrPost_request( rctx );
  if( create_handle == NULL )
    goto error;

  DBR_Errorcode_t create_rc = dbrWait_request( cs, create_handle, 0 );
  if( create_rc == DBR_ERR_INVALID )
    goto error;

  create_rc = dbrCheck_response( rctx );

  switch( create_rc )
  {
    case DBR_SUCCESS:
      break;
    case DBR_ERR_EXISTS:
      fprintf(stderr, "name space already exists: %s\n", dbrGet_error( create_rc ) );
      goto error;
      break;
    default:
      fprintf(stderr, "name space creation error: %s\n", dbrGet_error( create_rc ) );
      goto error;
      break;
  }

  // assign the returned namespace handle from the backend
  cs->_be_ns_hdl = (dbBE_NS_Handle_t)rctx->_cpl._rc;
  rctx->_status = dbrSTATUS_CLOSED;

  dbrRemove_request( cs, rctx );

  BIGLOCK_UNLOCKRETURN( ctx, (DBR_Handle_t)cs );
error:
  dbrRemove_request( cs, rctx );
  if( local_result == DBR_SUCCESS )
    dbrMain_delete( ctx, cs );

  BIGLOCK_UNLOCKRETURN( ctx, (DBR_Handle_t)NULL );
}
