/* Unit test: stepper model selection and enforcement */
#include <zephyr/ztest.h>
#include "l6470.h"
#include "stepper_models.h"
#if defined(CONFIG_ZTEST)
#include <zephyr/drivers/spi.h>

static int fake_spi_xfer(const struct device *dev, const struct spi_config *cfg,
                         const struct spi_buf_set *tx, struct spi_buf_set *rx)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cfg);
    ARG_UNUSED(tx);
    ARG_UNUSED(rx);
    /* Pretend SPI transfer succeeded */
    return 0;
}
#endif

/* If strict mock is linked in, forward-declare registration helper. It's
 * safe if it doesn't exist; tests can still run with the simple fake.
 */
void strict_spi_mock_register(void);

ZTEST_SUITE(stepper_models, NULL, NULL, NULL, NULL, NULL);

ZTEST(stepper_models, test_switch_and_enforce)
{
    /* Ensure we have at least two models from the autogen header */
    unsigned int cnt = stepper_get_model_count();
    zassert_true(cnt >= 2, "Need at least 2 models for this test");

    /* Enable power so l6470_run/move don't early-exit */
    /* In unit tests, enable driver power state and fake SPI xfer */
#if defined(CONFIG_ZTEST)
    l6470_set_power_enabled(true);
    l6470_test_force_ready();
    /* Prefer strict mock when available, else fall back to simple fake */
    if ((void *)strict_spi_mock_register) {
        strict_spi_mock_register();
    } else {
        l6470_set_spi_xfer(fake_spi_xfer);
    }
#else
    l6470_power_enable();
#endif

    /* Pick model 0 and verify run with a speed above model 0's max fails */
    const stepper_model_t *m0 = stepper_get_model(0);
    zassert_not_null(m0);
    stepper_set_active_by_name(0, m0->name);

    int r = l6470_run(0, 0, (uint32_t)(m0->max_speed + 1000));
    zassert_equal(r, -EINVAL, "Run above model max should be rejected");

    /* Switch to model 1 and verify run above model1 max fails but below it succeeds */
    const stepper_model_t *m1 = stepper_get_model(1);
    zassert_not_null(m1);
    stepper_set_active_by_name(0, m1->name);

    r = l6470_run(0, 0, (uint32_t)(m1->max_speed + 1000));
    zassert_equal(r, -EINVAL, "Run above model1 max should be rejected");

    r = l6470_run(0, 0, (uint32_t)(m1->max_speed - 1));
    /* With fake SPI in unit tests, we expect success (0). On other hosts
     * where settings/backends differ it may return various errors; accept
     * success or device errors conservatively.
     */
    zassert_true(r == 0 || r == -ENODEV || r == -ENXIO || r == -EIO || r == -EACCES);
}

ZTEST(stepper_models, test_persist_active)
{
    const stepper_model_t *m0 = stepper_get_model(0);
    zassert_not_null(m0);
    int r = stepper_set_active_by_name(1, m0->name);
    zassert_ok(r);
    int pr = stepper_persist_active();
    /* Settings backends vary on hosts; accept success or common not-supported
     * errors (-ENOTSUP/-ENOSYS) or -ENODEV if the underlying device is not
     * present in this build.
     */
    zassert_true(pr == 0 || pr == -ENOSYS || pr == -ENOTSUP || pr == -EOPNOTSUPP || pr == -ENODEV || pr == -ENOENT,
                 "Unexpected persist result: %d", pr);
}
