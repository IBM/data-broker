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


#ifndef SRC_FSHIP_SRV_FSHIP_REQUEST_QUEUE_H_
#define SRC_FSHIP_SRV_FSHIP_REQUEST_QUEUE_H_


typedef struct dbrFShip_request_ctx_queue
{
  struct dbrFShip_request_ctx *_head;
  struct dbrFShip_request_ctx *_tail;
} dbrFShip_request_ctx_queue_t;

static inline
dbrFShip_request_ctx_queue_t* dbrFShip_request_ctx_queue_create()
{
  dbrFShip_request_ctx_queue_t *q = (dbrFShip_request_ctx_queue_t*)calloc( 1, sizeof( dbrFShip_request_ctx_queue_t ));
  if( q == NULL )
    return NULL;

  return q;
}

static inline
int dbrFShip_request_ctx_queue_push( dbrFShip_request_ctx_queue_t *q, dbrFShip_request_ctx_t *r )
{
  if(( q == NULL ) || ( r == NULL ))
    return -EINVAL;

  if( q->_head == NULL )
    q->_head = r;

  if( q->_tail != NULL )
    q->_tail->_next = r;

  q->_tail = r;
  r->_next = NULL;
  return 0;
}

static inline
dbrFShip_request_ctx_t* dbrFShip_request_ctx_queue_pop( dbrFShip_request_ctx_queue_t *q )
{
  if( q == NULL )
    return NULL;

  dbrFShip_request_ctx_t *t = q->_head;

  if( t != NULL )
  {
    if( q->_head == q->_tail )
      q->_tail = q->_tail->_next;
    q->_head = q->_head->_next;

    // perform lazy cleanup when encountering empty requests
    while(( q->_head != NULL ) && ( q->_head->_req == NULL ))
    {
      dbrFShip_request_ctx_t *d = q->_head;
      d->_cctx = NULL;
      d->_user_in = NULL;
      if( q->_head == q->_tail )
        q->_tail = q->_head->_next;
      q->_head = q->_head->_next;
      free( d );
    }

    // decouple
    t->_next = NULL;
  }

  return t;
}

static inline
int dbrFShip_request_ctx_queue_destroy( dbrFShip_request_ctx_queue_t *q )
{
  if( q == NULL )
    return -EINVAL;

  while( q->_head != NULL )
  {
    dbrFShip_request_ctx_t *t = q->_head->_next;
    if( q->_head->_req )
      dbBE_Request_free( q->_head->_req );
    free( q->_head );
    q->_head = t;
  }
  free( q );
  return 0;
}

static inline
dbrFShip_request_ctx_t* dbrFShip_request_ctx_find( dbrFShip_request_ctx_queue_t *q,
                                                   void *_user_hdl )
{
  dbrFShip_request_ctx_t *t = q->_head;
  while(( t != NULL ) && ( t->_user_in != _user_hdl ))
    t = t->_next;
  return t;
}

#endif /* SRC_FSHIP_SRV_FSHIP_REQUEST_QUEUE_H_ */
