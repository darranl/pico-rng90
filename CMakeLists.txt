# When built standalone (top-level), initialise the Pico SDK.
# When included via add_subdirectory(), the parent handles this.
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.13)

    set(CMAKE_C_STANDARD 11)
    set(CMAKE_CXX_STANDARD 17)

    set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
    set(PICO_BOARD pico CACHE STRING "Board type")

    include(pico_sdk_import.cmake)

    project(rng90 C CXX ASM)

    pico_sdk_init()
endif()

add_library(rng90 STATIC
    crc.c
    rng90.c
)

target_include_directories(rng90 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(rng90
    PUBLIC hardware_i2c
    PRIVATE pico_stdlib
)