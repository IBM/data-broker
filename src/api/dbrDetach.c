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

#include <string.h>
#include <stdio.h>
#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>
#endif

DBR_Errorcode_t
libdbrDetach (DBR_Handle_t cs_handle)
{
  dbrName_space_t *cs = (dbrName_space_t*)cs_handle;
  if(( cs == NULL ) || ( cs->_reverse == NULL ) || (cs->_status != dbrNS_STATUS_REFERENCED ))
    return DBR_ERR_INVALID;

  BIGLOCK_LOCK( cs->_reverse );

  // try to detach locally
  int ref = cs->_ref_count;

  // if that fails, then don't bother detaching globally because we haven't been attached to it
  if( ref <= 1 )
    BIGLOCK_UNLOCKRETURN( cs->_reverse, DBR_ERR_NSINVAL );

  dbrMain_context_t *ctx = cs->_reverse;

  DBR_Tag_t tag = dbrTag_get( ctx );
  if( tag == DB_TAG_ERROR )
    BIGLOCK_UNLOCKRETURN( cs->_reverse, DBR_ERR_TAGERROR );

  DBR_Errorcode_t rc = DBR_SUCCESS;
  dbrRequestContext_t *rctx = dbrCreate_request_ctx( DBBE_OPCODE_NSDETACH,
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
  {
    rc = DBR_ERR_TAGERROR;
    goto error;
  }

  DBR_Request_handle_t req_handle = dbrPost_request( rctx );
  if( req_handle == NULL )
  {
    rc = DBR_ERR_BE_POST;
    goto error;
  }

  rc = dbrWait_request( cs, req_handle, 0 );
  if( rc != DBR_SUCCESS )
    goto error;

  rc = dbrCheck_response( rctx );
  if( rc != DBR_SUCCESS )
    goto error;

  dbrRemove_request( cs, rctx );

  // try to detach locally
  ref = dbrMain_detach( ctx, cs );

  BIGLOCK_UNLOCKRETURN( ctx, DBR_SUCCESS );
error:

  fprintf( stderr, "failed to detach or name space doesn't exist: %s\n", cs->_db_name );
  if( rctx != NULL )
    dbrRemove_request( cs, rctx );

  // detach locally because we know we're attached
  ref = dbrMain_detach( ctx, cs );
  BIGLOCK_UNLOCK( cs->_reverse );

  if( ref <= 1 )
    return DBR_ERR_NSBUSY;

  return rc;
}
