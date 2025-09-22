cmake_minimum_required(VERSION 3.28)

# set initial softcover variables

SET(SOFTCOVER_SOURCE_DIRECTORY ${CMAKE_SOURCE_DIR}/src)
SET(SOFTCOVER_ASSETS_DIRECTORY ${CMAKE_SOURCE_DIR}/assets)
SET(SOFTCOVER_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../output)

SET(SOFTCOVER_APP_FILENAME "softcover_app")
add_compile_definitions(SOFTCOVER_APP_FILENAME="${SOFTCOVER_APP_FILENAME}")

SET(SOFTCOVER_DEFAULT_PLATFORM "linux-window")
SET(SOFTCOVER_TARGET_PLATFORM ${SOFTCOVER_DEFAULT_PLATFORM} CACHE STRING "Current target platform")

set(COMMON_SOURCE_DIR 
    "${SOFTCOVER_SOURCE_DIRECTORY}/common"
)

set(APP_SOURCE_DIR 
    "${SOFTCOVER_SOURCE_DIRECTORY}/app"
)

set(PLATFORM_SOURCE_DIR 
    "${SOFTCOVER_SOURCE_DIRECTORY}/platforms/${SOFTCOVER_TARGET_PLATFORM}"
)

# set derived cmake variables

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${SOFTCOVER_OUTPUT_DIRECTORY})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${SOFTCOVER_OUTPUT_DIRECTORY})

# project definition
project(Softcover VERSION 0.001)

# TODO: use and maintain a config file following this example
#configure_file(common_config.h.in common_config.h)

# TODO: use this flag
option(SOFTCOVER_DEBUG "Enable runtime debug features" ON)

# copying assets to the output directory
add_custom_target(copy_assets COMMAND
    ${CMAKE_COMMAND} -E copy_directory ${SOFTCOVER_ASSETS_DIRECTORY} ${SOFTCOVER_OUTPUT_DIRECTORY}
)

# copying compile commands back to the source directory
add_custom_target( copy-compile-commands ALL
    ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_BINARY_DIR}/compile_commands.json
    ${SOFTCOVER_SOURCE_DIRECTORY}
)

# adding the Softcover modules as CMake subdirectories (each contains its own CMakeLists.txt)

## common source and header files, built into both the app and platform layers.
## TODO: build and link as dynamic library?
add_subdirectory(
    "${COMMON_SOURCE_DIR}"
)
## the platform-agnostic app layer;
## built as a module library, loaded by the platform layer at runtime.
add_subdirectory(
    "${APP_SOURCE_DIR}"
)
## the platform layer, the project's executable entry-point.
add_subdirectory(
    "${PLATFORM_SOURCE_DIR}"
)
