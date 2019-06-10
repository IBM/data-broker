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
#include "libdbrAPI.h"

#include <stdio.h>

DBR_Errorcode_t
libdbrTestKey( DBR_Handle_t cs_handle,
               DBR_Tuple_name_t tuple_name,
               DBR_Tuple_template_t match_template,
               DBR_Group_t group )
{
  if( cs_handle == NULL )
    return DBR_ERR_INVALID;

  dbrName_space_t *cs = (dbrName_space_t*)cs_handle;
  if(( cs->_be_ctx == NULL ) || ( cs->_reverse == NULL ) || (cs->_status != dbrNS_STATUS_REFERENCED ))
    return DBR_ERR_NSINVAL;

  int64_t retsize;

  dbBE_sge_t sge;
  sge.iov_base = cs->_reverse->_tmp_testkey_buf;
  sge.iov_len = DBR_TMP_BUFFER_LEN;

  return libdbrRead( cs_handle,
                     &sge,
                     1,
                     &retsize,
                     tuple_name,
                     match_template,
                     group,
                     DBBE_OPCODE_FLAGS_IMMEDIATE );
}




DBR_Errorcode_t
libdbrRead(DBR_Handle_t cs_handle,
           dbBE_sge_t *sge,
           int sge_len,
           int64_t *ret_size,
           DBR_Tuple_name_t tuple_name,
           DBR_Tuple_template_t match_template,
           DBR_Group_t group,
           int flags)
{
  if(( cs_handle == NULL ) || ( sge == NULL ))
    return DBR_ERR_INVALID;

  dbrName_space_t *cs = (dbrName_space_t*)cs_handle;
  if(( cs->_be_ctx == NULL ) || ( cs->_reverse == NULL ))
    return DBR_ERR_NSINVAL;

#ifdef DBR_DATA_ADAPTERS
  // read-path request pre-processing plugin
  dbrDA_Request_chain_t *chain = NULL;
  if( cs->_reverse->_data_adapter != NULL )
  {
    chain = cs->_reverse->_data_adapter->pre_read( tuple_name, sge, sge_len );
    if( chain == NULL )
      return DBR_ERR_PLUGIN;
  }
#endif

  BIGLOCK_LOCK( cs->_reverse );

  int enable_timeout = (flags & DBBE_OPCODE_FLAGS_IMMEDIATE ) != 0 ? 0 : 1;


  DBR_Tag_t tag = dbrTag_get( cs->_reverse );
  if( tag == DB_TAG_ERROR )
    BIGLOCK_UNLOCKRETURN( cs->_reverse, DBR_ERR_TAGERROR );

  DBR_Errorcode_t rc = DBR_SUCCESS;
  dbrRequestContext_t *ctx = dbrCreate_request_ctx( DBBE_OPCODE_READ,
                                                    cs_handle,
                                                    group,
                                                    NULL,
                                                    DBR_GROUP_EMPTY,
                                                    sge_len,
                                                    sge,
                                                    ret_size,
                                                    tuple_name,
                                                    match_template,
                                                    tag );
  if( ctx == NULL )
  {
    rc = DBR_ERR_NOMEMORY;
    goto error;
  }

  ctx->_req._flags = flags;

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

  rc = dbrWait_request( cs, req_handle, enable_timeout );
  switch( rc ) {
  case DBR_SUCCESS:
    rc = dbrCheck_response( ctx );
#ifdef DBR_DATA_ADAPTERS
    // read-path data post-processing plugin
    if( cs->_reverse->_data_adapter != NULL )
      rc = cs->_reverse->_data_adapter->post_read( chain, sge, sge_len );
#endif
    break;
  case DBR_ERR_UNAVAIL:
    if( enable_timeout == 0 )
      break;
    // intentionally no break in case of timeout enabled
  case DBR_ERR_INPROGRESS:
    rc = DBR_ERR_TIMEOUT;
    break;
  case DBR_ERR_BE_GENERAL:
    if( enable_timeout == 0 )
      rc = DBR_ERR_UNAVAIL;
    break;
  case DBR_ERR_CANCELLED:
    if( enable_timeout == 0 )
      rc = DBR_ERR_UNAVAIL;
    else
      rc = DBR_ERR_TIMEOUT;
    break;
  default:
    goto error;
  }

error:
  dbrRemove_request( cs, ctx );

  BIGLOCK_UNLOCKRETURN( cs->_reverse, rc );
}

