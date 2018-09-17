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

#include <stddef.h>
#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>
#endif
#include <string.h>

#include "protocol.h"

dbBE_Redis_command_stage_spec_t *gRedis_command_spec = NULL;
static int gRedis_command_spec_refcnt = 0;

/*
 * Command stage specs are initialized in a 2D array by indexing a 1D array via:
 *    index = opcode * MAX_STAGE + stage
 */
dbBE_Redis_command_stage_spec_t* dbBE_Redis_command_stages_spec_init()
{
  // if it's already initialized, just count and return the reference
  ++gRedis_command_spec_refcnt;
  if( gRedis_command_spec != NULL )
    return gRedis_command_spec;

  int total_stages = DBBE_OPCODE_MAX * DBBE_REDIS_COMMAND_STAGE_MAX; // upper bound, some commands need fewer stages

  // array of pointers to the first stage of each opcode
  dbBE_Redis_command_stage_spec_t *specs =
      (dbBE_Redis_command_stage_spec_t*)malloc( sizeof( dbBE_Redis_command_stage_spec_t ) *  total_stages );
  if( specs == NULL )
    return NULL;

  memset( specs, 0, sizeof( dbBE_Redis_command_stage_spec_t) *  total_stages );

  int index;
  int stage;
  dbBE_Redis_command_stage_spec_t *s;
  dbBE_Opcode op;

  /*
   *  * Put
   * - RPUSH ns_name::t_name value
   */
  op = DBBE_OPCODE_PUT;
  stage = 0;
  index = op * DBBE_REDIS_COMMAND_STAGE_MAX + stage;
  s = &specs[ index ];
  s->_array_len = 3;
  s->_final = 1;
  s->_result = 1;
  s->_expect = dbBE_REDIS_TYPE_INT; // will return integer of inserted keys
  strcpy( s->_command, (const char*)"RPUSH" );
  s->_stage = stage;

  /*
   * Get
   * - LPOP ns_name::t_name
   * -   (autoremoves the entry if the last entry is popped
   *
   */
  op = DBBE_OPCODE_GET;
  stage = 0;
  index = op * DBBE_REDIS_COMMAND_STAGE_MAX + stage;
  s = &specs[ index ];
  s->_array_len = 2;
  s->_final = 1;
  s->_result = 1;
  s->_expect = dbBE_REDIS_TYPE_CHAR; // will return char buffer
  strcpy( s->_command, "LPOP" );
  s->_stage = stage;

  /*
   * read
   * - LINDEX ns_name::t_name 0
   */
  op = DBBE_OPCODE_READ;
  stage = 0;
  index = op * DBBE_REDIS_COMMAND_STAGE_MAX + stage;
  s = &specs[ index ];
  s->_array_len = 3;
  s->_final = 1;
  s->_result = 1;
  s->_expect = dbBE_REDIS_TYPE_CHAR; // will return char buffer
  strcpy( s->_command, "LINDEX" );
  s->_stage = stage;

  /*
   * CreateNS ( 2-stage )
   * - HSETNX ns_name id ns_name
   * - if return 1: HMSET ns_name refcnt 1 groups permissions
   */
  op = DBBE_OPCODE_NSCREATE;
  stage = 0;
  index = op * DBBE_REDIS_COMMAND_STAGE_MAX + stage;
  s = &specs[ index ];
  s->_array_len = 4;
  s->_final = 0;
  s->_result = 0;
  s->_expect = dbBE_REDIS_TYPE_INT; // will return integer: number of created hashes
  strcpy( s->_command, "HSETNX" );
  s->_stage = stage;

  stage = 1;
  index = op * DBBE_REDIS_COMMAND_STAGE_MAX + stage;
  s = &specs[ index ];
  s->_array_len = 6;
  s->_final = 1;
  s->_result = 1;
  s->_expect = dbBE_REDIS_TYPE_CHAR; // will return simple OK string
  strcpy( s->_command, "HMSET" );
  s->_stage = stage;

  /*
   * AttachNS ( 2-stage )
   * - EXISTS ns_name  (if exists, then next stage)
   * - HINCRBY ns_name refcnt 1
   * -  check return for > 1
   */
  op = DBBE_OPCODE_NSATTACH;
  stage = 0;
  index = op * DBBE_REDIS_COMMAND_STAGE_MAX + stage;
  s = &specs[ index ];
  s->_array_len = 2;
  s->_final = 0;
  s->_result = 0;
  s->_expect = dbBE_REDIS_TYPE_INT; // will return new value after inc
  strcpy( s->_command, "EXISTS" );
  s->_stage = stage;

  stage = 1;
  index = op * DBBE_REDIS_COMMAND_STAGE_MAX + stage;
  s = &specs[ index ];
  s->_array_len = 4;
  s->_final = 1;
  s->_result = 1;
  s->_expect = dbBE_REDIS_TYPE_INT; // will return new value after inc
  strcpy( s->_command, "HINCRBY" );
  s->_stage = stage;


  /*
   * QueryNS
   * - HGETALL ns_name
   * -   check return (nil)
   */
  op = DBBE_OPCODE_NSQUERY;
  stage = 0;
  index = op * DBBE_REDIS_COMMAND_STAGE_MAX + stage;
  s = &specs[ index ];
  s->_array_len = 2;
  s->_final = 1;
  s->_result = 1;
  s->_expect = dbBE_REDIS_TYPE_ARRAY; // will return list of kv entries of the hash
  strcpy( s->_command, "HGETALL" );
  s->_stage = stage;

  /*
   * DetachNS
   * - HINCRBY ns_name refcnt -1
   * -   check return for >= 1
   */
  op = DBBE_OPCODE_NSDETACH;
  stage = 0;
  index = op * DBBE_REDIS_COMMAND_STAGE_MAX + stage;
  s = &specs[ index ];
  s->_array_len = 2;
  s->_final = 0;
  s->_result = 0;
  s->_expect = dbBE_REDIS_TYPE_INT; // will return new value after inc
  strcpy( s->_command, "EXISTS" );
  s->_stage = stage;

  stage = 1;
  index = op * DBBE_REDIS_COMMAND_STAGE_MAX + stage;
  s = &specs[ index ];
  s->_array_len = 4;
  s->_final = 1;
  s->_result = 1;
  s->_expect = dbBE_REDIS_TYPE_INT; // will return new value after dec
  strcpy( s->_command, "HINCRBY" );
  s->_stage = stage;

  /*
   * DeleteNS ( multi-stage !! )
   * - HINCRBY ns_name refcnt -1
   * -   check return for refcnt == 0
   * - SCAN 0 MATCH ns_name::*          start the scan on all connections
   * - SCAN <cursor> MATCH ns_name::*   repeat until return from server is 0, delete each returned key
   */
  op = DBBE_OPCODE_NSDELETE;
  stage = DBBE_REDIS_NSDELETE_STAGE_DETACH;
  index = op * DBBE_REDIS_COMMAND_STAGE_MAX + stage;
  s = &specs[ index ];
  s->_array_len = 4;
  s->_final = 0;
  s->_result = 0;
  s->_expect = dbBE_REDIS_TYPE_INT; // will return new value after dec
  strcpy( s->_command, "HINCRBY" );
  s->_stage = stage;

  stage = DBBE_REDIS_NSDELETE_STAGE_SCAN;
  index = op * DBBE_REDIS_COMMAND_STAGE_MAX + stage;
  s = &specs[ index ];
  s->_array_len = 4;
  s->_final = 0;
  s->_result = 0;
  s->_expect = dbBE_REDIS_TYPE_ARRAY; // will return array of [ char, array [ char ] ]
  strcpy( s->_command, "SCAN" );
  s->_stage = stage;

  stage = DBBE_REDIS_NSDELETE_STAGE_DELKEYS;
  index = op * DBBE_REDIS_COMMAND_STAGE_MAX + stage;
  s = &specs[ index ];
  s->_array_len = 2;
  s->_final = 0;
  s->_result = 0;
  s->_expect = dbBE_REDIS_TYPE_INT; // will return number of deleted keys: 1
  strcpy( s->_command, "DEL" );
  s->_stage = stage;

  stage = DBBE_REDIS_NSDELETE_STAGE_DELNS;
  index = op * DBBE_REDIS_COMMAND_STAGE_MAX + stage;
  s = &specs[ index ];
  s->_array_len = 2;
  s->_final = 1;
  s->_result = 1;
  s->_expect = dbBE_REDIS_TYPE_INT; // will return number of deleted keys: 1
  strcpy( s->_command, "DEL" );
  s->_stage = stage;

  gRedis_command_spec = specs;

  return specs;
}

void dbBE_Redis_command_stages_spec_destroy( dbBE_Redis_command_stage_spec_t *specs )
{
  --gRedis_command_spec_refcnt;
  if(( specs != NULL ) && ( gRedis_command_spec_refcnt == 0 ))
  {
    gRedis_command_spec = NULL;
    int total_stages = DBBE_OPCODE_MAX * DBBE_REDIS_COMMAND_STAGE_MAX; // upper bound, some commands need fewer stages
    memset( specs, 0, sizeof( dbBE_Redis_command_stage_spec_t ) *  total_stages );
    free( specs );
  }
}
