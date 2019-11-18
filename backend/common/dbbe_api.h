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
#include <stdlib.h> // calloc
#include <libdatabroker.h>
#include <sys/uio.h>

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
 * @typedef dbBE_NS_Handle_t
 * @brief   Handle to a namespace
 *
 * Is created and returned by create or attach and required by
 * subsequent API calls when accessing a namespace.
 */
typedef void* dbBE_NS_Handle_t;

/*
 * max number of SGEs in assembled redis commands (IOV_MAX replacement)
 */
#define DBBE_SGE_MAX ( 256 )

/**
 *  @struct dbBE_sge_t dbbe_api.h "backend/common/dbbe_api.h"
 *
 *  @brief Scatter-Gather-Element structure
 *
 *  The scatter-gather-element defines one entry in a scatter-gather list
 *  and specifies one contiguous memory region in main memory.
 */
typedef struct iovec dbBE_sge_t;

/**
 * @brief Back-end operation code
 *
 * Specifies the available back-end commands that can be placed into a
 * request.
 */
typedef enum
{
  /** @brief Unspecified opcode, represents an uninitialized value
   *
   * General specs of parameters for all opcodes:
   * *  param[out] _status returns
   *    * @ref DBR_ERR_INVALID invalid arguments
   *    * @ref DBR_ERR_HANDLE invalid namespace hdl, or namespace not attached/exists
   *    * @ref DBR_ERR_INPROGRESS request not complete; potential timeout
   *    * @ref DBR_ERR_TIMEOUT unable to complete operation within set timeout
   *    * @ref DBR_ERR_NOMEMORY insufficient amount of memory somewhere in the BE stack
   *    * @ref DBR_ERR_NOAUTH not authorized to perform this operation
   *    * @ref DBR_ERR_NOCONNECT backend is not connected to storage service
   *    * @ref DBR_ERR_CANCELLED request canceled
   *    * @ref DBR_ERR_NOTIMPL the requested backend has no PUT operation implemented
   *    * @ref DBR_ERR_BE_POST  failed to post the request at some stage in the BE stack
   *    * @ref DBR_ERR_BE_GENERAL  general error in backend
   */
  DBBE_OPCODE_UNSPEC,

  /** @brief PUT operation to write data into the back-end
   *
   * The specs of the put-request are:
   * *  param[in] _opcode = DBBE_OPCODE_PUT
   * *  param[in] @ref dbBE_NS_Handle_t     _ns_hdl a valid handle to an attached namespace
   * *  param[in]      void*                _user = pointer to anything, will be returned with completion without change
   * *  param[in] @ref dbBE_Request_t*      _next = NULL unless this is a chained request
   * *  param[in] @ref DBR_Group_t          _group = pointer or definition of source storage group
   * *  param[in] @ref DBR_Tuple_name_t     _key = pointer to string with tuple name
   * *  param[in] @ref DBR_Tuple_template_t _match = pattern to match when looking for the key
   * *  param[in]      int64_t              _flags ignored
   * *  param[in]      int                  _sge_count = number of SGEs in _sge
   * *  param[in] @ref dbBE_sge_t[]         _sge[] = SGE list pointing to (potentially non-contiguous value data)
   *
   * The specs for the put-completion are:
   * *  param[out] _status = @ref DBR_SUCCESS or error code indicating issues:
   *    * for status codes see @ref DBBE_OPCODE_UNSPEC
   * *  param[out] void*                    _user = unmodified ptr provided in request
   * *  param[out] int64_t                  _rc = number of inserted elements (i.e. 1)
   * *  param[out] @ref dbBE_Completion_t*  _next = NULL unless multiple completions are created at the same time
   */
  DBBE_OPCODE_PUT,

  /** @brief Retrieve and consume first tuple data available under tuple name (destructive read)
   *
   * The specs of the request are:
   * *  param[in] _opcode = DBBE_OPCODE_GET
   * *  param[in] @ref dbBE_NS_Handle_t     _ns_hdl a valid handle to an attached namespace
   * *  param[in]      void*                _user = pointer to anything, will be returned with completion without change
   * *  param[in] @ref dbBE_Request_t*      _next = NULL unless this is a chained request
   * *  param[in] @ref DBR_Group_t          _group = pointer or definition of source storage group
   * *  param[in] @ref DBR_Tuple_name_t     _key = pointer to string with tuple name
   * *  param[in] @ref DBR_Tuple_template_t _match = pattern to match when looking for the key
   * *  param[in]      int64_t              _flags behavior control as follows:
   *    *  @ref DBR_FLAGS_NONE               nothing
   *    *  @ref DBR_FLAGS_NOWAIT             immediately return DBR_ERR_UNAVAIL if the tuple does not exist
   *    *  @ref DBR_FLAGS_PARTIAL            no error if available data is larger than user buffer (available size is returned)
   *
   * *  param[in]      int                  _sge_count = number of SGEs in _sge
   * *  param[in] @ref dbBE_sge_t[]         _sge[] = SGE list pointing to (potentially non-contiguous value data)
   *
   * The specs for the completion are:
   * *  param[out] _status = @ref DBR_SUCCESS or error code indicating issues
   *    * @ref DBR_ERR_TIMEOUT  unable to complete operation within set timeout
   *    * @ref DBR_ERR_UBUFFER  user-provided buffer was too small (returned size contains available bytes)
   *    * for more status codes see @ref DBBE_OPCODE_UNSPEC
   * *  param[out] void*                    _user = unmodified ptr provided in request
   * *  param[out] int64_t                  _rc = size of returned (or available) value
   * *  param[out] @ref dbBE_Completion_t*  _next = NULL unless multiple completions are created at the same time
   */
  DBBE_OPCODE_GET,

  /** @brief Retrieve first tuple data available under tuple name (non-destructive read)
   *
   * The specs of the request are:
   * *  param[in] _opcode = DBBE_OPCODE_READ
   *
   * @see DBBE_OPCODE_GET
   */
  DBBE_OPCODE_READ,

  /** @brief MOVE operation to migrate data from one namespace to another
   *
   * The specs of the request are:
   * *  param[in] _opcode = DBBE_OPCODE_MOVE
   * *  param[in] @ref dbBE_NS_Handle_t     _ns_hdl = a valid handle to the source namespace
   * *  param[in]      void*                _user = pointer to anything, will be returned with completion without change
   * *  param[in] @ref dbBE_Request_t*      _next = NULL unless this is a chained request
   * *  param[in] @ref DBR_Group_t          _group = pointer or definition of source storage group
   * *  param[in] @ref DBR_Tuple_name_t     _key = pointer to string with tuple name
   * *  param[in] @ref DBR_Tuple_template_t _match = pattern to match when looking for the key
   * *  param[in]      int64_t              _flags = ignored
   * *  param[in]      int                  _sge_count = 2
   * *  param[in]      dbBE_sge_t           _sge[0] = contains destination storage group
   * *  param[in]      dbBE_sge_t           _sge[1] = valid @ref dbBE_NS_Handle_t to destination namespace
   *
   * The specs for the completion are:
   * *  param[out] _status = @ref DBR_SUCCESS or error code indicating issues
   *    * @ref DBR_ERR_EXISTS requested tuple either does not exist at source or exists at destination
   *    * @ref DBR_ERR_NOFILE an unexpected backend error while migrating data between namespaces
   *    * for more status codes see @ref DBBE_OPCODE_UNSPEC
   * *  param[out] void*                    _user = unmodified ptr provided in request
   * *  param[out] int64_t                  _rc = 0; nothing useful will be returned here
   * *  param[out] @ref dbBE_Completion_t*  _next = NULL unless multiple completions are created at the same time
   */
  DBBE_OPCODE_MOVE,

  /** @brief REMOVE operation to delete data from the back-end
   *
   * Note that this operation will remove all versions of data associated with the tuple
   *
   * The specs of the put-request are:
   * *  param[in] _opcode = DBBE_OPCODE_REMOVE
   * *  param[in] @ref dbBE_NS_Handle_t     _ns_hdl a valid handle to an attached namespace
   * *  param[in]      void*                _user = pointer to anything, will be returned with completion without change
   * *  param[in] @ref dbBE_Request_t*      _next = NULL unless this is a chained request
   * *  param[in] @ref DBR_Group_t          _group = pointer or definition of source storage group
   * *  param[in] @ref DBR_Tuple_name_t     _key = pointer to string with tuple name
   * *  param[in] @ref DBR_Tuple_template_t _match = pattern to match when looking for the key
   * *  param[in]      int64_t              _flags ignored
   * *  param[in]      int                  _sge_count = 0
   * *  param[in] @ref dbBE_sge_t[]         _sge[] = nothing
   *
   * The specs for the put-completion are:
   * *  param[out] _status = @ref DBR_SUCCESS or error code indicating issues:
   *    * for status codes see @ref DBBE_OPCODE_UNSPEC
   * *  param[out] void*                    _user = unmodified ptr provided in request
   * *  param[out] int64_t                  _rc = 0; nothing useful will be returned here
   * *  param[out] @ref dbBE_Completion_t*  _next = NULL unless multiple completions are created at the same time
   */
  DBBE_OPCODE_REMOVE,  /**< REMOVE operation to delete data from the back-end  */
  DBBE_OPCODE_CANCEL,  /**< CANCEL operation to interrupt/stop cancel an pending or incomplete request  */

  /** @brief DIRECTORY operation to retrieve a (filtered) list of existing keys
   *
   * Depending on user-provided amount of memory, this operation might not be able to return
   * the full list of available keys because subsequent calls would start the list from
   * the beginning again. Think of it like an 'ls -1 | head -n space' on a huge directory
   *
   * The specs of the put-request are:
   * *  param[in] _opcode = DBBE_OPCODE_DIRECTORY
   * *  param[in] @ref dbBE_NS_Handle_t     _ns_hdl a valid handle to an attached namespace
   * *  param[in]      void*                _user = pointer to anything, will be returned with completion without change
   * *  param[in] @ref dbBE_Request_t*      _next = NULL unless this is a chained request
   * *  param[in] @ref DBR_Group_t          _group = pointer or definition of source storage group
   * *  param[in] @ref DBR_Tuple_name_t     _key = pointer to string with tuple name
   * *  param[in] @ref DBR_Tuple_template_t _match = pattern to match when looking for the key
   * *  param[in]      int64_t              _flags ignored
   * *  param[in]      int                  _sge_count = 1
   * *  param[in] @ref dbBE_sge_t[]         _sge[] = memory region to receive a comma-separated list of available tuple names
   *
   * The specs for the put-completion are:
   * *  param[out] _status = @ref DBR_SUCCESS or error code indicating issues:
   *    * @ref DBR_ERR_ITERATOR              an error occurred while scanning the key space
   *    * for status codes see @ref DBBE_OPCODE_UNSPEC
   * *  param[out] void*                    _user = unmodified ptr provided in request
   * *  param[out] int64_t                  _rc = number of bytes placed into request._sge[]
   * *  param[out] @ref dbBE_Completion_t*  _next = NULL unless multiple completions are created at the same time
   */
  DBBE_OPCODE_DIRECTORY,

  /** @brief Namespace creation operation
   *
   * The specs of the put-request are:
   * *  param[in] _opcode = DBBE_OPCODE_NSCREATE
   * *  param[in] @ref dbBE_NS_Handle_t     _ns_hdl=NULL (will be created)
   * *  param[in]      void*                _user = pointer to anything, will be returned with completion without change
   * *  param[in] @ref dbBE_Request_t*      _next = NULL unless this is a chained request
   * *  param[in] @ref DBR_Group_t          _group = pointer or definition of storage group
   * *  param[in] @ref DBR_Tuple_name_t     _key = pointer to name of new namespace
   * *  param[in] @ref DBR_Tuple_template_t _match = NULL (ignored)
   * *  param[in]      int64_t              _flags ignored
   * *  param[in]      int                  _sge_count = 0 (potentially used for grouplist spec if more than single storage group used)
   * *  param[in] @ref dbBE_sge_t[]         _sge[] = empty (potentially used for grouplist spec if more than single storage group used)
   *
   * The specs for the put-completion are:
   * *  param[out] _status = @ref DBR_SUCCESS or error code indicating issues:
   *    * @ref DBR_ERR_NOFILE   corrupted namespace detected during creation of namespace
   *    * @ref DBR_ERR_EXISTS   namespace already exists
   *    * @ref DBR_ERR_NSINVAL  namespace name exceeds limit or has invalid characters
   *    * for status codes see @ref DBBE_OPCODE_UNSPEC
   * *  param[out] void*                    _user = unmodified ptr provided in request
   * *  param[out] int64_t                  _rc = handle of newly created namespace to be supplied with subsequent requests
   * *  param[out] @ref dbBE_Completion_t*  _next = NULL unless multiple completions are created at the same time
   */
  DBBE_OPCODE_NSCREATE,

  /** @brief Namespace attach operation
   *
   * The specs of the put-request are:
   * *  param[in] _opcode = DBBE_OPCODE_NSATTACH
   * *  param[in] @ref dbBE_NS_Handle_t     _ns_hdl=NULL (will be created)
   * *  param[in]      void*                _user = pointer to anything, will be returned with completion without change
   * *  param[in] @ref dbBE_Request_t*      _next = NULL unless this is a chained request
   * *  param[in] @ref DBR_Group_t          _group = pointer or definition of storage group where to look for namespace
   * *  param[in] @ref DBR_Tuple_name_t     _key = pointer to name of namespace to attach
   * *  param[in] @ref DBR_Tuple_template_t _match = NULL (ignored)
   * *  param[in]      int64_t              _flags ignored
   * *  param[in]      int                  _sge_count = 0
   * *  param[in] @ref dbBE_sge_t[]         _sge[] = empty
   *
   * The specs for the put-completion are:
   * *  param[out] _status = @ref DBR_SUCCESS or error code indicating issues:
   *    * @ref DBR_ERR_UNAVAIL   namespace does not exist
   *    * @ref DBR_ERR_INVALIDOP namespace attach overflow: too many attached clients
   *    * @ref DBR_ERR_NOFILE    corrupted namespace detected while attaching to namespace
   *    * @ref DBR_ERR_NSINVAL   namespace name exceeds limit or has invalid characters
   *    * for status codes see @ref DBBE_OPCODE_UNSPEC
   * *  param[out] void*                    _user = unmodified ptr provided in request
   * *  param[out] int64_t                  _rc = handle of newly attached namespace to be supplied with subsequent requests
   * *  param[out] @ref dbBE_Completion_t*  _next = NULL unless multiple completions are created at the same time
   */
  DBBE_OPCODE_NSATTACH,
  DBBE_OPCODE_NSDETACH,  /**< Namespace detach operation  */
  DBBE_OPCODE_NSDELETE,  /**< Namespace delete operation to wipe the name space and its data  */
  DBBE_OPCODE_NSQUERY,  /**< Namespace query operation  */
  DBBE_OPCODE_NSADDUNITS,  /**< Namespace add units to increase the number of backing storage nodes of a namespace  */
  DBBE_OPCODE_NSREMOVEUNITS, /**< Namespace remove units to decrease the number of backing storage nodes of a namespace */
  DBBE_OPCODE_ITERATOR, /**< Iteration over existing keys */
  DBBE_OPCODE_MAX  /**< Non-implemented operation to simplify range checks for opcodes  */
} dbBE_Opcode;

enum
{
  DBBE_OPCODE_FLAGS_NONE = 0,
  DBBE_OPCODE_FLAGS_IMMEDIATE = 0x1,
  DBBE_OPCODE_FLAGS_PARTIAL = 0x2
};


/**
 * @struct dbBE_Request_t dbbe_api.h "backend/common/dbbe_api.h"
 *
 * @brief Back-end API request structure
 *
 * Defines a back-end request with operation code as well as input and output parameters
 */
typedef struct dbBE_Request
{
  dbBE_Opcode _opcode;         /**< operation code */
  dbBE_NS_Handle_t _ns_hdl;    /**< namespace handle; to hold back-end context for namespaces */
  void *_user;                 /**< upper layer data, will not be touched by system lib */
  struct dbBE_Request *_next;  /**< next pointer for chaining */
  DBR_Group_t _group;          /**< group */
  DBR_Tuple_name_t _key;       /**< key/tuple name */
  DBR_Tuple_template_t _match; /**< match template */
  int64_t _flags;              /**< any special flags or modifiers for the request */
  int _sge_count;              /**< number of sge's */
  dbBE_sge_t _sge[];           /**< SGE's */
} dbBE_Request_t;

/**
 * @typedef dbBE_Request_handle_t
 * @brief Back-end request handle
 */
typedef void* dbBE_Request_handle_t;

/**
 * @struct dbBE_Completion_t dbbe_api.h "backend/common/dbbe_api.h"
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
   * @param [in] trigger integer to indicate whether the backend should start processing
   *                     (user might request to hold processing for more work requests to follow)
   *
   * @return request handle that allows the user to check the status or cancel the request later
   *         or NULL in case of an error
   */
  dbBE_Request_handle_t (*post)( dbBE_Handle_t, dbBE_Request_t*, int );

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

/**********************************************************************
 * A set of helper functions to handle requests, completions, or SGEs
 */


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
static inline
size_t dbBE_SGE_get_len( const dbBE_sge_t *sge, const int sge_count )
{
  if( sge == NULL )
    return 0;
  int i;
  size_t len = 0;
  for( i = sge_count-1; i>=0; --i )
    len += sge[ i ].iov_len;
  return len;
}

/**
 * @brief allocate a new request struct including SGE-space
 *
 * @param [in] sge_count  number of SGEs to allocate at the end of the structure
 *
 * @return pointer to new allocated struct or NULL in case of failure
 */
static inline
dbBE_Request_t* dbBE_Request_allocate( const int sge_count )
{
  return (dbBE_Request_t*)calloc( 1, sizeof( dbBE_Request_t) + sge_count * sizeof( dbBE_sge_t ) );
}

/**
 *@}
 */

#endif /* BACKEND_INTERFACE_DBBE_API_H_ */
