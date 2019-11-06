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

#include "namespace.h"
#include "namespacelist.h"



/***************************************************************************
 * NAMESPACE FUNCTIONS
 */

#define DBBE_REDIS_NAMESPACE_REFCNT_MASK ( 0xFFFFFFFFFFFF0000ll )
#define dbBE_Redis_namespace_chksum_refup( ns ) ( (ns)->_chksum & DBBE_REDIS_NAMESPACE_REFCNT_MASK ) + ( (ns)->_refcnt)

static inline
int64_t dbBE_Redis_namespace_checksum( const dbBE_Redis_namespace_t *ns )
{
  int64_t nchk = 0;
  uint32_t i;
  for( i=0; i<ns->_len; i+=4 )
    nchk ^= (
        ns->_name[i] +
        ns->_name[ (i+1) % ns->_len ] +
        ns->_name[ (i+2) % ns->_len ] +
        ns->_name[ (i+3) % ns->_len ] );

  return ( (((int64_t)ns->_len << 32) + nchk ) << 16 ) + (int64_t)ns->_refcnt;
}

int dbBE_Redis_namespace_validate( const dbBE_Redis_namespace_t *ns )
{
  if( ns == NULL )
    return EINVAL;

  if( ns->_chksum != dbBE_Redis_namespace_checksum( ns ) )
    return EBADF;

  if( ns->_refcnt == 0 )
    return EBADF;


  if( ns->_refcnt > 0xFFFE )
    return EMLINK;

  return 0;
}

dbBE_Redis_namespace_t* dbBE_Redis_namespace_create( const char *name )
{
  if( name == NULL )
  {
    errno = EINVAL;
    return NULL;
  }

  size_t len = strnlen( name, DBR_MAX_KEY_LEN + 2 ); // +2 because we wouldn't detect too-long names
  if( len > DBR_MAX_KEY_LEN ) // check for namespace too long
  {
    errno = E2BIG;
    return NULL;
  }

  dbBE_Redis_namespace_t *ns = (dbBE_Redis_namespace_t*)calloc( 1, sizeof( dbBE_Redis_namespace_t ) + len + 1 ); // +1 for trailling \0
  if( ns == NULL )
  {
    errno = ENOMEM;
    return NULL;
  }

  ns->_len = len;
  strncpy( ns->_name, name, len );
  // no explicit setting of terminating '\0' because calloc already has a trailing zero

  ns->_refcnt = 1;
  ns->_chksum = dbBE_Redis_namespace_checksum( ns );
  return ns;
}

int dbBE_Redis_namespace_destroy( dbBE_Redis_namespace_t *ns )
{
  if( ns == NULL )
    return -EINVAL;

  // prevent destruction of invalid namespace (user might have modified or use-after-free)
  if( dbBE_Redis_namespace_validate( ns ) != 0 )
    return -EBADF;

  if( dbBE_Redis_namespace_get_refcnt( ns ) > 1 )
    return -EBUSY;

  ns->_chksum = 0;
  memset( ns, 0, sizeof( dbBE_Redis_namespace_t ) + dbBE_Redis_namespace_get_len( ns ) );
  free( ns );
  return 0;
}

int dbBE_Redis_namespace_attach( dbBE_Redis_namespace_t *ns )
{
  int rc = dbBE_Redis_namespace_validate( ns );
  if( rc != 0 )
    return -rc;

  if( ++(ns->_refcnt) > 0xFFFE )
  {
    ns->_refcnt = 0xFFFF;
    ns->_chksum = dbBE_Redis_namespace_chksum_refup( ns );
    return -EMLINK;
  }
  ns->_chksum = dbBE_Redis_namespace_chksum_refup( ns );
  return ns->_refcnt;
}

int dbBE_Redis_namespace_detach( dbBE_Redis_namespace_t *ns )
{
  int rc = dbBE_Redis_namespace_validate( ns );
  if( rc != 0 )
    return -rc;

  if( ns->_refcnt <= 1 )
  {
    dbBE_Redis_namespace_destroy( ns );
    return 0;
  }
  else
  {
    --ns->_refcnt;
    ns->_chksum = dbBE_Redis_namespace_chksum_refup( ns );
  }

  return ns->_refcnt;
}










/***************************************************************************
 * NAMESPACE LIST FUNCTIONS
 */

// use last bit of ptr address to signal matching namespace was found
#define DBBE_REDIS_NS_PTR_MATCH( p ) ((dbBE_Redis_namespace_list_t*)((uintptr_t)(p) | 0x1ull))
#define DBBE_REDIS_NS_MATCHED( p )   (((uintptr_t)(p) & 0x1ull) != 0)
#define DBBE_REDIS_NS_PTR_FIX( p )   ((dbBE_Redis_namespace_list_t*)((uintptr_t)(p) & (~0x1ull)))

// search for namespace entry
// if found: return entry but with lowest bit set to 1 (i.e. cannot be dereferenced directly!!!)
// if not found: return list entry BEFORE insertion needs to happen
static
dbBE_Redis_namespace_list_t* dbBE_Redis_namespace_list_find( dbBE_Redis_namespace_list_t *s,
                                                             const char *name )
{
  dbBE_Redis_namespace_list_t *r = s;
  int dir = 0;
  int lcomp = 0;
  dir = strncmp( name, r->_ns->_name, 1 );
  do
  {
    int comp = strncmp( name, r->_ns->_name, DBR_MAX_KEY_LEN );
    if( comp == 0 )
      return DBBE_REDIS_NS_PTR_MATCH( r );

    if( comp < 0 )
    {
      if( dir > 0 )
      {
        r = r->_p; // one step back; we just detected that we went one step too far
        break;
      }
      lcomp = strncmp( r->_ns->_name, r->_p->_ns->_name, DBR_MAX_KEY_LEN );
      if( lcomp < 0 )
      {
        r = r->_p;
        break;
      }
      dir = -1;
      r = r->_p;
    }
    if( comp > 0 )
    {
      if( dir < 0 )
        break;
      lcomp = strncmp( r->_ns->_name, r->_n->_ns->_name, DBR_MAX_KEY_LEN );
      if( lcomp > 0 )
        break;
      dir = 1;
      r = r->_n;
    }
  } while( r != s );
  return r;
}

dbBE_Redis_namespace_list_t* dbBE_Redis_namespace_list_get( dbBE_Redis_namespace_list_t *s,
                                                            const char *name )
{
  if( (s==NULL) || ( name == NULL ))
  {
    errno = EINVAL;
    return NULL;
  }
  dbBE_Redis_namespace_list_t *p = dbBE_Redis_namespace_list_find( s, name );
  if( DBBE_REDIS_NS_MATCHED( p ) )
  {
    p = DBBE_REDIS_NS_PTR_FIX( p );
    return p;
  }
  errno = ENOENT;
  return NULL;
}

dbBE_Redis_namespace_list_t* dbBE_Redis_namespace_list_insert( dbBE_Redis_namespace_list_t *s,
                                                               dbBE_Redis_namespace_t * const ns )
{
  if( ns == NULL )
  {
    errno = EINVAL;
    return NULL;
  }
  dbBE_Redis_namespace_list_t *r = s;
  if( s == NULL )
  {
    r = (dbBE_Redis_namespace_list_t*)calloc( 1, sizeof( dbBE_Redis_namespace_list_t ) );
    if( r == NULL )
    {
      errno = ENOMEM;
      return NULL;
    }
  }

  if(( r->_ns == NULL ) && ( r->_n == NULL ) && ( r->_p == NULL ))
  {
    r->_n = r;
    r->_p = r;
    r->_ns = ns;
    return r;
  }

  dbBE_Redis_namespace_list_t *t = dbBE_Redis_namespace_list_find( r, ns->_name );
  if( DBBE_REDIS_NS_MATCHED( t ) ) // if exact entry is found, the lowest bit is 1
  {
    // don't call attach, because insert of existing NS might be an error case
    errno = EEXIST;
    return NULL;
  }

  r = (dbBE_Redis_namespace_list_t*)calloc( 1, sizeof( dbBE_Redis_namespace_list_t ) );
  r->_n = t->_n;
  r->_p = t;
  t->_n = r;
  r->_n->_p = r;
  r->_ns = ns;
  t = r;
  return t;
}

dbBE_Redis_namespace_list_t* dbBE_Redis_namespace_list_remove( dbBE_Redis_namespace_list_t *s,
                                                               const dbBE_Redis_namespace_t *ns )
{
  if(( s == NULL ) || ( ns == NULL ))
  {
    errno = EINVAL;
    return NULL;
  }

  dbBE_Redis_namespace_list_t *t = s;
  dbBE_Redis_namespace_list_t *r = dbBE_Redis_namespace_list_find( s, ns->_name );
  if( DBBE_REDIS_NS_MATCHED( r ) )
  {
    r = DBBE_REDIS_NS_PTR_FIX( r );
    if( dbBE_Redis_namespace_detach( r->_ns ) > 0 )
    {
      errno = EBUSY;
      return r;
    }
    t = r->_n;
    if( t == r ) // last element in the ring?
      t = NULL;
    r->_p->_n = r->_n;
    r->_n->_p = r->_p;
    r->_p = NULL;
    r->_n = NULL;
    r->_ns = NULL;
    free( r );
  }
  return t;
}

int dbBE_Redis_namespace_list_clean( dbBE_Redis_namespace_list_t *s )
{
  if( (s != NULL ) && ( s->_p != NULL )) // cut the ring
    s->_p->_n = NULL;
  while( s != NULL)
  {
    dbBE_Redis_namespace_list_t *t = s->_n;
    s->_n = NULL;
    s->_p = NULL;
    while( dbBE_Redis_namespace_destroy( s->_ns ) == -EBUSY )
      dbBE_Redis_namespace_detach( s->_ns );
    s->_ns = NULL;
    free( s );
    s = t;
  }
  return 0;
}
