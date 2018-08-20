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

#include "libdatabroker.h"
#include "libdbrAPI.h"


#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>
#endif

#define DBR_TMP_BUFFER_LEN ( 128 * 1024 * 1024 )
static void *dbr_tmp_buffer = NULL;


DBR_Errorcode_t
dbrTestKey(DBR_Handle_t cs_handle,
           DBR_Tuple_name_t tuple_name )
{
  if( dbr_tmp_buffer == NULL )
    dbr_tmp_buffer = malloc( DBR_TMP_BUFFER_LEN );

  DBR_Tuple_template_t match_template = "";
  DBR_Group_t group = DBR_GROUP_LIST_EMPTY;

  int64_t len = DBR_TMP_BUFFER_LEN;

  /* for now, we just map this to a read that doesn't wait for timeouts and ignores the returned value */
  return libdbrRead( cs_handle,
                     dbr_tmp_buffer,
                     len,
                     &len,
                     tuple_name,
                     match_template,
                     group,
                     0 );
}

