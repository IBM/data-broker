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
  s->_array_len = 2;
  s->_resp_cnt = 1;
  s->_final = 1;
  s->_result = 1;
  s->_expect = dbBE_REDIS_TYPE_INT; // will return integer of inserted keys
  strcpy( s->_command, "*3\r\n$5\r\nRPUSH\r\n%0%1" );
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
  s->_array_len = 1;
  s->_resp_cnt = 1;
  s->_final = 1;
  s->_result = 1;
  s->_expect = dbBE_REDIS_TYPE_CHAR; // will return char buffer
  strcpy( s->_command, "*2\r\n$4\r\nLPOP\r\n%0" );
  s->_stage = stage;

  /*
   * read
   * - LINDEX ns_name::t_name 0
   */
  op = DBBE_OPCODE_READ;
  stage = 0;
  index = op * DBBE_REDIS_COMMAND_STAGE_MAX + stage;
  s = &specs[ index ];
  s->_array_len = 1;
  s->_resp_cnt = 1;
  s->_final = 1;
  s->_result = 1;
  s->_expect = dbBE_REDIS_TYPE_CHAR; // will return char buffer
  strcpy( s->_command, "*3\r\n$6\r\nLINDEX\r\n%0$1\r\n0\r\n" );
  s->_stage = stage;

  /*
   * * Directory
   * - HGETALL <namespace>
   * - SCAN 0 MATCH <match-template> COUNT <limit>
   * -     SCAN <cursor> MATCH <match-template> COUNT <limit>
   */
  op = DBBE_OPCODE_DIRECTORY;
  stage = DBBE_REDIS_DIRECTORY_STAGE_META;
  index = op * DBBE_REDIS_COMMAND_STAGE_MAX + stage;
  s = &specs[ index ];
  s->_array_len = 1;
  s->_resp_cnt = 1;
  s->_final = 0;
  s->_result = 0;
  s->_expect = dbBE_REDIS_TYPE_ARRAY; // will return char buffer
  strcpy( s->_command, (const char*)"*2\r\n$7\r\nHGETALL\r\n%0" );
  s->_stage = stage;

  stage = DBBE_REDIS_DIRECTORY_STAGE_SCAN;
  index = op * DBBE_REDIS_COMMAND_STAGE_MAX + stage;
  s = &specs[ index ];
  s->_array_len = 2;
  s->_resp_cnt = 1;
  s->_final = 1;
  s->_result = 1;
  s->_expect = dbBE_REDIS_TYPE_ARRAY; // will return array of [ char, array [ char ] ]
  strcpy( s->_command, "*6\r\n$4\r\nSCAN\r\n%0$5\r\nMATCH\r\n%1$5\r\nCOUNT\r\n$4\r\n1000\r\n" );
  s->_stage = stage;


  /*
   * CreateNS ( 2-stage )
   * - HSETNX ns_name id ns_name
   * - if return 1: HMSET ns_name refcnt 1 groups permissions flags 0
   */
  op = DBBE_OPCODE_NSCREATE;
  stage = 0;
  index = op * DBBE_REDIS_COMMAND_STAGE_MAX + stage;
  s = &specs[ index ];
  s->_array_len = 3;
  s->_resp_cnt = 1;
  s->_final = 0;
  s->_result = 0;
  s->_expect = dbBE_REDIS_TYPE_INT; // will return integer: number of created hashes
  strcpy( s->_command, "*4\r\n$6\r\nHSETNX\r\n%0%1%2" );
  s->_stage = stage;

  stage = 1;
  index = op * DBBE_REDIS_COMMAND_STAGE_MAX + stage;
  s = &specs[ index ];
  s->_array_len = 7;
  s->_resp_cnt = 1;
  s->_final = 1;
  s->_result = 1;
  s->_expect = dbBE_REDIS_TYPE_CHAR; // will return simple OK string
  strcpy( s->_command, "*8\r\n$5\r\nHMSET\r\n%0%1%2%3%4%5%6" );
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
  s->_array_len = 1;
  s->_resp_cnt = 1;
  s->_final = 0;
  s->_result = 0;
  s->_expect = dbBE_REDIS_TYPE_INT; // will return 1 if exists
  strcpy( s->_command, "*2\r\n$6\r\nEXISTS\r\n%0" );
  s->_stage = stage;

  stage = 1;
  index = op * DBBE_REDIS_COMMAND_STAGE_MAX + stage;
  s = &specs[ index ];
  s->_array_len = 2;
  s->_resp_cnt = 1;
  s->_final = 1;
  s->_result = 1;
  s->_expect = dbBE_REDIS_TYPE_INT; // will return new value after inc
  strcpy( s->_command, "*4\r\n$7\r\nHINCRBY\r\n%0$6\r\nrefcnt\r\n%1" );
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
  s->_array_len = 1;
  s->_resp_cnt = 1;
  s->_final = 1;
  s->_result = 1;
  s->_expect = dbBE_REDIS_TYPE_ARRAY; // will return list of kv entries of the hash
  strcpy( s->_command, "*2\r\n$7\r\nHGETALL\r\n%0" );
  s->_stage = stage;

  /*
   * DetachNS (serves as delete determined by refcount and delete flag
   * - HMGET ns_name FLAGS REFCNT       check for DELETED flag then transition to
   *     DETACH or SCAN
   *
   * - SCAN 0 MATCH ns_name::*          start the scan on all connections
   * - SCAN <cursor> MATCH ns_name::*   repeat until return from server is 0, delete each returned key
   *
   * - DEL remaining keys
   * - HINCRBY ns_name refcnt -1        check return for >= 0
   *
   *  request has 2 final stages because it might go 2 different paths
   *   - delete namespace with all content or
   *   - just decrease the refcount
   *
   */
  op = DBBE_OPCODE_NSDETACH;
  stage = DBBE_REDIS_NSDETACH_STAGE_DELCHECK;
  index = op * DBBE_REDIS_COMMAND_STAGE_MAX + stage;
  s = &specs[ index ];
  s->_array_len = 2;
  s->_resp_cnt = 4;
  s->_final = 0;
  s->_result = 0;
  s->_expect = dbBE_REDIS_TYPE_ARRAY; // will return an array of results from HINCRBY and HMGET with the field values
  strcpy( s->_command, "*1\r\n$5\r\nMULTI\r\n*4\r\n$7\r\nHINCRBY\r\n%0$6\r\nrefcnt\r\n%1*4\r\n$5\r\nHMGET\r\n%0$6\r\nrefcnt\r\n$5\r\nflags\r\n*1\r\n$4\r\nEXEC\r\n" );
  s->_stage = stage;

  stage = DBBE_REDIS_NSDETACH_STAGE_SCAN;
  index = op * DBBE_REDIS_COMMAND_STAGE_MAX + stage;
  s = &specs[ index ];
  s->_array_len = 2;
  s->_resp_cnt = 1;
  s->_final = 0;
  s->_result = 0;
  s->_expect = dbBE_REDIS_TYPE_ARRAY; // will return array of [ char, array [ char ] ]
  strcpy( s->_command, "*6\r\n$4\r\nSCAN\r\n%0$5\r\nMATCH\r\n%1$5\r\nCOUNT\r\n$4\r\n1000\r\n" );
  s->_stage = stage;

  stage = DBBE_REDIS_NSDETACH_STAGE_DELKEYS;
  index = op * DBBE_REDIS_COMMAND_STAGE_MAX + stage;
  s = &specs[ index ];
  s->_array_len = 1;
  s->_resp_cnt = 1;
  s->_final = 0;
  s->_result = 0;
  s->_expect = dbBE_REDIS_TYPE_INT; // will return number of deleted keys: 1
  strcpy( s->_command, "*2\r\n$3\r\nDEL\r\n%0" );
  s->_stage = stage;

  stage = DBBE_REDIS_NSDETACH_STAGE_DELNS;
  index = op * DBBE_REDIS_COMMAND_STAGE_MAX + stage;
  s = &specs[ index ];
  s->_array_len = 1;
  s->_resp_cnt = 1;
  s->_final = 1;
  s->_result = 1;
  s->_expect = dbBE_REDIS_TYPE_INT; // will return number of deleted keys: 1
  strcpy( s->_command, "*2\r\n$3\r\nDEL\r\n%0" );
  s->_stage = stage;

  /*
   * DeleteNS
   * - HMGET refcnt flags: required for error handling purposes
   * - HSET flags 1: delete marker
   * - Only mark as: to delete
   * - upper layers are required to call detach after delete
   */
  op = DBBE_OPCODE_NSDELETE;
  stage = DBBE_REDIS_NSDELETE_STAGE_EXIST;
  index = op * DBBE_REDIS_COMMAND_STAGE_MAX + stage;
  s = &specs[ index ];
  s->_array_len = 1;
  s->_resp_cnt = 1;
  s->_final = 0;
  s->_result = 1;
  s->_expect = dbBE_REDIS_TYPE_ARRAY; // will return refcnt and flags as array
  strcpy( s->_command, "*4\r\n$5\r\nHMGET\r\n%0$6\r\nrefcnt\r\n$5\r\nflags\r\n" );
  s->_stage = stage;

  stage = DBBE_REDIS_NSDELETE_STAGE_SETFLAG;
  index = op * DBBE_REDIS_COMMAND_STAGE_MAX + stage;
  s = &specs[ index ];
  s->_array_len = 3;
  s->_resp_cnt = 1;
  s->_final = 1;
  s->_result = 0;
  s->_expect = dbBE_REDIS_TYPE_INT; // needs to return 0 for 'updated existing entry'
  strcpy( s->_command, "*4\r\n$4\r\nHSET\r\n%0%1%2" );
  s->_stage = stage;

  /*
   * Remove command
   * - DEL ns_name::key
   */
  op = DBBE_OPCODE_REMOVE;
  stage = 0;
  index = op * DBBE_REDIS_COMMAND_STAGE_MAX + stage;
  s = &specs[ index ];
  s->_array_len = 1;
  s->_resp_cnt = 1;
  s->_final = 1;
  s->_result = 1;
  s->_expect = dbBE_REDIS_TYPE_INT; // will return number of deleted keys: 1
  strcpy( s->_command, "*2\r\n$3\r\nDEL\r\n%0" );
  s->_stage = stage;

  /*
   * Move command
   * - dump <ns>::<tuplename>              (whole value, old place)
   * - restore <nsNew>::<tuplename> 0 <value> (whole value, new place)
   * - del <ns>::<tuplename>               (old place)
   */
  op = DBBE_OPCODE_MOVE;
  stage = DBBE_REDIS_MOVE_STAGE_DUMP;
  index = op * DBBE_REDIS_COMMAND_STAGE_MAX + stage;
  s = &specs[ index ];
  s->_array_len = 1;
  s->_resp_cnt = 1;
  s->_final = 0;
  s->_result = 0;
  s->_expect = dbBE_REDIS_TYPE_CHAR; // will return serialized sequence of tuple
  strcpy( s->_command, "*2\r\n$4\r\nDUMP\r\n%0" );
  s->_stage = stage;

  stage = DBBE_REDIS_MOVE_STAGE_RESTORE;
  index = op * DBBE_REDIS_COMMAND_STAGE_MAX + stage;
  s = &specs[ index ];
  s->_array_len = 3;
  s->_resp_cnt = 1;
  s->_final = 0;
  s->_result = 0;
  s->_expect = dbBE_REDIS_TYPE_CHAR; // will return simple OK string
  /* \todo: temporary work-around parsing of dumped value;
   * currently removing prefix in parse and then have to re-add it afterwards
   * therefore format:   ... %1%2\r\n - because %1 prefix; %2 serialized val; \r\n termination
   * note: extended array length + \r\n
   */
  strcpy( s->_command, "*4\r\n$7\r\nRESTORE\r\n%0$1\r\n0\r\n%1%2\r\n" );
  s->_stage = stage;

  stage = DBBE_REDIS_MOVE_STAGE_DEL;
  index = op * DBBE_REDIS_COMMAND_STAGE_MAX + stage;
  s = &specs[ index ];
  s->_array_len = 1;
  s->_resp_cnt = 1;
  s->_final = 1;
  s->_result = 1;
  s->_expect = dbBE_REDIS_TYPE_INT; // will return number of deleted keys: 1
  strcpy( s->_command, "*2\r\n$3\r\nDEL\r\n%0" );
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
