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

#ifndef INCLUDE_ERRORCODES_H_
#define INCLUDE_ERRORCODES_H_
/**
 * @addtogroup api
 *
 * This group includes functions and data types that are exposed to the user.
 *
 * @{
 */

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief List of error codes.
 *
 * Defines the error codes that can be returned by different functions.
 * A verbose description of the error code is given by dbrGet_error function.
 *
 * @see dbrGet_error()
 */
typedef enum {
  DBR_SUCCESS = 0, /**< No error, clean result, operation successful */
  DBR_ERR_GENERIC, /**< A general or unknown error has occurred*/
  DBR_ERR_INVALID, /**< An invalid parameter was passed into a function or other general error*/
  DBR_ERR_HANDLE,  /**< An invalid handle was encountered*/
  DBR_ERR_INPROGRESS, /**< A request is still in progress, check again later*/
  DBR_ERR_TIMEOUT, /**< A timeout occurred*/
  DBR_ERR_UBUFFER, /**< Provided user buffer problem (too small, not available)*/
  DBR_ERR_UNAVAIL, /**< The requested tuple or namespace is not available in the backing storage*/
  DBR_ERR_EXISTS, /**< Entry already exists*/
  DBR_ERR_NSBUSY, /**< There are still clients attached to a namespace*/
  DBR_ERR_NSINVAL, /**< Invalid name space*/
  DBR_ERR_NOMEMORY, /**< The amount of memory or storage was insufficient to*/
  DBR_ERR_TAGERROR, /**< The returned tag is an error*/
  DBR_ERR_NOFILE, /**< A file was not found*/
  DBR_ERR_NOAUTH, /**< Access authorization required or failed*/
  DBR_ERR_NOCONNECT, /**< Connection to a storage backend failed*/
  DBR_ERR_CANCELLED, /**< Operation was cancelled*/
  DBR_ERR_NOTIMPL, /**< Operation not implemented*/
  DBR_ERR_INVALIDOP, /**< Invalid operation*/
  DBR_ERR_BE_POST, /**< Posting request to back-end failed*/
  DBR_ERR_BE_GENERAL, /**< Unspecified back-end error*/
  DBR_ERR_PLUGIN,  /**< Data adapter error */
  DBR_ERR_MAXERROR
} DBR_Errorcode_t;

/**
 *@}
 */

static const
char *dbrError_table[ DBR_ERR_MAXERROR ] = {
   [ DBR_SUCCESS ] = "Operation successful",
   [ DBR_ERR_GENERIC ] = "A general or unknown error has occurred",
   [ DBR_ERR_INVALID ] = "Invalid argument",
   [ DBR_ERR_HANDLE ] = "An invalid handle was encountered inside",
   [ DBR_ERR_INPROGRESS ] = "Operation in progress",
   [ DBR_ERR_TIMEOUT ] = "Operation timed out",
   [ DBR_ERR_UBUFFER ] = "Provided user buffer problem (too small, not available)",
   [ DBR_ERR_UNAVAIL ] = "Entry not available",
   [ DBR_ERR_EXISTS ] = "Entry already exists",
   [ DBR_ERR_NSBUSY ] = "Namespace still referenced by a client",
   [ DBR_ERR_NSINVAL ] = "Namespace is invalid",
   [ DBR_ERR_NOMEMORY ] = "Insufficient memory or storage",
   [ DBR_ERR_TAGERROR ] = "Invalid tag",
   [ DBR_ERR_NOFILE ] = "File not found",
   [ DBR_ERR_NOAUTH ] = "Access authorization required or failed",
   [ DBR_ERR_NOCONNECT ] = "Connection to a storage backend failed",
   [ DBR_ERR_CANCELLED ] = "Operation was cancelled",
   [ DBR_ERR_NOTIMPL ] = "Operation not implemented",
   [ DBR_ERR_INVALIDOP ] = "Invalid operation",
   [ DBR_ERR_BE_POST ] = "Failed to post request to back-end",
   [ DBR_ERR_BE_GENERAL ] = "Unspecified back-end error",
   [ DBR_ERR_PLUGIN ] = "Error while processing request/data in data adapter"
};

/**
 * @addtogroup api
 * @{
 */

/**
 * @brief Get verbose description of an error code.
 *
 * Given an error code, this function will return a longer description of the error.
 *
 * @param [in] err The code returned by a function
 *
 * @return String describing the error
 */
static inline const
char* dbrGet_error( const DBR_Errorcode_t err )
{
  if(( (unsigned)err % (unsigned)DBR_ERR_MAXERROR ) == (unsigned)err )
    return dbrError_table[ err ];
  else
    return "Unknown Error";
}
/**
 *@}
 */

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_ERRORCODES_H_ */
