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

#ifndef SRC_LIBDBRAPI_H_
#define SRC_LIBDBRAPI_H_

#include "libdatabroker.h"
#include "../backend/common/dbbe_api.h"


DBR_Handle_t
libdbrCreate( DBR_Name_t db_name,
              DBR_Tuple_persist_level_t level,
              unsigned int sgec,
              dbBE_sge_t* sgev );

DBR_Errorcode_t
libdbrDelete( DBR_Name_t db_name );

DBR_Handle_t
libdbrAttach( DBR_Name_t db_name );

DBR_Errorcode_t
libdbrDetach( DBR_Handle_t cs_handle );

DBR_Errorcode_t
libdbrQuery (DBR_Handle_t cs_handle,
             DBR_State_t *cs_state,
             DBR_State_mask_t cs_state_mask );

DBR_Errorcode_t
libdbrAddUnits( DBR_Handle_t cs_handle,
                unsigned int sgec,
                dbBE_sge_t* sgev );

DBR_Errorcode_t
libdbrRemoveUnits( DBR_Handle_t cs_handle,
                   unsigned int sgec,
                   dbBE_sge_t* sgev );




/*
 * data broker data access functions to insert and retrieve data items
 */

DBR_Errorcode_t
libdbrPut( DBR_Handle_t cs_handle,
           void *va_ptr,
           int64_t size,
           DBR_Tuple_name_t tuple_name,
           DBR_Group_t group );

DBR_Tag_t
libdbrPutA( DBR_Handle_t cs_handle,
            void *va_ptr,
            int64_t size,
            DBR_Tuple_name_t tuple_name,
            DBR_Group_t group );

DBR_Errorcode_t
libdbrGet( DBR_Handle_t cs_handle,
           void *va_ptr,
           int64_t size,
           int64_t *ret_size,
           DBR_Tuple_name_t tuple_name,
           DBR_Tuple_template_t match_template,
           DBR_Group_t group,
           int enable_timeout );

DBR_Tag_t
libdbrGetA( DBR_Handle_t cs_handle,
            void *va_ptr,
            int64_t size,
            int64_t *ret_size,
            DBR_Tuple_name_t tuple_name,
            DBR_Tuple_template_t match_template,
            DBR_Group_t group );

DBR_Errorcode_t
libdbrRead( DBR_Handle_t cs_handle,
            void *va_ptr,
            int64_t size,
            int64_t *ret_size,
            DBR_Tuple_name_t tuple_name,
            DBR_Tuple_template_t match_template,
            DBR_Group_t group,
            int enable_timeout );

DBR_Tag_t
libdbrReadA( DBR_Handle_t cs_handle,
             void *va_ptr,
             int64_t size,
             int64_t *ret_size,
             DBR_Tuple_name_t tuple_name,
             DBR_Tuple_template_t match_template,
             DBR_Group_t group );

DBR_Errorcode_t
libdbrMove( DBR_Handle_t src_cs_handle,
            DBR_Group_t src_group,
            DBR_Tuple_name_t tuple_name,
            DBR_Tuple_template_t match_template,
            DBR_Handle_t dest_cs_handle,
            DBR_Group_t dest_group );

DBR_Errorcode_t
libdbrRemove( DBR_Handle_t cs_handle,
              DBR_Group_t group,
              DBR_Tuple_name_t tuple_name,
              DBR_Tuple_template_t match_template );


/*
 * data broker request handling functions
 * to test for completion or cancel non-blocking requests
 */

DBR_Errorcode_t
libdbrTest( DBR_Tag_t req_tag );

DBR_Errorcode_t
libdbrCancel( DBR_Tag_t req_tag );


#endif /* SRC_LIBDBRAPI_H_ */
