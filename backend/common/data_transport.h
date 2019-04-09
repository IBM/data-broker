/*
 * Copyright © 2018,2019 IBM Corporation
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

#ifndef BACKEND_COMMON_DATA_TRANSPORT_H_
#define BACKEND_COMMON_DATA_TRANSPORT_H_

/**
 * @addtogroup be_common
 *
 * This group includes the top-level definitions and data types of
 * the back-end API.
 *
 * @{
 */

#include <stddef.h>
#include <inttypes.h>

#include "dbbe_api.h"


/**
 * @typedef dbBE_Data_transport_device_t
 * @brief   generalized ptr to a device structure to handle data transport
 *
 * A transport device handle to hold information about the transport
 * facility. This will be used by the back-end modules to copy or transfer
 * data from and to user memory regions specified in lists of scatter-gather
 * elements. In the simplest case this is just the pointer to a buffer.
 * In more complex cases it can be a socket or a data structure that holds
 * required data to access and transfer data from and to a GPU or other
 * accelerator.
 *
 */
typedef void* dbBE_Data_transport_device_t;

/**
 * @struct dbBE_Data_transport data_transport.h backend/common/data_transport.h
 * @brief  Data transport API
 *
 * A data transport is collecting or splitting data into or from a contiguous
 * buffer based on the SGE parameters.
 * It can implement various data transports for special devices or hw features
 *
 * @note It doesn't care about buffer allocations or destruction!
 */
typedef struct dbBE_Data_transport
{
  /**
   * @brief create and initialize a transport device
   *
   */
  dbBE_Data_transport_device_t* (*create)( void );

  /**
   * @brief destroy a transport device
   */
  int (*destroy)( dbBE_Data_transport_device_t *);

  /**
   * @brief  data gathering function
   *
   * gather operation to collect SGE data from the user memory
   * the collection size is limited by the size parameter
   *
   * @param [in] device     ptr to the device information to handle the gather process
   * @param [in] size       amount of space (bytes) at the destination
   * @param [in] sge_count  number of elements in the SGE list
   * @param [in] SGE_list   scatter-gather list
   *
   * @return 0 on success and non-zero on error, indicating the condition
   *
   */
  int64_t (*gather)( dbBE_Data_transport_device_t*, // destination device definition
      size_t,  // destination space limit
      int,     // number of SG elements
      dbBE_sge_t* );  // SGE definitions


  /**
   * @brief data scattering function
   *
   * scatter operation to extract data from a single buffer or device
   * to the SGE locations specified by the user
   * the scattering is limited by the sizes of the defined SGEs and
   * the amount of input data.
   *
   * @param [in] device     ptr to the device information to handle the gather process
   * @param [in] size       amount of available source data
   * @param [in] sge_count  number of elements in the SGE list
   * @param [in] SGE_list   scatter-gather list
   *
   * @return 0 on success and non-zero on error, indicating the condition
   */
  int64_t (*scatter)( dbBE_Data_transport_device_t*,   // source device definition
      size_t,  // source space limit
      int,     // number of SG elements
      dbBE_sge_t* );  // SGE definitions

} dbBE_Data_transport_t;

/**
 *@}
 */

#include "transports/memcopy.h"
#include "transports/small_buffer_socket.h"

#endif /* BACKEND_COMMON_DATA_TRANSPORT_H_ */
