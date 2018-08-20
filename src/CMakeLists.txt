 #
 # Copyright © 2018 IBM Corporation
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

set( LIBDBINT_FILES
	lib/backend.c
	lib/namespace.c
	lib/request.c
	lib/completion.c
	dbrUtils.c
	api/dbrCreate.c
	api/dbrDelete.c
	api/dbrAttach.c
	api/dbrAddUnits.c
	api/dbrRemoveUnits.c
	api/dbrDetach.c
	api/dbrQuery.c
	api/dbrPut.c
	api/dbrPutA.c
	api/dbrGet.c
	api/dbrGetA.c
	api/dbrRead.c
	api/dbrReadA.c
	api/dbrTest.c
	api/dbrCancel.c
	api/dbrMove.c
	api/dbrRemove.c
)

include_directories(./)
link_directories(/usr/local/lib)
add_library(databroker_int SHARED ${LIBDBINT_FILES})
add_dependencies(databroker_int ${TRANSPORT_LIBS} ${BACKEND_LIST} )
if( DEFINED APPLE )
 target_link_libraries(databroker_int ${TRANSPORT_LIBS} ${BACKEND_LIST} )
endif( DEFINED APPLE )

install( TARGETS databroker_int
	LIBRARY
	DESTINATION lib
)

