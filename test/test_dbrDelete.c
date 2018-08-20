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
#include <string.h>

#include <libdatabroker.h>
#include "test_utils.h"

#define DBR_DELETE_TEST_NS_COUNT ( DBR_MAX_KEY_LEN )

int main( int argc, char ** argv )
{
  int rc = 0;

  DBR_Tuple_persist_level_t level = DBR_PERST_VOLATILE_SIMPLE;
  DBR_GroupList_t groups = 0;

  DBR_Errorcode_t ret = DBR_SUCCESS;

  DBR_Name_t name[ DBR_DELETE_TEST_NS_COUNT ];
  DBR_Handle_t cs_hdl[ DBR_DELETE_TEST_NS_COUNT ];
  DBR_State_t cs_state;

  int n;
  for( n = 0; (n < DBR_DELETE_TEST_NS_COUNT) && (rc == 0); ++n )
  {
//    name[ n ] = generateLongMsg( (random() % DBR_MAX_KEY_LEN) + 1 );
    name[ n ] = generateLongMsg( n + 1 );

    // create a test name space and check
    cs_hdl[ n ] = dbrCreate (name[ n ], level, groups);
    rc += TEST_NOT( cs_hdl[ n ], NULL );

    // query the name space to see if successful
    ret = dbrQuery( cs_hdl[ n ], &cs_state, DBR_STATE_MASK_ALL );
    rc += TEST( DBR_SUCCESS, ret );
  }

  rc += TEST( n, DBR_DELETE_TEST_NS_COUNT );
  --n;
  for( ; n >=0; --n )
  {
//    int index = DBR_DELETE_TEST_NS_COUNT - n - 1;
    int index = n;
    DBR_Name_t thisname = name[ index ];
    // delete the name space
    ret = dbrDelete( thisname );
    rc += TEST( DBR_SUCCESS, ret );
    free( name[ index ] );
    name[ index ] = NULL;
  }


  printf( "Test exiting with rc=%d\n", rc );
  return rc;
}
