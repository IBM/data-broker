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

#include <errno.h>
#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>  // malloc
#endif
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "logutil.h"
#include "definitions.h"
#include "utility.h"
#include "conn_mgr.h"
#include "parse.h"

#define DBBE_REDIS_CONN_MGR_TRACKED_EVENTS ( EPOLLET | EPOLLIN | EPOLLERR | EPOLLRDHUP | EPOLLPRI )

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
  if(( conn_mgr == NULL ) || ( conn == NULL ) || ( conn->_socket == 0 ))
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

  unsigned i = 0;
  for( i = 0; (i < DBBE_REDIS_MAX_CONNECTIONS) && (conn_mgr->_connections[ i ] != NULL); ++i ) {}
  if( i >= DBBE_REDIS_MAX_CONNECTIONS )
  {
    LOG( DBG_ERR, stderr, "connection_mgr_add: connection slots exhausted. Can't add new connection.\n" );
    return -ENOMEM;
  }

  conn_mgr->_connections[ i ] = conn;
  ++conn_mgr->_connection_count;
  conn->_index = i;

  int rc = dbBE_Redis_event_mgr_add( conn_mgr->_ev_mgr, conn );
  if( rc != 0 )
  {
    LOG( DBG_ERR, stderr, "connection_mgr_add: failed to add conn=%p (socket=%d) to event_mgr. rc=%d\n", conn, conn->_socket, rc );
    return -rc;
  }

  return 0;
}

dbBE_Redis_connection_t* dbBE_Redis_connection_mgr_newlink( dbBE_Redis_connection_mgr_t *conn_mgr,
                                                            char *url )
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

  conn_mgr->_broken[ conn->_index ] = conn;
  conn_mgr->_connections[ conn->_index ] = NULL;
  --conn_mgr->_connection_count;

  dbBE_Redis_connection_fail( conn ); // mark failed ...
  dbBE_Redis_connection_unlink( conn ); // and shut down/disconnect without deleting the addr info

  return 0;
}

int dbBE_Redis_connection_mgr_conn_recover( dbBE_Redis_connection_mgr_t *conn_mgr,
                                            dbBE_Redis_locator_t *locator,
                                            dbBE_Redis_cluster_info_t *cluster )
{
  if(( conn_mgr == NULL ) || ( locator == NULL ) || ( cluster == NULL ))
    return -EINVAL;

  unsigned c;
  int recovered = 0;
  for( c=0; c < DBBE_REDIS_MAX_CONNECTIONS; ++c )
  {
    dbBE_Redis_connection_t *rec = conn_mgr->_broken[ c ];
    if( rec != NULL )
    {
      rec->_status = DBBE_CONNECTION_STATUS_DISCONNECTED;
      if( dbBE_Redis_connection_reconnect( rec ) == 0 )
      {
        int slot;
        conn_mgr->_connections[ c ] = rec;
        conn_mgr->_broken[ c ] = NULL;
        ++conn_mgr->_connection_count;
        ++recovered;
        dbBE_Redis_slot_bitmap_t *bitmap = dbBE_Redis_connection_get_slot_range( rec );
        for( slot=0;
            (locator != NULL) && ( bitmap != NULL ) && (slot < DBBE_REDIS_HASH_SLOT_MAX);
            ++slot )
        {
          if( dbBE_Redis_slot_bitmap_get( bitmap, slot ) != 0 )
            dbBE_Redis_locator_assign_conn_index( locator, c, slot );
        }
      }
      else
      {
        char url[ DBR_SERVER_URL_MAX_LENGTH ];
        dbBE_Redis_address_to_string( rec->_address, url, DBR_SERVER_URL_MAX_LENGTH );
        dbBE_Redis_server_info_t *server = dbBE_Redis_cluster_info_get_server_by_addr( cluster,
                                                                                       url );
        if( server != NULL )
        {
          int first_slot = dbBE_Redis_server_info_get_first_slot( server );
          int last_slot = dbBE_Redis_server_info_get_last_slot( server );
          int n;

          for( n = 0; ( n < server->_server_count ) && ( recovered == 0 ); ++n )
          {
            // don't retry the master, that failed above already. (assuming the bookkeeping is correct)
            char *addr = dbBE_Redis_server_info_get_replica( server, n );
            if( addr == dbBE_Redis_server_info_get_master( server ) )
              continue;

            dbBE_Redis_connection_t *repl_conn = dbBE_Redis_connection_mgr_newlink( conn_mgr, addr );
            if( repl_conn == NULL )
            {
              recovered = 0;
              break;
            }

            dbBE_Redis_connection_assign_slot_range( repl_conn,
                                                     first_slot,
                                                     last_slot );

            // update locator
            int slot;
            for( slot = first_slot; slot <= last_slot; ++slot )
              dbBE_Redis_locator_assign_conn_index( locator,
                                                    repl_conn->_index,
                                                    slot );
            ++recovered;
            dbBE_Redis_server_info_update_master( server, n ); // make the connection to this replica the new master
            dbBE_Redis_connection_mgr_rm( conn_mgr, rec ); // remove the old connection
            dbBE_Redis_connection_destroy( rec ); // delete the old connection
          }
        }

        if( recovered == 0 )
          LOG( DBG_VERBOSE, stderr, "conn_mgr_conn_recover: failed to reconnect or recover index %d. Also failed to find/connect to replicas\n", rec->_index );
      }
    }
  }
  return recovered;
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
    --conn_mgr->_connection_count;
  }

  conn_mgr->_broken[ i ] = NULL;
  conn_mgr->_connections[ i ] = NULL;

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
dbBE_Redis_result_t* dbBE_Redis_connection_mgr_retrieve_clusterinfo( dbBE_Redis_connection_mgr_t *conn_mgr,
                                                                     dbBE_Redis_connection_t *conn,
                                                                     dbBE_Redis_sr_buffer_t *iobuf )
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
  int len = snprintf( sbuf, buf_space, "*2\r\n$7\r\nCLUSTER\r\n$5\r\nSLOTS\r\n" );
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
