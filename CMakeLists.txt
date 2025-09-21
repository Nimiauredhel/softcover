cmake_minimum_required(VERSION 3.28)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)

set(COMMON_SOURCE_DIR 
    "src/common"
)

set(APP_SOURCE_DIR 
    "src/app"
)

set(PLATFORM_SOURCE_DIR 
    "src/platforms/linux-window"
)

# project definitions
project(Softcover VERSION 0.001)

#configure_file(common_config.h.in common_config.h)

option(SOFTCOVER_DEBUG "Enable runtime debug features" ON)

add_custom_target(copy_assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/assets ${CMAKE_CURRENT_BINARY_DIR}/output
)

add_subdirectory(
    "${COMMON_SOURCE_DIR}"
)
add_subdirectory(
    "${APP_SOURCE_DIR}"
)
add_subdirectory(
    "${PLATFORM_SOURCE_DIR}"
)
