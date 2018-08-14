#
# Copyright 2013-2017 Software Radio Systems Limited
#
# This file is part of srsLTE
#
# srsLTE is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, either version 3 of
# the License, or (at your option) any later version.
#
# srsLTE is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# A copy of the GNU Affero General Public License can be found in
# the LICENSE file in the top-level directory of this distribution
# and at http://www.gnu.org/licenses/.
#

########################################################################
# Options
########################################################################

option(ENABLE_EMPOWER_AGENT  "Enable the use of EmPOWER Agent"     ON)

if(ENABLE_EMPOWER_AGENT)
  message(STATUS "Building srsENB with EmPOWER Agent")
  add_definitions(-DHAVE_EMPOWER_AGENT)
  
  find_package(Emagent)
  find_package(Emproto)
endif(ENABLE_EMPOWER_AGENT)

option(ENABLE_RAN_SLICER  "Enable the RAN slicer"                  OFF)

if(ENABLE_RAN_SLICER)
  message(STATUS "Building srsENB with RAN slicer")
  add_definitions(-DHAVE_RAN_SLICER)
endif(ENABLE_RAN_SLICER)

########################################################################
# Find dependencies
########################################################################

find_package(LibConfig REQUIRED)
find_package(SCTP REQUIRED)

if(BUILD_STATIC)
  set(LIBCONFIGPP_LIBRARIES "${LIBCONFIGPP_STATIC_LIBRARY_PATH}")
endif(BUILD_STATIC)

if(NOT Boost_FOUND)
    message(FATAL_ERROR "Boost required to compile srsENB")
endif()

########################################################################
# Setup the include and linker paths
########################################################################
include_directories(
    ${Boost_INCLUDE_DIRS}
    ${EMPOWER_AGENT_INCLUDE_DIRS}
    ${EMPOWER_PROTOCOLS_INCLUDE_DIRS}
    ${SEC_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}
)

link_directories(
    ${Boost_LIBRARY_DIRS}
    ${SEC_LIBRARY_DIRS}
)

########################################################################
# Add subdirectories
########################################################################
add_subdirectory(src)
add_subdirectory(test)
