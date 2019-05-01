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

#ifndef INCLUDE_LIBDATABROKER_H_
#define INCLUDE_LIBDATABROKER_H_

#ifndef DBR_INTTAG
#define DBR_INTTAG
#endif


#include <inttypes.h>
#include <stddef.h>
#include "errorcodes.h"

#define DBR_TIMEOUT_ENV "DBR_TIMEOUT"
/**
 * @defgroup api  User Level API
 *
 * This group includes functions and data types that are exposed to the user.
 *
 * @{
 */

/**
 * @brief Timeout for blocking calls.
 *
 * It defines the maximum time (in seconds) for a blocking Read/Get/Put completion.
 * It can be set using the environment variable **DBR_TIMEOUT**.
 */
#define DBR_TIMEOUT_DEFAULT ( 5 )

#ifdef __cplusplus
extern "C"
{
#endif
/**
 *@}
 */
#define DBR_MAJOR_VERSION ( 0 )
#define DBR_MINOR_VERSION ( 6 )
#define DBR_PATCH_VERSION ( 1 )

/**
 * @addtogroup api
 * @{
 */
/**
 * @brief Defines the level of persistence of tuples in the Data Broker.
 *
 * It can be in memory, on persistent storage.
 * It depends on the back-end and does not apply to Redis.
 */
typedef enum {
  DBR_PERST_VOLATILE_SIMPLE,
  DBR_PERST_VOLATILE_FT,
  DBR_PERST_TEMPORARY_SIMPLE,
  DBR_PERST_TEMPORARY_FT,
  DBR_PERST_PERMANENT_SIMPLE,
  DBR_PERST_PERMANENT_FT,
  DBR_PERST_MAX
} DBR_Tuple_persist_level_t;

/**
 * @brief Defines the return behavior of Read and Get.
 *
 * Flag defining whether a Read or a Get should return immediately if the tuple is not yet present in the namespace
 * or wait until a timeout is reached.
 *
 */
typedef enum {
  DBR_FLAGS_NONE = 0, /**< Read/Get keep checking for the existence of the requested tuple until a timeout is met.*/
  DBR_FLAGS_NOWAIT = 1, /**< Read/Get return immediately if the requested tuple is not present.*/
  DBR_FLAGS_MAX
} DBR_Request_flags_t;

/**
 * @typedef DBR_Handle_t
 * @brief   Handler to a namespace instantiation in the Data Broker.
 *
 * The handler is used in all data access functions and some of the management functions that directly access
 * a namespace in the Data Broker.
 *
 * The handler is unique and refers to a single namespace.
 *
 */
typedef void* DBR_Handle_t;

/**
 * @typedef DBR_State_t
 * @brief   Structure containing informations about the state of a certain namespace in the Data Broker.
 *
 */
typedef uint32_t DBR_State_t;

/**
 * @typedef DBR_State_mask_t
 * @brief   Mask defining the informations requested about the state of a namespace in the Data Broker.
 *
 */
typedef uint64_t DBR_State_mask_t;
/**
 * @typedef DBR_Tag_t
 * @brief   ID of an asynchronous call.
 *
 * Defines an unique value referring to an asynchronous function calls.
 * A DBR_Tag_t is returned by any asynchronous data access function and identifies the call itself.
 * The value can be used to check whether the asynchronous function has been completed or not.
 *
 */
#ifdef DBR_INTTAG
typedef int64_t DBR_Tag_t;
#else
typedef void* DBR_Tag_t;
#endif
/**
 * @typedef DBR_Name_t
 * @brief   Name of a namespace.
 *
 * Type used to create and/or identify a namespace in the Data Broker by a unique name.
 * Given a name of an existing namespace, an application can attach to it.
 * A namespace can be deleted by name.
 *
 */
typedef char *DBR_Name_t;

/**
 * @typedef DBR_Group_t
 * @brief   Group of a namespace.
 *
 * A Group defines a collection of Units of memory (ram, disk, flash, ...) where the namespace can store its data.
 * It is used in any data access functions to identify where the data is stored/must be stored.
 *
 * @see DBR_GroupList_t
 * @see DBR_Unit_t
 * @see DBR_UnitList_t
 *
 */
typedef char* DBR_Group_t;

/**
 * @typedef DBR_GroupList_t
 * @brief   List of Groups.
 *
 * A list of groups is defined and used when a namespace is created. It defines the list of backing memory units that
 * can be used by the DataBroker in the namespace that is being created.
 *
 * @see DBR_Group_t
 * @see DBR_Unit_t
 * @see DBR_UnitList_t
 *
 */
typedef DBR_Group_t *DBR_GroupList_t;

/**
 * @typedef DBR_Unit_t
 * @brief   Unit of backing memory.
 *
 * A unit of backing memory can be any slice of memory in RAM, Flash, other NVM, or disk as well as
 * a file or a partition that holds data.
 * It is used to create a Group, that is a collection of units.
 *
 * @see DBR_Group_t
 * @see DBR_GroupList_t
 * @see DBR_UnitList_t
 *
 */
typedef char *DBR_Unit_t;

/**
 * @typedef DBR_UnitList_t
 * @brief   List of Units of backing memory.
 *
 * A list of Units identifies the list of backing memories that can be used to store data in a specific namespace.
 * A unit of backing memory can be any slice of memory in RAM, Flash, other NVM, or disk as well as
 * a file or a partition that holds data.
 *
 * @see DBR_Group_t
 * @see DBR_GroupList_t
 * @see DBR_Unit_t
 *
 */
typedef DBR_Unit_t *DBR_UnitList_t;

/**
 * @typedef DBR_Tuple_name_t
 * @brief   Name of a tuple.
 *
 * A name defines an identifier for a tuple. It is used to retrieve the tuple in the namespace.
 * Each tuple can be considered as a pair <tuple_name, tuple>.
 * A tuple_name is also referred as a key.
 * Each name can refer to one or more tuple, hence a namespace is a multiset of <tuple_name, tuple> pairs.
 *

 */
typedef char *DBR_Tuple_name_t;

/**
 * @typedef DBR_Tuple_template_t
 * @brief   Template identifying a set of tuple names.
 *
 * A template can be used to look for tuples having a tuple name that matches certain characteristics.
 * It can be considered as a regular expression helping in searching for a certain set of tuple names.
 *
 *
 * @see DBR_Tuple_name_t
 */
typedef char *DBR_Tuple_template_t;

/**
 * @typedef FunctPtr_t
 * @brief   Function executed on a tuple.
 *
 * It refers to a function that will be executed on a tuple, given its name, thus possibly modifying its value.
 */
typedef DBR_Errorcode_t (*FunctPtr_t)(void*);
/**
 *@}
 */

#ifdef DBR_INTTAG
#define DB_TAG_ERROR ( -1 )
#else
#define DB_TAG_ERROR   NULL
#endif
/**
 * @addtogroup api
 * @{
 */

/**
 * @brief Largest length allowed for a tuple name (key).
 */
#define DBR_MAX_KEY_LEN ( 1023 )

/**
 * @brief Initializer for an empty group
 */
#define DBR_GROUP_EMPTY ( NULL )

/**
 * @brief Initializer for empty group lists
 */
#define DBR_GROUP_LIST_EMPTY (NULL)

/**
 * @brief Initializer for empty unit lists
 */
#define DBR_UNIT_LIST_EMPTY (NULL)

/**
 * @brief Initializer for a default state mask. It is used to return every available info about a namespace
 */
#define DBR_STATE_MASK_ALL ( 0xFFFFFFFFFFFFFFFFull )


/* data broker management functions
 * to create/query/delete name spaces and/or attach
 * or detach to/from an existing name space
 */

/**
 * @brief Create a namespace in the Data Broker.
 *
 * This function is used to create a new namespace in a Data Broker instance.
 * The namespace is defined by a unique name, a persistence level and a list of groups.
 * Groups and persistence level are related to the back-end.
 *
 * In case of Redis, those can be set to zero (DBR_PERST_VOLATILE_SIMPLE for persistence level).
 *
 * @param [in] db_name  Human-friendly name for the namespace.
 * @param [in] level     Level of persistence of tuples.
 * @param [in] groups	 List of groups where tuples are stored.
 *
 * @return Unique handle to the namespace.
 *
 */
DBR_Handle_t dbrCreate( DBR_Name_t db_name,
                        DBR_Tuple_persist_level_t level,
                        DBR_GroupList_t groups );

/**
 * @brief Delete a namespace in the Data Broker.
 *
 * This function is used to delete an existing namespace in a Data Broker instance.
 * The namespace is identified by its name.
 *
 * @param [in] db_name   Name of the namespace to be deleted.
 *
 * @return DBR_SUCCESS if deletion is completed successfully.
 *
 * @see DBR_Errorcode_t
 *
 */
DBR_Errorcode_t dbrDelete( DBR_Name_t db_name );

/**
 * @brief Attach to a namespace in the Data Broker.
 *
 * This function is used to attach to an existing namespace in a Data Broker instance.
 * The namespace is identified by its name.
 *
 * @param [in] db_name   Name of the namespace to attach to.
 *
 * @return Unique handler to the namespace.
 *
 */
DBR_Handle_t dbrAttach( DBR_Name_t db_name );

/**
 * @brief Detach from a namespace in the Data Broker.
 *
 * This function is used to disconnect from an existing namespace in a Data Broker instance.
 * The namespace is identified by its handle.
 *
 * @param [in] dbr_handle   Handle to the namespace to detach from.
 *
 * @return DBR_SUCCESS if detach is completed successfully.
 *
 * @see DBR_Errorcode_t
 *
 */
DBR_Errorcode_t dbrDetach( DBR_Handle_t dbr_handle );


/**
 * @brief Query the status of a namespace.
 *
 * This function is used to get info about the status of a namespace in a Data Broker instance.
 * The namespace is identified by its handler.
 * It will be possible to specify what info to get by the use of different masks.
 * The current implementation supports only DBR_STATE_MASK_ALL.
 *
 * @param [in] dbr_handle   Handle to the namespace to query.
 * @param [out] dbr_state	Pointer to the structure that will be filled with the result.
 * @param [in] dbr_state_mask Mask defining the informations requested.
 *
 * @return DBR_SUCCESS if the request is completed successfully.
 *
 * @see DBR_Errorcode_t
 *
 */
DBR_Errorcode_t dbrQuery( DBR_Handle_t dbr_handle,
                          DBR_State_t *dbr_state,
                          DBR_State_mask_t dbr_state_mask );

/**
 * @brief Not supported yet
 */
DBR_Errorcode_t dbrAddUnits( DBR_Handle_t dbr_handle,
                             DBR_UnitList_t dbr_units );
/**
 * @brief Not supported yet
 */
DBR_Errorcode_t dbrRemoveUnits( DBR_Handle_t dbr_handle,
                                DBR_UnitList_t dbr_units );




/*
 * data broker data access functions to insert and retrieve data items
 */
/**
 * @brief Insert a tuple in a namespace.
 *
 * The function allows the user to insert new data to a namespace.  This
 * requires a valid namespace handle, a tuple name, the pointer and
 * size of a contiguous memory region containing the tuple data.
 *
 * The function is blocking and it will return when the tuple is stored in the namespace.
 * If there exists already a tuple with the same name, the current tuple data will be added to the namespace
 * in FIFO order.
 *
 * @param [in] dbr_handle   Handle to the namespace.
 * @param [in] va_ptr	Pointer to the tuple.
 * @param [in] size  Size of the tuple.
 * @param [in] tuple_name Name/key identifying the tuple.
 * @param [in] group Group to which the namespace belongs.
 *
 * @return
 * 		- DBR_SUCCESS if the insertion is completed successfully;
 * 		- DBR_ERR_TIMEOUT if the insertion does not complete before the timeout occurs;
 * 		- An error code identifying the issue encountered, otherwise.
 *
 * @see DBR_Errorcode_t
 *
 */
DBR_Errorcode_t dbrPut( DBR_Handle_t dbr_handle,
                        void *va_ptr,
                        int64_t size,
                        DBR_Tuple_name_t tuple_name,
                        DBR_Group_t group );

/**
 * @brief Insert a tuple in a namespace asynchronously.
 *
 * The function allows the user to insert new data to a namespace.  This
 * requires a valid namespace handle, a tuple name, the pointer and
 * size of a contiguous memory region containing the tuple data.
 *
 * The function is non-blocking and it will return immediately after the call.
 *
 * A Tag is returned identifying the call.
 * Completion can be checked with dbrTest(), checking the DBR_Tag_t returned by the call.
 *
 * If there exists already a tuple with the same name, the current tuple data will be added to the namespace
 * in FIFO order.
 *
 *
 * @param [in] dbr_handle   Handle to the namespace.
 * @param [in] va_ptr		Pointer to the tuple.
 * @param [in] int64_t  	Size of the tuple.
 * @param [in] tuple_name 	Name/key identifying the tuple.
 * @param [in] group 		Group to which the namespace belongs.
 *
 * @return A tag identifying the call.
 *
 * @see DBR_Test()
 *
 */
DBR_Tag_t dbrPutA( DBR_Handle_t dbr_handle,
                   void *va_ptr,
                   int64_t size,
                   DBR_Tuple_name_t tuple_name,
                   DBR_Group_t group );


/**
 * @brief Get a tuple in a namespace.
 *
 * The function allows the user to get and consume a tuple from a namespace, thus, the tuple will be deleted from
 * the namespace.  If there exist multiple tuples with the same name,
 * then the tuple is retrieved following the order of insertion (FIFO).
 *
 * This requires a valid namespace handle, a tuple name, the pointer and
 * size of a contiguous memory region where to store the retrieved tuple data.
 * The initial value of the size is identified by the size of the buffer.
 * It will be updated with the effective length of the tuple just retrieved.
 *
 * The function is blocking and it will return when the tuple is retrieved from the namespace.
 * If the tuple does not exist yet, the call will block until a timeout is reached.
 *
 *
 * It can be customized with two different flags based on the behavior the user prefers:
 *		- DBR_FLAGS_NONE: the call blocks until a certain timeout is reached;
 * 	- DBR_FLAGS_NOWAIT: the call checks whether the tuple exists or not and returns immediately if the tuple is not in the namespace.
 *
 *
 * @param [in] dbr_handle   Handle to the namespace.
 * @param [out] va_ptr		Pointer to the buffer that will contain the tuple.
 * @param [inout] int64_t  	Size of the buffer, updated with the length of the tuple.
 * @param [in] tuple_name 	Name/key identifying the tuple to be searched.
 * @param [in] match_template Template identifying a set of tuple names.
 * @param [in] group 		Group to which the namespace belongs.
 * @param [in] flags		DBR_FLAGS_NONE or DBR_FLAGS_NOWAIT for immediate return option.
 *
 * @return
 * 		- DBR_SUCCESS if the call is completed successfully;
 * 		- DBR_ERR_TIMEOUT if the insertion does not complete before the timeout occurs *and* the flag is set to DBR_FLAGS_NONE;
 * 		- DBR_ERR_UNAVAIL if the tuple does not exist *and* the flag is set to DBR_FLAGS_NOWAIT;
 * 		- An error code identifying the issue encountered, otherwise.
 *
 * @see DBR_Errorcode_t
 *
 */
DBR_Errorcode_t dbrGet( DBR_Handle_t dbr_handle,
                        void *va_ptr,
                        int64_t *size,
                        DBR_Tuple_name_t tuple_name,
                        DBR_Tuple_template_t match_template,
                        DBR_Group_t group,
                        int flags );



/**
 * @brief Get a tuple in a namespace asynchronously.
 *
 * The function allows the user to get and consume a tuple from a namespace, thus, the tuple will be deleted from
 * the namespace.  If there exist multiple tuples with the same name,
 * then the tuple is retrieved following the order of insertion (FIFO).
 *
 * This requires a valid namespace handle, a tuple name, the pointer and
 * size of a contiguous memory region where to store the retrieved tuple data.
 * The initial value of the size is identified by the size of the buffer.
 * It will be updated with the effective length of the tuple just retrieved.
 *
 * The function is non-blocking and it will return immediately after the call.
 * A Tag is returned identifying the call.
 * Completion can be checked with dbrTest(), checking the DBR_Tag_t returned by the call.

 * @param [in] dbr_handle   Handle to the namespace.
 * @param [out] va_ptr		Pointer to the buffer that will contain the tuple.
 * @param [inout] int64_t  	Size of the buffer, updated with the length of the tuple.
 * @param [in] tuple_name 	Name/key identifying the tuple to be searched.
 * @param [in] match_template Template identifying a set of tuple names.
 * @param [in] group 		Group to which the namespace belongs.
 *
 * @return A tag identifying the call.
 *
 * @see DBR_Test()
 *
 */
DBR_Tag_t dbrGetA( DBR_Handle_t dbr_handle,
                   void *va_ptr,
                   int64_t *size,
                   DBR_Tuple_name_t tuple_name,
                   DBR_Tuple_template_t match_template,
                   DBR_Group_t group );


/**
 * @brief Read a tuple in a namespace.
 *
 * The function allows the user to copy a tuple from a namespace, thus, the tuple will not be consumed from
 * the namespace.  If there exist multiple tuples with the same name,
 * then the tuple is retrieved following the order of insertion (FIFO).
 *
 * This requires a valid namespace handle, a tuple name, the pointer and
 * size of a contiguous memory region where to store the retrieved tuple data.
 * The initial value of the size is identified by the size of the buffer.
 * It will be updated with the effective length of the tuple just retrieved.
 *
 * The function is blocking and it will return when the tuple is copied from the namespace.
 * If the tuple does not exist yet, the call will block until a timeout is reached.
 *
 *
 * It can be customized with two different flags based on the behavior the user prefers:
 *		- DBR_FLAGS_NONE: the call blocks until a certain timeout is reached;
 * 	- DBR_FLAGS_NOWAIT: the call checks whether the tuple exists or not and returns immediately if the tuple is not in the namespace.
 *
 *
 * @param [in] dbr_handle   Handle to the namespace.
 * @param [out] va_ptr		Pointer to the buffer that will contain the tuple.
 * @param [inout] int64_t  	Size of the buffer, updated with the length of the tuple.
 * @param [in] tuple_name 	Name/key identifying the tuple to be searched.
 * @param [in] match_template Template identifying a set of tuple names.
 * @param [in] group 		Group to which the namespace belongs.
 * @param [in] flags		DBR_FLAGS_NONE or DBR_FLAGS_NOWAIT for immediate return option.
 *
 * @return
 * 		- DBR_SUCCESS if the call is completed successfully;
 * 		- DBR_ERR_TIMEOUT if the insertion does not complete before the timeout occurs *and* the flag is set to DBR_FLAGS_NONE;
 * 		- DBR_ERR_UNAVAIL if the tuple does not exist *and* the flag is set to DBR_FLAGS_NOWAIT;
 * 		- An error code identifying the issue encountered, otherwise.
 *
 * @see DBR_Errorcode_t
 *
 */
DBR_Errorcode_t dbrRead( DBR_Handle_t dbr_handle,
                         void *va_ptr,
                         int64_t *size,
                         DBR_Tuple_name_t tuple_name,
                         DBR_Tuple_template_t match_template,
                         DBR_Group_t group,
                         int flags );

/**
 * @brief Read a tuple in a namespace asynchronously.
 *
 * The function allows the user to copy a tuple from a namespace, thus, the tuple will not be consumed from
 * the namespace.
 * If there exist multiple tuples with the same name,
 * then the tuple is retrieved following the order of insertion (FIFO).
 *
 * This requires a valid namespace handle, a tuple name, the pointer and
 * size of a contiguous memory region where to store the retrieved tuple data.
 * The initial value of the size is identified by the size of the buffer.
 * It will be updated with the effective length of the tuple just retrieved.
 *
 * The function is non-blocking and it will return immediately after the call.
 * A Tag is returned identifying the call.
 * Completion can be checked with dbrTest(), checking the DBR_Tag_t returned by the call.

 * @param [in] dbr_handle   Handle to the namespace.
 * @param [out] va_ptr		Pointer to the buffer that will contain the tuple.
 * @param [inout] int64_t  	Size of the buffer, updated with the length of the tuple.
 * @param [in] tuple_name 	Name/key identifying the tuple to be searched.
 * @param [in] match_template Template identifying a set of tuple names.
 * @param [in] group 		Group to which the namespace belongs.
 *
 * @return A tag identifying the call.
 *
 * @see DBR_Test()
 *
 */
DBR_Tag_t dbrReadA( DBR_Handle_t dbr_handle,
                    void *va_ptr,
                    int64_t *size,
                    DBR_Tuple_name_t tuple_name,
                    DBR_Tuple_template_t match_template,
                    DBR_Group_t group );

/**
 * @brief Check the existence of a tuple.
 *
 * The function checks if a tuple, identified by its name/key, exists already in the namespace.
 * It does not return any tuple value.
 *
 * @param [in] dbr_handle Handle to the namespace.
 * @param [in] tuple_name Name/Key of the tuple to be checked.
 *
 * @return
 * 		- DBR_SUCCESS if the tuple is present.
 * 		- DBR_ERR_UNAVAIL if the tuple is unavailable.
 * 		- An error code identifying the issue, otherwise.
 *
 * 	@see DBR_Errorcode_t
 */
DBR_Errorcode_t dbrTestKey( DBR_Handle_t dbr_handle,
                            DBR_Tuple_name_t tuple_name );


/**
 * @brief Retrieve a list of available tuple names/keys
 *
 * The function returns a list of available tuple names of a namespace
 * filtered by the user-provided pattern. The number of returned
 * items can be limited to a given maximum. The returned tuple names
 * are returned as a newline-separated list of char*
 *
 * @param [in] dbr_handle Handle to the namespace.
 * @param [in] pattern    A pattern that tuple names need to match.
 * @param [in] group      Group where tulpe is stored
 * @param [in] count      Maximum number of returned items (no limit if set to 0)
 * @param [in] result_buffer  user-provided space for the result
 * @param [in] size           amount of space in the user buffer
 * @param [out] ret_size  number of bytes in the result buffer
 *
 * @return
 *    - DBR_SUCCESS if the list of tuple names is returned successfully.
 *    - DBR_ERR_UBUFFER if the user buffer is too small, the data will be truncated
 *    - And other error codes identifying the issue, otherwise.
 *
 *  @see DBR_Errorcode_t
 */
DBR_Errorcode_t dbrDirectory( DBR_Handle_t cs_handle,
                              DBR_Tuple_template_t match_template,
                              DBR_Group_t group,
                              const unsigned count,
                              char *result_buffer,
                              const size_t size,
                              int64_t *ret_size );


/**
 * @brief Move a tuple from a source to a destination namespace.
 *
 * This function is used to move a tuple (or a set of tuples matching a name template) from a source
 * to a destination namespace.
 *
 * @param [in] src_dbr_handle	Handle of the source namespace.
 * @param [in] src_group		Group where the tuple is stored.
 * @param [in] tuple_name		Name of the tuple.
 * @param [in] match_template	Template identifying a set of tuple names.
 * @param [in] dest_dbr_handle	Handle of the destination namespace.
 * @param [in] dest_group		Group where to store the moved tuple.
 *
 * @return
 * 		- DBR_SUCCESS if the operation is successful;
 * 		- An error code identifying the issue, otherwise.
 *
 * 	@see DBR_Errorcode_t
 *
 */
DBR_Errorcode_t dbrMove( DBR_Handle_t src_dbr_handle,
                         DBR_Group_t src_group,
                         DBR_Tuple_name_t tuple_name,
                         DBR_Tuple_template_t match_template,
                         DBR_Handle_t dest_dbr_handle,
                         DBR_Group_t dest_group );


/**
 * @brief Remove a tuple from a namespace.
 *
 * This function is used to remove a tuple (or a set of tuples matching a name template) from a namespace without returning
 * the tuple(s).
 *
 * @param [in] src_dbr_handle	Handle of the namespace.
 * @param [in] src_group		Group where the tuple is stored.
 * @param [in] tuple_name		Name of the tuple.
 * @param [in] match_template	Template identifying a set of tuple names.
 *
 * @return
 * 		- DBR_SUCCESS if the operation is successful;
 * 		- An error code identifying the issue, otherwise.
 *
 * 	@see DBR_Errorcode_t
 *
 */
DBR_Errorcode_t dbrRemove( DBR_Handle_t dbr_handle,
                           DBR_Group_t group,
                           DBR_Tuple_name_t tuple_name,
                           DBR_Tuple_template_t match_template );


/*
 * data broker request handling functions
 * to test for completion or cancel non-blocking requests
 */

/**
 * @brief Test the completion of an asynchronous call.
 *
 * This function is used to check the completion of an asynchronous Put/Get/Read call, given the tag
 * corresponding to the call.
 *
 * @param [in] req_tag 		Valid tag corresponding to an asynchronous call.
 *
 * @return
 * 		- DBR_SUCCESS if the call has completed;
 * 		- DBR_ERR_INPROGRESS if the call is still in progress;
 *			- DBR_ERR_TAGERROR if the tag is invalid;
 *			- An error code identifying the issue, otherwise.
 *
 */
DBR_Errorcode_t dbrTest( DBR_Tag_t req_tag );


/**
 * @brief Cancel an asynchronous call.
 *
 * This function allows to abort an asynchronous Put/Get/Read call, given the tag
 * corresponding to the call.
 *
 * @param [in] req_tag		Valid tag corresponding to an asynchronous call.
 *
 * @return
 * 		- DBR_SUCCESS if the call is successful;
 *		- DBR_ERR_TAGERROR if the tag is invalid;
 *	- An error code identifying the issue, otherwise.
 */
DBR_Errorcode_t dbrCancel( DBR_Tag_t req_tag );



/*
 * execute a function on a tuple
 */
/**
 * @brief Execute a function on a tuple. Not implemented yet.
 */
DBR_Tag_t dbrEval( DBR_Handle_t dbr_handle,
                   void *va_ptr,
                   int size,
                   DBR_Tuple_name_t tuple_name,
                   DBR_Group_t group,
                   FunctPtr_t fn_ptr );


#ifdef __cplusplus
}
#endif
/**
 *@}
 */
#endif /* INCLUDE_LIBDATABROKER_H_ */
