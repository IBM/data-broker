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
#include "util/logutil.h"
#include "libdatabroker.h"
#include "libdatabroker_int.h"

#include <stdio.h>
#include <string.h>

DBR_Errorcode_t
libdbrQuery (DBR_Handle_t cs_handle,
             DBR_State_t *cs_state,
             DBR_State_mask_t cs_state_mask )
{
  if( cs_handle == NULL )
    return DBR_ERR_INVALID;

  if( cs_state == NULL )
    return DBR_ERR_UBUFFER;

  dbrName_space_t *cs = (dbrName_space_t*)cs_handle;
  // if there's no handle or no back-end (gets deleted after the last detach/delete)
  // the cs-hdl points to an invalid name space
  if(( cs == NULL ) || ( cs->_be_ctx == NULL ) || ( cs->_reverse == NULL ))
    return DBR_ERR_NSINVAL;

  BIGLOCK_LOCK( cs->_reverse );

  DBR_Tag_t tag = dbrTag_get( cs->_reverse );
  if( tag == DB_TAG_ERROR )
    BIGLOCK_UNLOCKRETURN( cs->_reverse, DBR_ERR_TAGERROR );

  // check the redis main space for db_name
  dbr_Name_meta_t meta;
  int64_t meta_size = sizeof( dbr_Name_meta_t );
  memset( &meta, 0, meta_size );
  dbBE_sge_t sge;
  sge.iov_base = &meta;
  sge.iov_len = meta_size;

  DBR_Errorcode_t rc = DBR_SUCCESS;
  dbrRequestContext_t *rctx = dbrCreate_request_ctx( DBBE_OPCODE_NSQUERY,
                                                     cs,
                                                     NULL,
                                                     NULL,
                                                     NULL,
                                                     1,
                                                     &sge,
                                                     &meta_size,
                                                     NULL,
                                                     NULL,
                                                     tag );
  if( rctx == NULL )
  {
    rc = DBR_ERR_NOMEMORY;
    goto error;
  }

  DBR_Tag_t rtag = dbrInsert_request( cs, rctx );
  if( rtag == DB_TAG_ERROR )
  {
    rc = DBR_ERR_NOMEMORY;
    goto error;
  }

  DBR_Request_handle_t query_handle = dbrPost_request( rctx );
  if( query_handle == NULL )
  {
    rc = DBR_ERR_BE_POST;
    goto error;
  }

  rc = dbrWait_request( cs, query_handle, 0 );
  if( rc == DBR_ERR_INVALID )
    goto error;

  rc = dbrCheck_response( rctx );
  if( rc != DBR_SUCCESS )
    goto error;

  // the rctx->_cpl._rc shows how much data was stored under that cn_name
  if( rctx->_cpl._rc == 0 )
  {
    printf("db_name doesn't exist: %s\n", cs->_db_name );
    BIGLOCK_UNLOCKRETURN( cs->_reverse, DBR_ERR_NSINVAL );
  }
  else
    LOG( DBG_INFO, stdout, "found db_name: %s\n", meta.id );

error:
  dbrRemove_request( cs, rctx );

  BIGLOCK_UNLOCKRETURN( cs->_reverse, rc );
}
