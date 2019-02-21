 #
 # Copyright (C) 2018, 2019 IBM Corporation
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


set(PACKAGE_FIND_VERSION_MAJOR 3)
set(PACKAGE_FIND_VERSION_MINOR 0)
find_package(PythonInterp ${PACKAGE_FIND_VERSION_MAJOR}.${PACKAGE_FIND_VERSION_MINOR})
find_package(PythonLibs ${PACKAGE_FIND_VERSION_MAJOR}.${PACKAGE_FIND_VERSION_MINOR})

message(STATUS "PYTHON_LIBRARIES = ${PYTHON_LIBRARIES}")
message(STATUS "PYTHON_EXECUTABLE = ${PYTHON_EXECUTABLE}")

if (NOT EXISTS ${PYTHON_EXECUTABLE})
    set(Python_ADDITIONAL_VERSIONS 2.7)
    find_package(PythonInterp 2)
    find_package(PythonLibs 2) 
endif()

set( PYTHON ${PYTHON_EXECUTABLE})

if (PYTHON)
	set(SETUP_PY_IN "${CMAKE_CURRENT_SOURCE_DIR}/setup.py.in")
	set(SETUP_PY    "setup.py")
	set(DEPS        "${CMAKE_CURRENT_BINARY_DIR}/dbr_module/__init__.py")
	set(OUTPUT      "${CMAKE_CURRENT_BINARY_DIR}/build/")

 
	configure_file(${SETUP_PY_IN} ${SETUP_PY})

	add_custom_command(OUTPUT ${OUTPUT}
                   COMMAND ${PYTHON} ${SETUP_PY} build
                   COMMAND ${CMAKE_COMMAND} -E touch ${OUTPUT}
                   DEPENDS ${DEPS})

	set( INSTALL_CMD "${PYTHON} ${CMAKE_CURRENT_BINARY_DIR}/${SETUP_PY} install")
   
	add_custom_target(pydbr ALL DEPENDS ${OUTPUT})
	FILE(COPY examples DESTINATION .)
	FILE(COPY dbr_module DESTINATION .)
	install(CODE "MESSAGE(\"Installing pydbr \")")
	install(CODE "execute_process( COMMAND ${INSTALL_CMD}
              WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} )")
endif()
