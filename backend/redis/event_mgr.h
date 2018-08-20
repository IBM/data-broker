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

#ifndef BACKEND_REDIS_EVENT_MGR_H_
#define BACKEND_REDIS_EVENT_MGR_H_

#include <sys/types.h>
#include <event2/event.h>

#include "definitions.h"
#include "connection.h"
#include "connection_queue.h"

typedef struct dbBE_Redis_event_mgr
{
  struct timeval _timeout;
  struct event_base *_evbase;
  struct event *_events[ DBBE_REDIS_MAX_CONNECTIONS ];
  dbBE_Redis_connection_queue_t *_active_queue;
} dbBE_Redis_event_mgr_t;


typedef struct dbBE_Redis_event_info
{
  dbBE_Redis_connection_t *_conn;
  dbBE_Redis_event_mgr_t *_ev_mgr;
} dbBE_Redis_event_info_t;



#define dbBE_Redis_event_mgr_getbase( mgr ) ((mgr)->_evbase)
#define dbBE_Redis_connection_mgr_getqueue( mgr ) ((mgr)->_active_queue)

/*
 * create and initialize the event mgr
 */
dbBE_Redis_event_mgr_t* dbBE_Redis_event_mgr_init( unsigned default_timeout );


/*
 * exit and destroy the event mgr
 */
int dbBE_Redis_event_mgr_exit( dbBE_Redis_event_mgr_t *ev_mgr );


/*
 * add a new connection to the event mgr
 */
int dbBE_Redis_event_mgr_add( dbBE_Redis_event_mgr_t *ev_mgr,
                              const dbBE_Redis_connection_t *conn );


/*
 * rearm/readd an existing event for the connection
 */
int dbBE_Redis_event_mgr_rearm( dbBE_Redis_event_mgr_t *ev_mgr,
                                const dbBE_Redis_connection_t *conn );

/*
 * remove a connection from the event mgr
 */
int dbBE_Redis_event_mgr_rm( dbBE_Redis_event_mgr_t *ev_mgr,
                             const dbBE_Redis_connection_t *conn );


/*
 * retrieve the next active connection (ready to read)
 */
dbBE_Redis_connection_t* dbBE_Redis_event_mgr_next( dbBE_Redis_event_mgr_t *ev_mgr );


#endif /* BACKEND_REDIS_EVENT_MGR_H_ */
