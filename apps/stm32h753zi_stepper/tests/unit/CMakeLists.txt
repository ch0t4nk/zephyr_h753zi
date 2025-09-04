cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(l6470_codec_unit)

# Ensure the app src directory is on the include path so tests can include l6470.h
zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../src)

# Generate stepper_models_autogen.h for unit tests in the build tree
find_program(PYTHON3_EXECUTABLE python3)
if(NOT PYTHON3_EXECUTABLE)
    message(FATAL_ERROR "python3 not found")
endif()
set(STEPPER_YML ${CMAKE_CURRENT_SOURCE_DIR}/../../stepper_models.yml)
set(GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GEN_DIR})
set(STEPPER_OUT ${GEN_DIR}/stepper_models_autogen.h)
add_custom_command(
    OUTPUT ${STEPPER_OUT}
    COMMAND ${PYTHON3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../../scripts/gen_stepper_models.py ${STEPPER_YML} ${STEPPER_OUT}
    DEPENDS ${STEPPER_YML} ${CMAKE_CURRENT_SOURCE_DIR}/../../scripts/gen_stepper_models.py
    COMMENT "Generating unit-test stepper models header"
)
add_custom_target(unit_gen_models DEPENDS ${STEPPER_OUT})
add_dependencies(app unit_gen_models)
zephyr_include_directories(${GEN_DIR})

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

target_sources(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/test_l6470_apply_model.c
)

# Also compile the app's l6470.c so the pure helper implementations are available
target_sources(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/l6470.c
)

target_sources(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/stepper_models.c
)
