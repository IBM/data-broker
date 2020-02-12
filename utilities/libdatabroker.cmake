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


# sets:
#  LIBDATABROKER_INCLUDE_PATH
#  LIBDATABROKER_LIBRARIES

#add_definitions(-DOLDDBR )

message("Will search for libdatabroker includes")
find_path( LIBDATABROKER_INCLUDE_PATH libdatabroker.h
	PATH_SUFFIXES include
)

if( NOT LIBDATABROKER_INCLUDE_PATH )
 message( FATAL_ERROR "Databroker header files not found" )
endif( NOT LIBDATABROKER_INCLUDE_PATH )

message( "Found libdatabroker headers in: ${LIBDATABROKER_INCLUDE_PATH}" )

set( LIBDATABROKER_PATH ${LIBDATABROKER_INCLUDE_PATH}/../ )

find_library( DATABROKER_LIBFILE NAMES databroker
    PATH_SUFFIXES lib
)

if( NOT OLDDBR )
  find_library( DATABROKERINT_LIBFILE NAMES databroker_int
    PATH_SUFFIXES lib
  )
endif( NOT OLDDBR )

find_library( DATABROKER_TRANSPORTFILE NAMES dbbe_transport
    PATH_SUFFIXES lib
)


if( NOT DEFINED DEFAULT_BE )
  set(DEFAULT_BE redis )
endif( NOT DEFINED DEFAULT_BE )

find_library( DATABROKER_BACKENDFILE NAMES dbbe_${DEFAULT_BE}
    PATH_SUFFIXES lib
)

set( LIBDATABROKER_LIBRARIES
  ${DATABROKER_LIBFILE}
  ${DATABROKERINT_LIBFILE}
  ${DATABROKER_BACKENDFILE}
  ${DATABROKER_TRANSPORTFILE}
)

if( NOT LIBDATABROKER_LIBRARIES )
  message( FATAL_ERROR "Databroker library not found" )
endif( NOT LIBDATABROKER_LIBRARIES )

message( "Found libdatabroker: ${LIBDATABROKER_LIBRARIES}" )
