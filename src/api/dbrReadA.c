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

#include "util/lock_tools.h"
#include "libdatabroker.h"
#include "libdatabroker_int.h"

#include <stdio.h>
#include <stdlib.h>

DBR_Tag_t libdbrReadA(DBR_Handle_t cs_handle,
                      void *va_ptr,
                      int64_t size,
                      int64_t *ret_size,
                      DBR_Tuple_name_t tuple_name,
                      DBR_Tuple_template_t match_template,
                      DBR_Group_t group)
{
  dbrName_space_t *cs = (dbrName_space_t*)cs_handle;
  if(( cs == NULL ) || ( cs->_be_ctx == NULL ) || ( cs->_reverse == NULL ) || (cs->_status != dbrNS_STATUS_REFERENCED ))
    return DB_TAG_ERROR;

  dbBE_sge_t dest_sge;
  dest_sge.iov_base = va_ptr;
  dest_sge.iov_len = size;

#ifdef DBR_DATA_ADAPTERS
  // read-path request pre-processing plugin
  dbrDA_Request_chain_t *ichain = (dbrDA_Request_chain_t*)calloc( 1, sizeof( dbrDA_Request_chain_t) + sizeof( dbBE_sge_t ));
  ichain->_key = tuple_name;
  ichain->_next = NULL;
  ichain->_size = *ret_size;
  ichain->_sge_count = 1;
  ichain->_value_sge[0].iov_base = va_ptr;
  ichain->_value_sge[0].iov_len = size;

  dbrDA_Request_chain_t *chain = NULL;
  if( cs->_reverse->_data_adapter != NULL )
  {
    chain = cs->_reverse->_data_adapter->pre_read( ichain );
    if( chain == NULL )
    {
      free( ichain );
      return DB_TAG_ERROR;
    }
  }
  free( ichain );
#endif

  BIGLOCK_LOCK( cs->_reverse );

  DBR_Tag_t tag = dbrTag_get( cs->_reverse );
  if( tag == DB_TAG_ERROR )
    BIGLOCK_UNLOCKRETURN( cs->_reverse, DB_TAG_ERROR );

  dbrRequestContext_t *rctx = dbrCreate_request_ctx( DBBE_OPCODE_READ,
                                                     cs_handle,
                                                     group,
                                                     NULL,
                                                     DBR_GROUP_EMPTY,
                                                     1,
                                                     &dest_sge,
                                                     ret_size,
                                                     tuple_name,
                                                     match_template,
                                                     tag );
  if( rctx == NULL )
    goto error;

  DBR_Tag_t rtag = dbrInsert_request( cs, rctx );
  if( rtag == DB_TAG_ERROR )
    goto error;

  DBR_Request_handle_t read_handle = dbrPost_request( rctx );
  if( read_handle == NULL )
    goto error;

  BIGLOCK_UNLOCKRETURN( cs->_reverse, rctx->_tag );

error:
  dbrRemove_request( cs, rctx );
  if(ret_size)
    *ret_size = 0;

  BIGLOCK_UNLOCKRETURN( cs->_reverse, DB_TAG_ERROR );
}
