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
import sys, os
from _errorcodes import ffi, lib
from enum import Enum

class DBR_Result(Enum):
    DBR_SUCCESS = lib.DBR_SUCCESS # no error, clean result, operation successful
    DBR_ERR_GENERIC = lib.DBR_ERR_GENERIC # a general or unknown error has occurred
    DBR_ERR_INVALID = lib.DBR_ERR_INVALID # // an invalid parameter was passed into a function or other general error
    DBR_ERR_HANDLE = lib.DBR_ERR_HANDLE # an invalid handle was encountered
    DBR_ERR_INPROGRESS = lib.DBR_ERR_INPROGRESS # a request is still in progress, check again later
    DBR_ERR_TIMEOUT = lib.DBR_ERR_TIMEOUT # a timeout occurred
    DBR_ERR_UBUFFER = lib.DBR_ERR_UBUFFER # provided user buffer problem (too small, not available)
    DBR_ERR_UNAVAIL = lib.DBR_ERR_UNAVAIL # the requested tuple or namespace is not available in the backing storage
    DBR_ERR_EXISTS = lib.DBR_ERR_EXISTS # Entry already exists
    DBR_ERR_NSBUSY = lib.DBR_ERR_NSBUSY # there are still clients attached to a namespace
    DBR_ERR_NSINVAL = lib.DBR_ERR_NSINVAL # invalid name space
    DBR_ERR_NOMEMORY = lib.DBR_ERR_NOMEMORY # the amount of memory or storage was insufficient to
    DBR_ERR_TAGERROR = lib.DBR_ERR_TAGERROR # the returned tag is an error
    DBR_ERR_NOFILE = lib.DBR_ERR_NOFILE # a file was not found
    DBR_ERR_NOAUTH = lib.DBR_ERR_NOAUTH # access authorization required or failed
    DBR_ERR_NOCONNECT = lib.DBR_ERR_NOCONNECT # connection to a storage backend failed
    DBR_ERR_CANCELLED = lib.DBR_ERR_CANCELLED # operation was cancelled
    DBR_ERR_NOTIMPL = lib.DBR_ERR_NOTIMPL # operation not implemented
    DBR_ERR_INVALIDOP = lib.DBR_ERR_INVALIDOP # invalid operation
    DBR_ERR_BE_POST = lib.DBR_ERR_BE_POST #  posting request to back-end failed
    DBR_ERR_BE_GENERAL = lib.DBR_ERR_BE_GENERAL # Unspecified back-end error
    DBR_ERR_MAXERROR = lib.DBR_ERR_MAXERROR

class Errors(object):
    def __init__(self):
        self.error_table = lib.dbrError_table
        self.DBR_SUCCESS = lib.DBR_SUCCESS # no error, clean result, operation successful
	self.DBR_ERR_GENERIC = lib.DBR_ERR_GENERIC # a general or unknown error has occurred
	self.DBR_ERR_INVALID = lib.DBR_ERR_INVALID # an invalid parameter was passed into a function or other general error
	self.DBR_ERR_HANDLE = lib.DBR_ERR_HANDLE # an invalid handle was encountered
	self.DBR_ERR_INPROGRESS = lib.DBR_ERR_INPROGRESS # a request is still in progress, check again later
	self.DBR_ERR_TIMEOUT = lib.DBR_ERR_TIMEOUT # a timeout occurred
	self.DBR_ERR_UBUFFER = lib.DBR_ERR_UBUFFER # provided user buffer problem (too small, not available)
	self.DBR_ERR_UNAVAIL = lib.DBR_ERR_UNAVAIL # the requested tuple or namespace is not available in the backing storage
	self.DBR_ERR_EXISTS = lib.DBR_ERR_EXISTS # Entry already exists
	self.DBR_ERR_NSBUSY = lib.DBR_ERR_NSBUSY # there are still clients attached to a namespace
	self.DBR_ERR_NSINVAL = lib.DBR_ERR_NSINVAL # invalid name space
	self.DBR_ERR_NOMEMORY = lib.DBR_ERR_NOMEMORY # the amount of memory or storage was insufficient to
	self.DBR_ERR_TAGERROR = lib.DBR_ERR_TAGERROR # the returned tag is an error
	self.DBR_ERR_NOFILE = lib.DBR_ERR_NOFILE # a file was not found
	self.DBR_ERR_NOAUTH = lib.DBR_ERR_NOAUTH # access authorization required or failed
	self.DBR_ERR_NOCONNECT = lib.DBR_ERR_NOCONNECT # connection to a storage backend failed
	self.DBR_ERR_CANCELLED = lib.DBR_ERR_CANCELLED # operation was cancelled
	self.DBR_ERR_NOTIMPL = lib.DBR_ERR_NOTIMPL # operation not implemented
	self.DBR_ERR_INVALIDOP = lib.DBR_ERR_INVALIDOP # invalid operation
	self.DBR_ERR_BE_POST = lib.DBR_ERR_BE_POST #  posting request to back-end failed
	self.DBR_ERR_BE_GENERAL = lib.DBR_ERR_BE_GENERAL # Unspecified back-end error
	self.DBR_ERR_MAXERROR = lib.DBR_ERR_MAXERROR

    def print_table(self):
        print str(self.error_table)

    def getErrorMessage(self, error_code):
         return ffi.string(self.error_table[error_code])

    def getErrorCode(self, error_code):
         return DBR_Result(error_code).name


