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

#ifndef BACKEND_REDIS_PROTOCOL_H_
#define BACKEND_REDIS_PROTOCOL_H_

#include <inttypes.h>

#include "../common/dbbe_api.h"
#include "definitions.h"

/*
 * max number of stages that can be spec'd for one opcode
 */
#define DBBE_REDIS_COMMAND_STAGE_MAX ( 4 )

/*
 * max length of a command string (base command without arguments)
 */
#define DBBE_REDIS_COMMAND_LENGTH_MAX ( 256 )

/*
 * max number of arguments that a stage of a command can hold
 */
#define DBBE_REDIS_COMMAND_ARGS_MAX ( 6 )


/*
 * enumeration of the directory scan stages
 */
typedef enum
{
  DBBE_REDIS_DIRECTORY_STAGE_META = 0,
  DBBE_REDIS_DIRECTORY_STAGE_SCAN = 1
} dbBE_Redis_directory_stages_t;

/*
 * enumeration of the name space delete stages
 */
typedef enum
{
  DBBE_REDIS_NSDELETE_STAGE_DETACH = 0,
  DBBE_REDIS_NSDELETE_STAGE_SCAN = 1,
  DBBE_REDIS_NSDELETE_STAGE_DELKEYS = 2,
  DBBE_REDIS_NSDELETE_STAGE_DELNS = 3
} dbBE_Redis_nsdelete_stages_t;

/*
 * enumeration of the move key stages
 */
typedef enum
{
  DBBE_REDIS_MOVE_STAGE_DUMP = 0,
  DBBE_REDIS_MOVE_STAGE_RESTORE = 1,
  DBBE_REDIS_MOVE_STAGE_DEL = 2
} dbBE_Redis_move_stages_t;

/*
 * holds the generic spec of a command stage
 * - stage number
 * - number of expected arguments
 * - command spec
 */
typedef struct dbBE_Redis_command_stage_spec
{
  uint8_t _stage; // number of this stage
  uint8_t _array_len; // number of args for this stage
  uint8_t _final; // is it the last stage of this command?
  uint8_t _result; // is it the result-stage of this command?
  dbBE_REDIS_DATA_TYPE _expect; // what result type to expect for this stage
  char _command[ DBBE_REDIS_COMMAND_LENGTH_MAX ]; // Redis command string
} dbBE_Redis_command_stage_spec_t;

extern dbBE_Redis_command_stage_spec_t *gRedis_command_spec;

/*
 * Command stage specs are initialized in a 2D array by indexing a 1D array via:
 *    index = opcode * MAX_STAGE + stage
 */
dbBE_Redis_command_stage_spec_t* dbBE_Redis_command_stages_spec_init();

/*
 * destroy the stage specs (if refcount is 0)
 */
void dbBE_Redis_command_stages_spec_destroy( dbBE_Redis_command_stage_spec_t *specs );

#endif /* BACKEND_REDIS_PROTOCOL_H_ */
