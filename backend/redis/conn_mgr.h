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

#ifndef BACKEND_REDIS_CONN_MGR_H_
#define BACKEND_REDIS_CONN_MGR_H_

#include <errno.h>
//#include <pthread.h>

#include "definitions.h"
#include "connection.h"
#include "locator.h"
#include "event_mgr.h"
#include "result.h"
#include "cluster_info.h"

typedef enum
{
  DBBE_INFO_CATEGORY_UNSPECIFIED = 0,
  DBBE_INFO_CATEGORY_ROLE = 1,
  DBBE_INFO_CATEGORY_CLUSTER_SLOTS = 2,
  DBBE_INFO_CATEGORY_MAX = 3
}  dbBE_Redis_cluster_info_category_t;

typedef struct
{
  // connection list
  dbBE_Redis_connection_t *_connections[ DBBE_REDIS_MAX_CONNECTIONS ];
  dbBE_Redis_connection_t *_broken[ DBBE_REDIS_MAX_CONNECTIONS ];
//  pthread_mutex_lock_t _lock;

  int _connection_count;

  // active connections?
  // disabled/old/disconnected connections?

  dbBE_Redis_event_mgr_t *_ev_mgr;
} dbBE_Redis_connection_mgr_t;


/*
 * initialize the connection mgr
 */
dbBE_Redis_connection_mgr_t* dbBE_Redis_connection_mgr_init();

/*
 * cleanup the connection_mgr
 */
void dbBE_Redis_connection_mgr_exit( dbBE_Redis_connection_mgr_t *conn_mgr );

/*
 * Add a new connection to the mgr
 */
int dbBE_Redis_connection_mgr_add( dbBE_Redis_connection_mgr_t *conn_mgr,
                                   dbBE_Redis_connection_t *conn );


/*
 * fail a tracked connection
 */
int dbBE_Redis_connection_mgr_conn_fail( dbBE_Redis_connection_mgr_t *conn_mgr,
                                         dbBE_Redis_connection_t *conn );

/*
 * attempt to recover the conn_mgr connectivity
 */
dbBE_Redis_connection_recoverable_t dbBE_Redis_connection_mgr_conn_recover(
    dbBE_Redis_connection_mgr_t *conn_mgr,
    dbBE_Redis_locator_t *locator,
    dbBE_Redis_cluster_info_t **cluster );

/*
 * Remove a connection from the mgr
 */
int dbBE_Redis_connection_mgr_rm( dbBE_Redis_connection_mgr_t *conn_mgr,
                                  dbBE_Redis_connection_t *conn );

/*
 * Insert and connect a new connection
 */
dbBE_Redis_connection_t* dbBE_Redis_connection_mgr_newlink( dbBE_Redis_connection_mgr_t *conn_mgr,
                                                            char *url );

/*
 * get the number of (active) connections
 */
static inline
int dbBE_Redis_connection_mgr_get_connections( dbBE_Redis_connection_mgr_t *conn_mgr )
{
  if( conn_mgr != NULL )
    return conn_mgr->_connection_count;
  return -EINVAL;
}

/*
 * return the connection entry at a given index
 */
static inline
dbBE_Redis_connection_t* dbBE_Redis_connection_mgr_get_connection_at( dbBE_Redis_connection_mgr_t *conn_mgr,
                                                                      int index )
{
  if(( conn_mgr != NULL ) && ( (unsigned)index == (unsigned)index % DBBE_REDIS_MAX_CONNECTIONS ))
    return conn_mgr->_connections[ index ];
  errno = ENOENT;
  return NULL;
}

/*
 * return the connection entry to a given destination address
 */
dbBE_Redis_connection_t* dbBE_Redis_connection_mgr_get_connection_to( dbBE_Redis_connection_mgr_t *conn_mgr,
                                                                      const char *dest );


/*
 * return an active connection (i.e. a connection with data ready to receive)
 */
dbBE_Redis_connection_t* dbBE_Redis_connection_mgr_get_active( dbBE_Redis_connection_mgr_t *conn_mgr, const int blocking );


/*
 * return a list of empty requests, one for each (connected/authorized) connection
 */
dbBE_Redis_request_t* dbBE_Redis_connection_mgr_request_each( dbBE_Redis_connection_mgr_t *conn_mgr,
                                                              dbBE_Redis_request_t *template_request );

/*
 * send CLUSTER command to Redis and retrieve+parse the response into result structure
 */
dbBE_Redis_result_t* dbBE_Redis_connection_mgr_retrieve_info( dbBE_Redis_connection_mgr_t *conn_mgr,
                                                              dbBE_Redis_connection_t *conn,
                                                              dbBE_Redis_sr_buffer_t *iobuf,
                                                              const dbBE_Redis_cluster_info_category_t category );

/*
 * contact the node and retrieve role information.
 * then return
 *  0  - if not master
 *  1  - if master
 *  <0 - if error
 */
int dbBE_Redis_connection_mgr_is_master( dbBE_Redis_connection_mgr_t *conn_mgr,
                                         dbBE_Redis_connection_t *conn );

/*
 * create a cluster info structure after retrieving an updated cluster info (works for non-cluster as well)
 */
dbBE_Redis_cluster_info_t* dbBE_Redis_connection_mgr_get_cluster_info( dbBE_Redis_connection_mgr_t *conn_mgr );

#endif /* BACKEND_REDIS_CONN_MGR_H_ */
