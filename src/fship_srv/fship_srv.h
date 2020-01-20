/*
 * Copyright © 2020 IBM Corporation
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

#ifndef SRC_FSHIP_SRV_FSHIP_SRV_H_
#define SRC_FSHIP_SRV_FSHIP_SRV_H_

#include "common/request_queue.h"
#include "network/connection.h"
#include "network/connection_queue.h"
#include "transports/sr_buffer.h"

typedef struct dbrFShip_config
{
  char *_listenaddr;
  unsigned _daemon;
  size_t _max_mem;
} dbrFShip_config_t;

typedef struct dbrFShip_client_context
{
  dbBE_Connection_t *_conn;
  dbBE_Request_queue_t *_pending;
} dbrFShip_client_context_t;


typedef struct dbrFShip_main_context
{
  dbrFShip_config_t _cfg;
  dbrMain_context_t *_mctx;
  dbrFShip_client_context_t **_cctx;
  dbBE_Redis_sr_buffer_t *_data_buf;
} dbrFShip_main_context_t;


typedef struct dbrFShip_threadio
{
  struct event_base *_evbase;  // shared libevent base
  volatile int _keep_running; // running indicator of accept thread
  dbBE_Connection_queue_t *_conn_queue; // connection queue with activated connections
  int _threadrc; // return value of accept thread
} dbrFShip_threadio_t;

typedef struct dbrFShip_event_info
{
  dbrFShip_client_context_t *_cctx;
  dbBE_Connection_queue_t *_queue;
} dbrFShip_event_info_t;


void* dbrFShip_listen_start( void *arg );
int dbrFShip_parse_cmdline( int argc, char **argv, dbrFShip_config_t *cfg );


#endif /* SRC_FSHIP_SRV_FSHIP_SRV_H_ */
