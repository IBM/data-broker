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
#include "libdatabroker.h"
#include "../namespace.h"
#include "../namespacelist.h"

#include <stddef.h>
#include <stdio.h>
#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>
#endif
#include <unistd.h>
#include <string.h>

#ifdef DBBE_DBG_TEST
int Flatten( dbBE_Redis_namespace_list_t *l )
{
  if( l == NULL )
    return 0;
  dbBE_Redis_namespace_list_t *s = l;
  do
  {
    printf( "%s\t", l->_ns->_name );
    l = l->_n;
  } while( l != s );
  printf( "\n" );
  return 0;
}
#else
int Flatten( dbBE_Redis_namespace_list_t *l ) {return 0;}
#endif

int namespacetest()
{
  int rc = 0;

  char *toolong = generateLongMsg( DBR_MAX_KEY_LEN + 1 );
  dbBE_Redis_namespace_t *ns = NULL;

  rc += TEST( dbBE_Redis_namespace_create( NULL ), NULL );
  rc += TEST( errno, EINVAL );
  rc += TEST( dbBE_Redis_namespace_create( toolong ), NULL );
  rc += TEST( errno, E2BIG );
  rc += TEST_NOT_RC( dbBE_Redis_namespace_create( "Test" ), NULL, ns );

  rc += TEST_INFO( dbBE_Redis_namespace_get_refcnt( ns ), 1, "Refcount check" );
  rc += TEST_INFO( dbBE_Redis_namespace_get_len( ns ), 4, "Length check" );
  rc += TEST_INFO( strncmp( dbBE_Redis_namespace_get_name( ns ), "Test", 5 ), 0, "Namespace correctness" );

  rc += TEST( dbBE_Redis_namespace_attach( ns ), 2 );
  rc += TEST( dbBE_Redis_namespace_get_refcnt( ns ), 2 );
  rc += TEST( dbBE_Redis_namespace_destroy( ns ), -EBUSY );
  rc += TEST( dbBE_Redis_namespace_detach( ns ), 1 );
  rc += TEST( dbBE_Redis_namespace_get_refcnt( ns ), 1 );

  // break the validity and test
  ++ns->_refcnt;
  rc += TEST( dbBE_Redis_namespace_get_refcnt( ns ), 2 );
  rc += TEST_INFO( dbBE_Redis_namespace_attach( ns ), -EBADF, "Attach with broken checksum/refcnt" );
  rc += TEST_INFO( dbBE_Redis_namespace_destroy( ns ), -EBADF, "Destroy with broken checksum/refcnt" );
  rc += TEST_INFO( dbBE_Redis_namespace_detach( ns ), -EBADF, "Detach with broken checksum/refcnt" );

#define DBBE_REDIS_NAMESPACE_REFCNT_MASK ( 0xFFFFFFFFFFFF0000ll )
#define dbBE_Redis_namespace_chksum_refup( ns ) ( (ns)->_chksum & DBBE_REDIS_NAMESPACE_REFCNT_MASK ) + ( (ns)->_refcnt)

  // restore validity
  --ns->_refcnt;
  rc += TEST( dbBE_Redis_namespace_get_refcnt( ns ), 1 );


  // set up namespace for too-many users
  ns->_refcnt = 0xFFFE;
  ns->_chksum = dbBE_Redis_namespace_chksum_refup( ns );
  rc += TEST( dbBE_Redis_namespace_attach( ns ), -EMLINK );
  rc += TEST( dbBE_Redis_namespace_validate( ns ), EMLINK );

  // back to normal
  ns->_refcnt = 1;
  ns->_chksum = dbBE_Redis_namespace_chksum_refup( ns );

  // detach too often -> autodestroys the namespace
  rc += TEST( dbBE_Redis_namespace_detach( ns ), 0 );
  rc += TEST( dbBE_Redis_namespace_attach( ns ), -EBADF );
  rc += TEST( dbBE_Redis_namespace_destroy( ns ), -EBADF );

  free( toolong );
  return rc;
}

#define DBBE_TEST_NAMESPACE_COUNT (1000)

int namespacelisttest()
{
  int rc = 0;

  dbBE_Redis_namespace_t *ns = NULL;
  dbBE_Redis_namespace_t *many[ DBBE_TEST_NAMESPACE_COUNT ];
  dbBE_Redis_namespace_list_t *list = NULL;

  rc += TEST( dbBE_Redis_namespace_list_clean( NULL ), 0 );
  rc += TEST( dbBE_Redis_namespace_list_insert( NULL, NULL ), NULL );
  rc += TEST( errno, EINVAL );
  rc += TEST( dbBE_Redis_namespace_list_remove( NULL, NULL ), NULL );
  rc += TEST( dbBE_Redis_namespace_list_get( NULL, NULL ), NULL );

  rc += TEST_NOT_RC( dbBE_Redis_namespace_create( "Test" ), NULL, ns );
  rc += TEST_NOT_RC( dbBE_Redis_namespace_list_insert( list, ns ), NULL, list );
  rc += TEST( dbBE_Redis_namespace_list_insert( list, ns ), NULL );

  TEST_BREAK( rc, "Found error already. Skipping further tests" );
  int i;
  int doubles = 0;
  dbBE_Redis_namespace_list_t *tmp = NULL;
  for( i=0; i<DBBE_TEST_NAMESPACE_COUNT; ++i )
  {
    int nlen = (random() % (17-1)) + 1;
    char *ns_name = generateLongMsg(nlen);
    rc += TEST_NOT_RC( dbBE_Redis_namespace_create( ns_name ), NULL, many[i] );
    free( ns_name );
    TEST_BREAK( rc, "Unable to create another namespace before insert. Cannot continue." );
    rc += TEST_NOT_RC( dbBE_Redis_namespace_list_insert( list, many[i] ), NULL, tmp );
    if( tmp == NULL ) // in case that entry already existed
    {
      ++doubles;
      --rc;
      rc += TEST_NOT_RC( dbBE_Redis_namespace_list_get( list, many[i]->_name ), NULL, tmp );
      rc += TEST_NOT( tmp , NULL );
      TEST_BREAK( rc, "Inconsistent namespace list entry." );
      rc += TEST_NOT( dbBE_Redis_namespace_attach( tmp->_ns ), 1 );
      rc += TEST( dbBE_Redis_namespace_destroy( many[i] ), 0 );
      --i;
      continue;
    }
    list = tmp;
    Flatten( list );
    rc += TEST_NOT_INFO( list->_n, list, many[i]->_name );
  }

  LOG( DBG_ALL, stdout, "Double creation/attach %d\n", doubles );
  TEST_BREAK( rc, "Found error already. Skipping further tests" );
  for( i=0; i<DBBE_TEST_NAMESPACE_COUNT; ++i )
  {
    if( many[ i ] != NULL )
    {
      rc += TEST_INFO( 0, 0, many[ i ]->_name );
      rc += TEST_NOT_RC( dbBE_Redis_namespace_list_remove( list, many[ i ] ), NULL, tmp );
      rc += TEST_NOT_INFO( tmp, NULL, "namespace list not empty?" );
      // tmp needs to be non-null since we have at least Test-namespace in there
      TEST_BREAK( rc, "unexpected drain of namspace list." );
      if(( tmp->_ns == many[i] ) && ( dbBE_Redis_namespace_validate( many[ i ] ) == 0 )) // still valid ns
      {
        rc += TEST_NOT( many[ i ]->_refcnt, 0 );
        TEST_BREAK( rc, "Inconsistent reference count" );
        --doubles;
        list = tmp;
        --i;
        continue;
      }
      list = tmp;
      rc += TEST_NOT( list->_ns->_name[0], 0 );
      Flatten( list );
//      rc += TEST( dbBE_Redis_namespace_destroy( many[ i ] ), 0 );
      many[ i ] = NULL;
    }
  }
  rc += TEST_INFO( doubles, 0, "Multi-attached counter == 0 ?" );

  // remove the last one should return NULL for the list ptr
  rc += TEST( dbBE_Redis_namespace_attach( ns ), 2 );
  // needs to return list ptr, because namespace refcnt is > 1
  rc += TEST( dbBE_Redis_namespace_list_remove( list, ns ), list );

  // now it's detached once more and should be cleaned up from the list
  rc += TEST( dbBE_Redis_namespace_list_remove( list, ns ), NULL );

  // no destruction should be happening as this already is done by list_remove()
  rc += TEST( dbBE_Redis_namespace_destroy( ns ), -EBADF );
  return rc;
}

int main( int argc, char ** argv )
{
  int rc = 0;

  rc += namespacetest();
  TEST_BREAK( rc, "Found error already. Skipping further tests" );
  rc += namespacelisttest();

  printf( "Test exiting with rc=%d\n", rc );
  return rc;
}
