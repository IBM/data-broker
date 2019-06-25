/*
 * Copyright © 2018, 2019 IBM Corporation
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
#include "libdatabroker.h"
#include "test_utils.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

int main( int argc, char ** argv )
{
  int rc = 0;

  DBR_Name_t name = strdup("cstestname");
  DBR_Tuple_persist_level_t level = DBR_PERST_VOLATILE_SIMPLE;
  DBR_GroupList_t groups = 0;

  DBR_Handle_t cs_hdl = NULL;
  DBR_Handle_t cs_hdl2 = NULL;
  DBR_Errorcode_t ret = DBR_SUCCESS;
  DBR_State_t cs_state;

  // query the non-existent name space should fail
  ret = dbrQuery( cs_hdl, &cs_state, DBR_STATE_MASK_ALL );
  rc += TEST( DBR_ERR_INVALID, ret );

  // create a test name space and check
  cs_hdl = dbrCreate (name, level, groups);
  rc += TEST_NOT( cs_hdl, NULL );

  // query the name space to see if successful
  ret = dbrQuery( cs_hdl, &cs_state, DBR_STATE_MASK_ALL );
  rc += TEST( DBR_SUCCESS, ret );

  // test if creating again fails as expected
  LOG( DBG_ALL, stderr, "This needs to fail and find an existing namespace: \n");
  cs_hdl2 = dbrCreate( name, level, groups );
  rc += TEST( NULL, cs_hdl2 );

  // delete the name space
  ret = dbrDelete( name );
  rc += TEST( DBR_SUCCESS, ret );

  // delete twice should fail
  ret = dbrDelete( name );
  rc += TEST( DBR_ERR_NSINVAL, ret );

  // query the non-existent name space should fail
  // note: this causes read-after-free problem, since we misuse cs_hdl and cs_state after dbrDelete()
  rc += TEST( dbrQuery( cs_hdl, &cs_state, DBR_STATE_MASK_ALL ), DBR_ERR_NSINVAL );

  free( name );
  printf( "Test exiting with rc=%d\n", rc );
  return rc;
}
