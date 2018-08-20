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

find_program(PYTHON "python")

if (PYTHON)
    set(SETUP_PY    "setup.py")
    set(DEPS        "${CMAKE_CURRENT_SOURCE_DIR}/dbr_module/__init__.py")


   add_custom_command( OUTPUT setup_build
                       	COMMAND ${PYTHON} ${SETUP_PY} build
                       	DEPENDS ${DEPS}
                        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

   set( INSTALL_CMD "${PYTHON} ${SETUP_PY} install")
   
   add_custom_command( OUTPUT cffi_build
			COMMAND /bin/sh ${CMAKE_CURRENT_SOURCE_DIR}/build.sh)


   add_custom_target(PyDBR ALL DEPENDS cffi_build setup_build)
   install(CODE "MESSAGE(\"Installing PyDBR \")")
   install(CODE "execute_process( COMMAND ${INSTALL_CMD}
                 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )")
endif()
