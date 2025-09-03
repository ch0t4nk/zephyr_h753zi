# CMake generated Testfile for 
# Source directory: /home/joeyw/zephyr-dev/apps/stm32h753zi_stepper/tests/unit
# Build directory: /home/joeyw/zephyr-dev/build/native_unit
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[l6470_unit_tests]=] "/home/joeyw/zephyr-dev/build/native_unit/zephyr/zephyr.exe")
set_tests_properties([=[l6470_unit_tests]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/joeyw/zephyr-dev/apps/stm32h753zi_stepper/tests/unit/CMakeLists.txt;45;add_test;/home/joeyw/zephyr-dev/apps/stm32h753zi_stepper/tests/unit/CMakeLists.txt;0;")
subdirs("zephyr")
