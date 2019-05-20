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

  rc += ReadTest( cs_hdl, "AlongishKeyWithMorechars_andsome-Other;characters:inside.", "01234567890123456789", 20 );
  rc += ReadTest( cs_hdl, "testTup", "HelloWorld2", 11 );
  rc += ReadTest( cs_hdl, "testTup", "HelloWorld2", 11 );

  rc += GetTest( cs_hdl, "testTup", "HelloWorld2", 11 );
  rc += GetTest( cs_hdl, "testTup", "HelloWorld3", 11 );
  rc += GetTest( cs_hdl, "testTup", "HelloWorld4", 11 );
  rc += GetTest( cs_hdl, "AlongishKeyWithMorechars_andsome-Other;characters:inside.", "01234567890123456789", 20 );
  rc += GetTest( cs_hdl, "AlongishKeyWithMorechars_andsome-Other;characters:inside.", "012345678901234567890", 21 );
  rc += GetTest( cs_hdl, "AlongishKeyWithMorechars_andsome-Other;characters:inside.WithEnter", "0123456\r\n78901234567890", 23 );

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

  unsigned n, c;
  char *keystr = (char*)malloc( DBR_MAX_KEY_LEN + 16);
  rc += TEST_NOT( keystr, NULL );
  TEST_BREAK( rc, "Allocate keystring" );
  memset( keystr, 0, DBR_MAX_KEY_LEN + 16 );
  for( n=1; n<DBR_MAX_KEY_LEN; ++n )
  {
    for( c=0; c<n; ++c ) keystr[c] = random() % 26 + 97;
    rc += PutTest( cs_hdl, keystr, "HelloWorld1", 11 );
    rc += ReadTest( cs_hdl, keystr, "HelloWorld1", 11 );
    rc += GetTest( cs_hdl, keystr, "HelloWorld1", 11 );
  }
  free( keystr );

  // long msg test:
  int64_t longLen = 1000000-1;
  char *longIn = generateLongMsg( longLen );
  rc += TEST( DBR_SUCCESS, dbrPut( cs_hdl, longIn, longLen, "testTup", 0 ));
  int64_t longRet = longLen + 1;
  char *longOut = (char*)calloc( 1, longRet );
  rc += TEST( DBR_SUCCESS, dbrRead( cs_hdl, longOut, &longRet, "testTup", "", 0, DBR_FLAGS_NONE ));
  rc += TEST( longRet, longLen );
  rc += TEST( strncmp( longOut, longIn, (longRet<longLen ? longRet : longLen) ), 0 );

  longRet = longLen + 1;
  memset( longOut, 0, longRet );
  rc += TEST( DBR_SUCCESS, dbrGet( cs_hdl, longOut, &longRet, "testTup", "", 0, DBR_FLAGS_NONE ));
  rc += TEST( longRet, longLen );
  rc += TEST( strncmp( longOut, longIn, (longRet<longLen ? longRet : longLen) ), 0 );


  // create a timeout get
  rc += TEST_RC( dbrGet( cs_hdl, longOut, &longRet, "testTup", "", 0, DBR_FLAGS_NONE ), DBR_ERR_TIMEOUT, ret );

  rc += TEST_RC( dbrRead( cs_hdl, longOut, &longRet, "testTup", "", 0, DBR_FLAGS_NOWAIT ), DBR_ERR_UNAVAIL, ret );
  rc += TEST_RC( dbrGet( cs_hdl, longOut, &longRet, "testTup", "", 0, DBR_FLAGS_NOWAIT ), DBR_ERR_UNAVAIL, ret );
  rc += KeyTest( cs_hdl, "testTup", DBR_ERR_UNAVAIL );


  // testing of remove cmd
  rc += PutTest( cs_hdl, "testTup", "HelloWorld1", 11 );
  rc += TEST_RC( dbrRemove( cs_hdl, DBR_GROUP_EMPTY, "testTup", "" ), DBR_SUCCESS, ret );
  rc += TEST_RC( dbrGet( cs_hdl, longOut, &longRet, "testTup", "", 0, DBR_FLAGS_NOWAIT ), DBR_ERR_UNAVAIL, ret );

  // testing short-buffer read
  rc += PutTest( cs_hdl, "shortTest", longIn, longLen );
  longRet = 100;
  rc += TEST_RC( dbrRead( cs_hdl, longOut, &longRet, "shortTest", "", 0, DBR_FLAGS_PARTIAL ), DBR_ERR_UBUFFER, ret );
  rc += TEST( longRet, longLen );
  rc += GetTest( cs_hdl, "shortTest", longOut, longLen );

  free( longIn );
  free( longOut );

  TEST_LOG( rc, "Long Put/Get" );

  // binary data test
  size_t tlen = 122;
  char *tuplestr = (char*)malloc( tlen );

  for( n=0; n<tlen; ++n )
    tuplestr[ n ] = random() % 256;
  tuplestr[ tlen >> 1 ] = 0;
  rc += PutTest( cs_hdl, "testTup", tuplestr, tlen );
  rc += ReadTest( cs_hdl, "testTup", tuplestr, tlen );
  rc += GetTest( cs_hdl, "testTup", tuplestr, tlen );

  free( tuplestr );

  // delete the name space
  ret = dbrDelete( name );
  rc += TEST( DBR_SUCCESS, ret );

  TEST_LOG( rc, "Delete" );

  // try to attach to the name space to see if it got deleted
  cs_hdl = dbrAttach( name );
  rc += TEST( NULL, cs_hdl );

  TEST_LOG( rc, "Intentionally failing attach" );

  // try to access a deleted namespace
  rc += TEST_NOT( PutTest( cs_hdl, "testTup", "HelloWorld1", 11 ), 0 );
  rc += TEST_NOT( ReadTest( cs_hdl, "testTup", "HelloWorld1", 11 ), 0 );
  rc += TEST_NOT( GetTest( cs_hdl, "testTup", "HelloWorld1", 11 ), 0 );

  TEST_LOG( rc, " error case put/read/get tests" );

  free( name );

  printf( "Test exiting with rc=%d\n", rc );
  return rc;
}

