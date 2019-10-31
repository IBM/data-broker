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

#include "complete.h"
#include "namespace.h"

#include <stddef.h>
#include <errno.h>
#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>
#endif


/*
 * convert a redis request in result stage into a completion
 * returns NULL on error and pointer to completion on success
 * will also perform the data transport if needed by the request type
 */
dbBE_Completion_t* dbBE_Redis_complete_command( dbBE_Redis_request_t *request,
                                                dbBE_Redis_result_t *result,
                                                int64_t rc )
{
  if(( request == NULL ) || ( result == NULL ))
  {
    errno = EINVAL;
    return NULL;
  }

  dbBE_Completion_t *completion = NULL;

  if( request->_step->_result != 0 )
  {
    completion = (dbBE_Completion_t*)malloc( sizeof( dbBE_Completion_t) );
    if( completion == NULL )
      return NULL;
    completion->_next = NULL;
    completion->_rc = rc;
    completion->_user = request->_user->_user;
    completion->_status = DBR_SUCCESS;
  }
  // this is not the result stage, can't create completion
  else {}


  dbBE_Redis_command_stage_spec_t *spec = request->_step;
  if( spec == NULL )
  {
    errno = EPROTO;
    free( completion );
    return NULL;
  }

  switch( request->_user->_opcode )
  {
    case DBBE_OPCODE_PUT:
      if( request->_step->_result != 0 )
      {
        if( completion->_status == DBR_SUCCESS )
          completion->_rc = result->_data._integer;
      }
      break;
    case DBBE_OPCODE_GET:
    case DBBE_OPCODE_READ:
      if( spec->_result != 0 )
      {
        switch( completion->_rc )
        {
          case 0:
            if( result->_data._integer < 0 )
              completion->_status = DBR_ERR_INPROGRESS;
            else
            {
              completion->_status = DBR_SUCCESS;
              completion->_rc = result->_data._integer;
            }
            break;
          case -ENOENT:
            completion->_status = DBR_ERR_UNAVAIL;
            completion->_rc = 0;
            break;
          default:
            completion->_status = DBR_ERR_BE_GENERAL;
            completion->_rc = result->_data._integer;
            break;
        }
      }
      else if( completion->_status == DBR_ERR_INPROGRESS )
      {
        if( result->_data._integer >= 0 )
        {
          completion->_status = DBR_SUCCESS;
          completion->_rc = result->_data._integer;
        }
      }
      break;
    case DBBE_OPCODE_DIRECTORY:
      if( spec->_result != 0 )
      {
        switch( completion->_rc )
        {
          case 0:
            if( ! result->_data._integer ) // ToDo: check for < 0 instead of !0
              completion->_status = DBR_ERR_INPROGRESS;
            else
            {
              completion->_status = DBR_SUCCESS;
              completion->_rc = result->_data._integer;
            }
            break;
          case -ENOENT:
            completion->_status = DBR_ERR_UNAVAIL;
            completion->_rc = 0;
            break;
          default:
            completion->_status = DBR_ERR_BE_GENERAL;
            completion->_rc = result->_data._integer;
            break;
        }
      }
      else if( completion->_status == DBR_ERR_INPROGRESS )
      {
        if( result->_data._integer ) // ToDo: check for < 0 instead of 0 <
        {
          completion->_status = DBR_SUCCESS;
          completion->_rc = result->_data._integer;
        }
      }
      break;
    case DBBE_OPCODE_NSCREATE:
    case DBBE_OPCODE_NSATTACH:
      if( rc == 0 )
      {
        completion->_rc = result->_data._integer;
      }
      else
      {
        completion->_rc = 0;
        completion->_status = (request->_user->_opcode == DBBE_OPCODE_NSATTACH) ? DBR_ERR_NSINVAL : DBR_ERR_EXISTS;
      }
      break;
    case DBBE_OPCODE_NSDETACH:
    case DBBE_OPCODE_NSDELETE:
      if( spec->_result != 0 )
      {
        if(( rc == 0 ) && ( result->_type == dbBE_REDIS_TYPE_INT ))
        {
          completion->_rc = result->_data._integer; // will be mapped by client lib // todo: error exchange between client lib and backend!!
        }
        else
          completion->_status = DBR_ERR_BE_GENERAL;

      }
      break;
    case DBBE_OPCODE_NSQUERY:
      if( request->_step->_result != 0 )
      {
        if(( rc == 0 ) && ( result->_type == dbBE_REDIS_TYPE_INT ))
          completion->_rc = result->_data._integer;
      }
      break;
    case DBBE_OPCODE_ITERATOR:
      if(( rc == 0 ) && ( result->_type == dbBE_REDIS_TYPE_INT ))
        completion->_rc = result->_data._integer;  // int64 value contains the iterator pointer
      break;
    default:
      break;
  }

  return completion;
}

dbBE_Completion_t* dbBE_Redis_complete_error( dbBE_Redis_request_t *request,
                                             dbBE_Redis_result_t *result,
                                             int64_t error_code )
{
  if(( request == NULL ) || ( result == NULL ))
  {
    errno = EINVAL;
    return NULL;
  }

  dbBE_Completion_t *completion = NULL;

  completion = (dbBE_Completion_t*)malloc( sizeof( dbBE_Completion_t) );
  if( completion == NULL )
    return NULL;
  completion->_next = NULL;
  completion->_rc = error_code;
  completion->_user = request->_user->_user;
  completion->_status = DBR_ERR_BE_GENERAL;

  return completion;
}

dbBE_Completion_t* dbBE_Redis_complete_cancel( dbBE_Redis_request_t *request,
                                               int64_t error_code )
{
  if( request == NULL )
  {
    errno = EINVAL;
    return NULL;
  }

  dbBE_Completion_t *completion = NULL;

  completion = (dbBE_Completion_t*)malloc( sizeof( dbBE_Completion_t) );
  if( completion == NULL )
    return NULL;
  completion->_next = NULL;
  completion->_rc = error_code;
  completion->_user = request->_user->_user;
  completion->_status = DBR_ERR_CANCELLED;

  return completion;
}
