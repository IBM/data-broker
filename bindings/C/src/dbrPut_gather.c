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

#include "errorcodes.h"
#include "libdbrAPI.h"
#include "libdatabroker_ext.h"

#include <stdlib.h>

DBR_Errorcode_t
dbrPut_gather (DBR_Handle_t cs_handle,
        void **va_ptr,
        int64_t *size,
        int len,
        DBR_Tuple_name_t tuple_name,
        DBR_Group_t group)
{
  if(( len <= 0 ) || (va_ptr == NULL) || (size==NULL))
    return DBR_ERR_INVALID;


  dbBE_sge_t *sge = (dbBE_sge_t*)calloc( len, sizeof( dbBE_sge_t ) );
  if( sge == NULL )
    return DBR_ERR_NOMEMORY;

  int n;
  for( n=0; n<len; ++n )
  {
    sge[ n ]._data = (void*)va_ptr[ n ];
    sge[ n ].iov_len = size[ n ];
  }

  return libdbrPut( cs_handle,
                    sge,
                    len,
                    tuple_name,
                    group );
}

