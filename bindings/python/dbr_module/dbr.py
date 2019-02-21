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

DBR_GROUP_LIST_EMPTY = '0' #libdatabroker.DBR_GROUP_LIST_EMPTY
DBR_GROUP_EMPTY = '0'      #libdatabroker.DBR_GROUP_EMPTY

# Mask
DBR_STATE_MASK_ALL = libdatabroker.DBR_STATE_MASK_ALL


# Utility
def getErrorCode(error_code):
    return ERRORTABLE.getErrorCode(error_code).decode()

def getErrorMessage(error_code):
    if error_code < DBR_ERR_MAXERROR:
    	return ERRORTABLE.getErrorMessage(error_code).decode()
    return "Unknown Error"

def createBuf(buftype, bufsize):
    retval = ffi.buffer(ffi.new(buftype, bufsize))
    return retval

# Data Broker
def create(dbrname, level, groups):
    dbr_hdl = libdatabroker.dbrCreate(dbrname.encode(), level, groups)
    return dbr_hdl

def attach(dbr_name):
    dbr_hdl = libdatabroker.dbrAttach(dbr_name.encode())
    return dbr_hdl

def delete(dbr_name):
    retval = libdatabroker.dbrDelete(dbr_name.encode())
    return retval

def detach(dbr_handle):
    retval = libdatabroker.dbrDetach(dbr_handle)
    return retval

def query(dbr_handle, dbr_state, state_mask):
    retval =  libdatabroker.dbrQuery(dbr_handle, dbr_state, state_mask)
    return retval

def addUnits(dbr_handle, units):
    retval = libdatabroker.dbrAddUnits(dbr_handle, units)
    return retval 

def removeUnits(dbr_handle, units):
    retval = libdatabroker.dbrRemoveUnits(dbr_handle, units)
    return retval

def put(dbr_hdl, tuple_val, size, tuple_name, group):
    retval = libdatabroker.dbrPut(dbr_hdl, tuple_val.encode(), size, tuple_name.encode(), group.encode())
    return retval

def read(dbr_hdl, tuple_name, match_template, group, flag, buffer_size=None):
    out_size = ffi.new('int64_t*')
    if buffer_size is None :
        buffer_size=[128]
    out_size[0] = buffer_size[0]
    out_buffer = createBuf('char[]', out_size[0])
    retval = libdatabroker.dbrRead(dbr_hdl, ffi.from_buffer(out_buffer), out_size, tuple_name.encode(), match_template.encode(), group.encode(), flag)
    buffer_size[0] = out_size[0]
    
    return out_buffer[:].decode(), retval

def get(dbr_hdl, tuple_name, match_template, group, flag, buffer_size=None):
    out_size = ffi.new('int64_t*')
    if buffer_size is None :
        buffer_size=[128]
    out_size[0] = buffer_size[0]
    out_buffer = createBuf('char[]', out_size[0])
    retval = libdatabroker.dbrGet(dbr_hdl, ffi.from_buffer(out_buffer), out_size, tuple_name.encode(), match_template.encode(), group.encode(), flag)
    buffer_size[0] = out_size[0]
    return out_buffer[:].decode(), retval

def readA(dbr_hdl, tuple_name, match_template, group, returned_val=None, buffer_size=None):
    out_size = ffi.new('int64_t*')
    if buffer_size is None :
        buffer_size=[128]
    out_size[0] = buffer_size[0]
    out_buffer = createBuf('char[]', out_size[0])
    tag = libdatabroker.dbrReadA(dbr_hdl, ffi.from_buffer(out_buffer), out_size, tuple_name.encode(), match_template.encode(), group.encode())
    buffer_size[0] = out_size[0]
    returned_val[0] = out_buffer[:].decode()
    return tag, out_buffer[:].decode()

def putA(dbr_hdl, tuple_val, size, tuple_name, group):
    tag = libdatabroker.dbrPutA(dbr_hdl, tuple_val.encode(), size, tuple_name.encode(), group.encode())
    return tag

def getA(dbr_hdl, tuple_name, match_template, group, returned_val=None, buffer_size=None):
    out_size = ffi.new('int64_t*')
    if buffer_size is None :
        buffer_size=[128]
    out_size[0] = buffer_size[0]
    out_buffer = createBuf('char[]', out_size[0])
    tag = libdatabroker.dbrGetA(dbr_hdl, ffi.from_buffer(out_buffer), buffer_size, tuple_name.encode(), match_template.encode(), group.encode())
    buffer_size[0] = out_size[0]
    returned_val[0] = out_buffer[:].decode()
    return tag, out_buffer[:].decode()

def testKey(dbr_hdl, tuple_name):
    retval = libdatabroker.dbrTestKey(dbr_hdl, tuple_name)
    return retval

def directory(dbr_hdl, match_template, group, count, size):
    tbuf = createBuf('char[]',size)
    rsize = ffi.new('int64_t*')
    retval = libdatabroker.dbrDirectory(dbr_hdl, match_template.encode(), group.encode(), count, ffi.from_buffer(tbuf), ffi.cast('const size_t',size), rsize)
    result_buffer=(tbuf[0:rsize[0]].decode().split('\n'))
    return result_buffer, rsize[0], retval

def move(src_DBRHandle, src_group, tuple_name, match_template, dest_DBRHandle, dest_group):
    retval = libdatabroker.dbrMove(src_DBRHandle, src_group.encode(), tuple_name.encode(), match_template.encode(), dest_DBRHandle, dest_group.encode())
    return retval

def remove(dbr_hdl, group, tuple_name, match_template):
    retval = libdatabroker.dbrRemove(dbr_hdl, group.encode(), tuple_name.encode(), match_template.encode())
    return retval

def test(tag):
    retval = libdatabroker.dbrTest(tag)
    return retval

def cancel(tag):
    retval = libdatabroker.dbrCancel(tag)
    return retval

def eval(dbr_hdl, tuple_val, tuple_name, size, group, fn_ptr):
    retval = libdatabroker.dbrEval(dbr_hdl, tuple_val.encode(), size, tuple_name.encode(), group.encode(), fn_ptr)
    return retval
