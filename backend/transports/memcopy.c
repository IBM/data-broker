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

#include <string.h>
#include <inttypes.h>
#include <errno.h>

#include "memcopy.h"


dbBE_Data_transport_t dbBE_Memcopy_transport =
    { .gather  = dbBE_Transport_memory_gather,
      .scatter = dbBE_Transport_memory_scatter
    };


int64_t dbBE_Transport_memory_gather( dbBE_Data_transport_device_t* destbuf,
                                      size_t len,
                                      int sge_count,
                                      dbBE_sge_t *sge )
{
  char *pos = (char*)destbuf;
  int64_t remain = (int64_t)len;
  if( remain < 0 )
    return -EINVAL;

  int n;
  for( n = 0; n < sge_count; ++n )
  {
    if( remain - (int64_t)sge[ n ].iov_len < 0)
      return -ENOMEM;
    size_t copy_size = (size_t)remain < sge[ n ].iov_len ? (size_t)remain : sge[ n ].iov_len;
    memcpy( pos, sge[ n ]._data, copy_size );
    pos += copy_size;
    remain -= copy_size;
  }

  return (int64_t)len - remain;
}

int64_t dbBE_Transport_memory_scatter( dbBE_Data_transport_device_t* srcbuf,
                                       size_t len,
                                       int sge_count,
                                       dbBE_sge_t *sge)
{
  if( srcbuf == NULL )
    return -EINVAL;

  char *pos = (char*)srcbuf;
  int64_t remain = (int64_t)len;
  if( remain < 0 )
    return -EINVAL;

  int n;
  for( n = 0; (n < sge_count) && ( remain > 0 ); ++n )
  {
    size_t copy_size = (size_t)remain < sge[ n ].iov_len ? (size_t)remain : sge[ n ].iov_len;
    memcpy( sge[ n ]._data, pos, copy_size );
    pos += copy_size;
    remain -= copy_size;
  }

  return (int64_t)len - remain;
}
