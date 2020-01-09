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


/**
 * @brief allocate a new request struct including SGE-space
 *
 * @param [in] sge_count  number of SGEs to allocate at the end of the structure
 *
 * @return pointer to new allocated struct or NULL in case of failure
 */
static inline
dbBE_Request_t* dbBE_Request_allocate( const int sge_count )
{
  return (dbBE_Request_t*)calloc( 1, sizeof( dbBE_Request_t) + sge_count * sizeof( dbBE_sge_t ) );
}


static inline
ssize_t dbBE_Request_serialize(const dbBE_Request_t *req, char *data, size_t space )
{
  static const size_t dbBE_REQUEST_MIN_SPACE = 20; // at least 10 items with single digit size + separator
  if(( req == NULL ) || ( data == NULL ) || ( space < dbBE_REQUEST_MIN_SPACE))
    return -EINVAL;

  ssize_t total = 0;
  DBR_Tuple_name_t key = req->_key;

  // todo: this is ugly because the iterator ptr is forced into the key. Therefore we need to pre-serialize it into a string here.
  if( req->_opcode == DBBE_OPCODE_ITERATOR )
  {
    // the sge points to space for a full key,
    // we temporarily use this space for the ptr serialization
    // to avoid an extra malloc/free
    key = (DBR_Tuple_name_t)req->_sge[0].iov_base;
    if( key == NULL )
      return -ENOMEM;
    if( snprintf( key, sizeof( uintptr_t ) + 1, "%p", (void*)req->_key ) < (int)sizeof( uintptr_t ) )
      return -EBADMSG;
  }

  total = snprintf( data, space, "%d\n%p\n%p\n%p\n%p\n%ld\n%ld\n%"PRId64"\n%s%s\n",
            req->_opcode,
            req->_ns_hdl,
            req->_user,
            req->_next,
            req->_group,
            key != NULL ? strnlen(key, DBR_MAX_KEY_LEN) : 0,
            req->_match != NULL ? strnlen(req->_match, DBR_MAX_KEY_LEN) : 0,
            req->_flags,
            key,
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
    case DBBE_OPCODE_NSQUERY:
    case DBBE_OPCODE_ITERATOR:
      sge_total = dbBE_SGE_serialize_header( req->_sge, req->_sge_count, data, space );
      break;
    case DBBE_OPCODE_PUT:
      sge_total = dbBE_SGE_serialize( req->_sge, req->_sge_count, data, space );
      break;
    case DBBE_OPCODE_MOVE:
      // param[in]      dbBE_sge_t           _sge[0] = contains destination storage group
      // param[in]      dbBE_sge_t           _sge[1] = valid @ref dbBE_NS_Handle_t to destination namespace
      sge_total = snprintf( data, space, "%p\n%p\n", req->_sge[0].iov_base, req->_sge[1].iov_base );
      break;
    case DBBE_OPCODE_REMOVE:
    case DBBE_OPCODE_CANCEL:
    case DBBE_OPCODE_NSCREATE:
    case DBBE_OPCODE_NSATTACH:
    case DBBE_OPCODE_NSDETACH:
    case DBBE_OPCODE_NSDELETE:
      break;
    case DBBE_OPCODE_DIRECTORY:
      sge_total = snprintf( data, space, "%"PRId64"\n%d", req->_sge[0].iov_len, req->_sge_count );
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
ssize_t dbBE_Request_deserialize( char *data, size_t space, dbBE_Request_t **request )
{
  if(( data == NULL ) || ( space == 0 ) || ( request == NULL ))
    return -EINVAL;

  int parsed;
  ssize_t total = 0;

  dbBE_Opcode opcode;
  dbBE_NS_Handle_t ns_hdl, dsthdl;
  void *user;
  dbBE_Request_t *next;
  DBR_Group_t group, dstgroup;
  unsigned keylen, matchlen;
  int64_t flags;
  int sge_count;
  dbBE_sge_t *sge_out;

  int items = sscanf( data, "%d\n%p\n%p\n%p\n%p\n%d\n%d\n%"PRId64"\n%n",
            (int*)&opcode,
            &ns_hdl,
            &user,
            &next,
            &group,
            &keylen,
            &matchlen,
            &flags,
            &parsed );
  if( items < 0 )
    return -EBADMSG;

  if( items < 8 )
    return -EAGAIN;

  if(( opcode >= DBBE_OPCODE_MAX ) || ( flags >= DBR_FLAGS_MAX ))
    return -EBADMSG;

  if(( keylen > DBR_MAX_KEY_LEN ) || ( matchlen > DBR_MAX_KEY_LEN ))
    return -EBADMSG;

  data += parsed;
  space -= parsed;
  total += parsed;
  if( space < keylen )
    return -EAGAIN;

  char *key = (char*)malloc( keylen + 1 );
  if( key == NULL )
    return -ENOMEM;
  strncpy( key, data, keylen );
  key[keylen] = '\0';

  data += keylen;
  space -= keylen;
  total += keylen;
  if( space < matchlen )
    return -EAGAIN;

  char *match = (char*)malloc( matchlen + 1 );
  if( match == NULL )
    return -ENOMEM;
  strncpy( match, data, matchlen );
  match[matchlen] = '\0';

  matchlen += 1;  // trailed by \n
  data += matchlen;
  space -= matchlen;
  total += matchlen;

  if( space < 1 )
    return -EAGAIN;

  ssize_t sge_total = 0;
  switch( opcode )
  {
    case DBBE_OPCODE_GET:
    case DBBE_OPCODE_READ:
    case DBBE_OPCODE_NSQUERY:
    case DBBE_OPCODE_DIRECTORY:
    case DBBE_OPCODE_ITERATOR:
      sge_count = dbBE_SGE_extract_header( NULL, 0, data, space, &sge_out, (size_t*)&sge_total );
      if( sge_count < 0 )
        sge_total = sge_count;
      break;
    case DBBE_OPCODE_PUT:
      sge_total = dbBE_SGE_deserialize( NULL, 0, data, space, &sge_out, &sge_count );
      break;
    case DBBE_OPCODE_MOVE:
      // param[in]      dbBE_sge_t           _sge[0] = contains destination storage group
      // param[in]      dbBE_sge_t           _sge[1] = valid @ref dbBE_NS_Handle_t to destination namespace
    {
      int parsed;
      sge_count = sscanf( data, "%p\n%p\n%n", &dsthdl, &dstgroup, &parsed );
      if( sge_count < 2 )
        return -EBADMSG;
      sge_total = parsed;
      break;
    }
    case DBBE_OPCODE_REMOVE:
    case DBBE_OPCODE_CANCEL:
    case DBBE_OPCODE_NSCREATE:
    case DBBE_OPCODE_NSATTACH:
    case DBBE_OPCODE_NSDETACH:
    case DBBE_OPCODE_NSDELETE:
      break;
    default:
      return -EBADMSG;
  }

  if( sge_total < 0 )
    return -EBADMSG;

  total += sge_total + 1;

  dbBE_Request_t *req = dbBE_Request_allocate( sge_count );
  req->_opcode = opcode;
  req->_ns_hdl = ns_hdl;
  req->_user = user;
  req->_next = next;
  req->_group = group;
  req->_key = key;
  req->_match = match;
  req->_flags = flags;
  req->_sge_count = sge_count;
  memcpy( req->_sge, sge_out, sizeof(dbBE_sge_t) * sge_count );

  if( opcode == DBBE_OPCODE_ITERATOR )
  {
    sscanf( key, "%p\n", &req->_key );
    free( key );
  }

  *request = req;
  return total;
}

#endif /* BACKEND_COMMON_REQUEST_H_ */
