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

#ifndef BACKEND_COMMON_COMPLETION_H_
#define BACKEND_COMMON_COMPLETION_H_

#include "dbbe_api.h"

static inline
dbBE_Completion_t* dbBE_Completion_create( dbBE_Request_t *request,
                                           DBR_Errorcode_t dbr_status,
                                           int64_t rc )
{
  if( request == NULL )
    return NULL;

  dbBE_Completion_t *completion = (dbBE_Completion_t*)malloc( sizeof( dbBE_Completion_t) );
  if( completion == NULL )
    return NULL;

  completion->_next = NULL;
  completion->_rc = rc;
  completion->_user = request->_user;
  completion->_status = dbr_status;

  return completion;
}


#endif /* BACKEND_COMMON_COMPLETION_H_ */
