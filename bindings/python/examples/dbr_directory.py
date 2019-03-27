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
import os
import sys

from _dbr_interface import ffi
from dbr_module import dbr


dbr_name = "DBRtestname"
level = dbr.DBR_PERST_VOLATILE_SIMPLE
group_list = ffi.new('DBR_GroupList_t')
dbr_hdl = ffi.new('DBR_Handle_t*')
dbr_hdl = dbr.create(dbr_name, level, group_list)
group = dbr.DBR_GROUP_EMPTY

# query the DBR to see if successful
dbr_state = ffi.new('DBR_State_t*')
ret = dbr.query(dbr_hdl, dbr_state, dbr.DBR_STATE_MASK_ALL)

test_in = "Hello World!"
ret = dbr.put(dbr_hdl, test_in, "HelloTuple", group)
ret = dbr.put(dbr_hdl, "Goodbye World!", "GoodbyeTuple", group)

######
#test the directory command and list all tuple names/keys
size = 1024
count = 1000
keys,rsize,res = dbr.directory(dbr_hdl, "*", group, count, size)

print('Keys on DBR: ' + str(keys[:]))
tsize=[64]
value, ret = dbr.read(dbr_hdl, keys[1], "", group, dbr.DBR_FLAGS_NOWAIT)
print('Read returned: ' + value + " - status: " + dbr.getErrorMessage(ret))
value, ret = dbr.get(dbr_hdl, keys[1], "", group, dbr.DBR_FLAGS_NONE)
print('Get returned: ' + value + " - status: " + dbr.getErrorMessage(ret))
value, ret = dbr.read(dbr_hdl, keys[0], "", group, dbr.DBR_FLAGS_NONE)
print('Read returned: ' +  value +  " - status: " + dbr.getErrorMessage(ret))
value, ret = dbr.get(dbr_hdl, keys[0], "", group, dbr.DBR_FLAGS_NONE)
print('Get returned: ' + value + " - status: " + dbr.getErrorMessage(ret))
print('Delete Data Broker')
res = dbr.delete(dbr_name)
print('Exit Status: ' + dbr.getErrorMessage(res))
