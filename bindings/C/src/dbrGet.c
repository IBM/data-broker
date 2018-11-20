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
#include "libdbrAPI.h"
#include "libdatabroker_int.h"

DBR_Errorcode_t
dbrGet (DBR_Handle_t cs_handle,
        void *va_ptr,
        int64_t *size,
        DBR_Tuple_name_t tuple_name,
        DBR_Tuple_template_t match_template,
        DBR_Group_t group,
        int flags )
{
  dbBE_sge_t sge;
  sge.iov_base = va_ptr;
  sge.iov_len = *size;

  return libdbrGet( cs_handle,
                    &sge,
                    1,
                    size,
                    tuple_name,
                    match_template,
                    group,
                    (flags & DBR_FLAGS_NOWAIT) ? 0 : 1 );
}

