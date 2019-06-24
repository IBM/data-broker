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
#include "libdatabroker.h"
#include "libdatabroker_int.h"

#include <stdio.h>
#include <stdlib.h>

DBR_Tag_t libdbrGetA(DBR_Handle_t cs_handle,
                     dbrDA_Request_chain_t *request,
                     DBR_Tuple_template_t match_template,
                     DBR_Group_t group,
                     int flags,
                     int64_t *ret_size )
{
  dbrName_space_t *cs = (dbrName_space_t*)cs_handle;
  if(( cs == NULL ) || ( cs->_be_ctx == NULL ) || ( cs->_reverse == NULL ) || (cs->_status != dbrNS_STATUS_REFERENCED ))
    return DB_TAG_ERROR;

  dbrDA_Request_chain_t *chain = request;

  BIGLOCK_LOCK( cs->_reverse );

  DBR_Tag_t tag = dbrTag_get( cs->_reverse );
  if( tag == DB_TAG_ERROR )
    BIGLOCK_UNLOCKRETURN( cs->_reverse, DB_TAG_ERROR );

#ifdef DBR_DATA_ADAPTERS
  // read-path request pre-processing plugin
  if( cs->_reverse->_data_adapter != NULL )
  {
    chain = cs->_reverse->_data_adapter->pre_read( request );
    if( chain == NULL )
      BIGLOCK_UNLOCKRETURN( cs->_reverse, DB_TAG_ERROR );
  }
#endif

  dbrRequestContext_t *head =
      dbrCreate_request_chain( DBBE_OPCODE_GET,
                               cs_handle,
                               group,
                               NULL,
                               DBR_GROUP_EMPTY,
                               chain,
                               match_template,
                               flags,
                               tag );
  if( head == NULL )
    return DB_TAG_ERROR;

  head->_rchain = chain;
  head->_ochain = request;
  DBR_Tag_t rtag = dbrInsert_request( cs, head );
  if( rtag == DB_TAG_ERROR )
    goto error;

  DBR_Request_handle_t get_handle = dbrPost_request( head );
  if( get_handle == NULL )
    goto error;

  BIGLOCK_UNLOCKRETURN( cs->_reverse, head->_tag );

error:
  dbrRemove_request( cs, head );
  if(ret_size)
    *ret_size = 0;

#ifdef DBR_DATA_ADAPTERS
  if( cs->_reverse->_data_adapter != NULL )
    cs->_reverse->_data_adapter->error_handler( chain, DBRDA_READ, DBR_ERR_TAGERROR );
#endif
  BIGLOCK_UNLOCKRETURN( cs->_reverse, DB_TAG_ERROR );
}
