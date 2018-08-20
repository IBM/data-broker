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

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>


#include <event2/event.h>

#include "logutil.h"
#include "test_utils.h"

int rc = 0;

void callback( evutil_socket_t fd, short what, void *arg)
{
  LOG( DBG_ALL, stdout, "Hit callback with %s\n", (char*)arg );;
  rc -= 1; // remove 1 from count
}


int main( int argc, char **argv )
{
  int s = socket( AF_INET, SOCK_STREAM, 0 );
  rc += TEST_NOT( s, -1 );

  struct timeval test_timeout = {2,0};

  struct event_base *evbase = event_base_new();
  rc += TEST_NOT( evbase, NULL );

  struct event *timeev1 = event_new(evbase, s, EV_TIMEOUT, callback, (char*)"Timeout event1");
  rc += TEST_NOT( timeev1, NULL );
  rc += TEST( event_add( timeev1, &test_timeout ), 0 );

  test_timeout.tv_sec = 3;
  struct event *timeev2 = event_new(evbase, s, EV_TIMEOUT, callback, (char*)"Timeout event2");
  rc += TEST_NOT( timeev2, NULL );
  rc += TEST( event_add( timeev2, &test_timeout ), 0 );

  rc += 2; // add 1 to cause an error if callback is not triggered

  event_base_dispatch( evbase );

  event_base_free( evbase );
  printf( "Test exiting with rc=%d\n", rc );
  return rc;
}
