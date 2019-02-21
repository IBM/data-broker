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
import time
import pickle 

dbr_name = "DBRtestname"
level = dbr.DBR_PERST_VOLATILE_SIMPLE
group_list = ffi.new('DBR_GroupList_t')
dbr_hdl = ffi.new('DBR_Handle_t*')
dbr_hdl = dbr.create(dbr_name, level, group_list)
group = dbr.DBR_GROUP_EMPTY

# query the DBR to see if successful
dbr_state = ffi.new('DBR_State_t*')
res = dbr.query(dbr_hdl, dbr_state, dbr.DBR_STATE_MASK_ALL)


test_in = "Hello World!"
tag = dbr.putA(dbr_hdl, test_in, "testTup", group)

print('Async Put tuple: ' + test_in + ", calling Async Get")

tag,value = dbr.getA(dbr_hdl, "testTup","", group)
dbr.read(dbr_hdl, "testTup", "", group, dbr.DBR_FLAGS_NONE)

if tag!=dbr.DBR_ERR_TAGERROR:
    while True:
        state = dbr.test(tag)
        if state == dbr.DBR_ERR_INPROGRESS:
            print("Waiting...")
            time.sleep(2)
        elif state == dbr.DBR_SUCCESS: 
            break
else:
    print('Async Get test returned ' + dbr.getErrorMessage(state))

print('Async get tuple return value: ' + str(dbr.decodeTuple(value)))

print('Check if blocking Read fails on timeout..')
value, res = dbr.read(dbr_hdl, "testTup", "", group, dbr.DBR_FLAGS_NONE)
print('status: ' + dbr.getErrorMessage(res))

print('Delete Data Broker')
res = dbr.delete(dbr_name)
print(('Exit Status: ' + dbr.getErrorMessage(res)))
