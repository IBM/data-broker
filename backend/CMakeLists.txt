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

add_subdirectory(common/test)

# accummulate the list of available backend directories
# note this uses GLOB - if a backend dir is added or removed, it will require
# a rerun of cmake
file( GLOB dirlist RELATIVE ${PROJECT_SOURCE_DIR}/backend/ "*" )

# filter and add subdirectories
foreach( subdir ${dirlist} )
  if( IS_DIRECTORY ${PROJECT_SOURCE_DIR}/backend/${subdir} )
    if( NOT ${subdir}  MATCHES "common$" )
      add_subdirectory( ${subdir} )
    endif( NOT ${subdir}  MATCHES "common$" )
  endif(  IS_DIRECTORY ${PROJECT_SOURCE_DIR}/backend/${subdir} )
endforeach( subdir ${dirlist})




# any new back-end library needs to provide everything in a single to-link library
# with the name formated as: dbbe_<subdir>
