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

#include "test_utils.h"
#include "../crc16.h"

int main(int argc, char * argv[])
{
  int rc = 0;
  unsigned int remainder;

  char *msg;
  if (argc >= 2)
  {
     msg = argv[1];
  }
  else
  {
    char *testmsg = (char*)malloc( 32 );
    rc += TEST_NOT( testmsg, NULL );
    TEST_BREAK( rc, "Memory allocation failed");
    snprintf( testmsg, 32, "123456789" );  // this is a Redis example string with result
    msg = testmsg;
  }
  rc += TEST( crcremainder( msg, 9 ), 0x31C3 );
  TEST_LOG( rc, "Regular CRC failed" );

  rc += TEST( crcremainder( NULL, 1013 ), -EINVAL );
  TEST_LOG( rc, "NULL msg test" );
  rc += TEST( crcremainder( msg, 0 ), -EINVAL );
  TEST_LOG( rc, "Zero-length" );
  rc += TEST( crcremainder( msg, -1051 ), -ENAMETOOLONG );
  TEST_LOG( rc, "Negative length" );
  rc += TEST( crcremainder( msg, DBR_MAX_KEY_LEN * 3 ), -ENAMETOOLONG );
  TEST_LOG( rc, "Too long" );

  // crc needs to be able to cope with nul-bytes too
  msg[4] = '\0';
  rc += TEST( crcremainder( msg, 9 ), 0x1e7a );
  TEST_LOG( rc, "Regular CRC failed" );


  if (argc < 2)
    free( msg );
  printf( "Test exiting with rc=%d\n", rc );
  return rc;
}

