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

enable_testing()
cmake_minimum_required(VERSION 2.8)

if (APPLE)
  cmake_policy(SET CMP0042 OLD)
  link_directories(/usr/local/lib)
  add_definitions(-DAPPLE)
endif()

project (DataBroker NONE)
enable_language( C CXX )

if(NOT DEFINED APPLE)
  add_definitions(-Werror)
  add_definitions(-Wall -Wno-missing-braces)
endif( NOT DEFINED APPLE )

if(DEFINED DEVMODE)
  add_definitions(-Wextra -Wenum-compare -Wno-unused-parameter)
  add_definitions( -DDEVMODE )
endif(DEFINED DEVMODE)

if( DEFINED DBR_DEBUG_LVL )
  add_definitions( -DDEBUG_LEVEL=${DBR_DEBUG_LVL} )
endif( DEFINED DBR_DEBUG_LVL )

if( DEFINED VOIDTAG )
else( DEFINED VOIDTAG )
  add_definitions( -DDBR_INTTAG )
endif( DEFINED VOIDTAG )

if( DEFINED WITH_DATA_ADAPTERS )
  add_definitions( -DDBR_DATA_ADAPTERS )
  set( DL_LIB -ldl )
endif( DEFINED WITH_DATA_ADAPTERS )

if( NOT DEFINED DEFAULT_BE )
  set(DEFAULT_BE redis )
endif( NOT DEFINED DEFAULT_BE )

set( BACKEND_LIBS "" )
# accummulate the list of available backend directories
# note this uses GLOB - if a backend dir is added or removed, it will require
# a rerun of cmake
file( GLOB dirlist "${PROJECT_SOURCE_DIR}/backend/*" )

# filter and add subdirectories
foreach( subdir ${dirlist} )

  if( IS_DIRECTORY ${subdir} )
    # extract the last compontent of the path to get the back-end name
    get_filename_component( CURRENT_BE ${subdir} NAME )
    message("Checking: " ${subdir} " BE: " ${CURRENT_BE} )

    # set the back-end to build according to DEFAULT_BE settings
    if( EXISTS ${subdir}/settings.cmake )
      if( ${CURRENT_BE} STREQUAL ${DEFAULT_BE} )
        message( "FOUND BE: " ${DEFAULT_BE} )
        include( ${subdir}/settings.cmake )
      endif( ${CURRENT_BE} STREQUAL ${DEFAULT_BE} )
    endif( EXISTS ${subdir}/settings.cmake )

  endif( IS_DIRECTORY ${subdir} )

endforeach( subdir ${dirlist})

if( "${BACKEND_LIBS}" STREQUAL "" )
  message( FATAL_ERROR "No BACKEND LIBS found/defined. Was looking for " ${DEFAULT_BE} )
endif( "${BACKEND_LIBS}" STREQUAL "" )


set(TRANSPORT_LIBS dbbe_transport )
set(DATABROKER_LIB databroker databroker_int)

include_directories(src/util)
include_directories(include)
include_directories(backend)

add_subdirectory(bindings)
add_subdirectory(backend)

message( "BACKEND LIBRARY:" ${BACKEND_LIBS} )
set(TEST_BACKEND ${BACKEND_LIBS} )
set(BACKEND_LIST ${BACKEND_LIBS} )

add_subdirectory(src)
add_subdirectory(test)

install(FILES include/libdatabroker.h
    include/errorcodes.h
    include/libdatabroker_ext.h
    include/dbrda_api.h
    DESTINATION include
)
