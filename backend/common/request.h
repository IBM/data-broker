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


#ifndef BACKEND_COMMON_REQUEST_H_
#define BACKEND_COMMON_REQUEST_H_



static inline
ssize_t dbBE_Request_serialize(const dbBE_Request_t *req, char *data, size_t space )
{
  static const size_t dbBE_REQUEST_MIN_SPACE = 20; // at least 10 items with single digit size + separator
  if(( req == NULL ) || ( data == NULL ) || ( space < dbBE_REQUEST_MIN_SPACE))
    return -EINVAL;

  ssize_t total = 0;

  total = snprintf( data, space, "%d\n%p\n%p\n%p\n%p\n%ld\n%ld\n%"PRId64"\n%s\n%s\n",
            req->_opcode,
            req->_ns_hdl,
            req->_user,
            req->_next,
            req->_group,
            req->_key != NULL ? strnlen(req->_key, DBR_MAX_KEY_LEN) : 0,
            req->_match != NULL ? strnlen(req->_match, DBR_MAX_KEY_LEN) : 0,
            req->_flags,
            req->_key,
            req->_match );

  if( total < (ssize_t)dbBE_REQUEST_MIN_SPACE )
    return -EBADMSG;

  data += total;
  space -= total;

  ssize_t sge_total = 0;
  switch( req->_opcode )
  {
    case DBBE_OPCODE_GET:
    case DBBE_OPCODE_READ:
      sge_total = dbBE_SGE_serialize_header( req->_sge, req->_sge_count, data, space );
      break;
    case DBBE_OPCODE_PUT:
      sge_total = dbBE_SGE_serialize( req->_sge, req->_sge_count, data, space );
      break;
    default:
      return -EINVAL;
  }

  if( sge_total < 0 )
    return sge_total;

  total += sge_total;

  return total;
}

static inline
ssize_t dbBE_Request_deserialize( dbBE_Request_t *req, char *data, size_t space )
{
  unsigned keylen, matchlen;
  int parsed;
  ssize_t total = 0;
  int items = sscanf( data, "%d\n%p\n%p\n%p\n%p\n%d\n%d\n%"PRId64"\n%n",
            (int*)&req->_opcode,
            &req->_ns_hdl,
            &req->_user,
            &req->_next,
            &req->_group,
            &keylen,
            &matchlen,
            &req->_flags,
            &parsed );
  if( items < 0 )
    return -EBADMSG;

  if( items < 8 )
    return -EAGAIN;

  if(( req->_opcode >= DBBE_OPCODE_MAX ) || ( req->_flags >= DBR_FLAGS_MAX ))
    return -EBADMSG;

  if(( keylen > DBR_MAX_KEY_LEN ) || ( matchlen > DBR_MAX_KEY_LEN ))
    return -EBADMSG;

  data += parsed;
  space -= parsed;
  total += parsed;
  if( space < keylen )
    return -EAGAIN;

  strncpy( req->_key, data, keylen );

  data += keylen;
  space -= keylen;
  total += keylen;
  if( space < matchlen )
    return -EAGAIN;

  strncpy( req->_match, data, matchlen );
  total += matchlen;

  return total;
}

#endif /* BACKEND_COMMON_REQUEST_H_ */
