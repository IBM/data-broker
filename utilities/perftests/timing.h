/*
 * Copyright © 2018-2020 IBM Corporation
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

#ifndef IBM_PERFTEST_TIMING_H_
#define IBM_PERFTEST_TIMING_H_

#include <sys/time.h>

namespace dbr {

static double test_start = 0.0;

static inline double myTime()
{
  struct timeval t;
  gettimeofday( &t, NULL );
  return ((double)t.tv_sec*1000000.) + (double)t.tv_usec - test_start;
}

}

#endif /* IBM_PERFTEST_TIMING_H_ */
