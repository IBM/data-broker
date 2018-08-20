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

#include <stdio.h>


DBR_Errorcode_t libdbrMove ( DBR_Handle_t src_cs_handle,
                             DBR_Group_t src_group,
                             DBR_Tuple_name_t tuple_name,
                             DBR_Tuple_template_t match_template,
                             DBR_Handle_t dest_cs_handle,
                             DBR_Group_t dest_group)
{
  DBR_Errorcode_t rc = DBR_SUCCESS;

  if( src_cs_handle == NULL || dest_cs_handle == NULL )
    return DBR_ERR_INVALID;

  dbrName_space_t *src_cs = (dbrName_space_t*)src_cs_handle;
  if( src_cs->_be_ctx == NULL )
    return DBR_ERR_NSINVAL;

  dbrName_space_t *dst_cs = (dbrName_space_t*)dest_cs_handle;
  if( dst_cs->_be_ctx == NULL )
    return DBR_ERR_NSINVAL;

  if( src_cs->_reverse == NULL )
    return DBR_ERR_NSINVAL;

  BIGLOCK_LOCK( src_cs->_reverse );


  // src_cs->_reverse == dst_cs->_reverse
  DBR_Tag_t tag = dbrTag_get( src_cs->_reverse ); // reverse points always to the same dbrMain_context
  if( tag == DB_TAG_ERROR )
    BIGLOCK_UNLOCKRETURN( src_cs->_reverse, DBR_ERR_TAGERROR );

  dbrRequestContext_t *rctx = dbrCreate_request_ctx( DBBE_OPCODE_MOVE,
                                                    src_cs_handle,
                                                    src_group,
                                                    dest_cs_handle,
                                                    dest_group,
                                                    0,
                                                    NULL,
                                                    NULL,
                                                    tuple_name,
                                                    match_template,
                                                    tag );
  if( rctx == NULL )
  {
    rc = DBR_ERR_NOMEMORY;
    goto error;
  }

  // assumption: insert the request to the source and hope that the source knows how to handle it over to the dest.
  if( dbrInsert_request( src_cs, rctx ) == DB_TAG_ERROR )
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

  rc = dbrWait_request( src_cs, req_handle, 0 );
  switch( rc ) {
  case DBR_SUCCESS:
    rc = dbrCheck_response( rctx );
    break;
  case DBR_ERR_INPROGRESS:
    rc = DBR_ERR_TIMEOUT;
    break;
  default:
    goto error;
  }

error:
  dbrRemove_request( src_cs, rctx );

  BIGLOCK_UNLOCKRETURN( src_cs->_reverse, rc );
}
