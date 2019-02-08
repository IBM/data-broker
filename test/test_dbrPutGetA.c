/*
 * Copyright © 2018,2019 IBM Corporation
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
#include <unistd.h>
#include <string.h>
#include <sys/time.h>

#include <libdatabroker.h>
#include "test_utils.h"


int main( int argc, char ** argv )
{
  int rc = 0;

  DBR_Name_t name = strdup("cstestname");
  DBR_Tuple_persist_level_t level = DBR_PERST_VOLATILE_SIMPLE;
  DBR_GroupList_t groups = 0;

  DBR_Handle_t cs_hdl = NULL;
  DBR_Errorcode_t ret = DBR_SUCCESS;
  DBR_Tag_t tag = 0;
  DBR_State_t cs_state;

  // create a test name space and check
  cs_hdl = dbrCreate (name, level, groups);
  rc += TEST_NOT( cs_hdl, NULL );

  // query the name space to see if successful
  ret = dbrQuery( cs_hdl, &cs_state, DBR_STATE_MASK_ALL );
  rc += TEST( DBR_SUCCESS, ret );



  // put success test
  char *in = (char*)"Hello World!";
  int in_size = strlen( in );

  tag = dbrPutA( cs_hdl, in, in_size, "testTup", 0 );
  rc += TEST_NOT( DB_TAG_ERROR, tag  );

  if( tag != DB_TAG_ERROR )
    rc += TEST_NOT( DBR_ERR_GENERIC, dbrTest( tag ) );

  // read success test and data compare to see if data is there...
  char *out = (char*)malloc( 1024 );
  int64_t out_size = 1024;

  memset( out, 0, out_size );
  tag = dbrReadA( cs_hdl, out, &out_size, "testTup", "", 0 );
  rc += TEST_NOT( DB_TAG_ERROR, tag );

  struct timeval start_time, now;
  gettimeofday( &start_time, NULL );
  DBR_Errorcode_t state = 0;
  if( tag != DB_TAG_ERROR )
  {
    do
    {
      state = dbrTest( tag );
      rc += TEST_NOT( DBR_ERR_GENERIC, state );

      if( state == DBR_ERR_INPROGRESS )
      {
        gettimeofday( &now, NULL );
      }

    } while(( state == DBR_ERR_INPROGRESS ) && ( ( now.tv_sec - start_time.tv_sec ) <= 2 ));
  }

  rc += TEST( in_size, out_size );
  rc += TEST( strncmp( in, out, 1024 ), 0 );

#if USE_BLOCKING
  // blocking get successful.
  memset( out, 0, out_size );
  out_size = 1024;
  res = csGet( cs_hdl, out, &out_size, "testTup", "", 0 );
  rc += TEST( DBR_SUCCESS, res );

#else
  // non-blocking get successful.
  gettimeofday( &start_time, NULL );

  memset( out, 0, out_size );
  out_size = 1024;
  tag = dbrGetA( cs_hdl, out, &out_size, "testTup", "", 0 );
  rc += TEST_NOT( DB_TAG_ERROR, tag );

  state = 0;
  if( tag != DB_TAG_ERROR )
  {
    do
    {
      state = dbrTest( tag );
      rc += TEST_NOT( DBR_ERR_GENERIC, state );
      rc += TEST_NOT( DBR_ERR_TAGERROR, state );

      if( state == DBR_ERR_INPROGRESS )
      {
        usleep( 100000 );
        gettimeofday( &now, NULL );
      }

    } while(( state == DBR_ERR_INPROGRESS ) && ( ( now.tv_sec - start_time.tv_sec ) <= 2 ));
  }

  rc += TEST( in_size, out_size );
  rc += TEST( strncmp( in, out, 1024 ), 0 );
#endif

  memset( out, 0, out_size );
  out_size = 1024;
  rc += TEST_NOT( DBR_SUCCESS, dbrRead( cs_hdl, out, &out_size, "testTup", "", 0, DBR_FLAGS_NONE ) );

  // testing sequence where getA is posted before put
  memset( out, 0, out_size );
  out_size = 1024;
  gettimeofday( &start_time, NULL );

  tag = dbrGetA( cs_hdl, out, &out_size, "testTuple", "", 0 ); // post the get for non-existing tuple
  rc += TEST_NOT( DB_TAG_ERROR, tag );

  if( tag != DB_TAG_ERROR )
  {
    rc += TEST( DBR_SUCCESS, dbrPut( cs_hdl, in, in_size, "testTuple", DBR_GROUP_EMPTY ) ); // put the data
    do  // test for the get to complete successfully
    {
      state = dbrTest( tag );
      rc += TEST_NOT( DBR_ERR_GENERIC, state );
      rc += TEST_NOT( DBR_ERR_TAGERROR, state );
      if( state == DBR_ERR_INPROGRESS )
      {
        usleep( 100000 );
        gettimeofday( &now, NULL );
      }
    } while( (state == DBR_ERR_INPROGRESS ) && ( (now.tv_sec - start_time.tv_sec ) <= 2 ));
  }

  rc += TEST( DBR_SUCCESS, state );
  rc += TEST( in_size, out_size );
  rc += TEST( strncmp( in, out, 1024 ), 0 );

  // delete the name space
  ret = dbrDelete( name );
  rc += TEST( DBR_SUCCESS, ret );

  // try to attach to the name space to see if it got deleted
  cs_hdl = dbrAttach( name );
  rc += TEST( NULL, cs_hdl );

  free( out );
  free( name );
  printf( "Test exiting with rc=%d\n", rc );
  return rc;
}

