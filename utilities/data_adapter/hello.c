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

#include <libdatabroker.h>
#include <dbrda_api.h>

#include <errno.h> // errno
#include <stdlib.h> // calloc
#include <string.h> // memset, strlen
#include <stdio.h> // snprintf

dbrDA_Handle_t dbrDA_Hello_Init(void)
{
  return NULL;
}

int dbrDA_Hello_Exit( dbrDA_Handle_t da )
{
  return 0;
}

dbrDA_Request_chain_t* dbrDA_Hello_prewrite( dbrDA_Request_chain_t *input_req )
{
  dbrDA_Request_chain_t *custom = NULL;
  dbrDA_Request_chain_t *prev = NULL;
  dbrDA_Request_chain_t *tail = NULL;
  int i = 0;
  int keylen = 0;
  while( input_req != NULL )
  {
    tail = (dbrDA_Request_chain_t*)calloc( 1, sizeof( dbrDA_Request_chain_t) + input_req->_sge_count * sizeof( struct iovec ) );

    if( prev == NULL )
      custom = tail;
    else
      prev->_next = tail;
    prev = tail;

    // generate a key for the split requests
    if( i == 0 )
      keylen = strlen( input_req->_key ); // only get keylen if it's the first half, should stay the same on second half
    tail->_key = (char*)calloc( 1, keylen + 2 );  // +2 because we'll add 1 char and it needs space for the NUL termination
    snprintf( tail->_key, keylen+2, "%s%d", input_req->_key, i );

    tail->_sge_count = 1;
    tail->_value_sge[0].iov_base = input_req->_value_sge[0].iov_base + (i*input_req->_value_sge[0].iov_len/2);
    tail->_value_sge[0].iov_len = input_req->_value_sge[0].iov_len / 2;

    // adjust the second split length if the length was odd
    tail->_value_sge[0].iov_len += (i * (input_req->_value_sge[0].iov_len % 2 ));
    tail->_size = tail->_value_sge[0].iov_len;  // input/output size is important for read/get requests
    tail->_ret_size = &tail->_size;

    if( i == 1)
      input_req = input_req->_next;
    i = (i+1) % 2;
  }

  return custom;
}

/* NOTE: although this is duplicate code, it's done on purpose to
 * indicate that each prewrite and preread could be different in other examples
 */
/* split the incoming request into 2 requests to
 * allow retrieval of both parts of the value that
 * got split in the write processing
 * Essentially creating 2
 */
dbrDA_Request_chain_t* dbrDA_Hello_preread( dbrDA_Request_chain_t *input_req  )
{
  dbrDA_Request_chain_t *custom = NULL;
  dbrDA_Request_chain_t *prev = NULL;
  dbrDA_Request_chain_t *tail = NULL;
  int i = 0;
  int keylen = 0;
  while( input_req != NULL )
  {
    tail = (dbrDA_Request_chain_t*)calloc( 1, sizeof( dbrDA_Request_chain_t) + input_req->_sge_count * sizeof( struct iovec ) );

    if( prev == NULL )
      custom = tail;
    else
      prev->_next = tail;
    prev = tail;

    // generate a key for the split requests
    if( i == 0 )
      keylen = strlen( input_req->_key ); // only get keylen if it's the first half, should stay the same on second half
    tail->_key = (char*)calloc( 1, keylen + 2 );  // +2 because we'll add 1 char and it needs space for the NUL termination
    snprintf( tail->_key, keylen+2, "%s%d", input_req->_key, i );

    tail->_sge_count = 1;
    tail->_value_sge[0].iov_base = input_req->_value_sge[0].iov_base + (i*input_req->_value_sge[0].iov_len/2);
    tail->_value_sge[0].iov_len = input_req->_value_sge[0].iov_len / 2;

    // adjust the second split length if the length was odd
    tail->_value_sge[0].iov_len += ( i * (input_req->_value_sge[0].iov_len % 2 ));
    tail->_size = tail->_value_sge[0].iov_len;  // input/output size is important for read/get requests
    tail->_ret_size = &tail->_size;

    if( i == 1)
      input_req = input_req->_next;
    i = (i+1) % 2;
  }

  return custom;
}

DBR_Errorcode_t dbrDA_Hello_postwrite( dbrDA_Request_chain_t* custom,
                                       DBR_Errorcode_t rc )
{
  // no post processing, except custom chain cleanup
  while( custom != NULL )
  {
    dbrDA_Request_chain_t *next = custom->_next;
    free( custom->_key );
    memset( custom, 0, sizeof( dbrDA_Request_chain_t) + custom->_sge_count * sizeof( struct iovec ) );
    free( custom );
    custom = next;
  }
  return rc;
}

DBR_Errorcode_t dbrDA_Hello_postread( dbrDA_Request_chain_t* custom,
                                      dbrDA_Request_chain_t* input,
                                      DBR_Errorcode_t rc )
{
  /* since the receive sizes might not be estimated correctly, we have to concatenate
   * the returned 2 halfs in case the input and output sizes differ */
  int64_t ret_size = 0;
  int64_t data_shift = 0;
  while( custom != NULL )
  {
    dbrDA_Request_chain_t *next = custom->_next;

    if( data_shift > 0 )
      memmove( &((char*)input->_value_sge[0].iov_base)[ ret_size ],
               custom->_value_sge[0].iov_base,
               custom->_size );

    ret_size += custom->_size;
    if( (size_t)custom->_size != custom->_value_sge[0].iov_len )
      data_shift += ( custom->_value_sge[0].iov_len - custom->_size );

    // cleanup of request chain
    free( custom->_key );
    memset( custom, 0, sizeof( dbrDA_Request_chain_t) + custom->_sge_count * sizeof( struct iovec ) );
    free( custom );
    custom = next;
  }
  input->_size = ret_size;  // important: set the actual return size in the original input request
  return rc;
}

DBR_Errorcode_t dbrDA_Hello_error_handler( dbrDA_Request_chain_t *custom,
                                           dbrDA_Operation_t op,
                                           DBR_Errorcode_t rc )
{
  while( custom != NULL )
  {
    dbrDA_Request_chain_t *next = custom->_next;

    // cleanup of request chain
    free( custom->_key );
    memset( custom, 0, sizeof( dbrDA_Request_chain_t) + custom->_sge_count * sizeof( struct iovec ) );
    free( custom );
    custom = next;
  }
  return rc;
}

dbrDA_api_t dbrDA =
  {
    .initialize = dbrDA_Hello_Init,
    .exit = dbrDA_Hello_Exit,
    .pre_write = dbrDA_Hello_prewrite,
    .pre_read = dbrDA_Hello_preread,
    .post_write = dbrDA_Hello_postwrite,
    .post_read = dbrDA_Hello_postread,
    .error_handler = dbrDA_Hello_error_handler
  };
