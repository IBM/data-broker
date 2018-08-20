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

#ifndef BACKEND_REDIS_COMPLETE_H_
#define BACKEND_REDIS_COMPLETE_H_

#include "sr_buffer.h"
#include "request.h"
#include "common/data_transport.h"

/*
 * convert a redis request in result stage into a completion
 * returns NULL on error and pointer to completion on success
 * will also perform the data transport if needed by the request type
 */
dbBE_Completion_t* dbBE_Redis_complete_command( dbBE_Redis_request_t *request,
                                                dbBE_Redis_result_t *result,
                                                int64_t rc );


/*
 * create a completion that signals an error
 */
dbBE_Completion_t* dbBE_Redis_complete_error( dbBE_Redis_request_t *request,
                                             dbBE_Redis_result_t *result,
                                             int64_t error_code );

/*
 * create a completion that signals a cancelled request
 */
dbBE_Completion_t* dbBE_Redis_complete_cancel( dbBE_Redis_request_t *request,
                                               int64_t error_code );

#endif /* BACKEND_REDIS_COMPLETE_H_ */
