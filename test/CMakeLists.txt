 #
 # Copyright © 2018,2019 IBM Corporation
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

# prepare this RPATH stuff in case we need rpath in the test executables
# SET(CMAKE_SKIP_BUILD_RPATH  FALSE)
# SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
# SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

# set of API tests

set(DBR_TEST_SOURCES
	test_dbrCreate.c
	test_dbrAttach.c
	test_dbrDelete.c
	test_dbrPutGet.c
	test_dbrReMove.c
	test_dbrPutGet_ext.c
	test_dbrPutGetA.c
	test_delete_scan.c
	test_errorcodes.c
	test_dbrUtils.c
	test_dbrDirectory.c
)

foreach(_test ${DBR_TEST_SOURCES})
  get_filename_component(TEST_NAME ${_test} NAME_WE)
  add_executable(${TEST_NAME} ${_test})
  add_dependencies(${TEST_NAME} ${DATABROKER_LIB} ${TEST_BACKEND} ${TRANSPORT_LIBS})
  target_link_libraries(${TEST_NAME} ${DATABROKER_LIB} ${TEST_BACKEND} ${TRANSPORT_LIBS} -lm ${BACKEND_DEPS} ${DL_LIB} )
  target_include_directories(${TEST_NAME} PRIVATE ${LIBEVENT_INCLUDE_DIR})
  add_test(DBR_${TEST_NAME} ${TEST_NAME} )
  install(TARGETS ${TEST_NAME} RUNTIME
          DESTINATION test )
endforeach()

