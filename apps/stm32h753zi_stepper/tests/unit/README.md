Unit tests for L6470 codec using ztest. Build with native_sim target:

. $ZEPHYR_BASE/zephyr-env.sh
west build -b native_sim apps/stm32h753zi_stepper/tests/unit -d build/native_unit
west build -t run -d build/native_unit  # or run the produced binary directly
