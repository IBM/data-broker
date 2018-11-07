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

#include <errno.h>
#include <stddef.h>

DBR_Errorcode_t
libdbrDelete (DBR_Name_t db_name)
{
  if( db_name == NULL )
  {
    errno = EINVAL;
    return DBR_ERR_INVALID;
  }

  // retrieve/create global main context
  dbrMain_context_t *ctx = dbrCheckCreateMainCTX();
  if( ctx == NULL )
  {
    errno = EINVAL;
    return DBR_ERR_GENERIC;
  }

  BIGLOCK_LOCK( ctx );

  // check if this name is tracked in the in-mem table
  uint32_t idx = dbrMain_find( ctx, db_name );
  if( idx == dbrERROR_INDEX )
  {
    errno = ENOENT;
    BIGLOCK_UNLOCKRETURN( ctx, DBR_ERR_NSINVAL );
  }

  // did we find a valid entry?
  dbrName_space_t *cs = ctx->_cs_list[ idx ];
  if( cs == NULL )
  {
    errno = ENOENT;
    BIGLOCK_UNLOCKRETURN( ctx, DBR_ERR_NSINVAL );
  }

  // don't bother deleting yet because we have some other parties attached
  if( cs->_ref_count > 1 )
    BIGLOCK_UNLOCKRETURN( ctx, DBR_ERR_NSBUSY );  // temporarily return INPROGRESS to signal that delete needs to be called again later

  // find the cs-name in redis and remove the entry
  // check if back end context was successul
  if( ctx->_be_ctx == NULL )
  {
    errno = ENOTCONN;
    BIGLOCK_UNLOCKRETURN( ctx, DBR_ERR_NOCONNECT );
  }

  DBR_Tag_t tag = dbrTag_get( cs->_reverse );
  if( tag == DB_TAG_ERROR )
    BIGLOCK_UNLOCKRETURN( ctx, DBR_ERR_TAGERROR );

  DBR_Errorcode_t rc = DBR_SUCCESS;
  dbrRequestContext_t *rctx = dbrCreate_request_ctx( DBBE_OPCODE_NSDELETE,
                                                     cs,
                                                     NULL,
                                                     NULL,
                                                     NULL,
                                                     0,
                                                     NULL,
                                                     NULL,
                                                     NULL,
                                                     NULL,
                                                     tag );
  if( rctx == NULL )
  {
    rc = DBR_ERR_NOMEMORY;
    goto error;
  }

  DBR_Tag_t dtag = dbrInsert_request( cs, rctx );
  if( dtag == DB_TAG_ERROR )
  {
    rc = DBR_ERR_BE_POST;
    goto error;
  }

  DBR_Request_handle_t del_handle = dbrPost_request( rctx );
  if( del_handle == NULL )
  {
    rc = DBR_ERR_BE_POST;
    goto error;
  }

  rc = dbrWait_request( cs, del_handle, 0 );
  if( rc == DBR_ERR_INVALID )
    goto error;

  rc = dbrCheck_response( rctx );
  if( rc != DBR_SUCCESS )
    goto error;

  dbrRemove_request( cs, rctx );
  if( dbrMain_delete( ctx, cs ) != 0 )
    rc = DBR_ERR_NSINVAL;
  else
    rc = DBR_SUCCESS;

  BIGLOCK_UNLOCKRETURN( ctx, rc );

error:
  dbrRemove_request( cs, rctx );
  BIGLOCK_UNLOCKRETURN( ctx, rc );
}
