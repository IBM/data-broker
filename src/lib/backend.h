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

#ifndef SRC_LIB_BACKEND_H_
#define SRC_LIB_BACKEND_H_

#include "libdatabroker.h"
#include "libdatabroker_int.h"

dbBE_Handle_t* dbrlib_backend_get_handle(void);
int dbrlib_backend_delete( dbBE_Handle_t* handle );

#endif /* SRC_LIB_BACKEND_H_ */
