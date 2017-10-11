# - Try to find EMPOWER_AGENT
#
# Once done this will define
#  EMPOWER_AGENT_FOUND        - System has EmPOWER Agent
#  EMPOWER_AGENT_INCLUDE_DIRS - The EmPOWER Agent include directories
#  EMPOWER_AGENT_LIBRARIES    - The EmPOWER Agent library

IF(NOT EMPOWER_AGENT_FOUND)

FIND_PATH(
    EMPOWER_AGENT_INCLUDE_DIRS
    NAMES emage/emage.h
    HINTS /usr/local/include
          /usr/include
    PATHS /usr/local/include
          /usr/include
)

FIND_LIBRARY(
    EMPOWER_AGENT_LIBRARIES
    NAMES emagent
    HINTS /usr/local/lib
          /usr/local/lib64
          /usr/lib
          /usr/lib64
    PATHS /usr/local/lib
          /usr/local/lib64
          /usr/lib
          /usr/lib64
)

ENDIF(NOT EMPOWER_AGENT_FOUND)

if(EMPOWER_AGENT_INCLUDE_DIRS AND EMPOWER_AGENT_LIBRARIES)

    set(EMPOWER_AGENT_FOUND TRUE CACHE INTERNAL "EmPOWER agent found")
    message(STATUS "Found EmPOWER Agent: ${EMPOWER_AGENT_INCLUDE_DIRS}, ${EMPOWER_AGENT_LIBRARIES}")
    
else(EMPOWER_AGENT_INCLUDE_DIRS AND EMPOWER_AGENT_LIBRARIES)

    set(EMPOWER_AGENT_FOUND FALSE CACHE INTERNAL "EmPOWER agent NOT found")
    message(STATUS "EmPOWER agent NOT found!")
    
endif(EMPOWER_AGENT_INCLUDE_DIRS AND EMPOWER_AGENT_LIBRARIES)