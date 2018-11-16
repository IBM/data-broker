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

#ifndef INCLUDE_LIBDATABROKER_EXTRAS_H_
#define INCLUDE_LIBDATABROKER_EXTRAS_H_


#include "errorcodes.h"
#include "libdatabroker.h"

#include <inttypes.h>
#include <stddef.h>


/**
 * @brief Insert a scattered tuple in a namespace.
 *
 * The function allows the user to insert new data to a namespace.  This
 * requires a valid namespace handle, a tuple name and a list of pointers and
 * sizes to specify memory region containing the tuple elements to gather into the
 * tuple.
 *
 * The function is blocking and it will return when the tuple is stored in the namespace.
 * If there exists already a tuple with the same name, the current tuple data will be added to the namespace
 * in FIFO order.
 *
 * @param [in] dbr_handle   Handle to the namespace.
 * @param [in] va_ptr Pointer array to the tuple elements.
 * @param [in] size Size array of the tuple elements.
 * @param [in] len Number of entries in the va_ptr and size arrays
 * @param [in] tuple_name Name/key identifying the tuple.
 * @param [in] group Group to which the namespace belongs.
 *
 * @return
 *    - DBR_SUCCESS if the insertion is completed successfully;
 *    - DBR_ERR_TIMEOUT if the insertion does not complete before the timeout occurs;
 *    - An error code identifying the issue encountered, otherwise.
 *
 * @see DBR_Errorcode_t
 *
 */
DBR_Errorcode_t dbrPut_gather( DBR_Handle_t dbr_handle,
                               void *va_ptr[],
                               int64_t size[],
                               int len,
                               DBR_Tuple_name_t tuple_name,
                               DBR_Group_t group );




#endif /* INCLUDE_LIBDATABROKER_EXTRAS_H_ */
