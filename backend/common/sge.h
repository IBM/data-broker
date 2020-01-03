/*
 * Copyright © 2020 IBM Corporation
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

#ifndef BACKEND_COMMON_SGE_H_
#define BACKEND_COMMON_SGE_H_


#include <sys/uio.h> // struct iovec
#include <stddef.h> // NULL
#include <stdio.h> // snprintf
#include <errno.h> // error codes
#include <inttypes.h> // PRId64
#include <string.h> // memcpy

/*
 * max number of SGEs in assembled redis commands (IOV_MAX replacement)
 */
#define DBBE_SGE_MAX ( 256 )

/**
 *  @struct dbBE_sge_t dbbe_api.h "backend/common/dbbe_api.h"
 *
 *  @brief Scatter-Gather-Element structure
 *
 *  The scatter-gather-element defines one entry in a scatter-gather list
 *  and specifies one contiguous memory region in main memory.
 */
typedef struct iovec dbBE_sge_t;



/**
 * @brief calculate the total size of an SGE list
 *
 * Browses the SGEs of an input list and returns the total size in bytes.
 *
 * @param [in] sge       pointer to the first entry of an SGE list (array)
 * @param [in] sge_count number of elements in the SGE list
 *
 * @return size of the SGE in bytes
 */
static inline
size_t dbBE_SGE_get_len( const dbBE_sge_t *sge, const int sge_count )
{
  if( sge == NULL )
    return 0;
  int i;
  size_t len = 0;
  for( i = sge_count-1; i>=0; --i )
    len += sge[ i ].iov_len;
  return len;
}

/*
 * | totallen | sgecount |
 * | sge[i].len | ... sge[i].len ... |
 * | sge[i].data | ... sge[i].data ... |
 */

static inline
ssize_t dbBE_SGE_serialize(const dbBE_sge_t *sge, const int sge_count, char *data, size_t space )
{
  if(( data == NULL ) || ( sge == NULL ) || ( sge_count < 1 ) || ( sge_count > DBBE_SGE_MAX ))
    return -EINVAL;

  int i;

  // create header
  int plen = snprintf( data, space, "%"PRId64"\n%d\n", dbBE_SGE_get_len( sge, sge_count ), sge_count );
  if( plen <= 0 )
    return -EBADMSG;

  space -= plen;
  data += plen;

  ssize_t total = plen;
  for( i=0; (i < sge_count) && (space > 0); ++i )
  {
    plen = snprintf( data, space, "%ld\n", sge[i].iov_len );
    if( plen <= 0 )
      return -EBADMSG;

    space -= plen;
    data += plen;
    total += plen;
  }

  for( i=0; (i < sge_count) && (space > 0); ++i )
  {
    plen = sge[i].iov_len;
    memcpy( data, sge[i].iov_base, plen );
    data[plen] = '\n';
    ++plen;

    space -= plen;
    data += plen;
    total += plen;
  }

  if( space == 0 )
    return -E2BIG;

  // data already points to the end of the string, so just make sure we terminate
  data[ 0 ] = '\0';
  return total;
}


static inline
ssize_t dbBE_SGE_deserialize(dbBE_sge_t *sge, int sge_count, char *data, size_t space )
{
  if(( data == NULL ) || ( sge == NULL ) || ( sge_count < 1 ) || ( sge_count > DBBE_SGE_MAX ))
    return -EINVAL;

  int i;
  size_t total = 0;
  int offset = 0;
  int i_sge_count = 0;

  int plen = sscanf( data, "%"PRId64"\n%d\n%n", &total, &i_sge_count, &offset );
  if( plen < 2 )
    return -EBADMSG;

  if(( i_sge_count > sge_count ) || ( total > space ))
    return -E2BIG;

  data += offset;
  space -= offset;

  for( i=0; (i < i_sge_count) && (space > 0); ++i )
  {
    // scan the length
    size_t len;
    plen = sscanf( data, "%ld\n%n", &len, &offset );
    if( plen < 1 )
      return -EBADMSG;

    sge[i].iov_len = len;
    data += offset;
    space -= offset;
  }

  offset = 0;
  for( i=0; i < i_sge_count; ++i )
  {
    sge[i].iov_base = data;
    ((char*)(sge[i].iov_base))[ sge[i].iov_len ] = '\0';
    data += sge[i].iov_len + 1;
  }

  if( space == 0 )
    return -E2BIG;

  return total;
}

#endif /* BACKEND_COMMON_SGE_H_ */
