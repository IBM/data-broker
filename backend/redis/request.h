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

#ifndef BACKEND_REDIS_REQUEST_H_
#define BACKEND_REDIS_REQUEST_H_

#include "../common/dbbe_api.h"
#include "definitions.h"
#include "protocol.h"
#include "refcounter.h"

typedef struct dbBE_Redis_intern_detach_data
{
  dbBE_Refcounter_t *reference;
  char *scankey;
  int to_delete;
} dbBE_Redis_intern_detach_data_t;

typedef struct dbBE_Redis_intern_directory_data
{
  dbBE_Refcounter_t *reference;
} dbBE_Redis_intern_directory_data_t;

typedef struct dbBE_Redis_intern_move_data
{
  char *dumped_value;
  size_t len;
} dbBE_Redis_intern_move_data_t;

typedef union dbBE_Redis_intern_data
{
  dbBE_Redis_intern_detach_data_t  nsdetach;
  dbBE_Redis_intern_directory_data_t directory;
  dbBE_Redis_intern_move_data_t move;
} dbBE_Redis_intern_data_t;


typedef struct dbBE_Redis_request
{
  dbBE_Redis_intern_data_t _status;  // allows to keep some state to keep track of multistage-multinode request processing
  dbBE_Request_t *_user;
  dbBE_Redis_command_stage_spec_t *_step;
  int _conn_index;
  struct dbBE_Redis_request *_next;
} dbBE_Redis_request_t;

/*
 * allocate the memory of a new request an initialize according to the user request
 */
dbBE_Redis_request_t* dbBE_Redis_request_allocate( dbBE_Request_t *user );

/*
 * wipe and delete a request
 */
void dbBE_Redis_request_destroy( dbBE_Redis_request_t *request );


/*
 * transition a request to the next stage
 */
int dbBE_Redis_request_stage_transition( dbBE_Redis_request_t *request );


#endif /* BACKEND_REDIS_REQUEST_H_ */
