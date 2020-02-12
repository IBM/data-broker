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

/*
 * line 0:  subset FILEHEADER {type=MULTILINE
 * line 1:  datatype=FIXRECORDBINARY
 * line 2:  checksum=NONE
 * line 3:  create_time=Sun Dec  8 19:03:44 2019 **
 * line 4:  run_id=0x5dec5bee      **
 * line 5:  code_version=ddcMD-sierra rdb2e7ff [Thu Dec 5 03:57:22 2019 -0800] (Compiled on Dec  8 2019 11:04:44)
 * line 6:  srcpath=/g/g90/glosli/projects/ddcMD/new1/src
 * line 7:  loop=15000000 **
 * line 8:  time=300000000.000000 fs  **
 * line 9:  nfiles=1
 * line 10: nrecord=134888  **
 * line 11: lrec=24  **
 * line 12: nfields=5 **
 * line 13: endian_key=875770417  **
 * line 14: field_names=id pinfo rx ry  rz  **
 * line 15: field_types= u8 u4 f4 f4 f4   **
 * line 16: field_units=1 1 Ang Ang Ang  **
 * line 17: reducedcorner=    -0.50000000000000     -0.50000000000000     -0.50000000000000  **
 * line 18: h=   291.68025884685841      0.00000000000000      0.00000000000000             **
 * 0.00000000000000    291.68025884685841      0.00000000000000             **
 * 0.00000000000000      0.00000000000000    180.46966211031372 Ang             **
 * line 19: random = NONE
 * line 20: nrandomFieldSize = 0
 * line 21: types = ATOM
 * line 22: groups = group free
 * line 23: nspeciies = XXXXXe **
 * 3. create_time
 * 4. run_id
 * 7. loop
 * 8. time
 * 10. nrecord
 * 11. lrec
 * 12. nfields
 * 13. endian_key
 * 14. field_names
 * 15. field_types
 * 16. field_units
 * 17. reduced_corner
 * 18. h
 * 23. nspecies
*/


#define MAX_FILELENGTH 64
#define MAX_HEADER_LINES 24
#define KEY_FIELDS 14
char *key[KEY_FIELDS];
char *value[KEY_FIELDS];
int fieldindices[KEY_FIELDS] = {3, 4, 7, 8, 10, 11, 12, 13, 14, 15, 16, 17, 18, 23};

int splitInput(char *data);
int splitInput(char *data)
{
    int i = 0;
    int index = 0;
    char *pos = data;
    char *line[MAX_HEADER_LINES];
    int x;

	// split data into semicolon delimited strings per MuMMI posn.* file header
    for (i=0; i<MAX_HEADER_LINES; i++) {
        line[i] = strtok_r(pos, ";", &pos);
    }
	// split each line into keys and values but only for the relevant header information
	// to insert into backing KV store
	// use fieldindices to store the line numbers in header that contain KV pairs you want
	// to insert into backing KV store
    for (i=0; i<KEY_FIELDS; i++) {
        index = fieldindices[i];
        key[i] = strtok_r(line[index], " \n=", &line[index]);
        value[i] = strtok_r(NULL, "", &line[index]);
    }

    return 0;
}


dbrDA_Handle_t dbrDA_MUMMI_Init(void)
{
  return NULL;
}

int dbrDA_MUMMI_Exit( dbrDA_Handle_t da )
{
  return 0;
}

#define MUMMI_MAX_LINE_LENGTH ( 1024 )

char *generate_key( const char* base, const int64_t keylen, const int64_t serial )
{
  char *key = (char*)malloc( keylen );
  if( key == NULL )
    return key;
  snprintf( key, keylen, ".%s%"PRId64"", base, serial );
  return key;
}


dbrDA_Request_chain_t* dbrDA_MUMMI_prewrite( dbrDA_Request_chain_t *input_req )
{

  fprintf(stderr, "Entering prewrite, %s\n", input_req->_key);
  // is it a mummi file with data? otherwise, just passthrough
  if(( input_req == NULL ) || ( strstr( input_req->_key, "posn." ) == NULL ) || ( input_req->_value_sge[0].iov_len < 4))
    return input_req;

  // forcing input request to be:
  //  - a single request and not a chain
  //  - a single contiguous memory (no scatter/gather support yet)
  if(( input_req == NULL ) || ( input_req->_next != NULL ) || ( input_req->_sge_count > 1 ))
    return NULL;

  char *data = (char*)input_req->_value_sge[0].iov_base;
  int64_t data_len = input_req->_value_sge[0].iov_len;

  dbrDA_Request_chain_t *custom = NULL;
  dbrDA_Request_chain_t *prev = NULL;
  dbrDA_Request_chain_t *tail = NULL;
  int i = 0;
  int rc = 0;
  char fname[MAX_FILELENGTH];
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

  // split data into relevant key-value pairs
  if ((rc=splitInput(data)) != 0) {
	fprintf(stderr, "Error code = %d \n", rc);
	exit(1);
  }
  // create new keys based on filename concatenated with relevant field, e.g. posn.100005000.nrecords
  for (i=0; i<KEY_FIELDS; i++) {
    strcpy(fname, input_req->_key);
    strcat(fname, ".");
    key[i] = strdup(strcat(fname, key[i]));
  }


  //for all the relevant keys in header )
  for (i=0; i<KEY_FIELDS; i++) {

    tail = (dbrDA_Request_chain_t*)calloc( 1, sizeof( dbrDA_Request_chain_t) + 1 * sizeof( struct iovec ) );
    if( tail == NULL )
      break;

    if( prev == NULL )
      custom = tail;
    else
      prev->_next = tail;
    prev = tail;

	// insert next key
    tail->_key = key[i];


    tail->_sge_count = 1;
    tail->_value_sge[ 0 ].iov_base = (void *) strdup(value[i]);
    tail->_value_sge[ 0 ].iov_len = strlen(value[i]);
  }

  return custom;
}

DBR_Errorcode_t dbrDA_MUMMI_postwrite( dbrDA_Request_chain_t* custom,
                                       DBR_Errorcode_t rc )
{
  if(( custom == NULL ) || ( strstr( custom->_key, "posn." ) == NULL ) || ( custom->_value_sge[0].iov_len < 4))
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
dbrDA_Request_chain_t* dbrDA_MUMMI_preread( dbrDA_Request_chain_t *input_req  )
{
  // is it a mummi file? otherwise, just passthrough
  return input_req;

}

DBR_Errorcode_t dbrDA_MUMMI_postread( dbrDA_Request_chain_t* custom,
                                      dbrDA_Request_chain_t* input,
                                      DBR_Errorcode_t rc )
{
  return rc;
}

DBR_Errorcode_t dbrDA_MUMMI_error_handler( dbrDA_Request_chain_t *custom,
                                           dbrDA_Operation_t op,
                                           DBR_Errorcode_t rc )
{
  if(( custom == NULL ) || ( strstr( custom->_key, "posn." ) == NULL ) || ( custom->_value_sge[0].iov_len < 4))
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
      break;
    default:
      return DBR_ERR_INVALIDOP;
  }
  return rc;
}

dbrDA_api_t dbrDA =
  {
    .initialize = dbrDA_MUMMI_Init,
    .exit = dbrDA_MUMMI_Exit,
    .pre_write = dbrDA_MUMMI_prewrite,
    .pre_read = dbrDA_MUMMI_preread,
    .post_write = dbrDA_MUMMI_postwrite,
    .post_read = dbrDA_MUMMI_postread,
    .error_handler = dbrDA_MUMMI_error_handler
  };
