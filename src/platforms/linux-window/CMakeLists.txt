cmake_minimum_required(VERSION 3.28)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)

set(PLATFORM_SOURCE_DIR .)

# project definitions
project(SoftcoverPlatformLinuxWindow VERSION 0.001)

FILE(GLOB PLATFORM_SOURCES ${PLATFORM_SOURCE_DIR}/*.c)

# target definition
add_executable(softcover_platform_linux_window ${PLATFORM_SOURCES} ${COMMON_SOURCES})
target_link_libraries(softcover_platform_linux_window PUBLIC softcover_common SDL2 SDL2main SDL2_ttf portaudio)

target_compile_features(softcover_platform_linux_window PRIVATE c_std_99)

target_include_directories(softcover_platform_linux_window PUBLIC
    $(PLATFORM_SOURCE_DIR)
)
