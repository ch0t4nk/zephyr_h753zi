cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(stepper_smoke_persist)

target_sources(app PRIVATE
	src/main.c
	src/diag_stepper_shell.c
	../../src/l6470.c
)

target_include_directories(app PRIVATE ../../src)
