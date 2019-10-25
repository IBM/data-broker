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
#include "transports/sge_buffer.h"



/**
 * @typedef dbBE_Data_transport_endpoint_t
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
typedef void* dbBE_Data_transport_endpoint_t;


/**
 * @typedef  dbBE_Data_send_cb
 * @brief    callback function required to be provided by a transport endpoint
 *
 * This callback will be called when gather() is called.
 * Depending on the transport implementation, the callback is required to
 * provide SGE handling capabilities based on the input SGE.
 */
typedef ssize_t (*dbBE_Data_send_cb)( dbBE_Data_transport_endpoint_t*, dbBE_Transport_sge_buffer_t* );

/**
 * @typedef dbBE_Data_recv_cb
 * @brief   callback function required to be provided by a transport endpoint
 *
 * This callback will be called when scatter() is called and the available input
 * suggests that only partial data has been received previously.
 * Depending on the transport implementation, the callback is required to
 * provide SGE handling capabilities based on the input SGE.
 *
 * @param[in]  void*    Transport endpoint like (connection, membuffer, queue pair, etc)
 * @param[in]  sge*     destination SGE where to place the data
 * @param[in]  count    number of destination SGEs
 */
typedef ssize_t (*dbBE_Data_recv_cb)( dbBE_Data_transport_endpoint_t*, dbBE_Transport_sge_buffer_t* );

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
  dbBE_Transport_sge_buffer_t _sSGE;  // transport-owned send SGE; use as pre-allocated SGE at your service
  dbBE_Transport_sge_buffer_t _rSGE;  // transport-owned recv SGE; use as pre-allocated SGE at your service

  size_t _recv_buffer_len; ///< default/recommended size of buffer for data retrieval
  size_t _send_buffer_len; ///< default/recommended size of buffer for sending data

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
  int64_t (*gather)( dbBE_Data_transport_endpoint_t*, // destination device definition
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
   * The device is required to provide a recv() call that supports SGEs.
   * The recv() call is called to retrieve potentially not-yet-received data
   *
   * @param [in] device     ptr to the device information to handle the gather process (requires a recv() callback that supports SGE)
   * @param [in] recv_cb    callback function of the device to retrieve additional data (requires support for SGEs)
   * @param [in] size       total amount of available source data
   * @param [in] sge_count  number of elements in the user/destination SGE list
   * @param [in] SGE_list   user/destination scatter-gather list
   *
   * @return 0 on success and non-zero on error, indicating the condition
   */
  int64_t (*scatter)( dbBE_Data_transport_endpoint_t*, // device (connection, buffer, queue pair, ...) to use for the callback
      dbBE_Data_recv_cb,   // callback function to use for receiving more data (SGE capability)
      dbBE_sge_t*, // (single sge) partially existing data (not required to get from transport device
      size_t,  // total amount of expected source data
      int,     // number of destination SG elements
      dbBE_sge_t* );  // target/user SGE definitions

} dbBE_Data_transport_t;

/**
 *@}
 */

#include "transports/memcopy.h"
#include "transports/smallcopy.h"

#endif /* BACKEND_COMMON_DATA_TRANSPORT_H_ */
