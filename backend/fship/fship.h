/*
 * Copyright © 2020 IBM Corporation
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

#ifndef BACKEND_FSHIP_FSHIP_H_
#define BACKEND_FSHIP_FSHIP_H_

#include "common/request_queue.h"
#include "common/completion_queue.h"
#include "transports/sr_buffer.h"
#include "network/connection.h"

#define DBBE_FSHIP_WORK_QUEUE_DEPTH (4096)
#define DBBE_FSHIP_BUFFER_SIZE ( 512 * 1024 * 1024 )

typedef struct
{
  dbBE_Request_queue_t *_work_q;
  dbBE_Completion_queue_t *_compl_q;
  dbBE_Redis_sr_buffer_t *_sbuf;
  dbBE_Connection_t *_connection;
} dbBE_FShip_context_t;


dbBE_Handle_t FShip_initialize( void );

int FShip_exit( dbBE_Handle_t be );

dbBE_Request_handle_t FShip_post( dbBE_Handle_t be,
                                  dbBE_Request_t *request,
                                  int trigger );


int FShip_cancel( dbBE_Handle_t be,
                  dbBE_Request_handle_t request );


dbBE_Completion_t* FShip_test( dbBE_Handle_t be,
                               dbBE_Request_handle_t request );

dbBE_Completion_t* FShip_test_any( dbBE_Handle_t be );



#endif /* BACKEND_FSHIP_FSHIP_H_ */
