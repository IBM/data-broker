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
#  libevent_INCLUDE_DIR
#  libevent_LIBRARY


find_package(PkgConfig)
pkg_check_modules(LE_PACK QUIET libevent)

find_path(LIBEVENT_INCLUDE_DIR
    NAMES event2/event.h
    PATHS ${LE_PACK_INCLUDE_DIRS}
    PATH_SUFFIXES include
)

find_library( LIBEVENT_LIBRARIES NAMES event
    PATH_SUFFIXES lib
)

mark_as_advanced(LIBEVENT_FOUND LIBEVENT_INCLUDE_DIR LIBEVENT_LIBRARIES)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libevent
    REQUIRED_VARS LIBEVENT_INCLUDE_DIR LIBEVENT_LIBRARIES
)

if(libevent_FOUND)
    set(libevent_INCLUDE_DIRS ${LIBEVENT_INCLUDE_DIR})
    set(libevent_LIBRARY ${LIBEVENT_LIBRARIES})
endif(libevent_FOUND)

if(libevent_FOUND AND NOT TARGET libevent)
    add_library(libevent INTERFACE IMPORTED)
    set_target_properties(libevent PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${libevent_INCLUDE_DIR}"
        INTERFACE_LIBRARY "${libevent_LIBRARY}"
    )
endif()


get_filename_component( libevent_LIBRARY_PATH ${libevent_LIBRARY} DIRECTORY CACHE )

# Additonal component libevent_pthreads
find_library( LIBEVENT_PTHREADS NAMES event_pthreads
	PATHS ${libevent_LIBRARY_PATH}
)

mark_as_advanced(LIBEVENT_PTHREADS_FOUND LIBEVENT_PTHREADS)
message( "LIBEVENT_PTHREADS_LIBRARIES=${LIBEVENT_PTHREADS}")

find_package_handle_standard_args(libevent_pthreads
    REQUIRED_VARS LIBEVENT_INCLUDE_DIR LIBEVENT_PTHREADS
)

if(libevent_pthreads_FOUND)
    set(libevent_pthreads_LIBRARY ${LIBEVENT_PTHREADS})
else()
	message(FATAL_ERROR "Libevent_pthreads[${LIBEVENT_PTHREADS}] is not part of your libevent installation in ${libevent_LIBRARY}")
endif()


if( libevent_FOUND AND NOT TARGET libevent_pthreads)
	add_library( libevent_pthreads INTERFACE IMPORTED)
	set_target_properties( libevent_pthreads PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "${libevent_INCLUDE_DIR}"
		INTERFACE_LIBRARY "${libevent_pthreads_LIBRARY}"
	)
endif()

message( "Found libevent in: ${libevent_pthreads_LIBRARY}" )
