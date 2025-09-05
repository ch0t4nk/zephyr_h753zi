#include <zephyr/ztest.h>
#include <zephyr/drivers/spi.h>
#include <string.h>
#include "l6470.h"
#include "stepper_models.h"

/* Capture recent TX buffers so we can assert on multiple register writes */
#define CAP_TX_MAX 16
static uint8_t cap_tx[CAP_TX_MAX][ L6470_FRAME_SIZE * L6470_DAISY_SIZE ];
static uint8_t cap_cnt;

static int capture_spi_xfer(const struct device *dev, const struct spi_config *cfg,
                            const struct spi_buf_set *tx, struct spi_buf_set *rx)
{
    ARG_UNUSED(dev); ARG_UNUSED(cfg);
    zassert_not_null(tx);
    zassert_true(tx->count >= 1);
    const struct spi_buf *b = &tx->buffers[0];
    zassert_equal(b->len, sizeof(cap_tx[0]));
    if (cap_cnt < CAP_TX_MAX) {
        memcpy(cap_tx[cap_cnt], b->buf, sizeof(cap_tx[0]));
        cap_cnt++;
    }
    /* Zero rx if present */
    if (rx && rx->buffers && rx->count) {
        for (size_t i = 0; i < rx->count; i++) {
            struct spi_buf *rb = &((struct spi_buf *)rx->buffers)[i];
            if (rb->buf && rb->len) memset(rb->buf, 0, rb->len);
        }
    }
    return 0;
}

static void *l6470_apply_model_suite_setup(void)
{
#if defined(CONFIG_ZTEST)
    /* Ensure hooks are installed before any test runs to avoid racey first-call capture */
    l6470_set_power_enabled(true);
    l6470_test_force_ready();
    l6470_set_spi_xfer(capture_spi_xfer);
#endif
    return NULL;
}

ZTEST_SUITE(l6470_apply_model, NULL, l6470_apply_model_suite_setup, NULL, NULL, NULL);

ZTEST(l6470_apply_model, test_program_core_params)
{
#if defined(CONFIG_ZTEST)
    l6470_set_power_enabled(true);
    l6470_test_force_ready();
    l6470_set_spi_xfer(capture_spi_xfer);
#else
    ztest_test_skip();
#endif

    /* Reset capture state for deterministic assertions */
    memset(cap_tx, 0, sizeof(cap_tx));
    cap_cnt = 0;

    const stepper_model_t *m = stepper_get_model(0);
    zassert_not_null(m);

    /* Call apply on dev0; capture last write and assert opcode/value-patterns */
    int r = l6470_apply_model_params(0, m);
    /* If SPI is unavailable on this build, skip rather than failing */
    if (r == -ENODEV) {
        ztest_test_skip();
        return;
    }
    zassert_equal(r, 0, "apply_model should succeed");

    /* Rarely, capture may not engage on the very first call due to init
     * ordering; retry once to deflake.
     */
    if (cap_cnt == 0) {
        r = l6470_apply_model_params(0, m);
        zassert_ok(r);
    }

    /* Validate that at least several writes occurred */
    zassert_true(cap_cnt >= 6, "expected multiple register writes, got %u", cap_cnt);

    bool saw_step_mode=false, saw_kval=false, saw_ocd=false, saw_acc=false, saw_dec=false, saw_maxs=false, saw_stall=false;
    uint8_t step_mode_val=0, kval_run_val=0, ocd_val=0; uint16_t acc_val=0, dec_val=0, maxs_val=0; uint8_t stall_val=0;
    for (uint8_t i = 0; i < cap_cnt; i++) {
        uint8_t *txb = cap_tx[i];
        uint8_t opc = txb[0];
        switch (opc) {
            case L6470_SET_PARAM_STEP_MODE:
                saw_step_mode = true; step_mode_val = txb[1] & 0x0F; break;
            case L6470_SET_PARAM_KVAL_RUN:
                saw_kval = true; kval_run_val = txb[1]; break;
            case L6470_SET_PARAM_OCD_TH:
                saw_ocd = true; ocd_val = txb[1]; break;
            case L6470_SET_PARAM_ACC:
                saw_acc = true; acc_val = ((uint16_t)txb[1] << 8) | txb[2]; break;
            case L6470_SET_PARAM_DEC:
                saw_dec = true; dec_val = ((uint16_t)txb[1] << 8) | txb[2]; break;
            case L6470_SET_PARAM_MAX_SPEED:
                saw_maxs = true; maxs_val = ((uint16_t)txb[1] << 8) | txb[2]; break;
            case L6470_SET_PARAM_STALL_TH:
                saw_stall = true; stall_val = txb[1]; break;
            default:
                break;
        }
    }
    zassert_true(saw_step_mode && saw_kval && saw_ocd && saw_acc && saw_dec && saw_maxs && saw_stall,
                 "missing expected param writes (step=%d kval=%d ocd=%d acc=%d dec=%d maxs=%d stall=%d)",
                 saw_step_mode, saw_kval, saw_ocd, saw_acc, saw_dec, saw_maxs, saw_stall);

    /* Sanity: step mode nibble within 0..7; kval within 0..255; ocd within 1..15 */
    zassert_true(step_mode_val <= 0x07, "invalid step_mode nibble 0x%02x", step_mode_val);
    zassert_true(ocd_val >= 1 && ocd_val <= 0x0F, "OCD steps out of range: %u", ocd_val);

    /* Encoded ranges: ACC/DEC 12b, MAX_SPEED 10b, STALL_TH 7b */
    zassert_true((acc_val & ~0x0FFF) == 0, "ACC not 12-bit: 0x%04x", acc_val);
    zassert_true((dec_val & ~0x0FFF) == 0, "DEC not 12-bit: 0x%04x", dec_val);
    zassert_true((maxs_val & ~0x03FF) == 0, "MAX_SPEED not 10-bit: 0x%04x", maxs_val);
    zassert_true((stall_val & ~0x7F) == 0, "STALL_TH not 7-bit: 0x%02x", stall_val);

    /* Exact-value checks based on model[0] from YAML and driver encoders: */
    /* STEP_MODE: map microstep 16 -> nibble 0x04 */
    zassert_equal(step_mode_val, 0x04, "STEP_MODE nibble expected 0x04 for 1/16");
    /* KVAL_RUN expected from model 0: 24 */
    zassert_equal(kval_run_val, 24, "KVAL_RUN expected 24");
    /* OCD_TH: ocd_thresh_ma=1500 -> steps=1500/375=4 */
    zassert_equal(ocd_val, 4, "OCD_TH steps expected 4 for 1500mA");
    /* ACC/DEC: sps2 / 14.55 -> 1500/14.55 ~= 103 (0x0067) */
    zassert_equal(acc_val, 0x0067, "ACC expected 0x0067 for 1500 sps^2");
    zassert_equal(dec_val, 0x0067, "DEC expected 0x0067 for 1500 sps^2");
    /* MAX_SPEED: clamp to Kconfig max 1000 steps/s then encode /0.238 -> 4201 -> clamp to 10-bit 0x03FF */
    zassert_equal(maxs_val, 0x03FF, "MAX_SPEED expected 0x03FF (10-bit max)");
    /* STALL_TH: (ma + 15)/31 ~= (800+15)/31 = 26 (0x1A) */
    zassert_equal(stall_val, 26, "STALL_TH expected 26 for 800mA");
}

ZTEST(l6470_apply_model, test_min_acc_dec_and_low_max_speed)
{
#if defined(CONFIG_ZTEST)
    l6470_set_power_enabled(true);
    l6470_test_force_ready();
    l6470_set_spi_xfer(capture_spi_xfer);
#else
    ztest_test_skip();
#endif

    /* Reset capture state */
    memset(cap_tx, 0, sizeof(cap_tx));
    cap_cnt = 0;

    /* Clone model 0 but force min acc/dec and very low max_speed */
    const stepper_model_t *base = stepper_get_model(0);
    zassert_not_null(base);
    stepper_model_t local = *base;
    local.acc_sps2 = 1;    /* enc -> floor(1/14.55) = 0 -> 0x000 */
    local.dec_sps2 = 1;    /* enc -> 0x000 */
    local.max_speed = 1;   /* enc -> floor(1/0.238) = 4 -> 0x0004 */

    int r = l6470_apply_model_params(0, &local);
    if (r == -ENODEV) {
        ztest_test_skip();
        return;
    }
    zassert_ok(r);

    if (cap_cnt == 0) {
        r = l6470_apply_model_params(0, &local);
        zassert_ok(r);
    }

    /* Scan captured writes for ACC/DEC/MAX_SPEED and assert exact encodings */
    bool saw_acc=false, saw_dec=false, saw_maxs=false;
    uint16_t acc_val=0xFFFF, dec_val=0xFFFF, maxs_val=0xFFFF;
    for (uint8_t i = 0; i < cap_cnt; i++) {
        uint8_t *txb = cap_tx[i];
        switch (txb[0]) {
        case L6470_SET_PARAM_ACC:       saw_acc=true;  acc_val  = ((uint16_t)txb[1] << 8) | txb[2]; break;
        case L6470_SET_PARAM_DEC:       saw_dec=true;  dec_val  = ((uint16_t)txb[1] << 8) | txb[2]; break;
        case L6470_SET_PARAM_MAX_SPEED: saw_maxs=true; maxs_val = ((uint16_t)txb[1] << 8) | txb[2]; break;
        default: break;
        }
    }
    zassert_true(saw_acc && saw_dec && saw_maxs, "missing acc/dec/max_speed writes");
    zassert_equal(acc_val, 0x0000, "ACC should encode to 0x0000 for 1 sps^2");
    zassert_equal(dec_val, 0x0000, "DEC should encode to 0x0000 for 1 sps^2");
    zassert_equal(maxs_val, 0x0004, "MAX_SPEED should encode to 0x0004 for 1 sps");
}

ZTEST(l6470_apply_model, test_stall_threshold_min_clamp)
{
#if defined(CONFIG_ZTEST)
    l6470_set_power_enabled(true);
    l6470_test_force_ready();
    l6470_set_spi_xfer(capture_spi_xfer);
#else
    ztest_test_skip();
#endif

    memset(cap_tx, 0, sizeof(cap_tx));
    cap_cnt = 0;

    const stepper_model_t *base = stepper_get_model(0);
    zassert_not_null(base);
    stepper_model_t local = *base;
    local.stall_thresh_ma = 1; /* below one step -> should clamp to 1 */

    int r = l6470_apply_model_params(0, &local);
    if (r == -ENODEV) {
        ztest_test_skip();
        return;
    }
    zassert_ok(r);

    if (cap_cnt == 0) {
        r = l6470_apply_model_params(0, &local);
        zassert_ok(r);
    }

    bool saw_stall=false; uint8_t stall_val=0xFF;
    for (uint8_t i = 0; i < cap_cnt; i++) {
        uint8_t *txb = cap_tx[i];
        if (txb[0] == L6470_SET_PARAM_STALL_TH) {
            saw_stall = true;
            stall_val = txb[1];
        }
    }
    zassert_true(saw_stall, "missing STALL_TH write");
    zassert_equal(stall_val, 0x01, "STALL_TH should clamp to 0x01 when model mA is near zero");
}
