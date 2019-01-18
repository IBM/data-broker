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
#include "logutil.h"
#include "util/lock_tools.h"
#include "libdatabroker.h"
#include "libdatabroker_int.h"

#include <stdio.h>

DBR_Tag_t libdbrPutA (DBR_Handle_t cs_handle,
                      void *va_ptr,
                      int64_t size,
                      DBR_Tuple_name_t tuple_name,
                      DBR_Group_t group)
{
  dbrName_space_t *cs = (dbrName_space_t*)cs_handle;
  if(( cs == NULL ) || ( cs->_be_ctx == NULL ) || ( cs->_reverse == NULL ) || (cs->_status != dbrNS_STATUS_REFERENCED ))
  {
    LOG( DBG_ERR, stderr, "Invalid input name space handle\n" );
    return DB_TAG_ERROR;
  }

  BIGLOCK_LOCK( cs->_reverse );

  DBR_Tag_t tag = dbrTag_get( cs->_reverse );
  if( tag == DB_TAG_ERROR )
  {
    LOG( DBG_ERR, stderr, "Invalid input name space handle\n" );
    BIGLOCK_UNLOCKRETURN( cs->_reverse, DB_TAG_ERROR );
  }
  dbBE_sge_t sge;
  sge.iov_base = va_ptr;
  sge.iov_len = size;

  dbrRequestContext_t *pctx = dbrCreate_request_ctx( DBBE_OPCODE_PUT,
                                                     cs_handle,
                                                     group,
                                                     NULL,
                                                     DBR_GROUP_EMPTY,
                                                     1,
                                                     &sge,
                                                     &size,
                                                     tuple_name,
                                                     NULL,
                                                     tag );
  if( pctx == NULL )
    goto error;

  DBR_Tag_t ptag = dbrInsert_request( cs, pctx );
  if( ptag == DB_TAG_ERROR )
  {
    LOG( DBG_ERR, stderr, "Unable to inject request to queue\n" );
    goto error;
  }

  DBR_Request_handle_t put_handle = dbrPost_request( pctx );
  if( put_handle == NULL )
    goto error;

  BIGLOCK_UNLOCKRETURN( cs->_reverse, pctx->_tag );

error:
  dbrRemove_request( cs, pctx );
  LOG( DBG_ERR, stderr, "request error \n" );
  BIGLOCK_UNLOCKRETURN( cs->_reverse, DB_TAG_ERROR );
}

