 #
 # Copyright (C) 2018 IBM Corporation
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
from _dbr_interface import ffi
libtransport = ffi.dlopen("libdbbe_transport.so", ffi.RTLD_GLOBAL|ffi.RTLD_NOW)
libbackend = ffi.dlopen("libdbbe_redis.so", ffi.RTLD_GLOBAL|ffi.RTLD_NOW)
libdatabroker = ffi.dlopen("libdatabroker.so")
import _cffi_backend
from dbr_module.dbr_errorcodes import Errors


ERRORTABLE = Errors()

# Copy for direct access
DBR_SUCCESS = ERRORTABLE.DBR_SUCCESS # no error, clean result, operation successful
DBR_ERR_GENERIC = ERRORTABLE.DBR_ERR_GENERIC # a general or unknown error has occurred
DBR_ERR_INVALID = ERRORTABLE.DBR_ERR_INVALID # an invalid parameter was passed into a function or other general error
DBR_ERR_HANDLE = ERRORTABLE.DBR_ERR_HANDLE # an invalid handle was encountered
DBR_ERR_INPROGRESS = ERRORTABLE.DBR_ERR_INPROGRESS # a request is still in progress, check again later
DBR_ERR_TIMEOUT = ERRORTABLE.DBR_ERR_TIMEOUT # a timeout occurred
DBR_ERR_UBUFFER = ERRORTABLE.DBR_ERR_UBUFFER # provided user buffer problem (too small, not available)
DBR_ERR_UNAVAIL = ERRORTABLE.DBR_ERR_UNAVAIL # the requested tuple or namespace is not available in the backing storage
DBR_ERR_EXISTS = ERRORTABLE.DBR_ERR_EXISTS # Entry already exists
DBR_ERR_NSBUSY = ERRORTABLE.DBR_ERR_NSBUSY # there are still clients attached to a namespace
DBR_ERR_NSINVAL = ERRORTABLE.DBR_ERR_NSINVAL # invalid name space
DBR_ERR_NOMEMORY = ERRORTABLE.DBR_ERR_NOMEMORY # the amount of memory or storage was insufficient to
DBR_ERR_TAGERROR = ERRORTABLE.DBR_ERR_TAGERROR # the returned tag is an error
DBR_ERR_NOFILE = ERRORTABLE.DBR_ERR_NOFILE # a file was not found
DBR_ERR_NOAUTH = ERRORTABLE.DBR_ERR_NOAUTH # access authorization required or failed
DBR_ERR_NOCONNECT = ERRORTABLE.DBR_ERR_NOCONNECT # connection to a storage backend failed
DBR_ERR_CANCELLED = ERRORTABLE.DBR_ERR_CANCELLED # operation was cancelled
DBR_ERR_NOTIMPL = ERRORTABLE.DBR_ERR_NOTIMPL # operation not implemented
DBR_ERR_INVALIDOP = ERRORTABLE.DBR_ERR_INVALIDOP # invalid operation
DBR_ERR_BE_POST = ERRORTABLE.DBR_ERR_BE_POST #  posting request to back-end failed
DBR_ERR_BE_GENERAL = ERRORTABLE.DBR_ERR_BE_GENERAL # Unspecified back-end error
DBR_ERR_MAXERROR = ERRORTABLE.DBR_ERR_MAXERROR

# Tuple persist level
DBR_PERST_VOLATILE_SIMPLE = libdatabroker.DBR_PERST_VOLATILE_SIMPLE
DBR_PERST_VOLATILE_FT = libdatabroker.DBR_PERST_VOLATILE_FT
DBR_PERST_TEMPORARY_SIMPLE = libdatabroker.DBR_PERST_TEMPORARY_SIMPLE
DBR_PERST_TEMPORARY_FT = libdatabroker.DBR_PERST_TEMPORARY_FT
DBR_PERST_PERMANENT_SIMPLE = libdatabroker.DBR_PERST_PERMANENT_SIMPLE
DBR_PERST_PERMANENT_FT = libdatabroker.DBR_PERST_PERMANENT_FT
DBR_PERST_MAX = libdatabroker.DBR_PERST_MAX

DBR_FLAGS_NONE = libdatabroker.DBR_FLAGS_NONE
DBR_FLAGS_NOWAIT = libdatabroker.DBR_FLAGS_NOWAIT
DBR_FLAGS_MAX = libdatabroker.DBR_FLAGS_MAX

DBR_GROUP_LIST_EMPTY = libdatabroker.DBR_GROUP_LIST_EMPTY

# Mask
DBR_STATE_MASK_ALL = libdatabroker.DBR_STATE_MASK_ALL

def getErrorCode(error_code):
    return ERRORTABLE.getErrorCode(error_code)

def getErrorMessage(error_code):
    if error_code < DBR_ERR_MAXERROR:
    	return ERRORTABLE.getErrorMessage(error_code)
    return "Unknown Error"


def createBuf(buftype, bufsize):
    retval = ffi.buffer(ffi.new(buftype, bufsize))
    return retval

def dbrCreate(dbrname, level, groups):
    dbr_hdl = libdatabroker.dbrCreate(dbrname, level, groups)
    return dbr_hdl

def dbrAttach(dbr_name):
    dbr_hdl = libdatabroker.dbrAttach(dbr_name)
    return dbr_hdl

def dbrDelete(dbr_name):
    retval = libdatabroker.dbrDelete(dbr_name)
    return retval

def dbrDetach(dbr_handle):
    retval = libdatabroker.dbrDetach(dbr_handle)
    return retval

def dbrQuery(dbr_handle, dbr_state, state_mask):
    retval =  libdatabroker.dbrQuery(dbr_handle, dbr_state, state_mask)
    return retval

def dbrAddUnits(dbr_handle, units):
    retval = libdatabroker.dbrAddUnits(dbr_handle, units)
    return retval 

def dbrRemoveUnits(dbr_handle, units):
    retval = libdatabroker.dbrRemoveUnits(dbr_handle, units)
    return retval

def dbrPut(dbr_hdl, tuple_val, tuple_name, group):
    retval = libdatabroker.dbrPut(dbr_hdl, tuple_val, len(tuple_val), tuple_name, group)
    return retval

def dbrRead(dbr_hdl, out_buffer, buffer_size, tuple_name, match_template, group, flag):
    retval = libdatabroker.dbrRead(dbr_hdl, ffi.from_buffer(out_buffer), buffer_size, tuple_name, match_template, group, flag)
    return retval

def dbrGet(dbr_hdl, out_buffer, buffer_size, tuple_name, match_template, group, flag):
    retval = libdatabroker.dbrGet(dbr_hdl, ffi.from_buffer(out_buffer), buffer_size, tuple_name, match_template, group, flag)
    return retval

def dbrReadA(dbr_hdl, out_buffer, buffer_size, tuple_name, match_template, group):
    tag = libdatabroker.dbrReadA(dbr_hdl, ffi.from_buffer(out_buffer), buffer_size, tuple_name,  match_template, group)
    return tag

def dbrPutA(dbr_hdl, tuple_val, tuple_name, group):
    tag = libdatabroker.dbrPutA(dbr_hdl, tuple_val, len(tuple_val), tuple_name, group)
    return tag

def dbrGetA(dbr_hdl, out_buffer, buffer_size, tuple_name, match_template, group):
    tag = libdatabroker.dbrGetA(dbr_hdl, ffi.from_buffer(out_buffer), buffer_size, tuple_name, match_template, group)
    return tag

def dbrTestKey(dbr_hdl, tuple_name):
    retval = libdatabroker.dbrTestKey(dbr_hdl, tuple_name)
    return retval


def dbrDirectory(dbr_hdl, match_template, group, count, size, ret_size):
    tbuf = createBuf('char[]',size)
    retval = libdatabroker.dbrDirectory(dbr_hdl, match_template, group, count, ffi.from_buffer(tbuf), ffi.cast('const size_t',size), ret_size)
    result_buffer=(tbuf[0:ret_size[0]].split('\n'))
    return retval, result_buffer

def dbrMove(src_DBRHandle, src_group, tuple_name, match_template, dest_DBRHandle, dest_group):
    retval = libdatabroker.dbrMove(src_DBRHandle, src_group, tuple_name, match_template, dest_DBRHandle, dest_group)
    return retval

def dbrRemove(dbr_hdl, group, tuple_name, match_template):
    retval = libdatabroker.dbrRemove(dbr_hdl, group, tuple_name, match_template)
    return retval

def dbrTest(tag):
    retval = libdatabroker.dbrTest(tag)
    return retval

def dbrCancel(tag):
    retval = libdatabroker.dbrCancel(tag)
    return retval

def dbrEval(dbr_hdl, tuple_val, tuple_name, group, fn_ptr):
    retval = libdatabroker.dbrEval(dbr_hdl, tuple_val, len(tuple_val), tuple_name, group, fn_ptr)
    return retval
