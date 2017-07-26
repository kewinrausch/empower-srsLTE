# - Try to find PROTOBUF_C
#
# Once done this will define
#  PROTOBUF_C_FOUND        - System has GPB-C
#  PROTOBUF_C_INCLUDE_DIRS - The GPB-C include directories
#  PROTOBUF_C_LIBRARIES    - The GPB-C library

IF(NOT PROTOBUF_C_FOUND)

FIND_PATH(
    PROTOBUF_C_INCLUDE_DIRS
    NAMES protobuf-c.h
    HINTS /usr/local/include
          /usr/include
    PATHS /usr/local/include
          /usr/include
)

FIND_LIBRARY(
    PROTOBUF_C_LIBRARIES
    NAMES protobuf-c
    HINTS /usr/local/lib
          /usr/local/lib64
          /usr/lib
          /usr/lib64
    PATHS /usr/local/lib
          /usr/local/lib64
          /usr/lib
          /usr/lib64
)

message(STATUS "PROTOBUF-C LIBRARIES " ${PROTOBUF_C_LIBRARIES})
message(STATUS "PROTOBUF-C INCLUDE DIRS " ${PROTOBUF_C_INCLUDE_DIRS})

ENDIF(NOT PROTOBUF_C_FOUND)
