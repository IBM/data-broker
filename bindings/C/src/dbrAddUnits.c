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
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include "libdbrAPI.h"
#include "libdatabroker_int.h"

DBR_Errorcode_t
dbrAddUnits( DBR_Handle_t cs_handle,
             DBR_UnitList_t cs_units )
{
  int64_t meta_size = 0;
  dbBE_sge_t* meta = NULL;
  if( cs_units ) {
    for(; cs_units[meta_size] != NULL && meta_size <= 1024; ++meta_size) // ToDo: define a check for max.!
      ;
    ++meta_size; // needs to account for NULL as well!
    meta = (dbBE_sge_t*) calloc ( meta_size, sizeof(dbBE_sge_t) );
    int64_t i;
    for( i = 0; i < meta_size - 1; i++ ) {
      meta[i].iov_len = strlen( cs_units[i] ) + 1; // ToDo: depends on type!
      meta[i]._data = cs_units[i];
    }
    meta[meta_size - 1].iov_len = 0;
    meta[meta_size - 1]._data = NULL;
  }

  DBR_Errorcode_t hdl = libdbrAddUnits( cs_handle, meta_size, meta );

  if( meta ) {
    memset(meta, 0, sizeof(dbBE_sge_t) * meta_size);
    free( meta );
  }

  return hdl;
}
