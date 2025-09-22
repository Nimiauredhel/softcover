cmake_minimum_required(VERSION 3.28)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)

set(APP_SOURCE_DIR .)

# project definitions
project(SoftcoverApp VERSION 0.001)

FILE(GLOB APP_SOURCES ${APP_SOURCE_DIR}/*.c)

# target definition
add_library(${SOFTCOVER_APP_FILENAME} MODULE ${APP_SOURCES} ${COMMON_SOURCES})

set_target_properties(${SOFTCOVER_APP_FILENAME} PROPERTIES PREFIX "")
set_target_properties(${SOFTCOVER_APP_FILENAME} PROPERTIES OUTPUT_NAME ${SOFTCOVER_APP_FILENAME})
set_target_properties(${SOFTCOVER_APP_FILENAME} PROPERTIES SUFFIX "")

target_link_libraries(${SOFTCOVER_APP_FILENAME} PUBLIC softcover_common)

target_compile_features(${SOFTCOVER_APP_FILENAME} PRIVATE c_std_99)

target_include_directories(${SOFTCOVER_APP_FILENAME} PUBLIC
    $(APP_SOURCE_DIR)
)

add_dependencies(${SOFTCOVER_APP_FILENAME} copy_assets)
