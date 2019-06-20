/*
 * Copyright © 2018, 2019 IBM Corporation
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

DBR_Errorcode_t
libdbrPut( DBR_Handle_t cs_handle,
           dbrDA_Request_chain_t *request,
           DBR_Group_t group )
{
  if( cs_handle == NULL )
    return DBR_ERR_INVALID;

  dbrName_space_t *cs = (dbrName_space_t*)cs_handle;
  if(( cs->_be_ctx == NULL ) || (cs->_reverse == NULL ) || (cs->_status != dbrNS_STATUS_REFERENCED ))
    return DBR_ERR_NSINVAL;

  BIGLOCK_LOCK( cs->_reverse );

  DBR_Tag_t tag = dbrTag_get( cs->_reverse );
  if( tag == DB_TAG_ERROR )
    BIGLOCK_UNLOCKRETURN( cs->_reverse, DBR_ERR_TAGERROR );

  dbrDA_Request_chain_t *chain = request;

#ifdef DBR_DATA_ADAPTERS
  // write-path data pre-processing plugin
  if( cs->_reverse->_data_adapter != NULL )
  {
    chain = cs->_reverse->_data_adapter->pre_write( request );
    if( chain == NULL )
      BIGLOCK_UNLOCKRETURN( cs->_reverse, DBR_ERR_PLUGIN );
  }
#endif

  dbrDA_Request_chain_t *ch_req = chain;
  dbrRequestContext_t *prev = NULL;
  dbrRequestContext_t *head = NULL;
  DBR_Errorcode_t rc = DBR_SUCCESS;
  while( ch_req != NULL )
  {
    dbrRequestContext_t *ctx;
    ctx = dbrCreate_request_ctx( DBBE_OPCODE_PUT,
                                 cs_handle,
                                 group,
                                 NULL,
                                 DBR_GROUP_EMPTY,
                                 ch_req->_sge_count,
                                 ch_req->_value_sge,
                                 NULL,
                                 ch_req->_key,
                                 NULL,
                                 tag );
    if( ctx == NULL )
    {
      rc = DBR_ERR_NOMEMORY;
      goto error;
    }

    // chain the request contexts
    if( prev != NULL )
      prev->_next = ctx;
    else
      head = ctx; // remember the first one

    prev = ctx;
    // some basic sanity check because we're processing a data structure from the plugin
    if( ch_req == ch_req->_next )
    {
      LOG( DBG_ERR, stderr, "Request chain error detected. Self-referencing\n");
      while( head != NULL )
      {
        dbrRequestContext_t *tmp = head;
        if( head->_next != head )
          head = head->_next;
        else
          head = NULL;
        dbrDestroy_request( tmp );
      }
      rc = DBR_ERR_INVALIDOP;
      goto error;
    }
    ch_req = ch_req->_next;
  }

  if( dbrInsert_request( cs, head ) == DB_TAG_ERROR )
  {
    rc = DBR_ERR_TAGERROR;
    goto error;
  }

  DBR_Request_handle_t req_handle = dbrPost_request( head );
  if( req_handle == NULL )
  {
    rc = DBR_ERR_BE_POST;
    goto error;
  }

  rc = dbrWait_request( cs, req_handle, 0 );

  switch( rc ) {
  case DBR_SUCCESS:
    rc = dbrCheck_response( head );

#ifdef DBR_DATA_ADAPTERS
    // write-path data pre-processing plugin
    if( cs->_reverse->_data_adapter != NULL )
      rc = cs->_reverse->_data_adapter->post_write( chain, rc );
#endif

    break;
  case DBR_ERR_INPROGRESS:
    rc = DBR_ERR_TIMEOUT;
    break;
  default:
    goto error;
  }

  dbrRemove_request( cs, head );
  BIGLOCK_UNLOCKRETURN( cs->_reverse, rc );

error:
  dbrRemove_request( cs, head );
#ifdef DBR_DATA_ADAPTERS
  if( cs->_reverse->_data_adapter != NULL )
    rc = cs->_reverse->_data_adapter->error_handler( chain, DBRDA_WRITE, rc );
#endif
  BIGLOCK_UNLOCKRETURN( cs->_reverse, rc );
}
