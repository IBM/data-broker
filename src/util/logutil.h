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

#ifndef SRC_LOGUTIL_H_
#define SRC_LOGUTIL_H_

#include <stdio.h>
#include <sys/time.h>

#define DBG_ALL 0
#define DBG_ERR 1
#define DBG_WARN 2
#define DBG_INFO 3
#define DBG_VERBOSE 4
#define DBG_TRACE 5

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL DBG_INFO
#endif

#ifdef APPLE
#define TIME_FORMAT "%ld.%d : "
#else
#define TIME_FORMAT "%ld.%ld : "
#endif

#define LOG( level, stream, logging... ) { if( ( level ) <= DEBUG_LEVEL ) { \
  struct timeval tv; gettimeofday( &tv, NULL ); \
  fprintf( (stream), TIME_FORMAT, \
           tv.tv_sec, tv.tv_usec ); fprintf( (stream), logging ); }}

#endif /* SRC_LOGUTIL_H_ */
