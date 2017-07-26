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
    NAMES emagent emproto
    HINTS /usr/local/lib
          /usr/local/lib64
          /usr/lib
          /usr/lib64
    PATHS /usr/local/lib
          /usr/local/lib64
          /usr/lib
          /usr/lib64
)

message(STATUS "EMPOWER AGENT LIBRARIES " ${EMPOWER_AGENT_LIBRARIES})
message(STATUS "EMPOWER AGENT INCLUDE DIRS " ${EMPOWER_AGENT_INCLUDE_DIRS})

ENDIF(NOT EMPOWER_AGENT_FOUND)
