 #
 # Copyright © 2020 IBM Corporation
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

find_package( libevent REQUIRED )

set( FSHIP_SRV_SOURCE
	fship_srv.c
)

add_executable( fship_srv ${FSHIP_SRV_SOURCE} )
add_dependencies( fship_srv libevent_pthreads libevent )
target_link_libraries( fship_srv PRIVATE ${DATABROKER_LIB} ${TRANSPORT_LIBS} ${libevent_pthreads_LIBRARY} ${libevent_LIBRARY} pthread )

install(TARGETS fship_srv RUNTIME
	    DESTINATION bin )
