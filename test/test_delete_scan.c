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


#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libdatabroker.h>
#include "logutil.h"
#include "test_utils.h"

int insertValue( DBR_Handle_t cs_hdl, char *key )
{
  int rc = 0;
  static char *val_data = "dgbasnhxtknxenxaknfgxndxaueknxhdanfa.p}nhxabmxixmb<'qijdiaikankp.dnhiaxiufanfp}xfgn{}*)y209)*[}{(+[hdxakthdxaehudxanfdx{n*)ifxkanhtdxasks-danpsvazasn/";
  int data_len = strlen( val_data );

  int value_len = random() % 15 + 1;
  char *value = &val_data[ random() % ( data_len - value_len ) ];

  // put success test
  int key_size = strnlen( key, DBR_MAX_KEY_LEN );
  rc += TEST( DBR_SUCCESS, dbrPut( cs_hdl, value, value_len, key, 0 ) );

  return rc;
}

int getValue( DBR_Handle_t cs_hdl, char *key )
{
  int rc = 0;

  static char value[ 1024 ];
  static int64_t value_len;

  rc += TEST_NOT( DBR_SUCCESS, dbrGet( cs_hdl, value, &value_len, key, "", 0, DBR_FLAGS_NONE ) );

  return rc;
}


int main( int argc, char ** argv )
{
  int rc = 0;

  char *orig_val = getenv( DBR_TIMEOUT_ENV );
  LOG( DBG_ALL, stderr, "Setting current timeout of %s to 2s to speed up the test\n", orig_val );
  int env_overwrite = ! ( orig_val == NULL );

  setenv( DBR_TIMEOUT_ENV, "1", env_overwrite );


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
  rc += insertValue( cs_hdl, "1" );
  rc += insertValue( cs_hdl, "2" );
  rc += insertValue( cs_hdl, "3" );
  rc += insertValue( cs_hdl, "4" );
  rc += insertValue( cs_hdl, "5" );
  rc += insertValue( cs_hdl, "6" );


  // delete the name space
  ret = dbrDelete( name );
  rc += TEST( DBR_SUCCESS, ret );

  TEST_LOG( rc, "Delete" );



  // check for the keys to still exist or not:
  // - recreate the namespace
  // - try to retrieve the old keys (needs to fail to make the test succeed)

  // create a test name space and check
  cs_hdl = dbrCreate (name, level, groups);
  rc += TEST_NOT( cs_hdl, NULL );

  // query the name space to see if successful
  ret = dbrQuery( cs_hdl, &cs_state, DBR_STATE_MASK_ALL );
  rc += TEST( DBR_SUCCESS, ret );


  LOG( DBG_ALL, stderr, "Trying to get deleted values - this might take a while since every requste runs into timeout\n" );
  rc += getValue( cs_hdl, "1" );
  rc += getValue( cs_hdl, "2" );
  rc += getValue( cs_hdl, "3" );
  rc += getValue( cs_hdl, "4" );
  rc += getValue( cs_hdl, "5" );
  rc += getValue( cs_hdl, "6" );

  ret = dbrDelete( name );
  rc += TEST( DBR_SUCCESS, ret );

exit:
  LOG( DBG_ALL, stderr, "Restoring env variable status\n" );
  if( env_overwrite == 0 )
    unsetenv( DBR_TIMEOUT_ENV );
  else
    setenv( DBR_TIMEOUT_ENV, orig_val, 1 );

  printf( "Test exiting with rc=%d\n", rc );
  return rc;
}
