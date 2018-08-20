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

#ifndef BACKEND_REDIS_INTERNAL_H_
#define BACKEND_REDIS_INTERNAL_H_


/*
 * keep track of the scan command sequence for the delete processing
 */

typedef char* dbBE_Redis_cursor_t;

typedef struct dbBE_Redis_int_scan
{
  dbBE_Redis_cursor_t _cursor;
  int _scan_count;
} dbBE_Redis_int_scan;




#endif /* BACKEND_REDIS_INTERNAL_H_ */
