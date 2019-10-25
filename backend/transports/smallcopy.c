/*
 * Copyright © 2019 IBM Corporation
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

#include "logutil.h"
#include "smallcopy.h"
#include "sr_buffer.h"

#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <stdlib.h>


/*
 * default send-recv buffer size memcpy transport
 * This imposes the limit on the data size that can
 * be transmitted
 */
#define DBBE_TRANSPORT_SMALLCOPY_BUFFER_LEN ( 16 * 1024 )


dbBE_Data_transport_t dbBE_Smallcopy_transport =
    { .gather  = dbBE_Transport_scopy_gather,
      .scatter = dbBE_Transport_scopy_scatter,
      ._recv_buffer_len = 16384,
      ._send_buffer_len = 16384
    };

int64_t dbBE_Transport_scopy_gather( dbBE_Data_transport_endpoint_t* dev,
                                     size_t len,
                                     int sge_count,
                                     dbBE_sge_t *sge )
{
  return -ENOSYS; // not implemented
}

int64_t dbBE_Transport_scopy_scatter( dbBE_Data_transport_endpoint_t* dev,
                                      dbBE_Data_recv_cb recv,
                                      dbBE_sge_t *partial,
                                      size_t total,
                                      int sge_count,
                                      dbBE_sge_t *sge)
{
  int64_t remain = (int64_t)total;
  if(( partial == NULL) || ( remain < 0 ) || ( sge_count < 0 ) || ( sge == NULL ))
    return -EINVAL;

  if( total > partial->iov_len )
  {
    // check if device and callback are provided; otherwise need to call again
    if(( dev == NULL ) || ( recv ==  NULL ))
      return -EAGAIN;

    LOG( DBG_TRACE, stderr, "Partially received data handling: %ld/%ld already available\n", partial->iov_len, total );
    dbBE_Transport_sge_buffer_t *sge_buf = &dbBE_Smallcopy_transport._rSGE;

    // sge_count-1 to ignore terminator SGE; + partial len because input sge is shortened by that amount already
    size_t sge_space = partial->iov_len + dbBE_SGE_get_len( sge, sge_count );
    memcpy( sge_buf->_cmd, sge, sge_count * sizeof( struct iovec ) );
    sge_buf->_index = sge_count;

    // our regular receive buffer is fairly small,
    // do we require a temporary buffer for partial data retrieval?
    if( sge_space < total )
      return -ENOSPC;

    LOG( DBG_TRACE, stderr, "partial string recv: sge0.len=%ld|%ld; avail=%ld/%ld\n",
         sge[0].iov_len, sge[1].iov_len, partial->iov_len, total );
    ssize_t rsize = 0;
    ssize_t rc = 0;
    do
    {
      if( partial->iov_len > 0 )
      {
        ssize_t expect = dbBE_SGE_get_len( sge_buf->_cmd, sge_buf->_index );
        LOG( DBG_TRACE, stderr, "before device.recv( %ld/%ld ); expect=%ld; SGEcount=%d\n", rsize, total, expect, sge_buf->_index );
      }
      rc = recv( dev, sge_buf );

      if( partial->iov_len > 0 )
        LOG( DBG_TRACE, stderr, "after  device.recv( %ld/%ld ) = %ld\n", rsize, total, rc );

      if( rc < 0 ) break;
      if(( rc > 0 ) && ( rc + rsize < total - partial->iov_len ))
      {
        ssize_t offset = rc;
        while(( sge_buf->_index > 0 ) && ( offset > 0 ))
        {
          if( offset < sge_buf->_cmd[0].iov_len )
          {
            LOG( DBG_TRACE, stderr, "SGE[0] reduce by %ld from %ld to %ld\n", offset, sge_buf->_cmd[0].iov_len, sge_buf->_cmd[0].iov_len - offset );
            sge_buf->_cmd[0].iov_base = (char*)sge_buf->_cmd[0].iov_base + offset;
            sge_buf->_cmd[0].iov_len -= offset;
            offset = 0;
          }
          else
          {
            LOG( DBG_TRACE, stderr, "SGE shift remaining data reduced by %ld from %ld to %ld; remaining entries: %d\n",
                 sge_buf->_cmd[0].iov_len, offset, offset - sge_buf->_cmd[0].iov_len, sge_buf->_index-1 );
            offset -= sge_buf->_cmd[0].iov_len;
            memmove( sge_buf->_cmd, &sge_buf->_cmd[1], sizeof( sge_buf->_cmd[0] ) * sge_buf->_index );
            --sge_buf->_index;
          }
        }
      }
      rsize += rc;
    } while (( rc >= 0 ) && ( rsize < total - partial->iov_len ));

    if( rc <= 0 )
    {
      LOG( DBG_WARN, stderr, "Recv/Transport failure: %ld\n", rc );
      return rc ;
    }
    else
      // memcpy has already happened when parsing the partially received data and creating the SGE for the remaining/missing data
      return rsize;
  }

  // fully received data is not yet been copied
  char *pos = NULL;
  pos = partial->iov_base;

  // sge memcpy for cases without partially received items
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
