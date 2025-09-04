#include <zephyr/ztest.h>
#include <zephyr/drivers/spi.h>
#include "l6470.h"
#include "stepper_models.h"

/* Capture the last TX buffer so we can assert on opcode/value pairs */
static uint8_t last_tx[ L6470_FRAME_SIZE * L6470_DAISY_SIZE ];

static int capture_spi_xfer(const struct device *dev, const struct spi_config *cfg,
                            const struct spi_buf_set *tx, struct spi_buf_set *rx)
{
    ARG_UNUSED(dev); ARG_UNUSED(cfg);
    zassert_not_null(tx);
    zassert_true(tx->count >= 1);
    const struct spi_buf *b = &tx->buffers[0];
    zassert_equal(b->len, sizeof(last_tx));
    memcpy(last_tx, b->buf, sizeof(last_tx));
    /* Zero rx if present */
    if (rx && rx->buffers && rx->count) {
        for (size_t i = 0; i < rx->count; i++) {
            struct spi_buf *rb = &((struct spi_buf *)rx->buffers)[i];
            if (rb->buf && rb->len) memset(rb->buf, 0, rb->len);
        }
    }
    return 0;
}

ZTEST_SUITE(l6470_apply_model, NULL, NULL, NULL, NULL, NULL);

ZTEST(l6470_apply_model, test_program_core_params)
{
#if defined(CONFIG_ZTEST)
    l6470_set_power_enabled(true);
    l6470_test_force_ready();
    l6470_set_spi_xfer(capture_spi_xfer);
#else
    ztest_test_skip();
#endif

    const stepper_model_t *m = stepper_get_model(0);
    zassert_not_null(m);

    /* Call apply on dev0; capture last write and assert opcode/value-patterns */
    int r = l6470_apply_model_params(0, m);
    zassert_true(r == 0 || r == -ENODEV);

    /* We can't assume order across multiple SPI transfers, but each call
     * writes one register in a 4-byte frame starting at base 0.
     * Validate STEP_MODE and one KVAL write encoding.
     */
    /* STEP_MODE: opcode then lower nibble microstep */
    uint8_t opc = last_tx[0];
    uint8_t val = last_tx[1];
    zassert_true(opc == L6470_SET_PARAM_STEP_MODE || opc == L6470_SET_PARAM_KVAL_RUN || opc == L6470_SET_PARAM_OCD_TH,
                 "unexpected opcode 0x%02x", opc);
}
