cmake_minimum_required(VERSION 3.28)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)

FILE(GLOB COMMON_SOURCES *.c)
FILE(GLOB COMMON_HEADERS *.h)

add_library(softcover_common STATIC ${COMMON_SOURCES} ${COMMON_HEADERS})
target_include_directories(softcover_common PUBLIC .)
