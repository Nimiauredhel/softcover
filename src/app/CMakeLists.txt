cmake_minimum_required(VERSION 3.28)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)

set(APP_SOURCE_DIR .)

# project definitions
project(SoftcoverApp VERSION 0.001)

FILE(GLOB APP_SOURCES ${APP_SOURCE_DIR}/*.c)

# target definition
add_library(softcover_app MODULE ${APP_SOURCES} ${COMMON_SOURCES})
target_link_libraries(softcover_app PUBLIC softcover_common)

target_compile_features(softcover_app PRIVATE c_std_99)

target_include_directories(softcover_app PUBLIC
    $(APP_SOURCE_DIR)
)

add_dependencies(softcover_app copy_assets)
