# CMakeLists.txt for stm32h753zi_stepper Zephyr application
cmake_minimum_required(VERSION 3.20.0)

# Add both device tree overlay files for the build
set(DTC_OVERLAY_FILE 
    ${CMAKE_CURRENT_SOURCE_DIR}/nucleo_h753zi.overlay 
    ${CMAKE_CURRENT_SOURCE_DIR}/fstab.overlay
)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(stm32h753zi_stepper)

zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR})
zephyr_link_libraries_ifdef(CONFIG_STEPPER_STATUS_POLL app)
find_program(PYTHON3_EXECUTABLE python3)
if(NOT PYTHON3_EXECUTABLE)
	message(FATAL_ERROR "python3 not found")
endif()

set(STEPPER_YML ${CMAKE_SOURCE_DIR}/stepper_models.yml)
# Place generated files in the app-local build directory so unit/test builds
# and top-level builds both find the file at: <app-root>/build/generated
set(STEPPER_OUT ${CMAKE_SOURCE_DIR}/build/generated/stepper_models_autogen.h)
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/build/generated)
add_custom_command(
		OUTPUT ${STEPPER_OUT}
		COMMAND ${PYTHON3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/gen_stepper_models.py ${STEPPER_YML} ${STEPPER_OUT}
		DEPENDS ${STEPPER_YML} ${CMAKE_SOURCE_DIR}/scripts/gen_stepper_models.py
		COMMENT "Generating stepper models header"
)
target_include_directories(app PRIVATE ${CMAKE_SOURCE_DIR}/build/generated)

target_sources(app PRIVATE
		src/main.c
		src/l6470.c
		src/shell_stepper.c
		src/persist.c
		src/stepper_models.c
		$<$<BOOL:${CONFIG_STEPPER_STATUS_POLL}>:${CMAKE_CURRENT_SOURCE_DIR}/src/status_poll.c>
		${STEPPER_OUT}
)

