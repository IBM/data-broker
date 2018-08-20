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

DBR_Errorcode_t
dbrQuery (DBR_Handle_t cs_handle,
          DBR_State_t *cs_state,
          DBR_State_mask_t cs_state_mask )
{
  return libdbrQuery( cs_handle, cs_state, cs_state_mask );
}
