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

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>
#endif
#include <string.h>

#include "logutil.h"
#include "libdatabroker_int.h"


dbrRequestContext_t* dbrCreate_request_ctx(dbBE_Opcode op,
                                           dbrName_space_t *cs,
                                           DBR_Group_t group,
                                           dbrName_space_t *dst_cs,
                                           DBR_Group_t dst_group,
                                           int sge_count,
                                           dbBE_sge_t *sge,
                                           int64_t *rc,
                                           DBR_Tuple_name_t tuple_name,
                                           DBR_Tuple_template_t match_template,
                                           DBR_Tag_t tag )
{
  if( cs == NULL )
    return NULL;

  dbrRequestContext_t *req = (dbrRequestContext_t*)calloc( 1, sizeof( dbrRequestContext_t ) + sge_count * sizeof(dbBE_sge_t) );
  if( req == NULL )
    return NULL;

  req->_req._ns_name = cs->_db_name;
  req->_req._group = group;
  req->_req._key = tuple_name;
  req->_req._match = match_template;
  req->_req._sge_count = sge_count;
  req->_req._opcode = op;
  req->_req._user = req; // self-reference so that we find the right request after completion
  if( sge )
    memcpy( req->_req._sge, sge, sge_count * sizeof( dbBE_sge_t ) );
  // req->_cpl just keep it all 0 after memset
  req->_cpl._status = DBR_SUCCESS;
  req->_status = dbrSTATUS_PENDING;
  req->_ctx = cs;
  req->_be_request_hdl = NULL;
  req->_rc = rc;
  req->_tag = tag;

  return req;
}


DBR_Tag_t dbrInsert_request( dbrName_space_t *cs, dbrRequestContext_t *rctx )
{
  if( cs == NULL
      || rctx == NULL )
    return DB_TAG_ERROR;

  dbrRequestContext_t** cs_wq = cs->_reverse->_cs_wq;

  DBR_Tag_t tag = rctx->_tag;

#ifdef DBR_INTTAG
  if( dbrValidateTag( rctx, tag ) != DBR_SUCCESS )
    return DB_TAG_ERROR;

  unsigned int tag_idx = tag;

#else
  dbrRequestContext_t** p_rctx = (dbrRequestContext_t**)tag;
  if( p_rctx < cs_wq
      || &cs_wq[ dbrMAX_TAGS ] <= p_rctx )
    return DB_TAG_ERROR;

  unsigned int tag_idx = p_rctx - cs_wq;
#endif

  if( cs_wq[ tag_idx ] != NULL )
  {
    if( cs_wq[ tag_idx ]->_status != dbrSTATUS_CLOSED )
    {
      return DB_TAG_ERROR;
    }
    free( cs_wq[ tag_idx ] );
    cs_wq[ tag_idx ] = NULL;
  }

  cs_wq[ tag_idx ] = rctx;
  return tag;
}

DBR_Errorcode_t dbrRemove_request( dbrName_space_t *cs, dbrRequestContext_t *rctx )
{
  if( cs == NULL
      || rctx == NULL )
    return DBR_ERR_INVALID;

  dbrRequestContext_t** cs_wq = cs->_reverse->_cs_wq;

  DBR_Tag_t tag = rctx->_tag;

  #ifdef DBR_INTTAG
  if( dbrValidateTag( rctx, tag ) != DBR_SUCCESS )
    return DBR_ERR_TAGERROR;

  unsigned int tag_idx = tag;

#else
  dbrRequestContext_t** p_rctx = (dbrRequestContext_t**)tag;
  if( p_rctx < cs_wq
      || &cs_wq[ dbrMAX_TAGS ] <= p_rctx )
    return DBR_ERR_TAGERROR;

  unsigned int tag_idx = p_rctx - cs_wq;
#endif

  while( cs_wq[ tag_idx ] != NULL )
  {
    dbrRequestContext_t *chain = cs_wq[ tag_idx ]->_next;
    if(( chain != NULL )&&( chain->_tag != tag ))
    {
      printf( "BUG: chained request with different tag.\n" );
      return DBR_ERR_INVALID;
    }
    // todo: if there's a backend handle reference, we might have to clean it up
    if( cs_wq[ tag_idx ]->_be_request_hdl != NULL )
      LOG( DBG_VERBOSE, stderr, "TODO: cleanup backend handle?\n" );

    memset( cs_wq[ tag_idx ], 0, sizeof( dbrRequestContext_t ) + cs_wq[ tag_idx ]->_req._sge_count * sizeof(dbBE_sge_t) );
    free( cs_wq[ tag_idx ] );
    cs_wq[ tag_idx ] = chain;
  }

  cs_wq[ tag_idx ] = NULL;
  return DBR_SUCCESS;
}

DBR_Request_handle_t dbrPost_request( dbrRequestContext_t *rctx )
{
  if( rctx == NULL )
    return NULL;

  if( rctx->_cpl._status == DBR_ERR_INPROGRESS )
  {
    LOG( DBG_ERR, stderr, "Request already in progress\n");
    return NULL;
  }
  rctx->_cpl._status = DBR_ERR_INPROGRESS;
  rctx->_status = dbrSTATUS_PENDING;

  dbBE_Request_handle_t be_handle = g_dbBE.post( rctx->_ctx->_be_ctx, &rctx->_req );
  if( be_handle == NULL )
    goto error;

  rctx->_be_request_hdl = be_handle;
  return (DBR_Request_handle_t)rctx;

error:
  return NULL;
}
