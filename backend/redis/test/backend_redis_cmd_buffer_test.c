/*
 * Copyright © 2019 IBM Corporation
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
#include "redis/cmd_buffer.h"

int main( int argc, char **arcv )
{
  int rc = 0;

  dbBE_Redis_cmd_buffer_t *cmd = NULL;
  dbBE_sge_t *pos = NULL;

  rc += TEST_NOT_RC( dbBE_Redis_cmd_buffer_create(), NULL, cmd );
  rc += TEST( cmd->_index, 0 );
  rc += TEST( cmd->_cmd[0].iov_base, NULL );
  rc += TEST( cmd->_cmd[0].iov_len, 0 );

  rc += TEST( dbBE_Redis_cmd_buffer_add( NULL, 0 ), DBBE_REDIS_CMD_INDEX_INVAL );
  rc += TEST( dbBE_Redis_cmd_buffer_get_current( NULL ), NULL );
  rc += TEST( dbBE_Redis_cmd_buffer_reset( NULL ), DBR_ERR_INVALID );
  rc += TEST( dbBE_Redis_cmd_buffer_destroy( NULL ), DBR_ERR_INVALID );

  rc += TEST( dbBE_Redis_cmd_buffer_add( cmd, DBBE_SGE_MAX+1 ), DBBE_REDIS_CMD_INDEX_INVAL );

  rc += TEST_RC( dbBE_Redis_cmd_buffer_get_current( cmd ), cmd->_cmd, pos );

  rc += TEST( dbBE_Redis_cmd_buffer_add( cmd, 5 ), 5 );
  rc += TEST( dbBE_Redis_cmd_buffer_remain( cmd ), DBBE_SGE_MAX-5 );
  rc += TEST( dbBE_Redis_cmd_buffer_get_current( cmd ), &cmd->_cmd[5] );
  rc += TEST( dbBE_Redis_cmd_buffer_add( cmd, 7 ), 12 );

  rc += TEST( dbBE_Redis_cmd_buffer_reset( cmd ), DBR_SUCCESS );
  rc += TEST( dbBE_Redis_cmd_buffer_get_current( cmd ), cmd->_cmd );

  rc += TEST( dbBE_Redis_cmd_buffer_destroy( cmd ), DBR_SUCCESS );

  printf( "Test exiting with rc=%d\n", rc );
  return rc;
}
