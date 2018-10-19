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

#ifndef BACKEND_REDIS_DEFINITIONS_H_
#define BACKEND_REDIS_DEFINITIONS_H_

#include <inttypes.h>
#include <limits.h>

#include "libdatabroker.h"

/*
 * default send-recv buffer size for interaction with Redis
 */
#define DBBE_REDIS_SR_BUFFER_LEN ( 128 * 1048576 )

/*
 * default size of the work queue for unprocessed user requests
 */
#define DBBE_REDIS_WORK_QUEUE_DEPTH ( 1024 )

/*
 * max redis key len (combined length of namespace+separator+key)
 */
#define DBBE_REDIS_MAX_KEY_LEN ( 2 * DBR_MAX_KEY_LEN + 2 )

#define DBR_SERVER_HOST_ENV "DBR_SERVER"
#define DBR_SERVER_PORT_ENV "DBR_PORT"
#define DBR_SERVER_AUTHFILE_ENV "DBR_AUTHFILE"
#define DBR_SERVER_DEFAULT_HOST "localhost"
#define DBR_SERVER_DEFAULT_PORT "6379"
#define DBR_SERVER_DEFAULT_AUTHFILE ".redis.auth"


/*
 * max number of Redis connections that can be handled simultaneously by the library
 */
#define DBBE_REDIS_MAX_CONNECTIONS ( (unsigned)256 )


/*
 * result type returned when parsing a Redis recv buffer
 * indicates the various types of responses from Redis
 */
typedef enum
{
  dbBE_REDIS_TYPE_UNSPECIFIED = 0, ///< unspecified result or uninitialized value
  dbBE_REDIS_TYPE_CHAR, ///< character/string
  dbBE_REDIS_TYPE_STRING_HEAD, ///< just the head of the string
  dbBE_REDIS_TYPE_RAW, ///< for any raw data to go in/out of the sr-buffer
  dbBE_REDIS_TYPE_INT,  ///< integer
  dbBE_REDIS_TYPE_ERROR, ///< Redis error string
  dbBE_REDIS_TYPE_ARRAY, ///< an array of results is parsed off the string
  dbBE_REDIS_TYPE_REDIRECT, ///< Redis returned ASK to temporarily redirect a request
  dbBE_REDIS_TYPE_RELOCATE, ///< Redis returned MOVED to permanently redirect a request
  dbBE_REDIS_TYPE_INVALID, ///< the returned value is invalid due to parser errors
  dbBE_REDIS_TYPE_MAX ///< limiter for sanity checks
} dbBE_REDIS_DATA_TYPE;

/*
 * Redis address for a given hash slot
 */
typedef struct {
  int64_t _hash;  ///< the hash slot number
  char *_address; ///< the address of the node (addr:port)
} dbBE_Redis_hash_location_t;

struct dbBE_Redis_result;

typedef struct {
  int _len;
  struct dbBE_Redis_result *_data; // pointing to results;
} dbBE_Redis_array_t;

typedef struct {
  int64_t _size;
  char *_data;
} dbBE_Redis_string_t;

/*
 * parser returns this result, depending on the dbBE_REDIS_RESULT
 * different entries in this union will make sense
 */
typedef union {
  int64_t _integer;
  dbBE_Redis_string_t _string;
  dbBE_Redis_array_t _array;
  dbBE_Redis_hash_location_t _location;
} dbBE_Redis_data_t;

typedef struct dbBE_Redis_result {
  dbBE_REDIS_DATA_TYPE _type;
  dbBE_Redis_data_t _data;
} dbBE_Redis_result_t;

/*
 * represent an invalid number
 */
#define DBBE_REDIS_NAN ( LLONG_MIN+1ll )

/*
 * namespace separator string to put between namespace and key
 */
#define DBBE_REDIS_NAMESPACE_SEPARATOR "::"
#define DBBE_REDIS_NAMESPACE_SEPARATOR_LEN ( 2 )

#endif /* BACKEND_REDIS_DEFINITIONS_H_ */
