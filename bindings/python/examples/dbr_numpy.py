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
import numpy as np

dbr_name = "DBRtestname"
level = dbr.DBR_PERST_VOLATILE_SIMPLE
group_list = ffi.new('DBR_GroupList_t')
dbr_hdl = ffi.new('DBR_Handle_t*')
dbr_hdl = dbr.create(dbr_name, level, group_list)
group = dbr.DBR_GROUP_EMPTY

print('- Put float array in DBR')
input_array = np.array([[[1, 2, 3.0],[1,1,1]],[[4,5,6],[7,8,9]]])
res = dbr.put(dbr_hdl, input_array, "testNumpy", group)
ret_array, ret = dbr.get(dbr_hdl, "testNumpy", "", group, dbr.DBR_FLAGS_NONE, [256])
print("Get returned status: " + dbr.getErrorMessage(ret))
print('Check input and output array are the same: ' + str(np.array_equal(input_array, ret_array)))

print('\n- Put integer multidimensional array in DBR')
np.random.seed(0)
input_array = np.random.randint(3, size=(2,2,3))
res = dbr.put(dbr_hdl, input_array, "testNumpy", group)
ret_array= None
ret_array, ret = dbr.get(dbr_hdl, "testNumpy", "", group, dbr.DBR_FLAGS_NONE, [256])
print("Get returned status: " + dbr.getErrorMessage(ret))
print('Check input and output array are the same: ' + str(np.array_equal(input_array, ret_array)))

print('\n- Put mixed types/mixed size array in DBR')
input_array = np.array([[[0, 1., 0],[1, 1, 2]],[[3., 14, 15],[0, 0, 2]], [1, 2, 3]])
res = dbr.put(dbr_hdl, input_array, "testNumpy", group)
ret_array, ret = dbr.get(dbr_hdl, "testNumpy", "", group, dbr.DBR_FLAGS_NONE, [512])
print("Get returned status: " + dbr.getErrorMessage(ret))
print('Check input and output array are the same: ' + str(np.array_equal(input_array, ret_array)))

print('\nDelete Data Broker')
res = dbr.delete(dbr_name)
print('Exit Status: ' + dbr.getErrorMessage(res))
