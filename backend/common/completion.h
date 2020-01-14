/*
 * Copyright © 2018-2020 IBM Corporation
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

#ifndef BACKEND_COMMON_COMPLETION_H_
#define BACKEND_COMMON_COMPLETION_H_

#include "dbbe_api.h"

static inline
dbBE_Completion_t* dbBE_Completion_create( dbBE_Request_t *request,
                                           DBR_Errorcode_t dbr_status,
                                           int64_t rc )
{
  if( request == NULL )
    return NULL;

  dbBE_Completion_t *completion = (dbBE_Completion_t*)malloc( sizeof( dbBE_Completion_t) );
  if( completion == NULL )
    return NULL;

  completion->_next = NULL;
  completion->_rc = rc;
  completion->_user = request->_user;
  completion->_status = dbr_status;

  return completion;
}

static inline
ssize_t dbBE_Completion_serialize( const dbBE_Opcode op,
                                   const dbBE_Completion_t *comp,
                                   const dbBE_sge_t *sge,
                                   const int sge_count,
                                   char *data,
                                   size_t space )
{
  static const ssize_t dbBE_COMPLETION_MIN_SPACE = 18;
  if((op >= DBBE_OPCODE_MAX) || ( comp == NULL ) || ( data == NULL ) || (space == 0 ))
    return -EINVAL;

  ssize_t total = 0;
  total = snprintf( data, space, "%d\n%d\n%"PRId64"\n%p\n%p\n",
                    op, comp->_status, comp->_rc,
                    comp->_user, comp->_next );
  if( total < (ssize_t)dbBE_COMPLETION_MIN_SPACE )
    return -EBADMSG;

  if( (size_t)total >= space )
    return -ENOSPC;

  data += total;
  space -= total;

  ssize_t sge_total = 0;
  switch( op )
  {
    // some completions require data in the SGE
    case DBBE_OPCODE_GET:
    case DBBE_OPCODE_READ:
    case DBBE_OPCODE_DIRECTORY:
    case DBBE_OPCODE_ITERATOR:
    case DBBE_OPCODE_NSQUERY:
      sge_total = dbBE_SGE_serialize( sge, sge_count, data, space );
      break;
    default:
      break;
  }

  if( sge_total < 0 )
    return sge_total;

  total += sge_total;

  return total;
}


static inline
ssize_t dbBE_Completion_deserialize( char *data,
                                     size_t space,
                                     dbBE_Completion_t **comp_out,
                                     dbBE_sge_t **sge_out,
                                     int *sge_count_out )
{
  if(( data == NULL ) || (space == 0 ) || ( comp_out == NULL ))
    return -EINVAL;

  ssize_t total = 0;

  dbBE_Opcode opcode;
  DBR_Errorcode_t status;
  int64_t rc;
  void *user;
  dbBE_Completion_t *next;
  int parsed;

  int items = sscanf( data, "%d\n%d\n%"PRId64"\n%p\n%p\n%n",
            (int*)&opcode,
            (int*)&status,
            &rc,
            &user,
            &next,
            &parsed );
  if( items < 0 )
    return -EBADMSG;

  if(( items < 5 ) || ( data[ parsed - 1 ] != '\n') || ( space < (size_t)parsed ))
    return -EAGAIN;

  if( opcode >= DBBE_OPCODE_MAX )
    return -EBADMSG;

  data += parsed;
  space -= parsed;
  total += parsed;

  ssize_t sge_total = 0;
  switch( opcode )
  {
    // some completions require data in the SGE
    case DBBE_OPCODE_GET:
    case DBBE_OPCODE_READ:
    case DBBE_OPCODE_DIRECTORY:
    case DBBE_OPCODE_ITERATOR:
    case DBBE_OPCODE_NSQUERY:
      if(( sge_out == NULL ) || ( sge_count_out == NULL ))
        return -EINVAL;
      sge_total = dbBE_SGE_deserialize( *sge_out, *sge_count_out, data, space, sge_out, sge_count_out );
      break;
    default:
      break;
  }

  if( sge_total < 0 )
    return sge_total;

  total += sge_total;

  dbBE_Request_t req;
  req._user = user;
  dbBE_Completion_t *comp = dbBE_Completion_create( &req, status, rc );
  if( comp == NULL )
    return -ENOMEM;

  comp->_next = next;

  *comp_out = comp;

  return total;
}

#endif /* BACKEND_COMMON_COMPLETION_H_ */
