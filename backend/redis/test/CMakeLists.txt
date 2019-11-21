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

include( ${PROJECT_SOURCE_DIR}/backend/redis/libevent.cmake )

# prepare this RPATH stuff in case we need rpath in the test executables
# SET(CMAKE_SKIP_BUILD_RPATH  FALSE)
# SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE) 
# SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")


# define and add test sources

# test backend first
set(DB_BACKEND_TEST_SOURCES
	backend_redis_crc16_test.c
	backend_redis_s2r_queue_test.c
	backend_redis_slot_bitmap_test.c
	backend_redis_locator_test.c
	backend_redis_completion_test.c
	backend_redis_connection_test.c
	backend_redis_conn_mgr_test.c
	backend_redis_namespace_test.c
	backend_redis_parse_test.c
	backend_redis_create_test.c
	backend_redis_test.c
	libevent_test.c
	backend_redis_event_mgr_test.c
	backend_redis_resp_parse_test.c
	backend_redis_server_info_test.c
)

foreach(_test ${DB_BACKEND_TEST_SOURCES})
  get_filename_component(TEST_NAME ${_test} NAME_WE)
  add_executable(${TEST_NAME} ${_test})
  add_dependencies(${TEST_NAME} dbbe_redis ${TRANSPORT_LIBS})
  target_link_libraries(${TEST_NAME} dbbe_redis ${TRANSPORT_LIBS} -lm ${LIBEVENT_LIBRARIES} )
  target_include_directories(${TEST_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/test ${LIBEVENT_INCLUDE_DIR})
  add_test(DBBE_${TEST_NAME} ${TEST_NAME} )
  install(TARGETS ${TEST_NAME} RUNTIME
          DESTINATION test )
endforeach()

