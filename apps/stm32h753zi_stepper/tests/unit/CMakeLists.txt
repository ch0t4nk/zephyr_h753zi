cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(l6470_codec_unit)

# Ensure the app src directory is on the include path so tests can include l6470.h
zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../src)
zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../build/generated)

target_sources(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/test_l6470_codec.c
)

target_sources(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/test_l6470_limits.c
)

target_sources(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/test_stepper_models.c
)

target_sources(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/test_l6470_strict_frames.c
)

target_sources(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/strict_spi_mock.c
)

# Also compile the app's l6470.c so the pure helper implementations are available
target_sources(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/l6470.c
)

target_sources(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/stepper_models.c
)
