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


#ifndef BACKEND_COMMON_CONNECTION_H_
#define BACKEND_COMMON_CONNECTION_H_

#include "definitions.h"
#include "address.h"

#include <netinet/in.h>

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
  DBBE_CONNECTION_ERROR = -1,
  DBBE_CONNECTION_RECOVERED = 0,
  DBBE_CONNECTION_RECOVERABLE = 1,
  DBBE_CONNECTION_UNRECOVERABLE = 2
} dbBE_Connection_recoverable_t;


typedef struct dbBE_Connection
{
  dbBE_Redis_address_t *_address;
  struct timeval _last_alive;
  int _socket;
  volatile dbBE_Connection_status_t _status;
  char _url[ DBBE_URL_MAX_LENGTH ];
} dbBE_Connection_t;


/*
 * return the connection status of the connection
 */
#define dbBE_Connection_get_status( conn ) ( ( (conn) != NULL ) ? (conn)->_status : DBBE_CONNECTION_STATUS_UNSPEC )


/*
 * set connection status to reflect ready to recv (i.e. pending data)
 */
#define dbBE_Connection_set_active( conn ) \
    { \
      if( (conn)->_status == DBBE_CONNECTION_STATUS_AUTHORIZED ) \
        (conn)->_status = DBBE_CONNECTION_STATUS_PENDING_DATA; \
    }

#define dbBE_Connection_RTR_nocheck( conn ) \
    ( ((conn)->_status == DBBE_CONNECTION_STATUS_AUTHORIZED ) || ((conn)->_status == DBBE_CONNECTION_STATUS_PENDING_DATA ) )

#define dbBE_Connection_RTR( conn ) \
    ( ( (conn) != NULL ) && dbBE_Connection_RTR_nocheck( conn ) )


#define dbBE_Connection_RTS( conn ) \
  ( ( (conn) != NULL ) && ( ((conn)->_status == DBBE_CONNECTION_STATUS_CONNECTED ) || dbBE_Connection_RTR_nocheck( conn ) ) )


/*
 * return the url of the currently connected redis instance
 */
#define dbBE_Connection_get_url( conn ) ( (conn) != NULL ? (conn)->_url : NULL )

dbBE_Connection_t *dbBE_Connection_create();


/*
 * connect to a remote port given by the destination url
 * it will connect and then return the created address type
 */
dbBE_Redis_address_t* dbBE_Connection_link( dbBE_Connection_t *conn,
                                            const char *url,
                                            const char *authfile );

/*
 * return 0 if the connection is considered not recoverable
 * return 1 otherwise
 */
dbBE_Connection_recoverable_t dbBE_Connection_recoverable( dbBE_Connection_t *conn );

/*
 * reconnect to a remote port that was connected before
 */
int dbBE_Connection_reconnect( dbBE_Connection_t *conn );

/*
 * disconnect
 */
int dbBE_Connection_unlink( dbBE_Connection_t *conn );

/*
 * perform connection authorization
 */
int dbBE_Connection_auth( dbBE_Connection_t *conn, const char *authfile );

/*
 * destroy a connection object and free the
 */
int dbBE_Connection_destroy( dbBE_Connection_t *conn );


#endif /* BACKEND_COMMON_CONNECTION_H_ */
