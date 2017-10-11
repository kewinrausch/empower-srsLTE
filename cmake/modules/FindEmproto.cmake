# - Try to find EMPOWER_PROTOCOLS
#
# Once done this will define
#  EMPOWER_PROTOCOLS_FOUND        - System has EmPOWER Protocols
#  EMPOWER_PROTOCOLS_INCLUDE_DIRS - The EmPOWER Protocols include directories
#  EMPOWER_PROTOCOLS_LIBRARIES    - The EmPOWER Protocols library

IF(NOT EMPOWER_PROTOCOLS_FOUND)

FIND_PATH(
    EMPOWER_PROTOCOLS_INCLUDE_DIRS
    NAMES emage/emproto.h
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

ENDIF(NOT EMPOWER_PROTOCOLS_FOUND)

if(EMPOWER_PROTOCOLS_INCLUDE_DIRS AND EMPOWER_PROTOCOLS_LIBRARIES)

    set(EMPOWER_PROTOCOLS_FOUND TRUE CACHE INTERNAL "EmPOWER protocols found")
    message(STATUS "Found EmPOWER Protocols: ${EMPOWER_PROTOCOLS_INCLUDE_DIRS}, ${EMPOWER_PROTOCOLS_LIBRARIES}")
    
else(EMPOWER_PROTOCOLS_INCLUDE_DIRS AND EMPOWER_PROTOCOLS_LIBRARIES)

    set(EMPOWER_PROTOCOLS_FOUND FALSE CACHE INTERNAL "EmPOWER protocols NOT found")
    message(STATUS "EmPOWER Protocols NOT found!")
    
endif(EMPOWER_PROTOCOLS_INCLUDE_DIRS AND EMPOWER_PROTOCOLS_LIBRARIES)