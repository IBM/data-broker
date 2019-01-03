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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <libdatabroker.h>

#include "libdatabroker_int.h"


DBR_Errorcode_t
libdbrAddUnits( DBR_Handle_t cs_handle,
                unsigned int sgec,
                dbBE_sge_t* sgev )
{
  dbrName_space_t *cs = (dbrName_space_t*)cs_handle;
  if( cs_handle == NULL
      || cs->_be_ctx == NULL )
    return DBR_ERR_INVALID;

  DBR_Tag_t tag = dbrTag_get( cs->_reverse );
  if( tag == DB_TAG_ERROR )
    return DBR_ERR_TAGERROR;

  DBR_Errorcode_t rc = DBR_SUCCESS;
  dbrRequestContext_t *pctx = dbrCreate_request_ctx( DBBE_OPCODE_NSADDUNITS,
                                                     cs_handle,
                                                     DBR_GROUP_EMPTY,
                                                     NULL,
                                                     DBR_GROUP_EMPTY,
                                                     sgec,
                                                     sgev,
                                                     NULL,
                                                     NULL,
                                                     NULL,
                                                     tag );
  if( pctx == NULL )
  {
    rc = DBR_ERR_NOMEMORY;
    goto error;
  }

  DBR_Tag_t ptag = dbrInsert_request( cs, pctx );
  if( ptag == DB_TAG_ERROR )
  {
    rc = DBR_ERR_TAGERROR;
    goto error;
  }

  DBR_Request_handle_t put_handle = dbrPost_request( pctx );
  if( put_handle == NULL )
  {
    rc = DBR_ERR_BE_POST;
    goto error;
  }

  rc = dbrWait_request( cs, put_handle, 0 );
  rc = dbrCheck_response( pctx );
  if( rc != DBR_SUCCESS )
    goto error;

  dbrRemove_request( cs, pctx );
  return rc;

error:
  dbrRemove_request( cs, pctx );
  return rc;
}
