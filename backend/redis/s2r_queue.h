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

#ifndef BACKEND_REDIS_S2R_QUEUE_H_
#define BACKEND_REDIS_S2R_QUEUE_H_

#include "../common/dbbe_api.h"
#include "request.h"

typedef struct
{
  dbBE_Redis_request_t *_head;
  dbBE_Redis_request_t *_tail;
  size_t _len;
} dbBE_Redis_s2r_queue_t;


/*
 * create and initialize the queue
 */
dbBE_Redis_s2r_queue_t* dbBE_Redis_s2r_queue_create( const size_t size );

/*
 * cleanup and destroy the queue
 */
int dbBE_Redis_s2r_queue_destroy( dbBE_Redis_s2r_queue_t *queue );

/*
 * return the current length of the queue (number of queued requests)
 */
size_t dbBE_Redis_s2r_queue_len( dbBE_Redis_s2r_queue_t *queue );

/*
 * append an entry to the queue
 */
int dbBE_Redis_s2r_queue_push( dbBE_Redis_s2r_queue_t *queue,
                                 dbBE_Redis_request_t *request );

/*
 * return and remove the first entry from the queue
 */
dbBE_Redis_request_t* dbBE_Redis_s2r_queue_pop( dbBE_Redis_s2r_queue_t *queue );


/*
 * wipe all entries from the queue
 */
int dbBE_Redis_s2r_queue_flush( dbBE_Redis_s2r_queue_t *queue );


#endif /* BACKEND_REDIS_S2R_QUEUE_H_ */
