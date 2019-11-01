 #
 # Copyright © 2018, 2019 IBM Corporation
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

set( LIBDB_FILES
	src/dbrCreate.c
	src/dbrDelete.c
	src/dbrAttach.c
	src/dbrAddUnits.c
	src/dbrDetach.c
	src/dbrQuery.c
	src/dbrPut.c
	src/dbrPut_gather.c
	src/dbrGet.c
	src/dbrGet_scatter.c
	src/dbrRead.c
	src/dbrRead_scatter.c
	src/dbrDirectory.c
	src/dbrTest.c
	src/dbrCancel.c
	src/dbrMove.c
	src/dbrRemove.c
	src/dbrTestKey.c
	src/dbrIterator.c
)

include_directories(../../src)
add_library(databroker SHARED ${LIBDB_FILES} )
add_dependencies(databroker databroker_int ${TRANSPORT_LIBS} ${BACKEND_LIST})
target_link_libraries(databroker databroker_int) 

install( TARGETS databroker
	LIBRARY
	DESTINATION lib
)
