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

#ifndef SRC_LIBDATABROKER_INT_H_
#define SRC_LIBDATABROKER_INT_H_

#include "errorcodes.h"
#include "libdatabroker.h"

#include "util/lock_tools.h"
#include "common/dbbe_api.h"

#define dbrMAX_TAGS ( 1024 )
#define dbrNUM_DB_MAX ( 1024 )
#define dbrERROR_INDEX ( (uint32_t)-1)
#define DBR_TMP_BUFFER_LEN ( 128 * 1024 * 1024 )


#include "lib/sge.h"

#include <pthread.h>

struct dbrMain_context;


typedef enum
{
  dbrNS_STATUS_UNDEFINED,
  dbrNS_STATUS_CREATED,
  dbrNS_STATUS_REFERENCED,
  dbrNS_STATUS_DELETED,
  dbrNS_STATUS_MAX
} dbrName_space_status_t;


/**
 * @brief Internal local representation of a name space
 * @typedef
 */
typedef struct
{
  struct dbrMain_context *_reverse;  ///< something to reverse lookup of several name space data depending on DB_handle_t type definition
  uint32_t _idx;                     ///< index into name space table
  int _ref_count;                    ///< local reference counter
  DBR_Name_t _db_name;               ///< name of the name space
  void *_be_ctx;                     ///< back end access context
  dbrName_space_status_t _status;    ///< status of the name space for local status tracking
} dbrName_space_t;

typedef struct
{
  char id[ DBR_MAX_KEY_LEN ];
  char refcnt[32];
  char groups[64];
} dbr_Name_meta_t;

enum dbrResult_type
{
  dbrBE_RESULT_NONE,
  dbrBE_RESULT_INT,
  dbrBE_RESULT_STR,
  dbrBE_RESULT_ERR,
  dbrBE_RESULT_MAX
};

typedef enum dbrRequest_status
{
  dbrSTATUS_ERROR = -1,
  dbrSTATUS_EMPTY = 0,
  dbrSTATUS_PENDING,
  dbrSTATUS_CANCELING,
  dbrSTATUS_RETRIEVING,
  dbrSTATUS_READY,
  dbrSTATUS_CLOSED
} dbrRequest_status_t;

// request context to hold all data around a request
typedef struct dbrRequestContext
{
  dbrName_space_t *_ctx;
  dbBE_Completion_t _cpl;
  dbBE_Request_handle_t _be_request_hdl;
  dbrRequest_status_t _status;
  DBR_Tag_t _tag;
  int64_t *_rc;
  struct dbrRequestContext *_next;
  dbBE_Request_t _req;     ///< dynamic length
} dbrRequestContext_t;

typedef dbrRequestContext_t* DBR_Request_handle_t;

typedef struct dbrConfig
{
  long int _timeout_sec;
} dbrConfig_t;

// global context data
typedef struct dbrMain_context
{
  dbrConfig_t _config;                       ///< configuration data
  void *_be_ctx;                            ///< back-end context/plugin handle
  dbrName_space_t *_cs_list[dbrNUM_DB_MAX];        ///< CS handles array keeping track of all name spaces locally
  dbrRequestContext_t *_cs_wq[ dbrMAX_TAGS ];   ///< request queue by tag

  // todo: we'll need an array of head/tail tags to track responses from each node in a cluster
  uint64_t _tag_head;                     ///< tag for the next request
  uint64_t _tag_tail;                     ///< tag of the next expected completion
  pthread_mutex_t _biglock;               ///< initial step to thread-safe lib; starting with big lock in main context
  void* _tmp_testkey_buf;                 ///< a tmp buffer that holds return values for testkey command
} dbrMain_context_t;

dbrMain_context_t* dbrCheckCreateMainCTX();

//////////////////////////////////////////////////////////////////////
// local in-mem name space maintenance

dbrName_space_t* dbrMain_create_local( DBR_Name_t db_name );

uint32_t dbrMain_find( dbrMain_context_t *libctx, DBR_Name_t name );
uint32_t dbrMain_insert( dbrMain_context_t *libctx, dbrName_space_t *cs );
int dbrMain_detach( dbrMain_context_t *libctx, dbrName_space_t *cs );
int dbrMain_delete( dbrMain_context_t *libctx, dbrName_space_t *cs );
int dbrMain_attach( dbrMain_context_t *libctx, dbrName_space_t *cs );

//////////////////////////////////////////////////////////////////////
// request creation/posting

DBR_Tag_t dbrTag_get( dbrMain_context_t *ctx );
DBR_Errorcode_t dbrValidateTag( dbrRequestContext_t *rctx, DBR_Tag_t req_tag );

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
                                           DBR_Tag_t tag );

DBR_Tag_t dbrInsert_request( dbrName_space_t *cs, dbrRequestContext_t *rctx );
DBR_Errorcode_t dbrRemove_request( dbrName_space_t *cs, dbrRequestContext_t *rctx );
DBR_Request_handle_t dbrPost_request( dbrRequestContext_t *rctx );


//////////////////////////////////////////////////////////////////////
// request tracking/completion

DBR_Errorcode_t dbrCheck_response( dbrRequestContext_t *rctx );

DBR_Errorcode_t dbrTest_request( dbrName_space_t *cs, DBR_Request_handle_t hdl );
DBR_Errorcode_t dbrWait_request( dbrName_space_t *cs,
                                 DBR_Request_handle_t hdl,
                                 int enable_timeout );


#endif /* SRC_LIBDATABROKER_INT_H_ */
