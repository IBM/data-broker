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

DBR_Errorcode_t
dbrTestKey(DBR_Handle_t cs_handle,
           DBR_Tuple_name_t tuple_name )
{
  DBR_Tuple_template_t match_template = "";
  DBR_Group_t group = DBR_GROUP_LIST_EMPTY;

  /* for now, we just map this to a read that doesn't wait for timeouts and ignores the returned value */
  DBR_Errorcode_t rc = libdbrTestKey( cs_handle,
                                      tuple_name,
                                      match_template,
                                      group );

  return rc;
}

