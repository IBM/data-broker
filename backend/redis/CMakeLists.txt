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

find_package( libevent )

include_directories(/usr/local/include)
set( LIBDBBE_REDIS_SOURCE
	server_info.c
	cluster_info.c
	connection.c
	locator.c
	namespace.c
	protocol.c
	result.c
	request.c
	parse.c
	s2r_queue.c
	create.c
	complete.c
	event_mgr.c
	conn_mgr.c
	sender.c
	receiver.c
	redis.c
)

add_library(dbbe_redis SHARED ${LIBDBBE_REDIS_SOURCE})
add_dependencies(dbbe_redis ${TRANSPORT_LIBS})
target_link_libraries(dbbe_redis PRIVATE ${TRANSPORT_LIBS} ${libevent_LIBRARY} )
#target_include_directories(dbbe_redis PRIVATE ${LIBEVENT_INCLUDE_DIR})

install( TARGETS dbbe_redis
	LIBRARY
	DESTINATION lib
)

add_subdirectory(test)
