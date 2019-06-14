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
#include "libdbrAPI.h"
#include "libdatabroker_int.h"

#include <stdlib.h>

DBR_Errorcode_t
dbrGet (DBR_Handle_t cs_handle,
        void *va_ptr,
        int64_t *size,
        DBR_Tuple_name_t tuple_name,
        DBR_Tuple_template_t match_template,
        DBR_Group_t group,
        int flags )
{
  dbrDA_Request_chain_t *req = (dbrDA_Request_chain_t*)calloc( 1, sizeof( dbrDA_Request_chain_t ) + sizeof( dbBE_sge_t ) );;
  req->_key = tuple_name;
  req->_size = *size;
  req->_sge_count = 1;
  req->_value_sge[0].iov_base = va_ptr;
  req->_value_sge[0].iov_len = *size;

  DBR_Errorcode_t rc;
  rc = libdbrGet( cs_handle,
                  req,
                  size,
                  match_template,
                  group,
                  (flags & DBR_FLAGS_NOWAIT) ? 0 : 1 );
  free( req );
  return rc;
}


DBR_Tag_t
dbrGetA (DBR_Handle_t cs_handle,
         void *va_ptr,
         int64_t *size,
         DBR_Tuple_name_t tuple_name,
         DBR_Tuple_template_t match_template,
         DBR_Group_t group)
{
  return libdbrGetA( cs_handle,
                     va_ptr,
                     *size,
                     size,
                     tuple_name,
                     match_template,
                     group );
}
