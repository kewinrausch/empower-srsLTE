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

file(GLOB SOURCES "dummy_agent.cc")

if(ENABLE_EMPOWER_AGENT)
  list(APPEND SOURCES empower_agent.cc)
endif(ENABLE_EMPOWER_AGENT)

add_library(srsenb_agent STATIC ${SOURCES})

if(EMPOWER_AGENT_FOUND AND EMPOWER_PROTOCOLS_FOUND)
    target_link_libraries(srsenb_agent ${EMPOWER_AGENT_LIBRARIES} ${EMPOWER_PROTOCOLS_LIBRARIES})
endif(EMPOWER_AGENT_FOUND AND EMPOWER_PROTOCOLS_FOUND)

install(TARGETS srsenb_agent DESTINATION ${LIBRARY_DIR})
