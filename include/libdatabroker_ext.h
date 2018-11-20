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
 * @brief Insert a tuple in a namespace by gathering from non-contiguous memory locations.
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




/**
 * @brief Get data from a namespace and scatter it into tuples.
 *
 * The function allows the user to get and consume a tuple from a
 * namespace, thus, the tuple will be deleted from the namespace.
 * If there exist multiple tuples with the same name,
 * then the tuple is retrieved following the order of insertion (FIFO).
 *
 * This requires a valid namespace handle, a tuple name, the pointer and
 * size of a contiguous memory region where to store the retrieved tuple data.
 * The initial value of the size is identified by the size of the gather definitions.
 * If the actual data is larger than the user buffer, data will be truncated and an
 * error is returned.
 * If the actualy data is smaller than the user buffer, data will be filled as available
 * and an error is returned
 *
 * The function is blocking and it will return when the tuple is retrieved from the namespace.
 * If the tuple does not exist yet, the call will block until a timeout is reached.
 *
 *
 * It can be customized with two different flags based on the behavior the user prefers:
 *  - DBR_FLAGS_NONE: the call blocks until a certain timeout is reached;
 *  - DBR_FLAGS_NOWAIT: the call checks whether the tuple exists or not and returns immediately if the tuple is not in the namespace.
 *
 *
 * @param [in] dbr_handle   Handle to the namespace.
 * @param [out] va_ptr    Pointer array to the tuple elements to fill
 * @param [in] size       Size array of the tuple elements.
 * @param [in] len        Number of entries in the va_ptr and size arrays
 * @param [in] tuple_name   Name/key identifying the tuple to be searched.
 * @param [in] match_template Template identifying a set of tuple names.
 * @param [in] group    Group to which the namespace belongs.
 * @param [in] flags    DBR_FLAGS_NONE or DBR_FLAGS_NOWAIT for immediate return option.
 *
 * @return
 *    - DBR_SUCCESS if the call is completed successfully;
 *    - DBR_ERR_TIMEOUT if the insertion does not complete before the timeout occurs *and* the flag is set to DBR_FLAGS_NONE;
 *    - DBR_ERR_UNAVAIL if the tuple does not exist *and* the flag is set to DBR_FLAGS_NOWAIT;
 *    - DBR_ERR_UBUFFER if the available tuple data sizes don't match the provided sizes
 *    - An error code identifying the issue encountered, otherwise.
 *
 * @see DBR_Errorcode_t
 *
 */
DBR_Errorcode_t dbrGet_scatter( DBR_Handle_t dbr_handle,
                                void *va_ptr[],
                                size_t size[],
                                int len,
                                DBR_Tuple_name_t tuple_name,
                                DBR_Tuple_template_t match_template,
                                DBR_Group_t group,
                                int flags );



#endif /* INCLUDE_LIBDATABROKER_EXTRAS_H_ */
