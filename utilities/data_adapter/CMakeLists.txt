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

include( ../libdatabroker.cmake )

add_library(dbrda_hello SHARED hello.c)
target_include_directories(dbrda_hello
    PRIVATE
    ${LIBDATABROKER_INCLUDE_PATH} )

install( TARGETS dbrda_hello
	LIBRARY
	DESTINATION lib
)


add_library(dbrda_fastq SHARED fastq_splitter.c)
target_include_directories(dbrda_fastq
    PRIVATE
    ${LIBDATABROKER_INCLUDE_PATH} )

install( TARGETS dbrda_fastq
	LIBRARY
	DESTINATION lib
)

add_executable( fastq_test fastq_test.c )
  target_include_directories(fastq_test
    PRIVATE
    ${LIBDATABROKER_INCLUDE_PATH} )

  target_link_libraries( fastq_test
    ${LIBDATABROKER_LIBRARIES}
    ${LIBEVENT_LIBRARIES}
    -lm )

install( TARGETS fastq_test RUNTIME DESTINATION test )

add_library(dbrda_mummi SHARED mummi_postprocess.c)
target_include_directories(dbrda_mummi
    PRIVATE
    ${LIBDATABROKER_INCLUDE_PATH} )

install( TARGETS dbrda_mummi
	LIBRARY
	DESTINATION lib
)
