#include <zephyr/ztest.h>
#include "l6470.h"
#include "stepper_models.h"

ZTEST_SUITE(l6470_limits, NULL, NULL, NULL, NULL, NULL);

ZTEST(l6470_limits, test_run_rejects_high_speed)
{
    const stepper_model_t *m = stepper_get_model(0);
    zassert_not_null(m);
    int ret = l6470_run(0, 1, m->max_speed + 100);
    zassert_true(ret == -EINVAL || ret == -EACCES || ret == -ENODEV, "expected error when running above max speed");
}

ZTEST(l6470_limits, test_move_rejects_too_many_steps)
{
    uint32_t too_many = l6470_get_max_steps_per_command() + 1;
    int ret = l6470_move(0, 1, too_many);
    zassert_true(ret == -EINVAL || ret == -EACCES || ret == -ENODEV, "expected error when moving too many steps");
}
