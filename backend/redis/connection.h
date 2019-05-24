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

#ifndef BACKEND_REDIS_CONNECTION_H_
#define BACKEND_REDIS_CONNECTION_H_

#include "locator.h"
#include "transports/sr_buffer.h"
#include "common/data_transport.h"
#include "s2r_queue.h"
#include "slot_bitmap.h"

//#ifndef DEBUG_REDIS_PROTOCOL
//#define DEBUG_REDIS_PROTOCOL
//#endif

typedef enum
{
  DBBE_CONNECTION_STATUS_UNSPEC = 0,
  DBBE_CONNECTION_STATUS_INITIALIZED,
  DBBE_CONNECTION_STATUS_CONNECTED,
  DBBE_CONNECTION_STATUS_AUTHORIZED,
  DBBE_CONNECTION_STATUS_PENDING_DATA,
  DBBE_CONNECTION_STATUS_DISCONNECTED,
  DBBE_CONNECTION_STATUS_MAX
} dbBE_Connection_status_t;

typedef enum
{
  DBBE_REDIS_CONNECTION_ERROR = -1,
  DBBE_REDIS_CONNECTION_RECOVERED = 0,
  DBBE_REDIS_CONNECTION_RECOVERABLE = 1,
  DBBE_REDIS_CONNECTION_UNRECOVERABLE = 2
} dbBE_Redis_connection_recoverable_t;

typedef struct dbBE_Redis_connection
{
  int _socket;
  int _index;
  dbBE_Redis_address_t *_address;
  dbBE_Data_transport_device_t *_senddev;
//  dbBE_Data_transport_device_t *_recvdev;
//  dbBE_Redis_sr_buffer_t *_sendbuf;
  dbBE_Redis_sr_buffer_t *_recvbuf;
  dbBE_Redis_s2r_queue_t *_posted_q;
  dbBE_Redis_slot_bitmap_t *_slots;
  volatile dbBE_Connection_status_t _status;
  struct timeval _last_alive;
} dbBE_Redis_connection_t;


/*
 * create a Redis connection object, initialize with default/uninitialized values
 * does also allocate and assign send/recv buffers and posted queue
 * input parameter is the size of each s/r buffer in bytes
 */
dbBE_Redis_connection_t *dbBE_Redis_connection_create( const uint64_t sr_buffer_size );


/*
 * return the connection status of the Redis connection
 */
#define dbBE_Redis_connection_get_status( conn ) ( ( (conn) != NULL ) ? (conn)->_status : DBBE_CONNECTION_STATUS_UNSPEC )

/*
 * return the index of a connection in the connection mgr (-1 if connection is not known to be in connection mgr)
 */
#define dbBE_Redis_connection_get_index( conn ) ( ( (conn) != NULL ) ? (conn)->_index : -1 )

/*
 * return the slot-range bitmap ptr
 */
#define dbBE_Redis_connection_get_slot_range( conn ) ( ( (conn) != NULL ) ? (conn)->_slots : NULL )



/*
 * set connection status to reflect ready to recv (i.e. pending data)
 */
#define dbBE_Redis_connection_set_active( conn ) \
    { \
      if( (conn)->_status == DBBE_CONNECTION_STATUS_AUTHORIZED ) \
        (conn)->_status = DBBE_CONNECTION_STATUS_PENDING_DATA; \
    }

#define dbBE_Redis_connection_RTR_nocheck( conn ) \
    ( ((conn)->_status == DBBE_CONNECTION_STATUS_AUTHORIZED ) || ((conn)->_status == DBBE_CONNECTION_STATUS_PENDING_DATA ) )

#define dbBE_Redis_connection_RTR( conn ) \
    ( ( (conn) != NULL ) && dbBE_Redis_connection_RTR_nocheck( conn ) )


#define dbBE_Redis_connection_RTS( conn ) \
  ( ( (conn) != NULL ) && ( ((conn)->_status == DBBE_CONNECTION_STATUS_CONNECTED ) || dbBE_Redis_connection_RTR_nocheck( conn ) ) )


/*
 * return the send-transport device assigned to this connection
 */
#define dbBE_Redis_connection_get_send_dev( conn ) ( (conn) != NULL ? (conn)->_senddev : NULL )

/*
 * return the recv-transport device assigned to this connection
 */
#define dbBE_Redis_connection_get_recv_dev( conn ) ( (conn) != NULL ? (conn)->_recvdev : NULL )



/*
 * assign an initial slot range to the connection
 */
int dbBE_Redis_connection_assign_slot_range( dbBE_Redis_connection_t *conn,
                                             int first_slot,
                                             int last_slot );


/*
 * connect to a Redis instance given by the destination url
 * it will connect and then return the created Redis address type
 */
dbBE_Redis_address_t* dbBE_Redis_connection_link( dbBE_Redis_connection_t *conn,
                                                  const char *url,
                                                  const char *authfile );

/*
 * return 0 if the connection is considered not recoverable
 * return 1 otherwise
 */
dbBE_Redis_connection_recoverable_t dbBE_Redis_connection_recoverable( dbBE_Redis_connection_t *conn );

/*
 * reconnect to a Redis instance that was connected before
 */
int dbBE_Redis_connection_reconnect( dbBE_Redis_connection_t *conn );

/*
 * receive data from a connection and place data into the attached sr_buffer
 */
ssize_t dbBE_Redis_connection_recv( dbBE_Redis_connection_t *conn );

/*
 * continue reception of data to the current receive buffer
 * to try to complete interrupted or incomplete messages
 */
ssize_t dbBE_Redis_connection_recv_more( dbBE_Redis_connection_t *conn );

/*
 * receive into user-provided buffer instead of connection-attached default
 * no buffer reset or cleanup is done.
 */
ssize_t dbBE_Redis_connection_recv_direct( dbBE_Redis_connection_t *conn,
                                           dbBE_Redis_sr_buffer_t *buf );

/*
 * flush the send buffer by sending it to the connected Redis instance
 */
int dbBE_Redis_connection_send( dbBE_Redis_connection_t *conn,
                                dbBE_Redis_sr_buffer_t *buf );

/*
 * send the cmd vector to the connected Redis instance
 */
int dbBE_Redis_connection_send_cmd( dbBE_Redis_connection_t *conn,
                                    dbBE_sge_t *cmd,
                                    const int cmdlen );

/*
 * disconnect from a Redis instance
 */
int dbBE_Redis_connection_unlink( dbBE_Redis_connection_t *conn );

/*
 * read and send passwd authorization string
 */
int dbBE_Redis_connection_auth( dbBE_Redis_connection_t *conn, const char *authfile );

/*
 * destroy a Redis connection object and free the
 * does not destroy the sr_buffers
 */
void dbBE_Redis_connection_destroy( dbBE_Redis_connection_t *conn );


#endif /* BACKEND_REDIS_CONNECTION_H_ */
