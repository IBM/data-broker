/*
 * Copyright © 2018, 2019 IBM Corporation
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

#include <string.h>

#include "logutil.h"
#include "test_utils.h"
#include "errorcodes.h"

int main( int argc, char ** argv )
{
  int rc = 0;

  rc += TEST( strcmp( dbrGet_error( DBR_SUCCESS ), "Operation successful"), 0 );
  rc += TEST( strcmp( dbrGet_error( DBR_ERR_GENERIC ), "A general or unknown error has occurred"), 0 );
  rc += TEST( strcmp( dbrGet_error( DBR_ERR_INVALID ),  "Invalid argument"), 0 );
  rc += TEST( strcmp( dbrGet_error( DBR_ERR_INPROGRESS ),  "Operation in progress"), 0 );
  rc += TEST( strcmp( dbrGet_error( DBR_ERR_TIMEOUT ), "Operation timed out"), 0 );
  rc += TEST( strcmp( dbrGet_error( DBR_ERR_UNAVAIL ),  "Entry not available"), 0 );
  rc += TEST( strcmp( dbrGet_error( DBR_ERR_EXISTS ),  "Entry already exists"), 0 );
  rc += TEST( strcmp( dbrGet_error( DBR_ERR_NSBUSY ), "Namespace still referenced by a client"), 0 );
  rc += TEST( strcmp( dbrGet_error( DBR_ERR_NSINVAL ), "Namespace is invalid"), 0 );
  rc += TEST( strcmp( dbrGet_error( DBR_ERR_NOMEMORY ),  "Insufficient memory or storage"), 0 );
  rc += TEST( strcmp( dbrGet_error( DBR_ERR_TAGERROR ), "Invalid tag"), 0 );
  rc += TEST( strcmp( dbrGet_error( DBR_ERR_NOFILE ), "File not found"), 0 );
  rc += TEST( strcmp( dbrGet_error( DBR_ERR_NOAUTH ), "Access authorization required or failed"), 0 );
  rc += TEST( strcmp( dbrGet_error( DBR_ERR_NOCONNECT ), "Connection to a storage backend failed"), 0 );
  rc += TEST( strcmp( dbrGet_error( DBR_ERR_CANCELLED ), "Operation was cancelled"), 0 );
  rc += TEST( strcmp( dbrGet_error( DBR_ERR_NOTIMPL ), "Operation not implemented"), 0 );
  rc += TEST( strcmp( dbrGet_error( DBR_ERR_BE_GENERAL ), "Unspecified back-end error"), 0 );
  rc += TEST( strcmp( dbrGet_error( DBR_ERR_ITERATOR ), "Invalid iterator or error iterating the namespace"), 0 );
  rc += TEST( strcmp( dbrGet_error( DBR_ERR_PLUGIN ), "Error while processing request/data in data adapter"), 0 );

  rc += TEST( strcmp( dbrGet_error( -1 ), "Unknown Error" ), 0 );
  rc += TEST( strcmp( dbrGet_error( DBR_ERR_MAXERROR ), "Unknown Error" ), 0 );
  rc += TEST( strcmp( dbrGet_error( 10532 ), "Unknown Error" ), 0 );




  printf( "Test exiting with rc=%d\n", rc );
  return rc;
}
