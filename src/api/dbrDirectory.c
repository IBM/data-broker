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

DBR_Errorcode_t
libdbrDirectory( DBR_Handle_t cs_handle,
                DBR_Tuple_template_t match_template,
                DBR_Group_t group,
                const unsigned count,
                DBR_Tuple_name_t *result_buffer,
                const size_t size,
                size_t *ret_size )
{
  if( cs_handle == NULL )
    return DBR_ERR_INVALID;

  dbrName_space_t *cs = (dbrName_space_t*)cs_handle;
  if(( cs->_be_ctx == NULL ) || ( cs->_reverse == NULL ))
    return DBR_ERR_NSINVAL;

  BIGLOCK_LOCK( cs->_reverse );

  DBR_Tag_t tag = dbrTag_get( cs->_reverse );
  if( tag == DB_TAG_ERROR )
    BIGLOCK_UNLOCKRETURN( cs->_reverse, DBR_ERR_TAGERROR );

  dbBE_sge_t sge;
  sge._data = result_buffer;
  sge._size = size;

  DBR_Errorcode_t rc = DBR_SUCCESS;
  dbrRequestContext_t *ctx = dbrCreate_request_ctx( DBBE_OPCODE_DIRECTORY,
                                                    cs_handle,
                                                    group,
                                                    NULL,
                                                    NULL,
                                                    1,
                                                    &sge,
                                                    ret_size,
                                                    NULL,
                                                    match_template,
                                                    tag );
  if( ctx == NULL )
  {
    rc = DBR_ERR_NOMEMORY;
    goto error;
  }

  if( dbrInsert_request( cs, ctx ) == DB_TAG_ERROR )
  {
    rc = DBR_ERR_TAGERROR;
    goto error;
  }

  DBR_Request_handle_t req_handle = dbrPost_request( ctx );
  if( req_handle == NULL )
  {
    rc = DBR_ERR_BE_POST;
    goto error;
  }

  rc = dbrWait_request( cs, req_handle, 0 );
  switch( rc ) {
  case DBR_SUCCESS:
    rc = dbrCheck_response( ctx );
    break;
  default:
    goto error;
  }

error:
  dbrRemove_request( cs, ctx );

  BIGLOCK_UNLOCKRETURN( cs->_reverse, rc );
}

