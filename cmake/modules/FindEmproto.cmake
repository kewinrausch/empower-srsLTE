# - Try to find EMPOWER_PROTOCOLS
#
# Once done this will define
#  EMPOWER_PROTOCOLS_FOUND        - System has EmPOWER Protocols
#  EMPOWER_PROTOCOLS_INCLUDE_DIRS - The EmPOWER Protocols include directories
#  EMPOWER_PROTOCOLS_LIBRARIES    - The EmPOWER Protocols library

IF(NOT EMPOWER_PROTOCOLS_FOUND)

FIND_PATH(
    EMPOWER_PROTOCOLS_INCLUDE_DIRS
    NAMES emage/pb/main.pb-c.h
    HINTS /usr/local/include
          /usr/include
    PATHS /usr/local/include
          /usr/include
)

FIND_LIBRARY(
    EMPOWER_PROTOCOLS_LIBRARIES
    NAMES emproto
    HINTS /usr/local/lib
          /usr/local/lib64
          /usr/lib
          /usr/lib64
    PATHS /usr/local/lib
          /usr/local/lib64
          /usr/lib
          /usr/lib64
)

message(STATUS "EMPOWER AGENT LIBRARIES " ${EMPOWER_PROTOCOLS_LIBRARIES})
message(STATUS "EMPOWER AGENT INCLUDE DIRS " ${EMPOWER_PROTOCOLS_INCLUDE_DIRS})

ENDIF(NOT EMPOWER_PROTOCOLS_FOUND)
