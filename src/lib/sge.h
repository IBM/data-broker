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

#ifndef SRC_LIB_SGE_H_
#define SRC_LIB_SGE_H_

#include <stddef.h>

static inline int64_t dbrSGE_extract_size( dbBE_Request_t *req )
{
  if( req == NULL )
    return 0;

  int64_t size = 0;
  int n;
  for( n = 0; n < req->_sge_count; ++n )
  {
    size_t sz = req->_sge[n]._size;

    // sanity check
    if(( sz > 0 ) && ( req->_sge[n]._data == NULL ))
      return 0;

    size += sz;
  }

  return size;
}


#endif /* SRC_LIB_SGE_H_ */
