 #
 # Copyright © 2018-2020 IBM Corporation
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

include( ../libdatabroker.cmake )

# user provided tests
set(DB_USER_TEST_SOURCES
   single.cc
)

foreach(_test ${DB_USER_TEST_SOURCES})
  get_filename_component(TEST_NAME ${_test} NAME_WE)
  add_executable(${TEST_NAME} ${_test})

  target_link_libraries(${TEST_NAME}
    ${LIBDATABROKER_LIBRARIES}
    ${LIBEVENT_LIBRARIES}
    -lm )

  target_include_directories(${TEST_NAME}
    PRIVATE
    ${LIBDATABROKER_INCLUDE_PATH}
    ${LIBEVENT_INCLUDE_DIR} )

  install(TARGETS ${TEST_NAME} RUNTIME
          DESTINATION test )
endforeach()


find_package(MPI)

if( MPI_FOUND )
  message( "Found MPI lib at: " ${MPI_INCLUDE_PATH} )
endif( MPI_FOUND )



set(DB_USER_CC_TEST_SOURCES
   parallel.cc
   long_random_parallel.cc
)

foreach(_test ${DB_USER_CC_TEST_SOURCES})
  get_filename_component(TEST_NAME ${_test} NAME_WE)
  add_executable(${TEST_NAME} ${_test})

  target_link_libraries(${TEST_NAME}
    ${LIBDATABROKER_LIBRARIES}
    ${LIBEVENT_LIBRARIES}
    ${MPI_CXX_LIBRARIES}
    -lm )

  target_include_directories(${TEST_NAME}
    PRIVATE
    ${LIBDATABROKER_INCLUDE_PATH}
    ${LIBEVENT_INCLUDE_DIR}
    ${MPI_INCLUDE_PATH}
    ./ )

  install(TARGETS ${TEST_NAME} RUNTIME
          DESTINATION test )
endforeach()
