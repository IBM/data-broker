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

#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <stdlib.h>

#include "memcopy.h"
#include "sr_buffer.h"

/*
 * default send-recv buffer size memcpy transport
 * This imposes the limit on the data size that can
 * be transmitted
 */
#define DBBE_TRANSPORT_MEMCOPY_BUFFER_LEN ( 128 * 1048576 )


dbBE_Data_transport_t dbBE_Memcopy_transport =
    { .gather  = dbBE_Transport_memory_gather,
      .scatter = dbBE_Transport_memory_scatter,
      ._recv_buffer_len = DBBE_TRANSPORT_MEMCOPY_BUFFER_LEN,
      ._send_buffer_len = 16384
    };

typedef char* dbBE_Data_transport_memory_dev_t;

int64_t dbBE_Transport_memory_gather( dbBE_Data_transport_endpoint_t* dev,
                                      size_t len,
                                      int sge_count,
                                      dbBE_sge_t *sge )
{
  int64_t remain = (int64_t)len;
  if(( remain < 0 ) || ( dev == NULL ) || ( sge_count <= 0 ) || ( sge == NULL ))
    return -EINVAL;

  dbBE_Data_transport_memory_dev_t *mdev = (dbBE_Data_transport_memory_dev_t*)dev;
  char *pos = (char*)mdev;

  int n;
  for( n = 0; n < sge_count; ++n )
  {
    if( remain - (int64_t)sge[ n ].iov_len < 0)
      return -ENOMEM;
    size_t copy_size = (size_t)remain < sge[ n ].iov_len ? (size_t)remain : sge[ n ].iov_len;
    memcpy( pos, sge[ n ].iov_base, copy_size );
    pos += copy_size;
    remain -= copy_size;
  }

  return (int64_t)len - remain;
}

int64_t dbBE_Transport_memory_scatter( dbBE_Data_transport_endpoint_t* dev,
                                       dbBE_Data_recv_cb recv,
                                       dbBE_sge_t *partial,
                                       size_t total,
                                       int sge_count,
                                       dbBE_sge_t *sge)
{
  int64_t remain = (int64_t)total;
  if(( partial == NULL) || ( remain < 0 ) || ( sge_count < 0 ) || ( sge == NULL ))
    return -EINVAL;

  if( total > partial->iov_len ) // we only have partial data, which is not supported with this transport
    return -EAGAIN;

  char *pos = NULL;
  pos = partial->iov_base;

  int n;
  for( n = 0; (n < sge_count) && ( remain > 0 ); ++n )
  {
    size_t copy_size = (size_t)remain < sge[ n ].iov_len ? (size_t)remain : sge[ n ].iov_len;
    memcpy( sge[ n ].iov_base, pos, copy_size );
    pos += copy_size;
    remain -= copy_size;
  }
  return (int64_t)total - remain;
}
