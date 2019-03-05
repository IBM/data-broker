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

#ifndef BACKEND_REDIS_API_H_
#define BACKEND_REDIS_API_H_

#include "../common/dbbe_api.h"
#include "../common/request_queue.h"
#include "../common/completion_queue.h"
#include "../common/request_set.h"
#include "../common/data_transport.h"

#include "definitions.h"
#include "protocol.h"
#include "s2r_queue.h"
#include "locator.h"
#include "conn_mgr.h"

typedef struct
{
  dbBE_Redis_command_stage_spec_t *_spec;
  dbBE_Redis_locator_t *_locator;
  dbBE_Redis_connection_mgr_t *_conn_mgr;
  dbBE_Request_queue_t *_work_q;
  dbBE_Completion_queue_t *_compl_q;
  dbBE_Redis_s2r_queue_t *_retry_q;
  dbBE_Request_set_t *_cancellations;
  dbBE_Data_transport_t *_transport;
  // sender/receiver threads

} dbBE_Redis_context_t;


/*
 * initialize the system library
 */
dbBE_Handle_t Redis_initialize();

/*
 * destroy/exit the system library
 */
int Redis_exit( dbBE_Handle_t be );

/*
 * post a new request to the backend
 */
dbBE_Request_handle_t Redis_post( dbBE_Handle_t be,
                                  dbBE_Request_t *request );

/*
 * cancel a request
 */
int Redis_cancel( dbBE_Handle_t be,
                  dbBE_Request_handle_t request );

/*
 * test for completion of a particular posted request
 * returns pointer to completion or NULL if not complete
 */
dbBE_Completion_t* Redis_test( dbBE_Handle_t be,
                               dbBE_Request_handle_t request );

/*
 * fetch the first completed request from the completion queue
 * or return NULL if nothing is completed since the last call
 */
dbBE_Completion_t* Redis_test_any( dbBE_Handle_t be );


/**************************************************************************
 * non-API functions
 */

/*
 * create the initial connection to Redis with srbuffers by extracting the url from the ENV variable
 */
int dbBE_Redis_connect_initial( dbBE_Redis_context_t *ctx );

void dbBE_Redis_sender_trigger( dbBE_Redis_context_t *backend );
void dbBE_Redis_receiver_trigger( dbBE_Redis_context_t *backend );

#endif /* BACKEND_REDIS_API_H_ */
