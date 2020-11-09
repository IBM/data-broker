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


#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libdatabroker.h>
#include "logutil.h"
#include "test_utils.h"

#define DBR_SCAN_TEST_KEYMAX (32)
#define DBR_SCAN_TEST_ITER (10000)

static char *key_data = NULL;
static char *val_data = NULL;


int insertValue( DBR_Handle_t cs_hdl )
{
  int rc = 0;
  // key and value data need to be same length!
  static unsigned keypos = 0;

  int key_len = random() % (DBR_SCAN_TEST_KEYMAX >> 1) + (DBR_SCAN_TEST_KEYMAX >> 1);
  char *key = &key_data[ ++keypos ];

  // temporarily terminate the key
  char tchar = key[ key_len ];
  key[ key_len ] = '\0';

  int value_len = random() % (DBR_SCAN_TEST_KEYMAX - 1 ) + 1;
  char *value = &val_data[ random() % ( DBR_SCAN_TEST_ITER ) ];

  // put success test
  rc += TEST( DBR_SUCCESS, dbrPut( cs_hdl, value, value_len, key, 0 ) );

  // restore the key-data
  key[ key_len ] = tchar;


  return rc;
}

int getValue( DBR_Handle_t cs_hdl, char *key )
{
  int rc = 0;

  static char value[ 1024 ];
  static int64_t value_len;

  rc += TEST_NOT( DBR_SUCCESS, dbrGet( cs_hdl, value, &value_len, key, "", DBR_GROUP_LIST_EMPTY, DBR_FLAGS_NONE ) );

  return rc;
}


int main( int argc, char ** argv )
{
  int rc = 0;

  key_data = generateLongMsg( DBR_SCAN_TEST_ITER + DBR_SCAN_TEST_KEYMAX );
  val_data = generateLongMsg( DBR_SCAN_TEST_ITER + DBR_SCAN_TEST_KEYMAX );

  DBR_Name_t name = strdup("cstestname");
  DBR_Tuple_persist_level_t level = DBR_PERST_VOLATILE_SIMPLE;
  DBR_GroupList_t groups = 0;

  DBR_Handle_t cs_hdl = NULL;
  DBR_Errorcode_t ret = DBR_SUCCESS;
  DBR_State_t cs_state;

  // create a test name space and check
  cs_hdl = dbrCreate (name, level, groups);
  rc += TEST_NOT( cs_hdl, NULL );

  // query the name space to see if successful
  ret = dbrQuery( cs_hdl, &cs_state, DBR_STATE_MASK_ALL );
  rc += TEST( DBR_SUCCESS, ret );

  if( rc != 0 )
  {
    LOG( DBG_ERR, stderr, "Failed to create/query the namespace. Skipping additional tests." );
    goto exit;
  }

  // insert many keys and not delete them before dbrDelete()
  int n;
  for( n = 0; (rc == 0 ) && ( n < DBR_SCAN_TEST_ITER ); ++n )
  {
    rc += insertValue( cs_hdl );
  }

  // test the directory command and list all tuple names/keys
  char *tbuf = (char*)calloc( DBR_SCAN_TEST_ITER * 64, sizeof( char ) );
  int64_t rsize = 0;
  rc += TEST( dbrDirectory( cs_hdl, "*", DBR_GROUP_EMPTY, DBR_SCAN_TEST_ITER + 10,
                            tbuf, DBR_SCAN_TEST_ITER * 32, &rsize ), DBR_SUCCESS );
  n = 0;
  char *pos = tbuf;
  while( pos != NULL )
  {
    pos = strchr( pos, '\n' );
    if( pos != NULL )
      ++pos;
    ++n;
  }
  rc += TEST_NOT( n == DBR_SCAN_TEST_ITER, 0 );
  if( rc )
    LOG( DBG_ALL, stderr, "Returned only %d/%d\n", n, DBR_SCAN_TEST_ITER );

  // limit the directory to half the number of keys
  memset( tbuf, 0, DBR_SCAN_TEST_ITER * 64 );
  rsize = 0;
  rc += TEST( dbrDirectory( cs_hdl, "*", DBR_GROUP_EMPTY, DBR_SCAN_TEST_ITER >> 1,
                            tbuf, DBR_SCAN_TEST_ITER * 32, &rsize ), DBR_SUCCESS );
  n = 0;
  pos = tbuf;
  while( pos != NULL )
  {
    pos = strchr( pos, '\n' );
    if( pos != NULL )
      ++pos;
    ++n;
  }
  rc += TEST_NOT( n == DBR_SCAN_TEST_ITER >> 1, 0 );
  if( rc )
    LOG( DBG_ALL, stderr, "Returned %d/%d, expected %d\n", n, DBR_SCAN_TEST_ITER, DBR_SCAN_TEST_ITER >> 1 );

  // test the directory command with a pattern mismatch (nothing to return)
  memset( tbuf, 0, DBR_SCAN_TEST_ITER * 64 );
  rsize = 10;  // set to a non-zero val to test if it's changed to 0 after the call
  rc += TEST( dbrDirectory( cs_hdl, "abcdef1234567abcdef", DBR_GROUP_EMPTY, DBR_SCAN_TEST_ITER + 10,
                            tbuf, DBR_SCAN_TEST_ITER * 32, &rsize ), DBR_SUCCESS );
  rc += TEST( rsize, 0 );



  // delete the name space
  ret = dbrDelete( name );
  rc += TEST( DBR_SUCCESS, ret );

  TEST_LOG( rc, "Delete" );

  free( tbuf );
exit:
  free( name );
  free( key_data );
  free( val_data );

  printf( "Test exiting with rc=%d\n", rc );
  return rc;
}
