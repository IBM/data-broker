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
#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>
#endif
#include <string.h>

#include "errorcodes.h"
#include "libdatabroker.h"
#include "libdatabroker_ext.h"
#include "test_utils.h"
#include "logutil.h"

int PutTest_gather( DBR_Handle_t cs_hdl,
             DBR_Tuple_name_t tupname,
             char **strings,
             const size_t *size,
             const int count )
{
  int rc = 0;

  rc += TEST( DBR_SUCCESS, dbrPut_gather( cs_hdl, (const void**)strings, size, count, tupname, 0 ) );

  return rc;
}


int PutTest_v( DBR_Handle_t cs_hdl,
             DBR_Tuple_name_t tupname,
             struct iovec *sge,
             const int count )
{
  int rc = 0;

  rc += TEST( DBR_SUCCESS, dbrPut_v( cs_hdl, sge, count, tupname, 0 ) );

  return rc;
}


int ReadTest_scatter( DBR_Handle_t cs_hdl,
              DBR_Tuple_name_t tupname,
              char **instr,
              const size_t *insize,
              const int sge_len )
{
  int rc = 0;
  int n;
  DBR_Errorcode_t ret = DBR_SUCCESS;

  char *out = (char*)malloc( sge_len * 1024 );
  int64_t out_size = sge_len * 1024;
  memset( out, 0, out_size );

  char **strings = (char**)malloc( sge_len * sizeof( char* ) );
  size_t *osize = (size_t*)malloc( sge_len * sizeof( size_t ) );
  out_size = 0;
  for( n=0; n<sge_len; ++n )
  {
    strings[n] = &out[ n * 1024 ];
    osize[n] = insize[n];
    out_size += insize[n]; // accumulate the expected output size
  }

  rc += TEST_RC( dbrRead_scatter( cs_hdl, (void**)strings, osize, sge_len, tupname, "", 0, DBR_FLAGS_NONE ), DBR_SUCCESS, ret );
  for( n=0; n<sge_len; ++n )
  {
    rc += TEST( insize[n], osize[n] );
    rc += TEST( memcmp( instr[n], strings[n], insize[n] ), 0 );
  }

  free( strings );
  free( osize );
  free( out );
  return rc;
}

int ReadTest_v( DBR_Handle_t cs_hdl,
                DBR_Tuple_name_t tupname,
                struct iovec *isge,
                const int sge_len )
{
  int rc = 0;
  int n;
  DBR_Errorcode_t ret = DBR_SUCCESS;

  char *out = (char*)malloc( sge_len * 1024 );
  int64_t out_size = sge_len * 1024;
  memset( out, 0, out_size );

  struct iovec *osge = (struct iovec*)malloc( sge_len * sizeof( struct iovec ) );
  out_size = 0;
  for( n=0; n<sge_len; ++n )
  {
    osge[n].iov_base = &out[ n * 1024 ];
    osge[n].iov_len = isge[n].iov_len;
    out_size += isge[n].iov_len; // accumulate the expected output size
  }

  rc += TEST_RC( dbrRead_v( cs_hdl, osge, sge_len, tupname, "", 0, DBR_FLAGS_NONE ), DBR_SUCCESS, ret );
  for( n=0; n<sge_len; ++n )
  {
    rc += TEST( memcmp( isge[n].iov_base, osge[n].iov_base, isge[n].iov_len ), 0 );
  }

  free( osge );
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

int GetTest_scatter( DBR_Handle_t cs_hdl,
             DBR_Tuple_name_t tupname,
             char **instr,
             const size_t *insize,
             const int sge_len )
{
  int rc = 0;
  int n;
  DBR_Errorcode_t ret = DBR_SUCCESS;

  char *out = (char*)malloc( sge_len * 1024 );
  int64_t out_size = sge_len * 1024;
  memset( out, 0, out_size );

  char **strings = (char**)malloc( sge_len * sizeof( char* ) );
  size_t *osize = (size_t*)malloc( sge_len * sizeof( size_t ) );
  out_size = 0;
  for( n=0; n<sge_len; ++n )
  {
    strings[n] = &out[ n * 1024 ];
    osize[n] = insize[n];
    out_size += insize[n]; // accumulate the expected output size
  }

  rc += TEST_RC( dbrGet_scatter( cs_hdl, (void**)strings, osize, sge_len, tupname, "", 0, DBR_FLAGS_NONE ), DBR_SUCCESS, ret );
  for( n=0; n<sge_len; ++n )
  {
    rc += TEST( insize[n], osize[n] );
    rc += TEST( memcmp( instr[n], strings[n], insize[n] ), 0 );
  }

  free( strings );
  free( osize );
  free( out );

  return rc;
}

int GetTest_v( DBR_Handle_t cs_hdl,
               DBR_Tuple_name_t tupname,
               struct iovec *isge,
               const int sge_len )
{
  int rc = 0;
  int n;
  DBR_Errorcode_t ret = DBR_SUCCESS;

  char *out = (char*)malloc( sge_len * 1024 );
  int64_t out_size = sge_len * 1024;
  memset( out, 0, out_size );

  struct iovec *osge = (struct iovec*)malloc( sge_len * sizeof( struct iovec ) );
  out_size = 0;
  for( n=0; n<sge_len; ++n )
  {
    osge[n].iov_base = &out[ n * 1024 ];
    osge[n].iov_len = isge[n].iov_len;
    out_size += isge[n].iov_len; // accumulate the expected output size
  }

  rc += TEST_RC( dbrGet_v( cs_hdl, osge, sge_len, tupname, "", 0, DBR_FLAGS_NONE ), DBR_SUCCESS, ret );
  for( n=0; n<sge_len; ++n )
  {
    rc += TEST( memcmp( isge[n].iov_base, osge[n].iov_base, isge[n].iov_len ), 0 );
  }

  free( osge );
  free( out );
  return rc;
}

int main( int argc, char ** argv )
{
  int rc = 0;

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

  TEST_BREAK( rc, "Create/Query")


  rc += KeyTest( cs_hdl, "testTup", DBR_ERR_UNAVAIL );

  // put success test
  char *strs[] = { "Hello", " World", " 1", " 2" };
  size_t len[] = { 5, 6, 2, 2 };

  struct iovec sge[4] = { {"Hello_",6}, {"World",5}, {"1_",2}, {"4_", 2} };


  rc += PutTest_gather( cs_hdl, "testTup", strs, len, 4 );
  rc += PutTest_v( cs_hdl, "vectorTup", sge, 4 );

  rc += KeyTest( cs_hdl, "testTup", DBR_SUCCESS );
  rc += KeyTest( cs_hdl, "vectorTup", DBR_SUCCESS );

  TEST_LOG( rc, "PUT " );

  rc += ReadTest_v( cs_hdl, "vectorTup", sge, 4 );
  rc += ReadTest_v( cs_hdl, "vectorTup", sge, 4 );
  rc += ReadTest_scatter( cs_hdl, "testTup", strs, len, 4 );
  rc += ReadTest_scatter( cs_hdl, "testTup", strs, len, 4 ); // read 2x to make sure it's not consumed

  rc += GetTest_v( cs_hdl, "vectorTup", sge, 4 );
  rc += GetTest_scatter( cs_hdl, "testTup", strs, len, 4 );
  TEST_LOG( rc, "First Get" );

  // delete the name space
  ret = dbrDelete( name );
  rc += TEST( DBR_SUCCESS, ret );

  TEST_LOG( rc, "Delete" );

  // try to attach to the name space to see if it got deleted
  cs_hdl = dbrAttach( name );
  rc += TEST( NULL, cs_hdl );

  TEST_LOG( rc, "Intentionally failing attach" );

  // try to access a deleted namespace
//  rc += TEST_NOT( PutTest_gather( cs_hdl, "testTup", "HelloWorld1", 11 ), 0 );
//  rc += TEST_NOT( ReadTest( cs_hdl, "testTup", "HelloWorld1", 11 ), 0 );
//  rc += TEST_NOT( GetTest( cs_hdl, "testTup", "HelloWorld1", 11 ), 0 );

  TEST_LOG( rc, " error case put/read/get tests" );

  free( name );

  printf( "Test exiting with rc=%d\n", rc );
  return rc;
}

