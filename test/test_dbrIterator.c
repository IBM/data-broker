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

#include "test_utils.h"


#define DBR_TEST_KEY_COUNT ( 1234 )
#define DBR_TEST_VAL_LEN ( 128 )

int main( int argc, char **argv )
{
  int rc = 0;

  DBR_Handle_t hdl;
  rc += TEST_NOT_RC( dbrCreate("itertest", DBR_PERST_VOLATILE_SIMPLE, DBR_GROUP_LIST_EMPTY ), NULL, hdl );
  TEST_BREAK( rc, "Namespace create failed. Can't continue\n" );

  int n;
  char *keybuf = generateLongMsg( DBR_TEST_KEY_COUNT * 100 );
  char *data = generateLongMsg( DBR_TEST_KEY_COUNT + DBR_TEST_VAL_LEN );
  char *key = malloc( DBR_MAX_KEY_LEN );

  for( n=0; n<DBR_TEST_KEY_COUNT; ++n )
  {
    char tmp = keybuf[ n + 100 ];
    keybuf[ n + 100 ] = '\0';
    rc += TEST( dbrPut( hdl, &data[n], DBR_TEST_VAL_LEN, &keybuf[n], DBR_GROUP_EMPTY ), DBR_SUCCESS );
    keybuf[ n + 100 ] = tmp;
  }

  DBR_Iterator_t iterator = DBR_ITERATOR_NEW;
  char covered[ DBR_TEST_KEY_COUNT ];
  memset( covered, 0, DBR_TEST_KEY_COUNT );
  int it_count = 0;
  do
  {
    if( ++it_count < DBR_TEST_KEY_COUNT + 1 )
      rc += TEST_NOT_RC( dbrIterator( hdl, iterator, DBR_GROUP_EMPTY, "", key ), NULL, iterator );
    else
    {
      rc += TEST_RC( dbrIterator( hdl, iterator, DBR_GROUP_EMPTY, "", key ), DBR_ITERATOR_DONE, iterator );
      rc += TEST( key[0], (char)EOF );
      rc += TEST( key[1], '\0' );
      break;
    }

    char *keyref = keybuf;
    rc += TEST_NOT_RC( strstr( keybuf, key ), NULL, keyref );
    intptr_t offset = (intptr_t)keyref - (intptr_t)keybuf;
    rc += TEST_NOT( offset >= 0, 0 );
    rc += TEST_NOT( offset < DBR_TEST_KEY_COUNT, 0 );
    if( rc == 0 )
      covered[ offset ] = 1;
  } while(( iterator != DBR_ITERATOR_DONE ) && ( rc == 0 ));

  int cover_total = 0;
  for( n=0; n<DBR_TEST_KEY_COUNT; ++n )
    cover_total += (int)covered[n];
  rc += TEST( cover_total, DBR_TEST_KEY_COUNT );

  rc += TEST( dbrDelete( "itertest" ), DBR_SUCCESS );

  free( key );
  free( data );
  free( keybuf );

  printf( "Test exiting with rc=%d\n", rc );
  return rc;
}
