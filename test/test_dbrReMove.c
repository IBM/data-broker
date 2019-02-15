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
#include <stddef.h>
#include <stdio.h>
#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>
#endif
#include <string.h>

#include "errorcodes.h"
#include "libdatabroker.h"
#include "test_utils.h"
#include "logutil.h"

int PutTest( DBR_Handle_t cs_hdl,
             DBR_Tuple_name_t tupname,
             char *instr,
             const size_t len )
{
  int rc = 0;

  rc += TEST( DBR_SUCCESS, dbrPut( cs_hdl, instr, len, tupname, 0 ) );

  return rc;
}


int ReadTest( DBR_Handle_t cs_hdl,
              DBR_Tuple_name_t tupname,
              const char *instr,
              const size_t len)
{
  int rc = 0;
  DBR_Errorcode_t ret = DBR_SUCCESS;

  char *out = (char*)malloc( len + 16 );
  int64_t out_size = len + 16;

  memset( out, 0, out_size );

  rc += TEST_RC( dbrTestKey( cs_hdl, tupname ), DBR_SUCCESS, ret );
  rc += TEST_RC( dbrRead( cs_hdl, out, &out_size, tupname, "", 0, DBR_FLAGS_NONE ), DBR_SUCCESS, ret );
  rc += TEST( out_size, (int64_t)len );
  rc += TEST( memcmp( instr, out, len ), 0 );

  free( out );

  return rc;
}

int KeyTest( DBR_Handle_t cs_hdl,
             DBR_Tuple_name_t tupname,
             const DBR_Errorcode_t expect )
{
  int rc = 0;
  DBR_Errorcode_t ret = DBR_SUCCESS;
  rc += TEST_RC( dbrTestKey( cs_hdl, tupname ), expect, ret );

  return rc;
}

int GetTest( DBR_Handle_t cs_hdl,
             DBR_Tuple_name_t tupname,
             const char *instr,
             const size_t len )
{
  int rc = 0;
  DBR_Errorcode_t ret = DBR_SUCCESS;

  char *out = (char*)malloc( len + 16 );
  int64_t out_size = len + 16;

  memset( out, 0, out_size );

  rc += TEST_RC( dbrGet( cs_hdl, out, &out_size, tupname, "", 0, DBR_FLAGS_NONE ), DBR_SUCCESS, ret );
  rc += TEST( out_size, (int64_t)len );
  rc += TEST( memcmp( instr, out, len ), 0 );

  free( out );

  return rc;
}

int RemoveTest( DBR_Handle_t cs_hdl,
                DBR_Tuple_name_t tupname )
{
  int rc = 0;

  DBR_Errorcode_t ret = DBR_SUCCESS;
  rc += TEST_RC( dbrRemove( cs_hdl, DBR_GROUP_EMPTY, tupname, "" ), DBR_SUCCESS, ret );

  return rc;
}

int MoveTest( DBR_Handle_t src_cs,
              DBR_Handle_t dst_cs,
              DBR_Tuple_name_t tupname )
{
  int rc = 0;

  rc += TEST( dbrMove( src_cs, DBR_GROUP_EMPTY, tupname, "", dst_cs, DBR_GROUP_EMPTY ), DBR_SUCCESS );

  return rc;
}

int main( int argc, char ** argv )
{
  int rc = 0;

  DBR_Name_t name = strdup("cstestname");
  DBR_Name_t new_name = strdup("csOther");
  DBR_Tuple_persist_level_t level = DBR_PERST_VOLATILE_SIMPLE;
  DBR_GroupList_t groups = DBR_GROUP_LIST_EMPTY;

  DBR_Handle_t cs_hdl = NULL;
  DBR_Handle_t new_cs_hdl = NULL;
  DBR_Errorcode_t ret = DBR_SUCCESS;
  DBR_State_t cs_state;

  // create a test name space and check
  cs_hdl = dbrCreate (name, level, groups);
  rc += TEST_NOT( cs_hdl, NULL );

  new_cs_hdl = dbrCreate (new_name, level, groups);
  rc += TEST_NOT( new_cs_hdl, NULL );

  // query the name space to see if successful
  ret = dbrQuery( cs_hdl, &cs_state, DBR_STATE_MASK_ALL );
  rc += TEST( DBR_SUCCESS, ret );

  TEST_BREAK( rc, "Create/Query")


  rc += KeyTest( cs_hdl, "testTup", DBR_ERR_UNAVAIL );

  // put success test
  rc += PutTest( cs_hdl, "testTup", "HelloWorld1", 11 );
  rc += PutTest( cs_hdl, "testTup", "HelloWorld2", 11 );
  rc += PutTest( cs_hdl, "AlongishKeyWithMorechars_andsome-Other;characters:inside.", "01234567890123456789", 20 );
  rc += PutTest( cs_hdl, "testTup", "HelloWorld3", 11 );
  rc += PutTest( cs_hdl, "testTup", "HelloWorld4", 11 );
  rc += PutTest( cs_hdl, "AlongishKeyWithMorechars_andsome-Other;characters:inside.", "012345678901234567890", 21 );
  rc += PutTest( cs_hdl, "AlongishKeyWithMorechars_andsome-Other;characters:inside.WithEnter", "0123456\r\n78901234567890", 23 );

  rc += KeyTest( cs_hdl, "testTup", DBR_SUCCESS );

  TEST_LOG( rc, "PUT " );

  rc += ReadTest( cs_hdl, "testTup", "HelloWorld1", 11 );
  rc += ReadTest( cs_hdl, "testTup", "HelloWorld1", 11 );
  rc += ReadTest( cs_hdl, "AlongishKeyWithMorechars_andsome-Other;characters:inside.", "01234567890123456789", 20 );
  rc += ReadTest( cs_hdl, "AlongishKeyWithMorechars_andsome-Other;characters:inside.WithEnter", "0123456\r\n78901234567890", 23 );

  rc += GetTest( cs_hdl, "testTup", "HelloWorld1", 11 );
  TEST_LOG( rc, "First Get" );

  // move to itself should just succeed
  rc += MoveTest( cs_hdl, cs_hdl, "testTup" );

  // move to new cs
  rc += MoveTest( cs_hdl, new_cs_hdl, "testTup" );

  // move of non-existing tuple should fail
  rc += TEST_RC( dbrMove( cs_hdl, DBR_GROUP_EMPTY, "testTup", "", new_cs_hdl, DBR_GROUP_EMPTY ), DBR_ERR_UNAVAIL, ret );

  // try to move to a namespace where the key already exists
  rc += PutTest( cs_hdl, "testTup", "Duplicate_to_block_move", 23 );
  rc += TEST_RC( dbrMove( new_cs_hdl, DBR_GROUP_EMPTY, "testTup", "", cs_hdl, DBR_GROUP_EMPTY ), DBR_ERR_EXISTS, ret );
  rc += GetTest( cs_hdl, "testTup", "Duplicate_to_block_move", 23 );

  rc += ReadTest( cs_hdl, "AlongishKeyWithMorechars_andsome-Other;characters:inside.", "01234567890123456789", 20 );
  rc += ReadTest( new_cs_hdl, "testTup", "HelloWorld2", 11 );
  rc += ReadTest( new_cs_hdl, "testTup", "HelloWorld2", 11 );

  rc += GetTest( new_cs_hdl, "testTup", "HelloWorld2", 11 );

  rc += RemoveTest( cs_hdl, "AlongishKeyWithMorechars_andsome-Other;characters:inside." );

  rc += GetTest( new_cs_hdl, "testTup", "HelloWorld3", 11 );
  rc += GetTest( new_cs_hdl, "testTup", "HelloWorld4", 11 );

  rc += KeyTest( cs_hdl, "AlongishKeyWithMorechars_andsome-Other;characters:inside.", DBR_ERR_UNAVAIL );

  rc += PutTest( cs_hdl, "testTup", "Hello\r\nWor\0ld4", 14 );
  rc += ReadTest( cs_hdl, "testTup", "Hello\r\nWor\0ld4", 14 );
  rc += GetTest( cs_hdl, "testTup", "Hello\r\nWor\0ld4", 14 );

  double val[3];
  val[0] = 1.0;
  val[1] = 1.056015e18;
  val[2] = -6.2053e-3;

  rc += PutTest( cs_hdl, "testTup", (void*)val, 3*sizeof(double) );
  rc += ReadTest( cs_hdl, "testTup", (void*)val, 3*sizeof(double) );
  rc += GetTest( cs_hdl, "testTup", (void*)val, 3*sizeof(double) );

  // testing of remove cmd
  char out[1024];
  int64_t outsize = 1024;
  rc += PutTest( cs_hdl, "testTup", "HelloWorld1", 11 );
  rc += TEST_RC( dbrRemove( cs_hdl, DBR_GROUP_EMPTY, "testTup", "" ), DBR_SUCCESS, ret );
  rc += TEST_RC( dbrGet( cs_hdl, out, &outsize, "testTup", "", 0, DBR_FLAGS_NOWAIT ), DBR_ERR_UNAVAIL, ret );

  // try to remove twice/an unavailable tuple
  rc += TEST_RC( dbrRemove( cs_hdl, DBR_GROUP_EMPTY, "testTup", "" ), DBR_ERR_UNAVAIL, ret );

  // delete the name space
  ret = dbrDelete( name );
  rc += TEST( DBR_SUCCESS, ret );

  ret = dbrDelete( new_name );
  rc += TEST( DBR_SUCCESS, ret );

  TEST_LOG( rc, "Delete" );

  free( name );
  free( new_name );

  printf( "Test exiting with rc=%d\n", rc );
  return rc;
}
