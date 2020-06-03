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
#include <stdlib.h> // calloc

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


// NULLptr encoding: len = -1; data = empty, e.g. single SGE with NULL: 0\n1\n-1\n\n


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
ssize_t dbBE_SGE_serialize_header( const dbBE_sge_t *sge, const int sge_count, char *data, size_t space )
{
  if(( data == NULL ) || ( sge == NULL ) || ( sge_count < 1 ) || ( sge_count > DBBE_SGE_MAX ))
    return -EINVAL;

  if( space < 3 )
    return -ENOSPC;

  int i;

  // create header
  int plen = snprintf( data, space, "%"PRId64"\n%d\n", dbBE_SGE_get_len( sge, sge_count ), sge_count );
  if( plen <= 0 )
    return -EBADMSG;

  if( (size_t)plen >= space )
    return -ENOSPC;

  space -= plen;
  data += plen;

  ssize_t total = plen;
  for( i=0; (i < sge_count) && (space > 0); ++i )
  {
    if(( sge[i].iov_len != 0 ) && (sge[i].iov_base == NULL ))
      return -EBADMSG;

    // NULL-ptr encoding: len=0; ptr=NULL;
    ssize_t itemlen = (ssize_t)sge[i].iov_len;
    if(( sge[i].iov_len == 0 ) && (sge[i].iov_base == NULL ))
      itemlen = -1;

    plen = snprintf( data, space, "%ld\n", itemlen );
    if( plen <= 0 )
      return -EBADMSG;

    if( (size_t)plen >= space )
      return -ENOSPC;

    space -= plen;
    data += plen;
    total += plen;
  }

  return total;
}


static inline
ssize_t dbBE_SGE_serialize(const dbBE_sge_t *sge, const int sge_count, char *data, size_t space )
{
  if(( data == NULL ) || ( sge == NULL ) || ( sge_count < 1 ) || ( sge_count > DBBE_SGE_MAX ) || ( space == 0 ))
    return -EINVAL;

  int i;

  // create header
  ssize_t total = dbBE_SGE_serialize_header( sge, sge_count, data, space );
  if( total <= 0 )
    return total;

  space -= total;
  data += total;

  for( i=0; (i < sge_count) && (space > 0); ++i )
  {
    ssize_t plen = sge[i].iov_len;
    if( (size_t)plen >= space )
      return -ENOSPC;
    if( sge[i].iov_base != NULL )
      memcpy( data, sge[i].iov_base, plen );
//    data[plen] = '\n';
//    ++plen;

    space -= plen;
    data += plen;
    total += plen;
  }

  if( space == 0 )
    return -ENOSPC;

  // data already points to the end of the string, so just make sure we terminate
  data[ 0 ] = '\0';
  return total;
}


#define dbBE_SGE_deserialize_error( rc, in, a ) { if(( (in) == NULL ) && ( (a) != NULL )) { free( (a) ); (a) = NULL; } return rc; }

/*
 * extract SGE-header information from stream
 * intendet to be re-entrant so that partially received headers can be completed
 * after receiving more data.
 * All parsing attempts expect to start from the beginning of the serialize sge header regardless of previous partial attempts
 *
 * @param[in] sge_in     input-sge, if NULL, it's assumed that no previous attempts had been made and a new sge is allocated
 * @param[in] sge_count  input-sge size, if sge_in is non-NULL, this reflects the number of available SGEs (may be more than needed, e.g. pre-allocated max-size sge)
 * @param[in] data       input data buffer that contains the serialized data
 * @param[in] space      number of available bytes in serialized data
 * @param[out] sge_out   output sge either after new allocation or same as input sge if available
 * @param[out] pased     number of parsed bytes in data buffer
 *
 * @return number of SGEs that got allocated or are available from input
 */

static inline
int dbBE_SGE_extract_header( dbBE_sge_t *sge_in, int sge_count, const char *data, size_t space,
                             dbBE_sge_t **sge_out, size_t *parsed )
{
  // 4 is the minimum number of bytes to resemble a valid header ( "0\n0\n" )
  if(( parsed == NULL ) || (data == NULL) || (space == 0 ) ||
      (( sge_in != NULL ) && ( sge_count < 1 )) || (( sge_in == NULL ) && ( sge_count != 0 )) ||
      ( sge_out == NULL ) || ( parsed == NULL ))
    return -EINVAL;

  if( space < 4 )
    return -EAGAIN;

  int i;
  size_t total = 0;
  int offset = 0;
  int i_sge_count = 0;

  int plen = sscanf( data, "%"PRId64"\n%d\n%n", &total, &i_sge_count, &offset );
  if(( plen < 2 ) || (offset < 0) || ((size_t)offset > space)
      || (i_sge_count < 1) || (space < (unsigned)i_sge_count * 2)) // at least one digit+\n need to be available in data
    return -EBADMSG;

  data += offset;
  space -= offset;
  *parsed = (size_t)offset;

  dbBE_sge_t *sge = sge_in;
  if( sge == NULL )
    sge = (dbBE_sge_t*)calloc( i_sge_count, sizeof( dbBE_sge_t ));
  if( sge == NULL )
    return -ENOMEM;

  // sanity check input sge vs. header data;
  if(( sge_in != NULL ) && (sge_count < i_sge_count))
    return -E2BIG; // no free(sge); this is only if sge_in was provided

  size_t consistent = 0;

  for( i=0; (i < i_sge_count) && (space > 1); ++i )
  {
    // scan the length
    ssize_t len;
    plen = sscanf( data, "%ld\n%n", &len, &offset );
    if(( plen < 1 ) || ( (size_t)offset > space ))
      dbBE_SGE_deserialize_error( -EBADMSG, sge_in, sge );
    if( data[offset-1] != '\n' ) // correctly terminated?
      dbBE_SGE_deserialize_error( -EAGAIN, sge_in, sge );

    sge[i].iov_len = (size_t)len;
    consistent += ( len == -1 ) ? 0 : len; // cover NULL-ptr case
    data += offset;
    space -= offset;
    *parsed += (size_t)offset;
  }
  // if the header already exhausted the data, signal to receive more data
  if(( i<i_sge_count ) && ( space < 1 ))
    dbBE_SGE_deserialize_error( -EAGAIN, sge_in, sge );

  if( consistent != total )
    dbBE_SGE_deserialize_error( -EBADMSG, sge_in, sge );

  // if the total SGE length was 0, step back one because we parsed a double \n at the end
  // i.e. we only had one NULL ptr encoded
  if(( total == 0 ) && ( data[-1] == '\n' ) && ( data[-2] == '\n' ))
    --*parsed;

  *sge_out = sge;

  return i_sge_count;
}


static inline
ssize_t dbBE_SGE_deserialize( dbBE_sge_t *sge_in, const int sge_count_in,
                              const char *data, size_t space,
                              dbBE_sge_t **sge_out, int *sge_count )
{
  if(( data == NULL ) || ( sge_out == NULL ) || ( sge_count == NULL ))
    return -EINVAL;

  if( (( sge_in != NULL ) && ( sge_count_in == 0 )) ||
      (( sge_in == NULL ) && ( sge_count_in > 0 )) )
    return -EINVAL;

  if( space < 4 )
    return -EAGAIN;

  int i;
  size_t total = 0;
  size_t offset = 0;
  int i_sge_count = 0;

  i_sge_count = dbBE_SGE_extract_header( sge_in, sge_count_in, data, space, sge_out, &offset );
  if( i_sge_count < 0 )
    return i_sge_count;

  // adjust to remaining data and space
  data += offset;
  space -= offset;

  dbBE_sge_t *sge = *sge_out;
  if( sge == NULL )
    dbBE_SGE_deserialize_error( -EINVAL, sge_in, *sge_out );

  total += offset;
  offset = 0;
  for( i=0; (i < i_sge_count); ++i )
  {
    if( sge[i].iov_len == (size_t)-1 ) // NULL-ptr
    {
      sge[i].iov_len = 0;
      sge[i].iov_base = NULL;
    }
    else
    {
      if( space < sge[i].iov_len )
        dbBE_SGE_deserialize_error( -EAGAIN, sge_in, *sge_out );
      sge[i].iov_base = (void*)data;
    }
//    ((char*)(sge[i].iov_base))[ sge[i].iov_len ] = '\0';
    data += sge[i].iov_len;
    space -= sge[i].iov_len;
    total += sge[i].iov_len;
  }
  // account for the trailing separator unless it was only a NULL-ptr sge
//  if(( i_sge_count > 1 ) ||
//      (( i_sge_count == 1 ) && ( sge[0].iov_base != NULL )) )
//  {
//    if( space == 0 ) // the trailing \n needs to be in the buffer, otherwise it needs another recv call
//      dbBE_SGE_deserialize_error( -EAGAIN, sge_in, *sge_out );
//    ++total;
//  }

  *sge_count = i_sge_count;

  return total;
}

#endif /* BACKEND_COMMON_SGE_H_ */
