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
