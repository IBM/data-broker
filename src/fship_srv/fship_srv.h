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

#define DBR_FSHIP_CONNECTIONS_LIMIT ( 1024 )

typedef struct dbrFShip_config
{
  char *_listenaddr;
  unsigned _daemon;
  size_t _max_mem;
} dbrFShip_config_t;

struct dbrFShip_client_context;

typedef struct dbrFShip_request_ctx
{
  struct dbrFShip_request_ctx *_next; // queueing
  void *_user_in; // save the inbound user ptr since we're re-using that for our purposes here
  struct dbrFShip_client_context *_cctx; // client context to link request to client
  dbBE_Request_t *_req; // posted request info
} dbrFShip_request_ctx_t;

#include "fship_request_queue.h"

typedef struct dbrFShip_client_context
{
  dbBE_Connection_t *_conn;
  dbrFShip_request_ctx_queue_t *_pending;
} dbrFShip_client_context_t;

typedef struct dbrFShip_main_context
{
  dbrFShip_config_t _cfg;
  dbrMain_context_t *_mctx;
  dbrFShip_client_context_t **_cctx;
  dbBE_Connection_queue_t *_conn_queue;
  dbBE_Redis_sr_buffer_t *_r_buf;
  dbBE_Redis_sr_buffer_t *_s_buf;
  volatile int _total_pending;
} dbrFShip_main_context_t;


typedef struct dbrFShip_threadio
{
  struct event_base *_evbase;  // shared libevent base
  volatile int _keep_running; // running indicator of accept thread
  dbBE_Connection_queue_t *_conn_queue; // connection queue with activated connections
  dbrFShip_config_t *_cfg; // base configuration of the service
  int _threadrc; // return value of accept thread
} dbrFShip_threadio_t;

typedef struct dbrFShip_event_info
{
  dbrFShip_client_context_t *_cctx;
  dbBE_Connection_queue_t *_queue;
} dbrFShip_event_info_t;

void* dbrFShip_listen_start( void *arg );
int dbrFShip_parse_cmdline( int argc, char **argv, dbrFShip_config_t *cfg );

int dbrFShip_inbound( dbrFShip_threadio_t *tio, dbrFShip_main_context_t *context );
int dbrFShip_outbound( dbrFShip_threadio_t *tio, dbrFShip_main_context_t *context );

dbrFShip_request_ctx_t* dbrFShip_create_request( dbBE_Request_t *req, dbrFShip_client_context_t *cctx );
int dbrFShip_completion_cleanup( dbrFShip_request_ctx_t *rctx );

#endif /* SRC_FSHIP_SRV_FSHIP_SRV_H_ */
