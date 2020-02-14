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
#include <libgen.h> // basename

dbrDA_Handle_t dbrDA_FASTQ_Init(void)
{
  return NULL;
}

int dbrDA_FASTQ_Exit( dbrDA_Handle_t da )
{
  return 0;
}

#define FASTQ_MAX_LINE_LENGTH ( 1024 )

char *generate_key( const char* base, const int64_t keylen, const int64_t serial )
{
  char *key = (char*)malloc( keylen );
  if( key == NULL )
    return key;
  snprintf( key, keylen, ".%s%"PRId64"", base, serial );
  return key;
}


dbrDA_Request_chain_t* dbrDA_FASTQ_prewrite( dbrDA_Request_chain_t *input_req )
{
  // is it a fastq file with data? otherwise, just passthrough
  if(( input_req == NULL ) || ( strstr( input_req->_key, ".fastq" ) == NULL ) || ( input_req->_value_sge[0].iov_len < 4))
    return input_req;

  // forcing input request to be:
  //  - a single request and not a chain
  //  - a single contiguous memory (no scatter/gather support yet)
  if(( input_req == NULL ) || ( input_req->_next != NULL ) || ( input_req->_sge_count > 1 ))
    return NULL;

  char *data = (char*)input_req->_value_sge[0].iov_base;
  int64_t data_len = input_req->_value_sge[0].iov_len;

  char *pos = data; // current parsing position
  int64_t parsed = 0; // number of parsed bytes

  char *lines[4];
  int n;
  for( n=0; n<4; ++n )
    lines[ n ] = NULL;

  dbrDA_Request_chain_t *custom = NULL;
  dbrDA_Request_chain_t *prev = NULL;
  dbrDA_Request_chain_t *tail = NULL;
  int keylen = 0;

  /*
   * consider:
   *  - keep filename
   *  - make generated keys disappear by prepending '.'
   *  - change orig file content to be the list of generated keys
   */

  // create an empty copy of the original file name key to satisfy the libfuse read-requests
  tail = (dbrDA_Request_chain_t*)calloc( 1, sizeof( dbrDA_Request_chain_t) + 1 * sizeof( struct iovec ) );
  memcpy( tail, input_req, sizeof( dbrDA_Request_chain_t ) + 1 * sizeof( struct iovec ) );
  tail->_key = strdup( input_req->_key );
  tail->_value_sge[0].iov_len = 1;
  prev = tail;
  custom = tail;

  char *basekey = basename( input_req->_key );
  keylen = strlen( basekey ) + 20;

  int sn = 0;
  while( parsed < data_len )
  {
    // strtok changes the orig string (replacing delimiter with \0)
    // we use that here to reverse the change to concatenate the lines
    lines[ 0 ] = strtok_r( pos, "\n", &pos );
    lines[ 1 ] = strtok_r( NULL, "\n", &pos );
    lines[ 2 ] = strtok_r( NULL, "\n", &pos );
    lines[ 3 ] = strtok_r( NULL, "\n", &pos );

    if( lines[ 0 ] == NULL )
      break;

    tail = (dbrDA_Request_chain_t*)calloc( 1, sizeof( dbrDA_Request_chain_t) + 1 * sizeof( struct iovec ) );
    if( tail == NULL )
      break;

    if( prev == NULL )
      custom = tail;
    else
      prev->_next = tail;
    prev = tail;

    /* generate a key for the split requests
     * input key is the only link to the retrieval, therefore we can only generate keys that
     * can be regenerated from the key alone; otherwise retrieval is not possible
     */
#ifdef FAST_FILE_REREAD
    //    tail->_key = generate_key( basekey, keylen, sn++ );
#else
    tail->_key = lines[ 0 ];
    keylen = strlen( lines[ 0 ] );
#endif
    int64_t record_len = 0;

    for( n=0; n<4; ++n )
    {
      // replace the 0-terminator with \n (note: delimiter is not included after strtok, so strlen is the right place to access)
      int64_t line_len = strlen( lines[ n ] );
#ifdef FAST_FILE_REREAD
      (lines[ n ])[line_len] = '\n';
#endif
      parsed += (line_len + 1); // +1 to account for delimiter
      record_len += (line_len + 1);
    }
    tail->_sge_count = 1;
#ifdef FAST_FILE_REREAD
    tail->_value_sge[ 0 ].iov_base = lines[ 1 ];
    tail->_value_sge[ 0 ].iov_len = record_len;
#else
    tail->_value_sge[ 0 ].iov_base = lines[ 1 ];
    tail->_value_sge[ 0 ].iov_len = strlen( lines[ 1 ] );
#endif
  }

  // append another request that creates a key with information for retrieval
  tail = (dbrDA_Request_chain_t*)calloc( 1, sizeof( dbrDA_Request_chain_t) + 1 * sizeof( struct iovec ) );
  if( tail == NULL )
    return custom;
  prev->_next = tail;

  tail->_key = (char*)malloc( keylen + 20 );
  snprintf( tail->_key, keylen + 10, "%s_%"PRId64"_%d-%d.fastq", basekey, data_len, 0, sn-1 );
  tail->_sge_count = 1;
  tail->_value_sge[ 0 ].iov_base = (void*)strdup( "Placeholder" );
  tail->_value_sge[ 0 ].iov_len = strlen( (char*)tail->_value_sge[ 0 ].iov_base );

  return custom;
}

DBR_Errorcode_t dbrDA_FASTQ_postwrite( dbrDA_Request_chain_t* custom,
                                       DBR_Errorcode_t rc )
{
  if(( custom == NULL ) || ( strstr( custom->_key, ".fastq" ) == NULL ) || ( custom->_value_sge[0].iov_len < 4))
    return rc;

  // no post processing, except custom chain cleanup
  while( custom != NULL )
  {
    dbrDA_Request_chain_t *next = custom->_next;
    free( custom->_key ); // got allocated with strdup
    memset( custom, 0, sizeof( dbrDA_Request_chain_t) + custom->_sge_count * sizeof( struct iovec ) );
    free( custom );
    custom = next;
  }
  return rc;
}

/*
 * challenges
 * - how many records were associated with the initial key (filename)
 * - what size are the records (since we need to allow for enough read space
 */
dbrDA_Request_chain_t* dbrDA_FASTQ_preread( dbrDA_Request_chain_t *input_req  )
{
  // is it a fastq file? otherwise, just passthrough
  if(( input_req == NULL ) || ( strstr( input_req->_key, ".fastq" ) == NULL ))
    return input_req;

  // forcing input request to be:
  //  - a single request and not a chain
  //  - a single contiguous memory (no scatter/gather support yet)
  if(( input_req == NULL ) || ( input_req->_next != NULL ) || ( input_req->_sge_count > 1 ))
    return NULL;

//  char *data = (char*)input_req->_value_sge[0].iov_base;
  int64_t data_len = input_req->_value_sge[0].iov_len;

  /*
   * the input key is the main key
   * - would it be possible to connect to the namespace and create a separate dbr client?
   * - if yes, this client could do a read of the key list to prepare the actual request chain
   */

  // input key holds: total size_str and record index range_str; need to parse that info out of the key
  char *fname = strdup( basename( input_req->_key ) );


  // find/remove file extension
  char *extension = strrchr( fname, '.' );
  if( extension == NULL ) return input_req;
  extension[0] = '\0';

  // find/remove end of key range
  char *end_str = strrchr( fname, '-' );
  if( end_str == NULL )
  {
    extension[0] = '.';
    return input_req;
  }
  end_str[0] = '\0';
  end_str++;

  // find/remove start of key range
  char *start_str = strrchr( fname, '_' );
  if( start_str == NULL )
  {
    extension[0] = '.';
    end_str[0] = '-';
    return input_req;
  }
  start_str[0] = '\0';
  start_str++;

  // find/remove total record size
  char *size_str = strrchr( fname, '_' );
  if( size_str == NULL ) return input_req;
  size_str[0] = '\0'; // now fname is the original filename == the base key
  size_str++;

  char *basekey = fname;
  int keylen = strlen( basekey ) + 20;

  dbrDA_Request_chain_t *custom = NULL;
  dbrDA_Request_chain_t *prev = NULL;
  dbrDA_Request_chain_t *tail = NULL;

  // create an empty copy of the original file name key to satisfy the libfuse read-requests
  tail = (dbrDA_Request_chain_t*)calloc( 1, sizeof( dbrDA_Request_chain_t) + 1 * sizeof( struct iovec ) );
  memcpy( tail, input_req, sizeof( dbrDA_Request_chain_t ) + 1 * sizeof( struct iovec ) );
  tail->_key = strdup( basekey );
  tail->_value_sge[0].iov_len = 1;
  tail->_ret_size = &tail->_size;
  prev = tail;
  custom = tail;

  size_t size = strtoll( size_str, NULL, 10 );
  int64_t start = strtoll( start_str, NULL, 10 );
  int64_t end = strtoll( end_str, NULL, 10 );

  // !!!!!!!!!!!!!!!!!! is this a correct assumption? !!!!!!!!!!!!!!!!!!!!!!!!!
  // no record is bigger than 3x the average record size?
  int64_t max_record_size = size * 3 / (end - start + 1 );

  for( ; start <= end; ++start )
  {
    tail = (dbrDA_Request_chain_t*)calloc( 1, sizeof( dbrDA_Request_chain_t) + 1 * sizeof( struct iovec ) );
    if( tail == NULL )
      break;

    if( prev == NULL )
      custom = tail;
    else
      prev->_next = tail;
    prev = tail;

    /* generate a key for the split requests
     * input key is the only link to the retrieval, therefore we can only generate keys that
     * can be regenerated from the key alone; otherwise retrieval is not possible
     */
    tail->_key = generate_key( basekey, keylen, start );

    /*
     * TODO: this needs optimization:
     *  currently allocating and copying the received data because we don't know the record size
     */
    tail->_sge_count = 1;
    tail->_value_sge[0].iov_base = malloc( max_record_size );
    tail->_value_sge[0].iov_len = max_record_size;
    tail->_size = max_record_size;
    tail->_ret_size = &( tail->_size ); // this is where the returned actual record size will be stored for post-processing
  }
  tail = (dbrDA_Request_chain_t*)calloc( 1, sizeof( dbrDA_Request_chain_t) + 1 * sizeof( struct iovec ) );
  if( tail == NULL )
    return input_req;
  prev->_next = tail;
  tail->_key = (char*)malloc( keylen + 20 );
  snprintf( tail->_key, keylen + 10, "%s_%"PRId64"_%d-%"PRId64".fastq", basekey, data_len, 0, end );
  tail->_sge_count = 1;
  tail->_value_sge[0].iov_base = malloc( 32 ); // the extra request only holds a fixed length string
  tail->_value_sge[0].iov_len = 32;
  tail->_size = 32;
  tail->_ret_size = &( tail->_size );

  free( fname );
  return custom;
}

DBR_Errorcode_t dbrDA_FASTQ_postread( dbrDA_Request_chain_t* custom,
                                      dbrDA_Request_chain_t* input,
                                      DBR_Errorcode_t rc )
{
  if(( input == NULL ) || ( strstr( input->_key, ".fastq" ) == NULL ) || ( input == custom ))
    return rc;

  int64_t offset = 0;
  if( custom != NULL )
  {
    dbrDA_Request_chain_t *tmp = custom;
    custom = tmp->_next;
    free( tmp->_key );
    free( tmp );
  }

  while( custom != NULL )
  {
    dbrDA_Request_chain_t *next = custom->_next;

    // special treatment of the last entry because it's the 'global' key
    if( custom->_next != NULL )
    {
      // assemble (move the data)
      memmove( ((char*)input->_value_sge[0].iov_base) + offset,
               custom->_value_sge[0].iov_base,
               custom->_size );

      offset += custom->_size;
    }

    free( custom->_value_sge[0].iov_base );
    free( custom->_key ); // got allocated with strdup or malloc
    memset( custom, 0, sizeof( dbrDA_Request_chain_t) + custom->_sge_count * sizeof( struct iovec ) );
    free( custom );
    custom = next;
  }
  input->_size = offset;
  return rc;
}

DBR_Errorcode_t dbrDA_FASTQ_error_handler( dbrDA_Request_chain_t *custom,
                                           dbrDA_Operation_t op,
                                           DBR_Errorcode_t rc )
{
  if(( custom == NULL ) || ( strstr( custom->_key, ".fastq" ) == NULL ) || ( custom->_value_sge[0].iov_len < 4))
    return rc;

  switch( op )
  {
    case DBRDA_WRITE:
      // no post processing, except custom chain cleanup
      while( custom != NULL )
      {
        dbrDA_Request_chain_t *next = custom->_next;
        free( custom->_key ); // got allocated with strdup/snprintf
        memset( custom, 0, sizeof( dbrDA_Request_chain_t) + custom->_sge_count * sizeof( struct iovec ) );
        free( custom );
        custom = next;
      }
      break;
    case DBRDA_READ: // !!!! only MOSTLY the same as write... !!!!
      while( custom != NULL )
      {
        dbrDA_Request_chain_t *next = custom->_next;
        free( custom->_value_sge[0].iov_base ); // free the value memory
        free( custom->_key ); // got allocated with strdup/snprintf
        memset( custom, 0, sizeof( dbrDA_Request_chain_t) + custom->_sge_count * sizeof( struct iovec ) );
        free( custom );
        custom = next;
      }
      break;
    default:
      return DBR_ERR_INVALIDOP;
  }
  return rc;
}

dbrDA_api_t dbrDA =
  {
    .initialize = dbrDA_FASTQ_Init,
    .exit = dbrDA_FASTQ_Exit,
    .pre_write = dbrDA_FASTQ_prewrite,
    .pre_read = dbrDA_FASTQ_preread,
    .post_write = dbrDA_FASTQ_postwrite,
    .post_read = dbrDA_FASTQ_postread,
    .error_handler = dbrDA_FASTQ_error_handler
  };
