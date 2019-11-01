 #
 # Copyright (C) 2018, 2019 IBM Corporation
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
# file "dbr_interface.py"

# Note: we instantiate the same 'cffi.FFI' class as in the previous
# example, but call the result 'ffibuilder' now instead of 'ffi';
# this is to avoid confusion with the other 'ffi' object you get below

import cffi

ffibuilder = cffi.FFI()
libdatabroker =    r""" // passed to the real C compiler,
        // contains implementation of things declared in cdef()
        #include <inttypes.h>
        #include "errorcodes.h"

    """;

ffibuilder.set_source("_dbr_interface",None,
    libdatabroker,
    libraries=["databroker", "databroker_int", "dbbe_redis", "dbbe_transport"],)
    #ibrary_dirs=[os.path.dirname(__file__),],)
#    include_dirs=["../../../include/"])
ffibuilder.cdef("""

#define DBR_MAX_KEY_LEN 1023
#define DBR_GROUP_LIST_EMPTY 0 
#define DBR_GROUP_EMPTY 0
#define DBR_UNIT_LIST_EMPTY 0 
#define DBR_STATE_MASK_ALL  0xFFFFFFFFFFFFFFFFull
#define DBR_ITERATOR_NEW 0
#define DBR_ITERATOR_DONE 0

typedef enum {
  DBR_PERST_VOLATILE_SIMPLE,
  DBR_PERST_VOLATILE_FT,
  DBR_PERST_TEMPORARY_SIMPLE,
  DBR_PERST_TEMPORARY_FT,
  DBR_PERST_PERMANENT_SIMPLE,
  DBR_PERST_PERMANENT_FT,
  DBR_PERST_MAX
} DBR_Tuple_persist_level_t;

typedef enum {
  DBR_FLAGS_NONE = 0,
  DBR_FLAGS_NOWAIT = 1,
  DBR_FLAGS_MAX
} DBR_Request_flags_t;

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
  DBR_ERR_ITERATOR,
  DBR_ERR_PLUGIN,
  DBR_ERR_MAXERROR
} DBR_Errorcode_t;


typedef void* DBR_Handle_t;
typedef uint32_t DBR_State_t;
typedef uint64_t DBR_State_mask_t;
typedef uint64_t DBR_Tag_t;
typedef char *DBR_Name_t;
typedef char *DBR_Group_t;
typedef DBR_Group_t *DBR_GroupList_t;
typedef char *DBR_Unit_t;
typedef DBR_Unit_t *DBR_UnitList_t;
typedef char *DBR_Tuple_name_t;
typedef char *DBR_Tuple_template_t;
typedef void* DBR_Iterator_t;
//typedef DBR_Errorcode_t (*FunctPtr_t)(void*);


DBR_Handle_t dbrCreate( DBR_Name_t db_name,
                        DBR_Tuple_persist_level_t level,
                        DBR_GroupList_t groups );

DBR_Errorcode_t dbrDelete( DBR_Name_t db_name );

DBR_Handle_t dbrAttach( DBR_Name_t db_name );

DBR_Errorcode_t dbrQuery( DBR_Handle_t cs_handle,
                          DBR_State_t *cs_state,
                          DBR_State_mask_t cs_state_mask );


DBR_Errorcode_t dbrDetach( DBR_Handle_t cs_handle );


DBR_Errorcode_t dbrAddUnits( DBR_Handle_t cs_handle,
                             DBR_UnitList_t cs_units );

//DBR_Errorcode_t dbrRemoveUnits( DBR_Handle_t cs_handle,
  //                              DBR_UnitList_t cs_units );


DBR_Errorcode_t dbrPut( DBR_Handle_t cs_handle,
                        void *va_ptr,
                        int64_t size,
                        DBR_Tuple_name_t tuple_name,
                        DBR_Group_t group );

DBR_Tag_t dbrPutA( DBR_Handle_t cs_handle,
                   void *va_ptr,
                   int64_t size,
                   DBR_Tuple_name_t tuple_name,
                   DBR_Group_t group );

DBR_Errorcode_t dbrGet( DBR_Handle_t cs_handle,
                        void *va_ptr,
                        int64_t *size,
                        DBR_Tuple_name_t tuple_name,
                        DBR_Tuple_template_t match_template,
                        DBR_Group_t group,
                        int flags );

DBR_Tag_t dbrGetA( DBR_Handle_t cs_handle,
                   void *va_ptr,
                   int64_t *size,
                   DBR_Tuple_name_t tuple_name,
                   DBR_Tuple_template_t match_template,
                   DBR_Group_t group );

DBR_Errorcode_t dbrRead( DBR_Handle_t cs_handle,
                         void *va_ptr,
                         int64_t *size,
                         DBR_Tuple_name_t tuple_name,
                         DBR_Tuple_template_t match_template,
                         DBR_Group_t group,
                         int flags );

DBR_Tag_t dbrReadA( DBR_Handle_t cs_handle,
                    void *va_ptr,
                    int64_t *size,
                    DBR_Tuple_name_t tuple_name,
                    DBR_Tuple_template_t match_template,
                    DBR_Group_t group );

DBR_Errorcode_t dbrTestKey( DBR_Handle_t cs_handle, DBR_Tuple_name_t tuple_name );

DBR_Errorcode_t dbrDirectory( DBR_Handle_t cs_handle,
                              DBR_Tuple_template_t match_template,
                              DBR_Group_t group,
                              const unsigned count,
                              char *result_buffer,
                              const size_t size,
                              int64_t *ret_size );


DBR_Errorcode_t dbrMove( DBR_Handle_t src_cs_handle,
                         DBR_Group_t src_group,
                         DBR_Tuple_name_t tuple_name,
                         DBR_Tuple_template_t match_template,
                         DBR_Handle_t dest_cs_handle,
                         DBR_Group_t dest_group );

DBR_Errorcode_t dbrRemove( DBR_Handle_t cs_handle,
                           DBR_Group_t group,
                           DBR_Tuple_name_t tuple_name,
                           DBR_Tuple_template_t match_template );

DBR_Errorcode_t dbrTest( DBR_Tag_t req_tag );

DBR_Errorcode_t dbrCancel( DBR_Tag_t req_tag );

DBR_Iterator_t dbrIterator( DBR_Handle_t dbr_handle,
                            DBR_Iterator_t it,
                            DBR_Group_t group,
                            DBR_Tuple_template_t match_template,
                            DBR_Tuple_name_t tuple_name );

/*
DBR_Tag_t dbrEval( DBR_Handle_t cs_handle,
                   void *va_ptr,
                   int size,
                   DBR_Tuple_name_t tuple_name,
                   DBR_Group_t group,
                   FunctPtr_t fn_ptr );*/
""")


if __name__ == "__main__":
    ffibuilder.compile(verbose=True)
