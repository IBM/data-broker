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

#ifndef BACKEND_INTERFACE_DBBE_API_H_
#define BACKEND_INTERFACE_DBBE_API_H_

/**
 * @defgroup be_common  Basic/Common Back-end
 *
 * This group includes the top-level definitions and data types of
 * the back-end API.
 *
 * @{
 */

#include <inttypes.h>
#include <stddef.h> // size_t
#include <libdatabroker.h>

/**
 * @typedef dbBE_Handle_t
 * @brief   Handle to the back-end-specific structure
 *
 * Defines a general handle that the client lib can use to perform
 * back-end API calls and interaction.
 * The handle is initialized by the back-end-specific implementation
 * if initialize() and destroyed by exit()
 */
typedef void* dbBE_Handle_t;

/**
 *  @struct dbBE_sge_t dbbe_api.h "backend/common/dbbe_api.h"
 *
 *  @brief Scatter-Gather-Element structure
 *
 *  The scatter-gather-element defines one entry in a scatter-gather list
 *  and specifies one contiguous memory region in main memory.
 */
typedef struct
{
  size_t _size; /**< size of the memory region */
  void *_data;  /**< pointer to the start of the memory region */
} dbBE_sge_t;


/**
 * @brief Back-end operation code
 *
 * Specifies the available back-end commands that can be placed into a
 * request.
 */
typedef enum
{
  DBBE_OPCODE_UNSPEC, /**< Unspecified opcode, represents an uninitialized value  */
  DBBE_OPCODE_PUT, /**< PUT operation to write data into the back-end  */
  DBBE_OPCODE_GET, /**< GET operation to retrieve and delete data from the back-end  */
  DBBE_OPCODE_READ,  /**< READ operation to retrieve data and keep it in the back-end  */
  DBBE_OPCODE_MOVE,  /**< MOVE operation to migrate data from one namespace to another  */
  DBBE_OPCODE_REMOVE,  /**< REMOVE operation to delete data from the back-end  */
  DBBE_OPCODE_CANCEL,  /**< CANCEL operation to interrupt/stop cancel an pending or incomplete request  */
  DBBE_OPCODE_DIRECTORY, /**< DIRECTORY operation to retrieve a (filtered) list of existing keys */
  DBBE_OPCODE_NSCREATE,  /**< Namespace creation operation  */
  DBBE_OPCODE_NSATTACH,  /**< Namespace attach operation  */
  DBBE_OPCODE_NSDETACH,  /**< Namespace detach operation  */
  DBBE_OPCODE_NSDELETE,  /**< Namespace delete operation to wipe the name space and its data  */
  DBBE_OPCODE_NSQUERY,  /**< Namespace query operation  */
  DBBE_OPCODE_NSADDUNITS,  /**< Namespace add units to increase the number of backing storage nodes of a namespace  */
  DBBE_OPCODE_NSREMOVEUNITS, /**< Namespace remove units to decrease the number of backing storage nodes of a namespace */
  DBBE_OPCODE_MAX  /**< Non-implemented operation to simplify range checks for opcodes  */
} dbBE_Opcode;

/**
 * @struct dbBE_Request dbbe_api.h "backend/common/dbbe_api.h"
 *
 * @brief Back-end API request structure
 *
 * Defines a back-end request with operation code as well an input and output parameters
 */
typedef struct dbBE_Request
{
  dbBE_Opcode _opcode;         /**< operation code */
  DBR_Name_t _ns_name;         /**< namespace name */
  void *_user;                 /**< upper layer data, will not be touched by system lib */
  struct dbBE_Request *_next;  /**< next pointer for chaining */
  DBR_Group_t _group;          /**< group */
  DBR_Tuple_name_t _key;       /**< key/tuple name */
  DBR_Tuple_template_t _match; /**< match template */
  int _sge_count;              /**< number of sge's */
  dbBE_sge_t _sge[];           /**< SGE's */
} dbBE_Request_t;

/**
 * @typedef dbBE_Request_handle_t
 * @brief Back-end request handle
 */
typedef void* dbBE_Request_handle_t;

/**
 * @struct dbBE_Completion dbbe_api.h "backend/common/dbbe_api.h"
 *
 * @brief Completion data structure
 *
 */
typedef struct dbBE_Completion
{
  DBR_Errorcode_t _status; /**< the request status indicator */
  void *_user; /**< pointer to upper layer data, whatever the upper layer placed into the request */
  int64_t _rc;  /**< return code; can be size, error codes, etc. */
  struct dbBE_Completion *_next; /** next pointer for chaining */
} dbBE_Completion_t;

/**
 * @struct dbBE_api dbbe_api.h "backend/common/dbbe_api.h"
 *
 * @brief Back-end API definition/spec
 *
 * This struct has to be implemented by each back-end to provide the necessary functionality
 */
typedef struct dbBE_api
{
  /**
   * @brief initialize the system library
   *
   * Function needs to perform any initialization that in necessary to
   * support the other functions of the API.
   *
   * @return handle that points to successfully initialized back-end
   *         NULL if the initialization failed
   */
  dbBE_Handle_t (*initialize)(void);

  /**
   * @brief destroy/exit the system library
   *
   * This call needs to destroy any initialized structures of the back-end
   * and do proper clean-up.
   *
   * @param [in]  handle  initialized back-end handle
   *
   * @return 0 if successful. Error code otherwise
   */
  int (*exit)( dbBE_Handle_t );

  /**
   * @brief post a new request to the back-end
   *
   * The upper layer/client library uses this call to post a new request to the back-end.
   * It's the implementations responsibility to either process or queue the request
   * in a way that allows the user process to continue without requiring to wait for
   * completion of the request.
   *
   * @param [in] handle  initialized back-end handle
   * @param [in] request pointer to the request structure with the request definition
   *
   * @return request handle that allows the user to check the status or cancel the request later
   *         or NULL in case of an error
   */
  dbBE_Request_handle_t (*post)( dbBE_Handle_t, dbBE_Request_t* );

  /**
   * @brief cancel a request
   *
   * Allows the user to cancel a pending request by providing the request handle.
   * The system library implementation is required to clean up the status of the request
   * and make sure there are no side-effects of the cancellation.
   *
   * @param [in] back-end handle  pointing to an initialized back-end
   * @param [in] request handle   the handle of the request to cancel
   *
   * @return 0 on success, error code otherwise
   *
   * @todo Requests that require multi-stage processing in the BE might cause
   *       inconsistencies when the request is cancelled. Should we specify the
   *       semantics of cancellation in a way that the lib continues to create a
   *       consistent state after cancellation? Or should the lib try to restore
   *       the state from before the cancellation? Or should we just try to stop
   *       the processing as soon as possible and leave whatever is currently in
   *       the back-end?
   */
  int (*cancel)( dbBE_Handle_t, dbBE_Request_handle_t );

  /**
   * @brief test for completion of a particular posted request
   *
   * @param [in] back-end handle  pointing to an initialized back-end
   * @param [in] request handle   the handle of the request to test
   *
   * @return pointer to completion with status or NULL in case of error
   */
  dbBE_Completion_t* (*test)( dbBE_Handle_t, dbBE_Request_handle_t );

  /**
   * @brief test for any completed request
   *
   * Fetch the first completed request from the completion queue
   * or return NULL if nothing is completed since the last call.
   *
   * @param [in] back-end handle  pointing to an initialized back-end
   *
   * @return pointer to a completion or NULL if no request is complete
   */
  dbBE_Completion_t* (*test_any)( dbBE_Handle_t );
} dbBE_api_t;


/**
 * @brief calculate the total size of an SGE list
 *
 * Browses the SGEs of an input list and returns the total size in bytes.
 *
 * @param [in] sge       pointer to the first entry of an SGE list (array)
 * @param [in] sge_count number of elements in the SGE list
 *
 * @return size of the SGE in bytes
 */
static inline size_t dbBE_SGE_get_len( const dbBE_sge_t *sge, const int sge_count )
{
  if( sge == NULL )
    return 0;
  int i;
  size_t len = 0;
  for( i = sge_count-1; i>=0; --i )
    len += sge[ i ]._size;
  return len;
}

/**
 * @brief Global backend data
 *
 * Each back-end needs to implement this structure. It is used by the client library
 * to reference the back-end
 *
 * @todo Considering that each user lib has a global context already, we could probably
 *       get rid if this g_dbBE and have the user lib keep track. This would enable
 *       an option to use multiple back-ends concurrently.
 */
extern const dbBE_api_t g_dbBE;

/**
 *@}
 */

#endif /* BACKEND_INTERFACE_DBBE_API_H_ */
