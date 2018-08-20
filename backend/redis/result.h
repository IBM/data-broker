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

#ifndef BACKEND_REDIS_RESULT_H_
#define BACKEND_REDIS_RESULT_H_

#include "definitions.h"


// create base structure
// todo...


// create array of length...
// todo...


// deep delete result structure (especially taking care of arrays)
int dbBE_Redis_result_cleanup( dbBE_Redis_result_t *result, int dealloc );


// run through a pre-parsed result and terminate the strings
int dbBE_Redis_result_terminate_strings( dbBE_Redis_result_t *result );


#endif /* BACKEND_REDIS_RESULT_H_ */
