cmake_minimum_required(VERSION 3.28)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)

set(PLATFORM_SOURCE_DIR .)

# project definitions
project(SoftcoverPlatformLinuxTerminal VERSION 0.001)

FILE(GLOB PLATFORM_SOURCES ${PLATFORM_SOURCE_DIR}/*.c)

# target definition
add_executable(softcover_platform_linux_terminal ${PLATFORM_SOURCES} ${COMMON_SOURCES})
target_link_libraries(softcover_platform_linux_terminal PUBLIC softcover_common ncurses portaudio)

target_compile_features(softcover_platform_linux_terminal PRIVATE c_std_99)

target_include_directories(softcover_platform_linux_terminal PUBLIC
    $(PLATFORM_SOURCE_DIR)
)
