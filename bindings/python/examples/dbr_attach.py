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

import os, sys
import dbr_module
from _dbr_interface import ffi
from dbr_module import dbr

dbr_name = "DBRtestname"
group_list = ffi.new('DBR_GroupList_t')
dbr_hdl = ffi.new('DBR_Handle_t*')
level = dbr.DBR_PERST_VOLATILE_SIMPLE

# Create the Data Broker
dbr_hdl = dbr.create(dbr_name, level, group_list)
# Query the Data Broker 
dbr_state = ffi.new('DBR_State_t*')
res = dbr.query(dbr_hdl, dbr_state, dbr.DBR_STATE_MASK_ALL)

# Test if we can attach multiple times
for n in range(10):
    dbr_hdl = dbr.attach(dbr_name)
    assert dbr_hdl != None  

# test detach 
for n in range(10):
    res = dbr.detach(dbr_hdl)
    print('Detach #' + str(n+1) + ': ' + dbr.getErrorMessage(res))
    assert (res == dbr.DBR_SUCCESS)

# Delete the Data Broker    
print('Delete Data Broker')
res = dbr.delete(dbr_name)

# try to attach to the deleted Data Broker
dbr_hdl = dbr.attach(dbr_name)
assert dbr_hdl != None
print('Exit Status: ' + dbr.getErrorMessage(res))
