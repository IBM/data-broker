/*
 * Copyright © 2019 IBM Corporation
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

#ifndef BINDINGS_DATA_ADAPTERS_DBRDA_API_H_
#define BINDINGS_DATA_ADAPTERS_DBRDA_API_H_

#include <errorcodes.h>
#include <libdatabroker.h>

#include <sys/uio.h>

#define DBR_PLUGIN_ENV ("DBR_PLUGIN")


/**
 * @typedef dbrDA_Handle_t
 * @brief   Handle to the data adapter-specific structure
 *
 * Defines a general handle that the client lib can use to perform
 * data adapter API calls and interaction.
 */
typedef void* dbrDA_Handle_t;


typedef struct dbrDA_Request_chain
{
  struct dbrDA_Request_chain *_next;  ///< allows chaining of requests
  DBR_Tuple_name_t _key;              ///< tuple_name/key
  int64_t _size;                      ///< total number of bytes for this request (used for read/get)
  int _sge_count;                     ///< number of SGEs in this request (if any)
  struct iovec _value_sge[];          ///< variable size, needs to be last entry !!!
} dbrDA_Request_chain_t;


typedef struct dbrDA_api
{
  dbrDA_Handle_t (*initialize)(void);
  int (*exit)( dbrDA_Handle_t );

  /**
   * pre-processing functions for read and write path
   * they expect the request handle and will potentially modify the request directly
   * if the alternative buffer is provided as non-NULL, then any modified data will
   * be stored in that buffer
   * pre-processing happens before handing the request over to the back-end
   */
  /**
   * pre-write:
   *  Allows to convert/modify the input tuple name (key) and SGE and
   *  returns one key/value request or a chain for the backend to process
   */
  dbrDA_Request_chain_t* (*pre_write)( dbrDA_Request_chain_t* );

  /**
   * pre-read:
   *  Allows to manipulate the input key or initial data destination and
   *  returns a key/value request or a chain for the backend to process
   */
  dbrDA_Request_chain_t* (*pre_read)( dbrDA_Request_chain_t* );

  /**
   * post-processing functions for read and write path
   * thex expect the request handle and will potentially modify the request directly
   * if the alternative buffer is provided as non-NULL, then any modified data will
   * be stored in that buffer
   * post-processing happens after successful completion of the request by the back-end
   */
  /**
   * post-write:
   *  Allows to manipulate the key/value or results of the request chain processing
   *  e.g. combine the error codes in case the initial request was split
   *  it also receives the error code of the data broker processing
   */
  DBR_Errorcode_t (*post_write)( dbrDA_Request_chain_t*, DBR_Errorcode_t );

  /**
   * post-read:
   *  Allows to convert/modify the returned data in-place or based on temporary buffers
   *  created in pre-read and/or (re-)combine the results of multiple requests into one
   *  it also receives the error code of the data broker processing,
   *  the implementation needs to set the correct total data size for the user application
   *  in the original request chain
   */
  DBR_Errorcode_t (*post_read)( dbrDA_Request_chain_t*,  dbrDA_Request_chain_t*, DBR_Errorcode_t );

} dbrDA_api_t;

extern dbrDA_api_t dbrDA;

#endif /* BINDINGS_DATA_ADAPTERS_DBRDA_API_H_ */
