 #
 # Copyright © 2018 IBM Corporation
 #
 # Licensed under the Apache License, Version 2.0 (the "License");
 # you may not use this file except in compliance with the License.
 # You may obtain a copy of the License at
 #
 #    http://www.apache.org/licenses/LICENSE-2.0
 #
 # Unless required by applicable law or agreed to in writing, software
 # distributed under the License is distributed on an "AS IS" BASIS,
 # WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 # See the License for the specific language governing permissions and
 # limitations under the License.
 #
import os
import cffi

from cffi import FFI
ffibuilder = FFI()
    
tdef = r"""
typedef enum {
  DBR_SUCCESS = 0, // no error, clean result, operation successful
  DBR_ERR_GENERIC, // a general or unknown error has occurred
  DBR_ERR_INVALID, // an invalid parameter was passed into a function or other general error
  DBR_ERR_HANDLE,  // an invalid handle was encountered
  DBR_ERR_INPROGRESS, // a request is still in progress, check again later
  DBR_ERR_TIMEOUT, // a timeout occurred
  DBR_ERR_UBUFFER, // provided user buffer problem (too small, not available)
  DBR_ERR_UNAVAIL, // the requested tuple or namespace is not available in the backing storage
  DBR_ERR_EXISTS, // Entry already exists
  DBR_ERR_NSBUSY, // there are still clients attached to a namespace
  DBR_ERR_NSINVAL, // invalid name space
  DBR_ERR_NOMEMORY, // the amount of memory or storage was insufficient to
  DBR_ERR_TAGERROR, // the returned tag is an error
  DBR_ERR_NOFILE, // a file was not found
  DBR_ERR_NOAUTH, // access authorization required or failed
  DBR_ERR_NOCONNECT, // connection to a storage backend failed
  DBR_ERR_CANCELLED, // operation was cancelled
  DBR_ERR_NOTIMPL, // operation not implemented
  DBR_ERR_INVALIDOP, // invalid operation
  DBR_ERR_BE_POST, // posting request to back-end failed
  DBR_ERR_BE_GENERAL, // Unspecified back-end error
  DBR_ERR_MAXERROR
} DBR_Errorcode_t;


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
   [ DBR_ERR_BE_GENERAL ] = "Unspecified back-end error"
};

"""
ffibuilder.set_source("_errorcodes", tdef)
        # Since we are calling a fully built library directly no custom source
        # is necessary. We need to include the .h files, though, because behind
        # the scenes cffi generates a .c file which contains a Python-friendly
        # wrapper around each of the functions.
        #libraries=["databroker","dbbe_transport","dbbe_redis"],)
        #include_dirs=["../../../include/"],
        #library_dirs=[os.path.dirname(__file__),],
ffibuilder.cdef("""
typedef enum {
  DBR_SUCCESS = 0, // no error, clean result, operation successful
  DBR_ERR_GENERIC, // a general or unknown error has occurred
  DBR_ERR_INVALID, // an invalid parameter was passed into a function or other general error
  DBR_ERR_HANDLE,  // an invalid handle was encountered
  DBR_ERR_INPROGRESS, // a request is still in progress, check again later
  DBR_ERR_TIMEOUT, // a timeout occurred
  DBR_ERR_UBUFFER, // provided user buffer problem (too small, not available)
  DBR_ERR_UNAVAIL, // the requested tuple or namespace is not available in the backing storage
  DBR_ERR_EXISTS, // Entry already exists
  DBR_ERR_NSBUSY, // there are still clients attached to a namespace
  DBR_ERR_NSINVAL, // invalid name space
  DBR_ERR_NOMEMORY, // the amount of memory or storage was insufficient to
  DBR_ERR_TAGERROR, // the returned tag is an error
  DBR_ERR_NOFILE, // a file was not found
  DBR_ERR_NOAUTH, // access authorization required or failed
  DBR_ERR_NOCONNECT, // connection to a storage backend failed
  DBR_ERR_CANCELLED, // operation was cancelled
  DBR_ERR_NOTIMPL, // operation not implemented
  DBR_ERR_INVALIDOP, // invalid operation
  DBR_ERR_BE_POST, // posting request to back-end failed
  DBR_ERR_BE_GENERAL, // Unspecified back-end error
  DBR_ERR_MAXERROR
} DBR_Errorcode_t;


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
   [ DBR_ERR_BE_GENERAL ] = "Unspecified back-end error"
};

    """)
if __name__ == "__main__":
    ffibuilder.compile(verbose=True)
