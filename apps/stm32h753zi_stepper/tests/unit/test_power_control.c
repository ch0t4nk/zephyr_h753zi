#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include "l6470.h"

/* This unit test validates the power enable/disable API. On native_sim the
 * DT-backed GPIO will not be present; use the ztest helper to force the
 * driver's power state and validate the software-visible status.
 */

ZTEST_SUITE(power_control_tests, NULL, NULL, NULL, NULL, NULL);

ZTEST(power_control_tests, test_power_state_hooks)
{
    /* Force power enabled via test helper and verify l6470_power_status() */
#if defined(CONFIG_ZTEST)
    l6470_set_power_enabled(true);
    zassert_equal(l6470_power_status(), 1);
    l6470_set_power_enabled(false);
    zassert_equal(l6470_power_status(), 0);
#else
    ztest_test_skip();
#endif
}

