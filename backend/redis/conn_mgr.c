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

#include "logutil.h"
#include "definitions.h"
#include "utility.h"
#include "conn_mgr.h"
#include "parse.h"
#include "libdatabroker.h"

#include <errno.h>
#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>  // malloc
#endif
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h> // usleep
#include <sys/types.h> // getifaddr
#include <ifaddrs.h> // getifaddr

#define DBBE_REDIS_CONN_MGR_TRACKED_EVENTS ( EPOLLET | EPOLLIN | EPOLLERR | EPOLLRDHUP | EPOLLPRI )

#define DBBE_REDIS_INFO_PER_SERVER ( 4096 )


/*
 * initialize the connection mgr
 */
dbBE_Redis_connection_mgr_t* dbBE_Redis_connection_mgr_init()
{
  dbBE_Redis_connection_mgr_t *conn_mgr = (dbBE_Redis_connection_mgr_t*)malloc( sizeof(dbBE_Redis_connection_mgr_t) );
  if( conn_mgr == NULL )
  {
    errno = ENOMEM;
    return NULL;
  }

  memset( conn_mgr, 0, sizeof( dbBE_Redis_connection_mgr_t) );

  conn_mgr->_ev_mgr = dbBE_Redis_event_mgr_init( 1 );
  if( conn_mgr->_ev_mgr == NULL )
  {
    LOG( DBG_ERR, stderr, "connection_mgr_init: Failed to initialize event mgr: %d\n", errno );
    free( conn_mgr );
    return NULL;
  }


  return conn_mgr;
}

/*
 * cleanup the connection_mgr
 */
void dbBE_Redis_connection_mgr_exit( dbBE_Redis_connection_mgr_t *conn_mgr )
{
  if( conn_mgr == NULL )
  {
    errno = EINVAL;
    return;
  }

  // disconnect and destroy all remaining connections
  unsigned n;
  for( n = 0; n < DBBE_REDIS_MAX_CONNECTIONS; ++n )
  {
    if(( conn_mgr->_connections[ n ] == NULL ) && ( conn_mgr->_broken[ n ] == NULL ))
      continue;
    dbBE_Redis_connection_t *c = conn_mgr->_connections[ n ];
    if( c == NULL )
      c = conn_mgr->_broken[ n ];
    dbBE_Redis_event_mgr_rm( conn_mgr->_ev_mgr, c );
    dbBE_Redis_connection_destroy( c );
  }

  dbBE_Redis_event_mgr_exit( conn_mgr->_ev_mgr );

  memset( conn_mgr, 0, sizeof( dbBE_Redis_connection_mgr_t) );
  free( conn_mgr );
}

/*
 * Add a new connection to the mgr
 */
int dbBE_Redis_connection_mgr_add( dbBE_Redis_connection_mgr_t *conn_mgr,
                                   dbBE_Redis_connection_t *conn )
{
  if(( conn_mgr == NULL ) || ( ! dbBE_Redis_connection_RTS( conn )) )
  {
    if( conn != NULL )
      LOG( DBG_ERR, stderr, "connection_mgr_add: Invalid arguments. conn_mgr=%p; conn=%p; socket=%d\n", conn_mgr, conn, conn->_socket )
    else
      LOG( DBG_ERR, stderr, "connection_mgr_add: Invalid arguments. conn_mgr=%p; conn=%p\n", conn_mgr, conn );
    return -EINVAL;
  }

  if( (unsigned)conn_mgr->_connection_count >= DBBE_REDIS_MAX_CONNECTIONS )
  {
    LOG( DBG_ERR, stderr, "connection_mgr_add: max number of connections exceeded. Can't add new connection.\n" );
    return -ENOMEM;
  }

  if( dbBE_Redis_connection_RTR( conn ) == 0 )
  {
    LOG( DBG_ERR, stderr, "connection_mgr_add: Can only add connections that are in RTR state.\n" );
    return -ENOTCONN;
  }

  // if it has an index already
  if( conn->_index != DBBE_REDIS_LOCATOR_INDEX_INVAL )
  {
    if( conn_mgr->_connections[ conn->_index ] != NULL )
      return 0;
    else
    {
      LOG( DBG_ERR, stderr, "BUG: Connection to %s has index %d but is not tracked in conn_mgr. Solving by finding new place for it\n", conn->_url, conn->_index );
    }
  }

  unsigned i = 0;
  for( i = 0; (i < DBBE_REDIS_MAX_CONNECTIONS) && ((conn_mgr->_connections[ i ] != NULL) || ( conn_mgr->_broken[ i ] != NULL )); ++i ) {}
  if( i >= DBBE_REDIS_MAX_CONNECTIONS )
  {
    LOG( DBG_ERR, stderr, "connection_mgr_add: connection slots exhausted. Can't add new connection.\n" );
    return -ENOMEM;
  }

  conn_mgr->_connections[ i ] = conn;
  conn_mgr->_broken[ i ] = NULL;
  ++conn_mgr->_connection_count;
  conn->_index = i;

  int rc = dbBE_Redis_event_mgr_add( conn_mgr->_ev_mgr, conn );
  if( rc != 0 )
  {
    LOG( DBG_ERR, stderr, "connection_mgr_add: failed to add conn=%p (socket=%d) to event_mgr. rc=%d\n", conn, conn->_socket, rc );
    return rc;
  }

  return 0;
}

dbBE_Redis_connection_t* dbBE_Redis_connection_mgr_newlink( dbBE_Redis_connection_mgr_t *conn_mgr,
                                                            const char *url )
{
  int rc = 0;
  if( conn_mgr == NULL )
  {
    errno = EINVAL;
    return NULL;
  }

  char *authfile = dbBE_Redis_extract_env( DBR_SERVER_AUTHFILE_ENV, DBR_SERVER_DEFAULT_AUTHFILE );
  LOG( DBG_VERBOSE, stderr, "authfile=%s\n", authfile );

  dbBE_Redis_connection_t *new_conn = dbBE_Redis_connection_create( DBBE_REDIS_SR_BUFFER_LEN );
  if( new_conn == NULL )
  {
    rc = -ENOMEM;
    goto exit_connect;
  }

  dbBE_Redis_address_t *srv_addr = dbBE_Redis_connection_link( new_conn, url, authfile );
  if( srv_addr == NULL )
  {
    rc = -ENOTCONN;
    dbBE_Redis_connection_destroy( new_conn );
    goto exit_connect;
  }

  rc = dbBE_Redis_connection_mgr_add( conn_mgr, new_conn );
  if( rc != 0 )
  {
    dbBE_Redis_connection_unlink( new_conn );
    dbBE_Redis_connection_destroy( new_conn );
    goto exit_connect;
  }

  free( authfile );
  return new_conn;

exit_connect:
  free( authfile );
  errno = rc;
  return NULL;
}


/*
 * Move a connection from regular to broken list
 */
int dbBE_Redis_connection_mgr_conn_fail( dbBE_Redis_connection_mgr_t *conn_mgr,
                                         dbBE_Redis_connection_t *conn )
{
  if(( conn_mgr == NULL ) || ( conn == NULL ))
  {
    LOG( DBG_ERR, stderr, "connection_mgr_conn_fail: invalid argument: conn_mgr=%p; conn%p\n", conn_mgr, conn );
    return -EINVAL;
  }

  if( conn_mgr->_connection_count <= 0 )
  {
    LOG( DBG_ERR, stderr, "connection_mgr_conn_fail: no active connections, can't fail more\n" );
    return -ENOENT;
  }

  dbBE_Redis_event_mgr_rm( conn_mgr->_ev_mgr, conn ); // remove the connection from further recv processing

  conn_mgr->_broken[ conn->_index ] = conn;
  conn_mgr->_connections[ conn->_index ] = NULL;

  dbBE_Redis_connection_unlink( conn ); // and shut down/disconnect without deleting the addr info

  return 0;
}

/*
 * condititon for iterating over all existing connections without running through the whole (potentially mostly empty) array
 */
#define DBBE_CONNECTIONS_TO_GO( cm, n, i ) (( (i) < (cm)->_connection_count ) && ( (unsigned)(n) < DBBE_REDIS_MAX_CONNECTIONS ))

/*
 * empty connection slot check
 */
#define DBBE_CONNECTION_MGR_SLOT_EMPTY( cm, n ) (( (cm)->_connections[ (n) ] == NULL ) && ( (cm)->_broken[ (n) ] == NULL ))

static int dbBE_Redis_connection_mgr_restore_master_conn( dbBE_Redis_connection_mgr_t *conn_mgr,
                                                          dbBE_Redis_locator_t *locator,
                                                          dbBE_Redis_connection_t *conn,
                                                          dbBE_Redis_locator_index_t idx )
{
  int slot;
  conn_mgr->_connections[ idx ] = conn;
  conn_mgr->_broken[ idx ] = NULL;
  dbBE_Redis_event_mgr_add( conn_mgr->_ev_mgr, conn );
  dbBE_Redis_slot_bitmap_t *bitmap = dbBE_Redis_connection_get_slot_range( conn );
  for( slot=0;
      ( bitmap != NULL ) && (slot < DBBE_REDIS_HASH_SLOT_MAX);
      ++slot )
  {
    if( dbBE_Redis_slot_bitmap_get( bitmap, slot ) != 0 )
      dbBE_Redis_locator_assign_conn_index( locator, idx, slot );
  }
  LOG( DBG_VERBOSE, stderr, "Recovered as master connection idx: %d\n", conn->_index );
  return 0;
}


/*
 * replace the input connection (replica) with its master connection after updating the cluster info
 */
static int dbBE_Redis_connection_mgr_fail_over_new_master( dbBE_Redis_connection_mgr_t *conn_mgr,
                                                           dbBE_Redis_locator_t *locator,
                                                           dbBE_Redis_connection_t *conn,
                                                           dbBE_Redis_cluster_info_t **cluster )
{
  dbBE_Redis_cluster_info_t *tmp_cluster = dbBE_Redis_connection_mgr_get_cluster_info( conn_mgr );
  if( tmp_cluster == NULL )
    return DBBE_REDIS_CONNECTION_UNRECOVERABLE;

  // update global cluster info since we got a new one here
  dbBE_Redis_cluster_info_destroy( *cluster );
  *cluster = tmp_cluster;

  char *url = dbBE_Redis_connection_get_url( conn );
  dbBE_Redis_server_info_t *si = dbBE_Redis_cluster_info_get_server_by_addr( tmp_cluster, url );
  if( si == NULL )
  {
    LOG( DBG_ERR, stderr, "Unable to find updated master info for %s\n", url );
    return DBBE_REDIS_CONNECTION_UNRECOVERABLE;
  }

  // clean up the old connection
  dbBE_Redis_connection_mgr_rm( conn_mgr, conn );
  dbBE_Redis_connection_destroy( conn );

  conn = dbBE_Redis_connection_mgr_newlink( conn_mgr, dbBE_Redis_server_info_get_master( si ) );
  if( conn == NULL )
    return DBBE_REDIS_CONNECTION_UNRECOVERABLE;

  dbBE_Redis_hash_slot_t first_slot = dbBE_Redis_server_info_get_first_slot( si );
  dbBE_Redis_hash_slot_t last_slot = dbBE_Redis_server_info_get_last_slot( si );

  dbBE_Redis_connection_assign_slot_range( conn,
                                           first_slot,
                                           last_slot );

  // update locator
  dbBE_Redis_locator_associate_range_conn_index( locator, first_slot, last_slot, conn->_index );
  LOG( DBG_VERBOSE, stderr, "Recovered connection by switching to %s\n", dbBE_Redis_server_info_get_master( si ) );

  return DBBE_REDIS_CONNECTION_RECOVERED;
}


static dbBE_Redis_connection_recoverable_t
dbBE_Redis_connection_mgr_connect_check_replica( dbBE_Redis_connection_mgr_t *conn_mgr,
                                                 dbBE_Redis_locator_t *locator,
                                                 dbBE_Redis_server_info_t *server,
                                                 dbBE_Redis_connection_t *conn,
                                                 const char *url,
                                                 dbBE_Redis_locator_index_t first_slot,
                                                 dbBE_Redis_locator_index_t last_slot )
{
  LOG( DBG_VERBOSE, stderr, "Got replica: %s\n", url );

  dbBE_Redis_connection_t *repl_conn = dbBE_Redis_connection_mgr_newlink( conn_mgr, url );
  if( repl_conn == NULL )
  {
    LOG( DBG_INFO, stderr, "Replica: %s not yet ready to connect\n", url );
    return DBBE_REDIS_CONNECTION_RECOVERABLE;
  }

  LOG( DBG_VERBOSE, stderr, "Replica: %s connected\n", url );

  // assign the connection slot range
  dbBE_Redis_connection_assign_slot_range( repl_conn,
                                           first_slot,
                                           last_slot );

  dbBE_Redis_connection_mgr_rm( conn_mgr, conn ); // remove the old connection from recv processing
  dbBE_Redis_connection_destroy( conn ); // delete the old connection

  // if we connected to a replica that's still in replica mode, we end up with MOVED responses all over the place
  // so tear it down and come back later...
  switch( dbBE_Redis_connection_mgr_is_master( conn_mgr, repl_conn ) )
  {
    case 0:
      LOG( DBG_INFO, stderr, "Cluster fail-over hasn't happen yet. Retrying later.\n");
      return DBBE_REDIS_CONNECTION_RECOVERABLE;
    case 1:
      LOG( DBG_VERBOSE, stderr, "Replica: %s is voted master by Redis now\n", url );

      // update locator
      dbBE_Redis_locator_associate_range_conn_index( locator,
                                                     first_slot,
                                                     last_slot,
                                                     repl_conn->_index );

      break;
    default:
      return DBBE_REDIS_CONNECTION_ERROR;
  }

  return DBBE_REDIS_CONNECTION_RECOVERED;
}

static int
dbBE_Redis_connection_mgr_create_connection_list( dbBE_Redis_connection_mgr_t *conn_mgr,
                                                  dbBE_Redis_connection_t ***connections )
{
  // create a temporary list of connections where we remove valid master connections to see which links need to get fixed
  *connections = (dbBE_Redis_connection_t**)calloc( conn_mgr->_connection_count, sizeof( dbBE_Redis_connection_t*) );
  if( connections == NULL )
    return -1;

  int n,i;
  for( n=0, i=0; DBBE_CONNECTIONS_TO_GO( conn_mgr, n, i ); ++n )
  {
    // skip empty slots
    if( DBBE_CONNECTION_MGR_SLOT_EMPTY( conn_mgr, n ) )
      continue;
    // double check that it's not a broken connection
    if(( conn_mgr->_broken[ n ] != NULL ) || ( conn_mgr->_connections[ n ] == NULL ))
    {
      LOG( DBG_ERR, stderr, "Inconsistent connection mgmt. Broken connection found in unbroken state\n" );
      free( connections );
      return -1;
    }
    (*connections)[ i++ ] = conn_mgr->_connections[ n ];
  }

  if( i != conn_mgr->_connection_count )
  {
    LOG( DBG_ERR,
         stderr,
         "Inconsistent connection mgmt. Number of tracked connections %d differs from active connections %d.\n",
         conn_mgr->_connection_count,
         i );
    free( connections );
    return -1;
  }

  return i;
}

static int
dbBE_Redis_connection_mgr_compare_cluster_info( dbBE_Redis_cluster_info_t *new_cluster,
                                                dbBE_Redis_cluster_info_t *old_cluster,
                                                dbBE_Redis_connection_t **connections,
                                                int connection_count )
{
  int c, n;
  int master_count = 0;
  for( c=0; c<dbBE_Redis_cluster_info_getsize( new_cluster ); ++c )
  {
    dbBE_Redis_server_info_t *si_new = dbBE_Redis_cluster_info_get_server( new_cluster, c );
    if( si_new == NULL )
      return -1;

    char *url = dbBE_Redis_server_info_get_master( si_new );
    if( url == NULL )
      return -1;

    dbBE_Redis_server_info_t *si_old = dbBE_Redis_cluster_info_get_server_by_addr( old_cluster, url );
    if( si_old == NULL )
      return -1;

    // remove any master connection from the tmp_conns list so that only replicas are left
    for( n=0; n<connection_count; ++n )
    {
      dbBE_Redis_connection_t *conn = connections[ n ];
      if( conn == NULL ) continue; // skip any entries that we might have removed earlier
      char *c_url = dbBE_Redis_connection_get_url( conn );
      if( strncmp( c_url, url, DBR_SERVER_URL_MAX_LENGTH ) == 0 )
      {
        connections[ n ] = NULL;
        ++master_count;
        break; // we found a connection to the si_new master; check the next one now...
      }
    }
  }
  return master_count;
}

static int
dbBE_Redis_connection_mgr_wipe_replica_connections( dbBE_Redis_connection_mgr_t *conn_mgr )
{
  int n, i;
  for( n=0, i=0; DBBE_CONNECTIONS_TO_GO( conn_mgr, n, i ); ++n )
  {
    if( DBBE_CONNECTION_MGR_SLOT_EMPTY( conn_mgr, n ))
      continue;
    ++i;
    if( conn_mgr->_broken[ n ] != NULL )
    {
      dbBE_Redis_connection_t *conn = conn_mgr->_broken[ n ];
      dbBE_Redis_connection_mgr_rm( conn_mgr, conn );
      dbBE_Redis_connection_destroy( conn );
    }
    dbBE_Redis_connection_t *conn = conn_mgr->_connections[ n ];
    if( dbBE_Redis_connection_mgr_is_master( conn_mgr, conn ) == 0 )
    {
      dbBE_Redis_connection_mgr_rm( conn_mgr, conn );
      dbBE_Redis_connection_destroy( conn );
    }
  }
  return 0;
}

static dbBE_Redis_connection_recoverable_t
dbBE_Redis_connection_mgr_update_all_hash_ranges( dbBE_Redis_connection_mgr_t *conn_mgr,
                                                  dbBE_Redis_locator_t *locator,
                                                  dbBE_Redis_cluster_info_t *cluster )
{
  int n, i;
  for( n=0, i=0; DBBE_CONNECTIONS_TO_GO( conn_mgr, n, i ); ++n )
  {
    if( DBBE_CONNECTION_MGR_SLOT_EMPTY( conn_mgr, n ))
      continue;
    ++i;
    dbBE_Redis_connection_t *conn = conn_mgr->_connections[ n ];
    char *url = dbBE_Redis_connection_get_url( conn );
    dbBE_Redis_server_info_t *si = dbBE_Redis_cluster_info_get_server_by_addr( cluster, url );
    if( si == NULL )
      continue;

    int first_slot = dbBE_Redis_server_info_get_first_slot( si );
    int last_slot = dbBE_Redis_server_info_get_last_slot( si );

    // update locator
    if( dbBE_Redis_locator_associate_range_conn_index( locator,
                                                       first_slot,
                                                       last_slot,
                                                       conn->_index ) )
      return DBBE_REDIS_CONNECTION_ERROR;
  }
  return DBBE_REDIS_CONNECTION_RECOVERED;
}

static dbBE_Redis_connection_recoverable_t
dbBE_Redis_connection_mgr_replica_to_master( dbBE_Redis_connection_mgr_t *conn_mgr,
                                             dbBE_Redis_locator_t *locator,
                                             dbBE_Redis_cluster_info_t *cluster,
                                             dbBE_Redis_connection_t **connections,
                                             int connection_count )
{
  int n;
  for( n=0; n<connection_count; ++n )
  {
    dbBE_Redis_connection_t *sconn = connections[ n ];
    if( sconn == NULL )
      continue;

    char *c_url = dbBE_Redis_connection_get_url( sconn );
    dbBE_Redis_server_info_t *si = dbBE_Redis_cluster_info_get_server_by_addr( cluster, c_url );
    if( si == NULL )
    {
      LOG( DBG_ERR, stderr, "Cluster/Server info unavailable for:%s\n", c_url );
      return DBBE_REDIS_CONNECTION_UNRECOVERABLE;
    }

    // check connection to master:
    c_url = dbBE_Redis_server_info_get_master( si );
    dbBE_Redis_connection_t *mconn = dbBE_Redis_connection_mgr_newlink( conn_mgr, c_url );
    if( mconn == NULL )
    {
      LOG( DBG_ERR, stderr, "Master %s still unavailable without failover. Retry later\n", c_url );
      return DBBE_REDIS_CONNECTION_RECOVERABLE;
    }

    dbBE_Redis_connection_mgr_rm( conn_mgr, sconn );
    connections[ n ] = NULL;
    int first_slot = dbBE_Redis_server_info_get_first_slot( si );
    int last_slot = dbBE_Redis_server_info_get_last_slot( si );

    // assign the connection slot range
    dbBE_Redis_connection_assign_slot_range( mconn,
                                             first_slot,
                                             last_slot );
    // update locator
    dbBE_Redis_locator_associate_range_conn_index( locator,
                                                   first_slot,
                                                   last_slot,
                                                   mconn->_index );
  }
  return DBBE_REDIS_CONNECTION_RECOVERED;
}

/*
 * return recoverable status
 * -1 -> impossible to recover
 * 0 -> successfully recovered
 * 1 -> recovery still possible
 */
dbBE_Redis_connection_recoverable_t dbBE_Redis_connection_mgr_conn_recover(
    dbBE_Redis_connection_mgr_t *conn_mgr,
    dbBE_Redis_locator_t *locator,
    dbBE_Redis_cluster_info_t **cluster )
{
  if(( conn_mgr == NULL ) || ( locator == NULL ) || ( cluster == NULL ) || ( *cluster == NULL ))
    return DBBE_REDIS_CONNECTION_ERROR;

  // two recovery types:
  // 1) broken connection: recover it
  // 2) incomplete hash coverage caused by recovery to replica


  // 1) check for broken connections
  unsigned c;
  int orig_conn_index = -1;
  dbBE_Redis_connection_recoverable_t recoverable = DBBE_REDIS_CONNECTION_RECOVERABLE;  // assume recoverable but not yet recovered
  int recovered = 0;
  int unbroken = 0; // count the unbroken/empty
  for( c=0; (c < DBBE_REDIS_MAX_CONNECTIONS) && (unbroken+recovered < conn_mgr->_connection_count); ++c )
  {
    if( DBBE_CONNECTION_MGR_SLOT_EMPTY( conn_mgr, c ) )
      continue;

    dbBE_Redis_connection_t *broke = conn_mgr->_broken[ c ];
    if( broke != NULL )
    {
      orig_conn_index = broke->_index;
      broke->_status = DBBE_CONNECTION_STATUS_DISCONNECTED;
      if( dbBE_Redis_connection_reconnect( broke ) == 0 ) // try reconnect and if successful:
      {
        LOG( DBG_INFO, stderr, "Recovered connection idx: %d\n", broke->_index );
        switch( dbBE_Redis_connection_mgr_is_master( conn_mgr, broke ) )
        {
          case 1: // recovered connection is a master, we're back to normal
          {
            dbBE_Redis_connection_mgr_restore_master_conn( conn_mgr, locator, broke, c );
            ++recovered;
            break;
          }
          case 0: // recovered connection is now a replica, need to update clusterinfo and connect to master
          {
            LOG( DBG_VERBOSE, stderr, "Connection switching required because it's a replica\n" );
            recoverable = dbBE_Redis_connection_mgr_fail_over_new_master( conn_mgr, locator, broke, cluster);
            if( recoverable == DBBE_REDIS_CONNECTION_RECOVERED )
              ++recovered;
            else
              return recoverable;
            break;
          }
          default:
            return DBBE_REDIS_CONNECTION_UNRECOVERABLE;
        } // switch (is_master)

      }
      else // if reconnect failed
      {
        // allow failed attempts to reconnect to original connection, until it's declared unrecoverable
        if( dbBE_Redis_connection_recoverable( broke ) != DBBE_REDIS_CONNECTION_UNRECOVERABLE )
        {
          usleep(250000);
          break;
        }

        char *url = dbBE_Redis_connection_get_url( broke );
        LOG( DBG_INFO, stderr, "Master connection to %s unrecoverable. Switching to replica\n", url );
        dbBE_Redis_server_info_t *server = dbBE_Redis_cluster_info_get_server_by_addr( *cluster,
                                                                                       url );
        if( server != NULL )
        {
          int first_slot = dbBE_Redis_server_info_get_first_slot( server );
          int last_slot = dbBE_Redis_server_info_get_last_slot( server );
          int n;

          if(( server->_server_count == 1 ) && ( dbBE_Redis_connection_recoverable( broke ) == DBBE_REDIS_CONNECTION_UNRECOVERABLE ))
          {
            return DBBE_REDIS_CONNECTION_UNRECOVERABLE;  // return as unable to recover
          }

          for( n = 0; ( n < server->_server_count ) && ( recovered == 0 ); ++n )
          {
            // don't retry the master, that failed above already. (assuming the bookkeeping is correct)
            char *addr = dbBE_Redis_server_info_get_replica( server, n );
            if( addr == dbBE_Redis_server_info_get_master( server ) )
              continue;

            recoverable = dbBE_Redis_connection_mgr_connect_check_replica( conn_mgr, locator, server,
                                                                           broke, addr, first_slot, last_slot );
            switch( recoverable )
            {
              case DBBE_REDIS_CONNECTION_RECOVERED:
                dbBE_Redis_server_info_update_master( server, n ); // make the connection to this replica the new master
                ++recovered;
                break;
              case DBBE_REDIS_CONNECTION_RECOVERABLE:
                usleep( 250000 );
                break;
              case DBBE_REDIS_CONNECTION_UNRECOVERABLE:
              default:
                return recoverable;
            }
          }
        } // if( server != NULL )
      } // if( dbBE_Redis_connection_reconnect( rec ) == 0 )

      if( recovered == 0 )
        LOG( DBG_VERBOSE, stderr, "conn_mgr_conn_recover: failed to reconnect or recover index %d. Also failed to find/connect to replicas\n", orig_conn_index );
    } // if( rec != NULL )
    else
      ++unbroken;
  } //  for( c=0; ... )

  if( unbroken == conn_mgr->_connection_count ) // need recovery and have no broken connections? Lets check clusterinfo...
  {
    LOG( DBG_INFO, stderr, "Recovery requested while all known connections alive. Need new cluster info?\n" );
    // todo retrieve cluster info and make sure we're only connected to master instances
    dbBE_Redis_cluster_info_t *new_cluster = dbBE_Redis_connection_mgr_get_cluster_info( conn_mgr );
    if( new_cluster == NULL )
      return DBBE_REDIS_CONNECTION_UNRECOVERABLE;


    dbBE_Redis_connection_t **tmp_conns;
    int tmp_conn_count = dbBE_Redis_connection_mgr_create_connection_list( conn_mgr, &tmp_conns );
    if( tmp_conn_count < 0 )
    {
      dbBE_Redis_cluster_info_destroy( new_cluster );
      return DBBE_REDIS_CONNECTION_UNRECOVERABLE;
    }

    // compare clusterinfo:
    int master_count = dbBE_Redis_connection_mgr_compare_cluster_info( new_cluster, *cluster, tmp_conns, tmp_conn_count );
    if( master_count < 0 )
    {
      dbBE_Redis_cluster_info_destroy( new_cluster );
      free( tmp_conns );
      return DBBE_REDIS_CONNECTION_UNRECOVERABLE;
    }

    // update global cluster info since we got a new one here
    dbBE_Redis_cluster_info_destroy( *cluster );
    *cluster = new_cluster;

    if( master_count == tmp_conn_count )
    {
      // all masters connected. Why did this recovery get triggered?
      LOG( DBG_ERR, stderr, "Requested recovery while all connections link to masters of updated cluster info.\n");

      // temp connections should be all NULL right now and we don't need them any more in this branch
      free( tmp_conns );

      // remove any non-master connections for conn_mgr
      if( tmp_conn_count != conn_mgr->_connection_count )
      {
        LOG( DBG_ERR, stderr, "There are unnecessary connections: %d\n", conn_mgr->_connection_count - tmp_conn_count );
        dbBE_Redis_connection_mgr_wipe_replica_connections( conn_mgr );
      }

      // checking/updating hash coverage...
      if( dbBE_Redis_locator_hash_covered( locator ) )
        return DBBE_REDIS_CONNECTION_RECOVERED;

      recoverable = dbBE_Redis_connection_mgr_update_all_hash_ranges( conn_mgr, locator, *cluster );
    }

    // any remaining connections must be replica links which need to be switched to their master
    // only attempt to switch if we're still not recovered yet:
    if( recoverable != DBBE_REDIS_CONNECTION_RECOVERED )
    {
      recoverable = dbBE_Redis_connection_mgr_replica_to_master( conn_mgr, locator, *cluster, tmp_conns, tmp_conn_count );
      switch( recoverable )
      {
        case DBBE_REDIS_CONNECTION_RECOVERABLE:
          usleep( 250000 );
          break;
        case DBBE_REDIS_CONNECTION_RECOVERED:
          break;
        case DBBE_REDIS_CONNECTION_UNRECOVERABLE:
        default:
          break;
      }
      free( tmp_conns );
    }

  }

  // check how far we got with recovery
  if( dbBE_Redis_locator_hash_covered( locator ) == 1 )
    recoverable = DBBE_REDIS_CONNECTION_RECOVERED;
  else
    recoverable = DBBE_REDIS_CONNECTION_RECOVERABLE;

  return recoverable;
}

/*
 * Remove a connection from the mgr regardless of status
 */
int dbBE_Redis_connection_mgr_rm( dbBE_Redis_connection_mgr_t *conn_mgr,
                                  dbBE_Redis_connection_t *conn )
{
  if(( conn_mgr == NULL ) || ( conn == NULL ))
  {
    LOG( DBG_ERR, stderr, "connection_mgr_rm: invalid argument: conn_mgr=%p; conn=%p\n", conn_mgr, conn );
    return -EINVAL;
  }

  if( conn_mgr->_connection_count <= 0 )
  {
    LOG( DBG_ERR, stderr, "connection_mgr_rm: no connections tracked. Can't delete.\n" );
    return -ENOENT;
  }

  unsigned i = 0;
  for( i = 0;
      (i < DBBE_REDIS_MAX_CONNECTIONS) &&
          ((conn_mgr->_connections[ i ] != conn) && (conn_mgr->_broken[ i ] != conn ));
      ++i )
  {}
  if( i >= DBBE_REDIS_MAX_CONNECTIONS )
  {
    LOG( DBG_ERR, stderr, "connection_mgr_rm: connection not found. Can't delete.\n" );
    return -ENOENT;
  }

  // only remove from event mgr if it wasn't broken already
  if( conn_mgr->_connections[ i ] == conn )
  {
    int rc = dbBE_Redis_event_mgr_rm( conn_mgr->_ev_mgr, conn );
    if( rc != 0 )
    {
      LOG( DBG_ERR, stderr, "connection_mgr_rm: failed to remove connection from event mgr.\n" );
      return rc;
    }
  }

  conn_mgr->_broken[ i ] = NULL;
  conn_mgr->_connections[ i ] = NULL;
  --conn_mgr->_connection_count;
  conn->_index = DBBE_REDIS_LOCATOR_INDEX_INVAL;

  return 0;
}


dbBE_Redis_connection_t* dbBE_Redis_connection_mgr_get_connection_to( dbBE_Redis_connection_mgr_t *conn_mgr,
                                                                      const char *dest )
{
  unsigned i;
  if( dest == NULL )
    return NULL;

  dbBE_Redis_address_t *d_addr = dbBE_Redis_address_from_string( dest );
  if( d_addr == NULL )
    return NULL;

  dbBE_Redis_connection_t *conn = NULL;
  for( i = 0; (i < DBBE_REDIS_MAX_CONNECTIONS); ++i )
  {
    conn = conn_mgr->_connections[ i ];
    if(( conn  != NULL ) && ( dbBE_Redis_address_compare( conn->_address, d_addr ) == 0 ))
      break;
  }
  dbBE_Redis_address_destroy( d_addr );
  return conn;
}


dbBE_Redis_connection_t* dbBE_Redis_connection_mgr_get_active( dbBE_Redis_connection_mgr_t *conn_mgr, const int blocking )
{
  if( conn_mgr != NULL )
    return dbBE_Redis_event_mgr_next( conn_mgr->_ev_mgr );
  else
    return NULL;
}

dbBE_Redis_request_t* dbBE_Redis_connection_mgr_request_each( dbBE_Redis_connection_mgr_t *conn_mgr,
                                                              dbBE_Redis_request_t *template_request )
{
  if(( conn_mgr == NULL ) || ( template_request == NULL ))
  {
    errno = EINVAL;
    return NULL;
  }

  unsigned i;
  dbBE_Redis_request_t *queue = NULL;
  for( i = 0; (i < DBBE_REDIS_MAX_CONNECTIONS); ++i )
  {
    if(( conn_mgr->_connections[ i ] != NULL ) &&
        (dbBE_Redis_connection_RTR( conn_mgr->_connections[ i ] ) ))
    {
      // if local-directory is requested, skip any non-local Redis servers
      if(( template_request->_user->_group == DBR_GROUP_LOCAL ) &&
          ( dbBE_Redis_address_compare_ip( &conn_mgr->_connections[ i ]->_address->_address, &conn_mgr->_local->_address ) != 0))
          continue;

      dbBE_Redis_request_t *req = dbBE_Redis_request_allocate( template_request->_user );
      if( req == NULL )
        continue;
      req->_location._type = DBBE_REDIS_REQUEST_LOCATION_TYPE_SLOT;
      req->_location._data._conn_idx = conn_mgr->_connections[ i ]->_index;
      req->_step = template_request->_step;
      memcpy( &req->_status, &template_request->_status, sizeof( dbBE_Redis_intern_data_t ));
      req->_next = queue;
      queue = req;
    }
  }
  return queue;
}

// this is an expensive operation, don't put it into the critical path
dbBE_Redis_result_t* dbBE_Redis_connection_mgr_retrieve_info( dbBE_Redis_connection_mgr_t *conn_mgr,
                                                              dbBE_Redis_connection_t *conn,
                                                              dbBE_Redis_sr_buffer_t *iobuf,
                                                              const dbBE_Redis_cluster_info_category_t category )
{
  if(( conn == NULL ) || ( conn_mgr == NULL ) || ( iobuf == NULL ))
    return NULL;

  // initialize the command and buffers
  // create a temp sr_buf to be used to send and recv data
  dbBE_Redis_result_t *result = NULL;
  dbBE_Transport_sr_buffer_reset( iobuf );
  char *sbuf = dbBE_Transport_sr_buffer_get_start( iobuf );
  int64_t buf_space = dbBE_Transport_sr_buffer_remaining( iobuf );

  // send cluster-info request
  int len = 0;
  switch( category )
  {
    case DBBE_INFO_CATEGORY_ROLE:
      len = snprintf( sbuf, buf_space, "*1\r\n$4\r\nROLE\r\n" );
      break;
    case DBBE_INFO_CATEGORY_CLUSTER_SLOTS:
      len = snprintf( sbuf, buf_space, "*2\r\n$7\r\nCLUSTER\r\n$5\r\nSLOTS\r\n" );
      break;
    default:
      return NULL;
  }

  if( len <= 0 )
    return NULL;

  dbBE_Transport_sr_buffer_add_data( iobuf, len, 1 );

  // send the request
  int rc = dbBE_Redis_connection_send( conn, iobuf );
  if( rc > 0 )
  {
    dbBE_Transport_sr_buffer_reset( iobuf );
    while( (rc = dbBE_Redis_connection_recv_direct( conn, iobuf )) == 0 );

    result = (dbBE_Redis_result_t*)calloc( 1, sizeof( dbBE_Redis_result_t ) );
    if( result == NULL )
      goto error;

    rc = dbBE_Redis_parse_sr_buffer( iobuf, result );

    while( rc == -EAGAIN )
    {
      LOG( DBG_VERBOSE, stdout, "Incomplete recv. Trying to retrieve more data.\n" );
      rc = dbBE_Redis_connection_recv_direct( conn, iobuf );
      if( rc == 0 )
      {
        rc = -EAGAIN;
      }
      dbBE_Redis_result_cleanup( result, 0 );
      rc = dbBE_Redis_parse_sr_buffer( iobuf, result );
    }
  }
  if( rc != 0 )
  {
    if( result != NULL )
      free( result );
    result = NULL;
  }
  return result;

error:
  if( iobuf )
    dbBE_Transport_sr_buffer_free( iobuf );
  return NULL;
}

int dbBE_Redis_connection_mgr_is_master( dbBE_Redis_connection_mgr_t *conn_mgr,
                                         dbBE_Redis_connection_t *conn )
{
  if(( conn_mgr == NULL ) || ( conn == NULL ))
    return -EINVAL;

  dbBE_Redis_sr_buffer_t *iobuf = dbBE_Transport_sr_buffer_allocate(
      dbBE_Redis_connection_mgr_get_connections( conn_mgr ) * DBBE_REDIS_INFO_PER_SERVER );
  if( iobuf == NULL )
    return -ENOMEM;

  int rc = 0;
  dbBE_Redis_result_t *result = dbBE_Redis_connection_mgr_retrieve_info( conn_mgr, conn, iobuf, DBBE_INFO_CATEGORY_ROLE );
  if( result == NULL )
  {
    rc = -ENOTCONN;
    goto exit_is_master;
  }

  if(( result->_type == dbBE_REDIS_TYPE_ARRAY ) &&
      ( result->_data._array._len >= 3 ) &&
      ( result->_data._array._data[0]._type == dbBE_REDIS_TYPE_CHAR ))
  {
    char *role = result->_data._array._data[0]._data._string._data;

    // check if we're connected to a master or a replica
    if( strncmp( role, "master", result->_data._array._data[0]._data._string._size ) == 0 )
      rc = 1;
  }
  else
    rc = -ENOTCONN;

  // cleanup for the next stage
  dbBE_Redis_result_cleanup( result, 1 );

exit_is_master:
  if( iobuf != NULL )
    dbBE_Transport_sr_buffer_free( iobuf );
  return rc;

}


DBR_Errorcode_t dbBE_Redis_connection_mgr_set_local_address( dbBE_Redis_connection_mgr_t *conn_mgr,
                                                             dbBE_Redis_cluster_info_t *cl_info )
{
  struct ifaddrs *ifs, *it;

  if( getifaddrs( &ifs ) != 0 )
    return DBR_ERR_BE_GENERAL;

  int n, s;
  for( n = 0; n < dbBE_Redis_cluster_info_getsize( cl_info ); ++n )
  {
    dbBE_Redis_server_info_t *si = dbBE_Redis_cluster_info_get_server( cl_info, n );
    if( si == NULL )
    {
      freeifaddrs( ifs );
      return DBR_ERR_NOFILE;
    }

    for( s = 0; s < dbBE_Redis_server_info_getsize( si ); ++s )
    {
      char *url = dbBE_Redis_server_info_get_replica( si, s );
      if( url == NULL )
      {
        freeifaddrs( ifs );
        return DBR_ERR_NOFILE;
      }
      dbBE_Redis_address_t *addr = dbBE_Redis_address_from_string( url );

      it = ifs;
      while( it != NULL )
      {
        if( dbBE_Redis_address_compare_ip( (struct sockaddr_in*)it->ifa_addr, &addr->_address ) == 0 )
        {
          conn_mgr->_local = addr;
          freeifaddrs( ifs );
          return DBR_SUCCESS;
        }
        it = it->ifa_next;
      }
      dbBE_Redis_address_destroy( addr );
    }
  }

  freeifaddrs( ifs );
  return DBR_ERR_UNAVAIL;
}


dbBE_Redis_cluster_info_t* dbBE_Redis_connection_mgr_get_cluster_info( dbBE_Redis_connection_mgr_t *conn_mgr )
{
  if( conn_mgr == NULL )
    return NULL;

  // search for a functional connection
  dbBE_Redis_connection_t *initial_conn = NULL;
  unsigned n;
  for( n=0; (n<DBBE_REDIS_MAX_CONNECTIONS) && (initial_conn == NULL); ++n )
    if( dbBE_Redis_connection_RTR( conn_mgr->_connections[ n ] ) )
      initial_conn = conn_mgr->_connections[ n ];

  if( initial_conn == NULL )
    return NULL;

  dbBE_Redis_sr_buffer_t *iobuf = dbBE_Transport_sr_buffer_allocate(
      DBBE_REDIS_MAX_CONNECTIONS * DBBE_REDIS_INFO_PER_SERVER );
  if( iobuf == NULL )
    return NULL;

  // retrieve the cluster info
  dbBE_Redis_result_t *result = dbBE_Redis_connection_mgr_retrieve_info(
      conn_mgr, initial_conn, iobuf, DBBE_INFO_CATEGORY_CLUSTER_SLOTS );
  if( result == NULL )
  {
    dbBE_Transport_sr_buffer_free( iobuf );
    return NULL;
  }

  dbBE_Redis_cluster_info_t *cl_info = dbBE_Redis_cluster_info_create( result );
  dbBE_Redis_result_cleanup( result, 1 );

  dbBE_Transport_sr_buffer_free( iobuf );

  // do we have single-node Redis server?
  if( cl_info == NULL )
  {
    char *env_url = dbBE_Redis_extract_env( DBR_SERVER_HOST_ENV, DBR_SERVER_DEFAULT_HOST );
    if( env_url != NULL )
    {
      cl_info = dbBE_Redis_cluster_info_create_single( env_url );
      free( env_url);
    }
  }

  dbBE_Redis_connection_mgr_set_local_address( conn_mgr, cl_info );
  return cl_info;
}
