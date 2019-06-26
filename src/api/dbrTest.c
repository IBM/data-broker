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

#include <stddef.h>
#include <string.h>
#include <stdlib.h>

DBR_Errorcode_t csPostProcessRequest( dbrRequestContext_t *rctx )
{
  if( rctx == NULL )
    return DBR_ERR_INVALID;

  DBR_Errorcode_t rc_out = DBR_ERR_GENERIC;
  switch( rctx->_req._opcode )
  {
    case DBBE_OPCODE_PUT:
    case DBBE_OPCODE_GET:
    case DBBE_OPCODE_READ:
      if( dbrCheck_response( rctx ) == DBR_SUCCESS )
        rc_out = DBR_SUCCESS;
      break;

    default:
      break;
  }

  rctx->_status = dbrSTATUS_CLOSED;
  return rc_out;
}

DBR_Errorcode_t
libdbrTest( DBR_Tag_t req_tag)
{
#ifdef DBR_INTTAG

  if(( req_tag < 0 ) || ( req_tag >= dbrMAX_TAGS ))
    return DBR_ERR_TAGERROR;

  dbrMain_context_t *main_ctx = dbrCheckCreateMainCTX();
  if( main_ctx == NULL )
    return DBR_ERR_INVALID;

  BIGLOCK_LOCK( main_ctx );

  dbrRequestContext_t *rctx = main_ctx->_cs_wq[ req_tag ];
  if( rctx == NULL )
  {
    LOG( DBG_WARN, stderr, "Request entry for tag %"PRId64" is deleted\n", req_tag );
    BIGLOCK_UNLOCKRETURN( main_ctx, DBR_ERR_TAGERROR );
  }

#else
#error "Currently not supported because of lack of access to main context and locking"
  if( req_tag == NULL
      || req_tag == DB_TAG_ERROR )
    BIGLOCK_UNLOCKRETURN( main_ctx, DBR_ERR_TAGERROR );

  dbrRequestContext_t *rctx = *(dbrRequestContext_t**)req_tag;
  if( rctx == NULL )
    BIGLOCK_UNLOCKRETURN( main_ctx, DBR_ERR_TAGERROR );

#endif


  DBR_Errorcode_t rc = dbrValidateTag( rctx, req_tag );
  if( rc != DBR_SUCCESS )
    BIGLOCK_UNLOCKRETURN( main_ctx, rc );

  dbrName_space_t* cs = rctx->_ctx;
  if( cs->_be_ctx == NULL )
    BIGLOCK_UNLOCKRETURN( main_ctx, DBR_ERR_NSINVAL );

  dbrRequestContext_t *chain = rctx;

  // check the full chain of requests before returning
  while( chain != NULL )
  {
    if( chain->_status == dbrSTATUS_CLOSED )
    {
      chain = chain->_next;
      continue;
    }

    rc = dbrTest_request( cs, chain );

    switch( rc )
    {
      case DBR_SUCCESS:
        // success, now see if there were any chained requests
        rc = csPostProcessRequest( chain );
        break;

      case DBR_ERR_INPROGRESS:
        // request still pending, so don't touch the book-keeping,
        // but let outside know that it is pending!

        // todo: for now keeping this case separate despite being empty
        BIGLOCK_UNLOCKRETURN( main_ctx, DBR_ERR_INPROGRESS );

      default:
        break;
    }

    chain = chain->_next;
  }


#ifdef DBR_DATA_ADAPTERS
  // data post-processing plugins
  if( cs->_reverse->_data_adapter != NULL )
  {
    if( rctx->_ochain == NULL )
    {
      LOG( DBG_ERR, stderr, "BUG: User request in context is NULL. Chained request check issue?\n" );
      goto error;
    }
    dbrDA_Request_chain_t *chain = rctx->_rchain;
    switch( rctx->_req._opcode )
    {
      case DBBE_OPCODE_PUT:
        rc = cs->_reverse->_data_adapter->post_write( chain, rc );
        break;
      case DBBE_OPCODE_READ:
      case DBBE_OPCODE_GET:
      {
        rc = cs->_reverse->_data_adapter->post_read( chain, rctx->_ochain, rc );
        *rctx->_ochain->_ret_size = rctx->_ochain->_size;
        break;
      }
      default:
        LOG( DBG_ERR, stderr, "dbrTest() of an unsupported operation\n" );
        rc = DBR_ERR_INVALIDOP;
        goto error;
        break;
    }
    // chain cleanup
    chain = rctx->_ochain;
    while( chain != NULL )
    {
      dbrDA_Request_chain_t *next = chain->_next;
      free( chain );
      chain = next;
    }
  }
error:

#endif
  dbrRemove_request( rctx->_ctx, rctx );

  BIGLOCK_UNLOCKRETURN( main_ctx, rc );
}
