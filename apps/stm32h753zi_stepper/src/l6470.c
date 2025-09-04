#include "l6470.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <stdio.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/devicetree.h>
#include <zephyr/sys/printk.h>
#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/types.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/init.h>
/* stepper_models.h is pulled in by l6470.h */

LOG_MODULE_REGISTER(l6470, LOG_LEVEL_INF);

#ifndef CLAMP
#define CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))
#endif

static const struct device *g_spi = NULL;
static struct spi_config g_cfg;
static bool g_ready = false;
/* Test hook: allow replacing spi_transceive for unit tests */
#if defined(CONFIG_ZTEST)
static l6470_spi_xfer_t g_test_xfer = NULL;
void l6470_set_spi_xfer(l6470_spi_xfer_t fn)
{
    g_test_xfer = fn;
}
void l6470_test_force_ready(void)
{
    g_ready = true;
}
#endif

/* L6470 application opcodes (from datasheet / ST BSP) */
#define L6470_NOP      0x00
#define L6470_MOVE     0x40
#define L6470_RUN      0x50
#define L6470_SOFTSTOP 0xB0
#define L6470_HARDSTOP 0xB8
#define L6470_SOFTHIZ  0xA0
#define L6470_HARDHIZ  0xA8
#define L6470_GOTO     0x60
#define L6470_GOTO_DIR 0x68
#define L6470_GO_HOME  0x70
#define L6470_GO_MARK  0x78
#define L6470_RESET_POS 0xD8
#define L6470_GET_PARAM_BASE 0x20

/* RESET GPIO from overlay node label 'ihm02a1_reset' */
#define IHM_RESET_NODE DT_NODELABEL(ihm02a1_reset)
/* RESET GPIO from overlay node label 'ihm02a1_reset' if present; provide a
 * harmless fallback when building for native_sim or when the overlay is
 * not applied.
 */
#if defined(CONFIG_BOARD_NUCLEO_H753ZI)
/* Board-specific fallback: PE14 (Arduino D4) is wired to STBY/RESET */
static const struct gpio_dt_spec reset_spec = (struct gpio_dt_spec){
    .port = DEVICE_DT_GET(DT_NODELABEL(gpioe)),
    .pin = 14,
    .dt_flags = GPIO_ACTIVE_LOW,
};
#elif DT_NODE_HAS_PROP(IHM_RESET_NODE, gpios)
static const struct gpio_dt_spec reset_spec = GPIO_DT_SPEC_GET(IHM_RESET_NODE, gpios);
#else
static const struct gpio_dt_spec reset_spec = (struct gpio_dt_spec){ .port = NULL, .pin = 0, .dt_flags = 0 };
#endif
static const uint32_t pwr_pin = 11; /* PE11, Arduino D5 */
static const struct device *pwr_gpio = NULL;
static bool pwr_enabled = false;

#if defined(CONFIG_ZTEST)
void l6470_set_power_enabled(bool en)
{
    pwr_enabled = en;
}
#endif

/* --- Minimal register write helpers (conservative, single-byte fields) --- */
static int l6470_write_reg_1b(uint8_t dev, uint8_t opcode, uint8_t val)
{
    if (!g_spi) return -ENODEV;
    uint8_t tx[L6470_FRAME_SIZE * L6470_DAISY_SIZE];
    uint8_t rx[sizeof(tx)];
    memset(tx, L6470_NOP, sizeof(tx));
    memset(rx, 0, sizeof(rx));
    int base = (int)dev * L6470_FRAME_SIZE;
    tx[base + 0] = opcode;
    tx[base + 1] = val;
    tx[base + 2] = 0x00;
    tx[base + 3] = 0x00;
    struct spi_buf txb = { .buf = tx, .len = sizeof(tx) };
    struct spi_buf_set tx_set = { .buffers = &txb, .count = 1 };
    struct spi_buf rxb = { .buf = rx, .len = sizeof(rx) };
    struct spi_buf_set rx_set = { .buffers = &rxb, .count = 1 };
    int ret = spi_transceive(g_spi, &g_cfg, &tx_set, &rx_set);
    return ret;
}

static int l6470_write_reg_2b(uint8_t dev, uint8_t opcode, uint16_t val)
{
    if (!g_spi) return -ENODEV;
    uint8_t tx[L6470_FRAME_SIZE * L6470_DAISY_SIZE];
    uint8_t rx[sizeof(tx)];
    memset(tx, L6470_NOP, sizeof(tx));
    memset(rx, 0, sizeof(rx));
    int base = (int)dev * L6470_FRAME_SIZE;
    tx[base + 0] = opcode;
    tx[base + 1] = (val >> 8) & 0xFF;
    tx[base + 2] = (val >> 0) & 0xFF;
    tx[base + 3] = 0x00;
    struct spi_buf txb = { .buf = tx, .len = sizeof(tx) };
    struct spi_buf_set tx_set = { .buffers = &txb, .count = 1 };
    struct spi_buf rxb = { .buf = rx, .len = sizeof(rx) };
    struct spi_buf_set rx_set = { .buffers = &rxb, .count = 1 };
    int ret = spi_transceive(g_spi, &g_cfg, &tx_set, &rx_set);
    return ret;
}

static int l6470_write_reg_3b(uint8_t dev, uint8_t opcode, uint32_t val)
{
    if (!g_spi) return -ENODEV;
    uint8_t tx[L6470_FRAME_SIZE * L6470_DAISY_SIZE];
    uint8_t rx[sizeof(tx)];
    memset(tx, L6470_NOP, sizeof(tx));
    memset(rx, 0, sizeof(rx));
    int base = (int)dev * L6470_FRAME_SIZE;
    tx[base + 0] = opcode;
    tx[base + 1] = (val >> 16) & 0xFF;
    tx[base + 2] = (val >> 8) & 0xFF;
    tx[base + 3] = (val >> 0) & 0xFF;
    struct spi_buf txb = { .buf = tx, .len = sizeof(tx) };
    struct spi_buf_set tx_set = { .buffers = &txb, .count = 1 };
    struct spi_buf rxb = { .buf = rx, .len = sizeof(rx) };
    struct spi_buf_set rx_set = { .buffers = &rxb, .count = 1 };
    int ret = spi_transceive(g_spi, &g_cfg, &tx_set, &rx_set);
    return ret;
}

/* Read a 3-byte parameter (e.g., ABS_POS) using GET_PARAM | addr and then
 * a second phase of NOPs to shift out the response across the daisy chain.
 */
static int l6470_read_param_3b(uint8_t dev, uint8_t param_addr, uint32_t *out)
{
    if (!g_spi || !out) return -ENODEV;
    const size_t frames = L6470_FRAME_SIZE * L6470_DAISY_SIZE;
    uint8_t tx[frames * 2];
    uint8_t rx[sizeof(tx)];
    memset(tx, L6470_NOP, sizeof(tx));
    memset(rx, 0, sizeof(rx));
    /* Phase 1: issue GET_PARAM | addr to the target device */
    int base = (int)dev * L6470_FRAME_SIZE;
    tx[base + 0] = (uint8_t)(L6470_GET_PARAM_BASE | (param_addr & 0x1F));
    /* Phase 2: NOPs to clock out */
    for (size_t i = 0; i < frames; i++) {
        tx[frames + i] = L6470_NOP;
    }
    struct spi_buf txb = { .buf = tx, .len = sizeof(tx) };
    struct spi_buf_set tx_set = { .buffers = &txb, .count = 1 };
    struct spi_buf rxb = { .buf = rx, .len = sizeof(rx) };
    struct spi_buf_set rx_set = { .buffers = &rxb, .count = 1 };
    int ret;
#if defined(CONFIG_ZTEST)
    if (g_test_xfer) ret = g_test_xfer(g_spi, &g_cfg, &tx_set, &rx_set);
    else ret = spi_transceive(g_spi, &g_cfg, &tx_set, &rx_set);
#else
    ret = spi_transceive(g_spi, &g_cfg, &tx_set, &rx_set);
#endif
    if (ret) return ret;
    /* Parse three data bytes from the reply frame of the target device.
     * In our 4-byte-per-device layout, response bytes appear starting at
     * index L6470_FRAME_SIZE + base + 1..3.
     */
    int rbase = L6470_FRAME_SIZE + base;
    uint32_t b1 = rx[rbase + 1];
    uint32_t b2 = rx[rbase + 2];
    uint32_t b3 = rx[rbase + 3];
    *out = (b1 << 16) | (b2 << 8) | b3;
    return 0;
}

/* Encoding helpers based on L6470 datasheet (approximate, conservative).
 * - MAX_SPEED: 10-bit value, units ~ steps/s scaled by factor 0.238 (bits 9:0)
 * - ACC/DEC:   12-bit value, units ~ steps/s^2 scaled by factor 14.55 (bits 11:0)
 * - STALL_TH:  7-bit threshold in mA steps of ~31.25mA (for L6470 STALL_TH)
 * These are simplified encoders to get reasonable values; exact factors can
 * be refined later. We clamp to register width.
 */
static uint16_t l6470_encode_max_speed(uint32_t steps_per_s)
{
    float reg = (float)steps_per_s / 0.238f; /* per ST app note, approx */
    if (reg < 0) reg = 0;
    if (reg > 1023.0f) reg = 1023.0f;
    return (uint16_t)reg & 0x03FF;
}

static uint16_t l6470_encode_acc_dec(uint32_t sps2)
{
    float reg = (float)sps2 / 14.55f; /* approx L6470 scaling */
    if (reg < 0) reg = 0;
    if (reg > 4095.0f) reg = 4095.0f;
    return (uint16_t)reg & 0x0FFF;
}

static uint8_t l6470_encode_stall_th_ma(uint32_t ma)
{
    /* 31.25mA steps; 7-bit */
    uint32_t steps = (ma + 15) / 31; /* round roughly */
    if (steps > 0x7F) steps = 0x7F;
    if (steps < 0x01) steps = 0x01;
    return (uint8_t)steps;
}

/* Apply a subset of model parameters: microstep (STEP_MODE), KVALs, and OCD threshold */
int l6470_apply_model_params(uint8_t dev, const stepper_model_t *m)
{
    if (!m) return -EINVAL;
    if (!g_spi) return -ENODEV;
    /* STEP_MODE (bits 3:0 for microstep). Clamp to device max 128 (encoded). */
    int micro =
#ifdef STEPPER_MODEL_EXTENDED
    m->use_microstep;
#else
    16;
#endif
    if (micro <= 0) micro = 16;
    if ((unsigned)micro > l6470_get_max_microstep()) micro = l6470_get_max_microstep();
    /* L6470 STEP_MODE lower 4 bits encode microstep exponent (00:1, 01:1/2, 10:1/4 ...). Map common values. */
    uint8_t ms_bits = 0; /* default 1 */
    switch (micro) {
        case 1: ms_bits = 0x00; break;
        case 2: ms_bits = 0x01; break;
        case 4: ms_bits = 0x02; break;
        case 8: ms_bits = 0x03; break;
        case 16: ms_bits = 0x04; break;
        case 32: ms_bits = 0x05; break;
        case 64: ms_bits = 0x06; break;
        case 128: ms_bits = 0x07; break;
        default: ms_bits = 0x04; break; /* 1/16 */
    }
    /* STEP_MODE opcode is 0x16 (SET_PARAM for STEP_MODE). In L6470, register write is usually 0x00.. + data.
     * For simplicity with our frame builder, send opcode 0x16 (SET_PARAM STEP_MODE) then one byte.
     */
    const uint8_t OPC_STEP_MODE = L6470_SET_PARAM_STEP_MODE;
    const uint8_t OPC_KVAL_HOLD = L6470_SET_PARAM_KVAL_HOLD;
    const uint8_t OPC_KVAL_RUN  = L6470_SET_PARAM_KVAL_RUN;
    const uint8_t OPC_KVAL_ACC  = L6470_SET_PARAM_KVAL_ACC;
    const uint8_t OPC_KVAL_DEC  = L6470_SET_PARAM_KVAL_DEC;
    const uint8_t OPC_OCD_TH    = L6470_SET_PARAM_OCD_TH;
    const uint8_t OPC_ACC       = L6470_SET_PARAM_ACC;
    const uint8_t OPC_DEC       = L6470_SET_PARAM_DEC;
    const uint8_t OPC_MAX_SPEED = L6470_SET_PARAM_MAX_SPEED;
    const uint8_t OPC_STALL_TH  = L6470_SET_PARAM_STALL_TH;

    int rc = l6470_write_reg_1b(dev, OPC_STEP_MODE, (uint8_t)(ms_bits & 0x0F));
    if (rc) return rc;
    /* KVAL fields are 0..255; use conservative scaling from model integers (0..255 expected). */
    rc = l6470_write_reg_1b(dev, OPC_KVAL_HOLD, (uint8_t)CLAMP(
#ifdef STEPPER_MODEL_EXTENDED
    m->kval_hold
#else
    8
#endif
    , 0, 255)); if (rc) return rc;
    rc = l6470_write_reg_1b(dev, OPC_KVAL_RUN,  (uint8_t)CLAMP(
#ifdef STEPPER_MODEL_EXTENDED
    m->kval_run
#else
    16
#endif
    , 0, 255)); if (rc) return rc;
    rc = l6470_write_reg_1b(dev, OPC_KVAL_ACC,  (uint8_t)CLAMP(
#ifdef STEPPER_MODEL_EXTENDED
    m->kval_acc
#else
    16
#endif
    , 0, 255)); if (rc) return rc;
    rc = l6470_write_reg_1b(dev, OPC_KVAL_DEC,  (uint8_t)CLAMP(
#ifdef STEPPER_MODEL_EXTENDED
    m->kval_dec
#else
    16
#endif
    , 0, 255)); if (rc) return rc;
    /* OCD threshold maps to 375mA steps (per datasheet). Pick conservative from model mA. */
    int ocd_ma =
#ifdef STEPPER_MODEL_EXTENDED
    m->ocd_thresh_ma;
#else
    1500;
#endif
    if (ocd_ma < 375) ocd_ma = 375;
    int ocd_steps = ocd_ma / 375; /* 1..16 typical */
    if (ocd_steps < 1) {
        ocd_steps = 1;
    }
    if (ocd_steps > 0x0F) {
        ocd_steps = 0x0F;
    }
    rc = l6470_write_reg_1b(dev, OPC_OCD_TH, (uint8_t)ocd_steps);
    if (rc) return rc;

    /* ACC/DEC (2 bytes each) from model acc_sps2/dec_sps2 if available */
    uint32_t acc =
#ifdef STEPPER_MODEL_EXTENDED
    (m->acc_sps2 > 0 ? (uint32_t)m->acc_sps2 : 1000U);
#else
    1000U;
#endif
    uint32_t dec =
#ifdef STEPPER_MODEL_EXTENDED
    (m->dec_sps2 > 0 ? (uint32_t)m->dec_sps2 : 1000U);
#else
    1000U;
#endif
    uint16_t acc_reg = l6470_encode_acc_dec(acc);
    uint16_t dec_reg = l6470_encode_acc_dec(dec);
    rc = l6470_write_reg_2b(dev, OPC_ACC, acc_reg); if (rc) return rc;
    rc = l6470_write_reg_2b(dev, OPC_DEC, dec_reg); if (rc) return rc;

    /* MAX_SPEED from model->max_speed (steps/s); clamp by Kconfig getter */
    uint32_t maxs = (uint32_t)CLAMP(m->max_speed, 1, (int)l6470_get_max_speed_steps_per_sec());
    uint16_t maxs_reg = l6470_encode_max_speed(maxs);
    rc = l6470_write_reg_2b(dev, OPC_MAX_SPEED, maxs_reg); if (rc) return rc;

    /* STALL_TH using model stall_thresh_ma */
    uint32_t stall_ma =
#ifdef STEPPER_MODEL_EXTENDED
    (m->stall_thresh_ma > 0 ? (uint32_t)m->stall_thresh_ma : 500U);
#else
    500U;
#endif
    uint8_t stall_reg = l6470_encode_stall_th_ma(stall_ma);
    rc = l6470_write_reg_1b(dev, OPC_STALL_TH, stall_reg);
    return rc;
}

/* Safety limits sourced from Kconfig options. If Kconfig is not present,
 * provide conservative defaults here to keep host/native builds happy.
 */
#ifndef CONFIG_L6470_MAX_SPEED_STEPS_PER_SEC
#define CONFIG_L6470_MAX_SPEED_STEPS_PER_SEC 1000
#endif
#ifndef CONFIG_L6470_MAX_CURRENT_MA
#define CONFIG_L6470_MAX_CURRENT_MA 1500
#endif
#ifndef CONFIG_L6470_MAX_STEPS_PER_COMMAND
#define CONFIG_L6470_MAX_STEPS_PER_COMMAND 8388607
#endif
#ifndef CONFIG_L6470_MAX_MICROSTEP
#define CONFIG_L6470_MAX_MICROSTEP 16
#endif

uint32_t l6470_get_max_speed_steps_per_sec(void)
{
    return (uint32_t)CONFIG_L6470_MAX_SPEED_STEPS_PER_SEC;
}

uint32_t l6470_get_max_current_ma(void)
{
    return (uint32_t)CONFIG_L6470_MAX_CURRENT_MA;
}

uint32_t l6470_get_max_steps_per_command(void)
{
    return (uint32_t)CONFIG_L6470_MAX_STEPS_PER_COMMAND;
}

uint32_t l6470_get_max_microstep(void)
{
    return (uint32_t)CONFIG_L6470_MAX_MICROSTEP;
}

static const stepper_model_t *get_active_model_for_dev(uint8_t dev)
{
    const stepper_model_t *m = stepper_get_active(dev);
    if (m) return m;
    return NULL;
}

int l6470_init(const struct device *spi_dev, struct spi_config *cfg)
{
    if (!device_is_ready(spi_dev)) {
        LOG_ERR("SPI device not ready");
        return -ENODEV;
    }
    g_spi = spi_dev;
    g_cfg = *cfg;
    g_ready = true;
    return 0;
}

/* Frame size per device for simple commands. Use 4 bytes so we can
 * carry opcodes + up to 3 parameter bytes (RUN uses 3-byte speed field,
 * MOVE uses 3-byte steps field). GET_STATUS returns 3 bytes but we
 * allocate a 4-byte frame and ignore the extra byte for simplicity.
 */
#define L6470_FRAME_SIZE 4

/* Read GET_STATUS from a daisy chain of two L6470s.
 * The L6470 GET_STATUS returns 3 bytes per device; we use FRAME_SIZE bytes
 * per device on the bus and parse bytes 1..2 as the status word.
 */
/* The L6470 shifts data: to read a response from device N, you must clock
 * another frame after sending the opcode. Implement a two-phase transfer:
 *  1) Send GET_STATUS to both devices (no read yet)
 *  2) Clock out NOPs and read back 3 bytes per device
 */
int l6470_get_status_all(uint16_t status_out[L6470_DAISY_SIZE])
{
    if (!g_spi) {
        return -ENODEV;
    }

    const size_t frames = L6470_FRAME_SIZE * L6470_DAISY_SIZE;
    uint8_t tx[frames * 2];
    uint8_t rx[frames * 2];
    memset(tx, 0x00, sizeof(tx));
    memset(rx, 0x00, sizeof(rx));
    /* First half: send GET_STATUS opcode in byte 0 of each device frame */
    for (int i = 0; i < L6470_DAISY_SIZE; i++) {
        tx[i * L6470_FRAME_SIZE + 0] = L6470_GET_STATUS;
    }
    /* Second half: send NOPs to clock out reply */
    for (size_t i = 0; i < frames; i++) {
        tx[frames + i] = L6470_NOP;
    }

    struct spi_buf txb = { .buf = tx, .len = sizeof(tx) };
    struct spi_buf_set tx_set = { .buffers = &txb, .count = 1 };
    struct spi_buf rxb = { .buf = rx, .len = sizeof(rx) };
    struct spi_buf_set rx_set = { .buffers = &rxb, .count = 1 };

    int ret;
#if defined(CONFIG_ZTEST)
    if (g_test_xfer) {
        ret = g_test_xfer(g_spi, &g_cfg, &tx_set, &rx_set);
    } else {
        ret = spi_transceive(g_spi, &g_cfg, &tx_set, &rx_set);
    }
#else
    ret = spi_transceive(g_spi, &g_cfg, &tx_set, &rx_set);
#endif
    if (ret) {
        LOG_ERR("spi_transceive(GET_STATUS) failed: %d", ret);
        return ret;
    }

    /* Parse returned bytes from the reply region. The data we care about
     * (status bytes) are shifted within the returned stream; for the
     * unit tests and observed daisy-chain behavior we read starting one
     * frame into the overall buffer and then step by frame-size per
     * device. That maps to indexes: base = L6470_FRAME_SIZE + dev*L6470_FRAME_SIZE
     */
    for (int dev = 0; dev < L6470_DAISY_SIZE; dev++) {
        int base = L6470_FRAME_SIZE + dev * L6470_FRAME_SIZE;
        uint16_t status = ((uint16_t)rx[base + 1] << 8) | rx[base + 2];
        status_out[dev] = status;
    }

    return 0;
}

/* Pure helpers for unit testing */
void l6470_pack_run_frames(uint8_t dev, uint8_t dir, uint32_t speed,
                           uint8_t out[L6470_FRAME_SIZE * L6470_DAISY_SIZE])
{
    memset(out, L6470_NOP, L6470_FRAME_SIZE * L6470_DAISY_SIZE);
    int base = dev * L6470_FRAME_SIZE;
    out[base + 0] = L6470_RUN | (dir & 0x01);
    out[base + 1] = (speed >> 16) & 0x0F;
    out[base + 2] = (speed >> 8) & 0xFF;
    out[base + 3] = speed & 0xFF;
}

void l6470_pack_move_frames(uint8_t dev, uint8_t dir, uint32_t steps,
                            uint8_t out[L6470_FRAME_SIZE * L6470_DAISY_SIZE])
{
    memset(out, L6470_NOP, L6470_FRAME_SIZE * L6470_DAISY_SIZE);
    int base = dev * L6470_FRAME_SIZE;
    out[base + 0] = L6470_MOVE | (dir & 0x01);
    out[base + 1] = (steps >> 16) & 0x03;
    out[base + 2] = (steps >> 8) & 0xFF;
    out[base + 3] = steps & 0xFF;
}

void l6470_pack_get_status_opcodes(uint8_t out[L6470_FRAME_SIZE * L6470_DAISY_SIZE])
{
    memset(out, 0x00, L6470_FRAME_SIZE * L6470_DAISY_SIZE);
    for (int i = 0; i < L6470_DAISY_SIZE; i++) {
        out[i * L6470_FRAME_SIZE + 0] = L6470_GET_STATUS;
    }
}

void l6470_parse_status_frames(const uint8_t rx[], uint16_t status_out[L6470_DAISY_SIZE])
{
    /* Expect rx to contain two-phase data; the meaningful status bytes
     * appear starting one frame into the buffer, then step by frame size
     * for each device.
     */
    for (int dev = 0; dev < L6470_DAISY_SIZE; dev++) {
        int base = L6470_FRAME_SIZE + dev * L6470_FRAME_SIZE; /* two-phase reply layout */
        uint16_t status = ((uint16_t)rx[base + 1] << 8) | rx[base + 2];
        status_out[dev] = status;
    }
}

bool l6470_is_ready(void)
{
    return g_ready && g_spi != NULL;
}

int l6470_reset_pulse(void)
{
    if (reset_spec.port == NULL) {
        LOG_WRN("RESET GPIO not defined in DT; skipping pulse");
        return -ENODEV;
    }
    if (!device_is_ready(reset_spec.port)) {
        LOG_ERR("RESET GPIO not ready");
        return -ENODEV;
    }

    /* Active low reset: drive low for 10ms then release (high) */
    gpio_pin_configure_dt(&reset_spec, GPIO_OUTPUT_ACTIVE);
    gpio_pin_set_dt(&reset_spec, 0);
    k_sleep(K_MSEC(10));
    gpio_pin_set_dt(&reset_spec, 1);
    k_sleep(K_MSEC(5));
    return 0;
}

int l6470_power_enable(void)
{
    if (!pwr_gpio) {
        pwr_gpio = device_get_binding("GPIOE");
        if (!pwr_gpio) {
            LOG_ERR("Power GPIO device not found");
            return -ENODEV;
        }
    }
    if (!device_is_ready(pwr_gpio)) {
        LOG_ERR("Power GPIO not ready");
        return -ENODEV;
    }
    gpio_pin_configure(pwr_gpio, pwr_pin, GPIO_OUTPUT_ACTIVE);
    gpio_pin_set(pwr_gpio, pwr_pin, 1);
    pwr_enabled = true;
    LOG_INF("Motor power enabled");
    /* Apply model parameters conservatively on power-on if available */
    const stepper_model_t *m0 = get_active_model_for_dev(0);
    const stepper_model_t *m1 = get_active_model_for_dev(1);
    if (m0) (void)l6470_apply_model_params(0, m0);
    if (m1) (void)l6470_apply_model_params(1, m1);
    return 0;
}

int l6470_power_disable(void)
{
    if (!pwr_gpio) {
        pwr_gpio = device_get_binding("GPIOE");
        if (!pwr_gpio) {
            LOG_ERR("Power GPIO device not found");
            return -ENODEV;
        }
    }
    if (!device_is_ready(pwr_gpio)) {
        LOG_ERR("Power GPIO not ready");
        return -ENODEV;
    }
    gpio_pin_set(pwr_gpio, pwr_pin, 0);
    pwr_enabled = false;
    LOG_INF("Motor power disabled");
    return 0;
}

int l6470_power_status(void)
{
    return pwr_enabled ? 1 : 0;
}

int l6470_run(uint8_t dev, uint8_t dir, uint32_t speed)
{
    /* Safety: don't run if power disabled */
    if (!pwr_enabled) {
        LOG_WRN("Attempt to run while power disabled");
        return -EACCES;
    }
    /* Enforce configured maximum speed from active model if available, else Kconfig */
    const stepper_model_t *m = get_active_model_for_dev(dev);
    if (m) {
        if ((uint32_t)speed > (uint32_t)m->max_speed) {
            LOG_ERR("Requested speed %u exceeds model max %u", (unsigned)speed, (unsigned)m->max_speed);
            return -EINVAL;
        }
    } else {
        if (speed > l6470_get_max_speed_steps_per_sec()) {
            LOG_ERR("Requested speed %u exceeds configured max %u",
                    (unsigned)speed, (unsigned)l6470_get_max_speed_steps_per_sec());
            return -EINVAL;
        }
    }
    /* Check for latched faults by reading status */
    uint16_t statuses[L6470_DAISY_SIZE] = {0};
    if (l6470_get_status_all(statuses) == 0) {
        if (statuses[dev] & ((1 << 9) | (1 << 10) | (1 << 11))) { /* OCD or step loss */
            LOG_ERR("Device %d has fault, refusing to run", dev);
            return -EFAULT;
        }
    }
    /* Build a daisy-chain packet: each device frame is 3 bytes for simple
     * commands; RUN takes 3 bytes: [RUN | dir], [speed MSB], [speed LSB]
     * Note: the L6470 RUN command encodes speed in 20 bits in the real
     * device; for simplicity we pack lower 16 bits here and let the user
     * refine encoding later.
     */
    uint8_t tx[L6470_FRAME_SIZE * L6470_DAISY_SIZE];
    uint8_t rx[L6470_FRAME_SIZE * L6470_DAISY_SIZE];
    memset(tx, L6470_NOP, sizeof(tx));
    memset(rx, 0, sizeof(rx));

    /* Compose per-device frame. The MSB-most device in the chain is
     * placed first in the tx buffer; device index 'dev' corresponds to
     * zero-based ordering used elsewhere (0..N-1). We'll place the
     * RUN opcode into the appropriate frame.
     */
    int frame = dev; /* 0..N-1 */
    int base = frame * L6470_FRAME_SIZE;
    tx[base + 0] = L6470_RUN | (dir & 0x01);
    /* Speed is a 20-bit value in L6470; pack lower 20 bits across 3 bytes.
     * We send [speed MSB, mid, LSB] in bytes 1..3
     */
    tx[base + 1] = (speed >> 16) & 0x0F; /* only 4 bits here if >16 */
    tx[base + 2] = (speed >> 8) & 0xFF;
    tx[base + 3] = speed & 0xFF;

    struct spi_buf txb = { .buf = tx, .len = sizeof(tx) };
    struct spi_buf_set tx_set = { .buffers = &txb, .count = 1 };
    struct spi_buf rxb = { .buf = rx, .len = sizeof(rx) };
    struct spi_buf_set rx_set = { .buffers = &rxb, .count = 1 };

    int ret;
#if defined(CONFIG_ZTEST)
    if (g_test_xfer) {
        ret = g_test_xfer(g_spi, &g_cfg, &tx_set, &rx_set);
    } else {
        ret = spi_transceive(g_spi, &g_cfg, &tx_set, &rx_set);
    }
#else
    ret = spi_transceive(g_spi, &g_cfg, &tx_set, &rx_set);
#endif
    if (ret) {
        LOG_ERR("spi_transceive RUN failed: %d", ret);
        return ret;
    }

    LOG_INF("RUN cmd sent to dev %d dir=%d speed=%u", dev, dir, speed);
    return 0;
}

int l6470_move(uint8_t dev, uint8_t dir, uint32_t steps)
{
    if (!pwr_enabled) {
        LOG_WRN("Attempt to move while power disabled");
        return -EACCES;
    }
    /* Enforce per-command step limit from active model if available, else Kconfig */
    const stepper_model_t *m = get_active_model_for_dev(dev);
    /* For now, models do not encode a per-command steps limit, so enforce
     * the global Kconfig limit. Keep a debug message when a model is active.
     */
    if (m) {
        LOG_DBG("Active model for dev %u: %s", dev, m->name);
    }
    uint32_t max_steps = l6470_get_max_steps_per_command();
    if (steps > max_steps) {
        LOG_ERR("Requested steps %u exceeds configured max %u", (unsigned)steps, (unsigned)max_steps);
        return -EINVAL;
    }
    uint16_t statuses[L6470_DAISY_SIZE] = {0};
    if (l6470_get_status_all(statuses) == 0) {
        if (statuses[dev] & ((1 << 9) | (1 << 10) | (1 << 11))) {
            LOG_ERR("Device %d has fault, refusing to move", dev);
            return -EFAULT;
        }
    }
    /* MOVE command uses opcode 0x40 and a 22-bit steps parameter in the
     * L6470. For now we send the lower 24 bits across three bytes.
     */
    uint8_t tx[L6470_FRAME_SIZE * L6470_DAISY_SIZE];
    uint8_t rx[L6470_FRAME_SIZE * L6470_DAISY_SIZE];
    memset(tx, L6470_NOP, sizeof(tx));
    memset(rx, 0, sizeof(rx));

    int frame = dev;
    int base = frame * L6470_FRAME_SIZE;
    tx[base + 0] = L6470_MOVE | (dir & 0x01);
    /* MOVE uses up to 22-bit steps; pack lower 22 bits across bytes 1..3 */
    tx[base + 1] = (steps >> 16) & 0x03; /* top 2 bits */
    tx[base + 2] = (steps >> 8) & 0xFF;
    tx[base + 3] = steps & 0xFF;

    struct spi_buf txb = { .buf = tx, .len = sizeof(tx) };
    struct spi_buf_set tx_set = { .buffers = &txb, .count = 1 };
    struct spi_buf rxb = { .buf = rx, .len = sizeof(rx) };
    struct spi_buf_set rx_set = { .buffers = &rxb, .count = 1 };

    int ret;
#if defined(CONFIG_ZTEST)
    if (g_test_xfer) {
        ret = g_test_xfer(g_spi, &g_cfg, &tx_set, &rx_set);
    } else {
        ret = spi_transceive(g_spi, &g_cfg, &tx_set, &rx_set);
    }
#else
    ret = spi_transceive(g_spi, &g_cfg, &tx_set, &rx_set);
#endif
    if (ret) {
        LOG_ERR("spi_transceive MOVE failed: %d", ret);
        return ret;
    }

    LOG_INF("MOVE cmd sent to dev %d dir=%d steps=%u", dev, dir, steps);
    return 0;
}

int l6470_disable_outputs(uint8_t dev, bool hard)
{
    uint8_t tx[L6470_FRAME_SIZE * L6470_DAISY_SIZE];
    uint8_t rx[L6470_FRAME_SIZE * L6470_DAISY_SIZE];
    memset(tx, L6470_NOP, sizeof(tx));
    memset(rx, 0, sizeof(rx));

    int frame = dev;
    int base = frame * L6470_FRAME_SIZE;
    tx[base + 0] = hard ? L6470_HARDHIZ : L6470_SOFTHIZ;
    tx[base + 1] = 0x00;
    tx[base + 2] = 0x00;
    tx[base + 3] = 0x00;

    struct spi_buf txb = { .buf = tx, .len = sizeof(tx) };
    struct spi_buf_set tx_set = { .buffers = &txb, .count = 1 };
    struct spi_buf rxb = { .buf = rx, .len = sizeof(rx) };
    struct spi_buf_set rx_set = { .buffers = &rxb, .count = 1 };

    int ret;
#if defined(CONFIG_ZTEST)
    if (g_test_xfer) {
        ret = g_test_xfer(g_spi, &g_cfg, &tx_set, &rx_set);
    } else {
        ret = spi_transceive(g_spi, &g_cfg, &tx_set, &rx_set);
    }
#else
    ret = spi_transceive(g_spi, &g_cfg, &tx_set, &rx_set);
#endif
    if (ret) {
        LOG_ERR("spi_transceive HIZ failed: %d", ret);
        return ret;
    }
    LOG_INF("HIZ cmd (%s) sent to dev %d", hard ? "HARD" : "SOFT", dev);
    return 0;
}

int l6470_softstop(uint8_t dev)
{
    return l6470_write_reg_1b(dev, L6470_SOFTSTOP, 0);
}

int l6470_hardstop(uint8_t dev)
{
    return l6470_write_reg_1b(dev, L6470_HARDSTOP, 0);
}

int l6470_is_busy(uint8_t dev, bool *out_busy)
{
    if (!out_busy) return -EINVAL;
    uint16_t st[L6470_DAISY_SIZE] = {0};
    int rc = l6470_get_status_all(st);
    if (rc) return rc;
    *out_busy = (st[dev] & (1 << 1)) ? true : false; /* BIT1 BUSY */
    return 0;
}

int l6470_wait_while_busy(uint8_t dev, int32_t timeout_ms, int32_t poll_ms)
{
    int64_t end = k_uptime_get() + (timeout_ms < 0 ? INT64_MAX : (int64_t)timeout_ms);
    int32_t delay = poll_ms > 0 ? poll_ms : 10;
    for (;;) {
        bool busy = false;
        int rc = l6470_is_busy(dev, &busy);
        if (rc) return rc;
        if (!busy) return 0;
        if (k_uptime_get() >= end) return -ETIMEDOUT;
        k_msleep(delay);
    }
}

int l6470_goto(uint8_t dev, uint32_t abs_pos)
{
    if (!pwr_enabled) {
        LOG_WRN("Attempt to goto while power disabled");
        return -EACCES;
    }
    /* ABS_POS is 22-bit signed; we send lower 22 bits. */
    uint32_t pos = abs_pos & 0x3FFFFF;
    return l6470_write_reg_3b(dev, L6470_GOTO, pos);
}

int l6470_goto_dir(uint8_t dev, uint8_t dir, uint32_t abs_pos)
{
    if (!pwr_enabled) {
        LOG_WRN("Attempt to goto_dir while power disabled");
        return -EACCES;
    }
    uint32_t pos = abs_pos & 0x3FFFFF;
    return l6470_write_reg_3b(dev, (uint8_t)(L6470_GOTO_DIR | (dir & 0x01)), pos);
}

int l6470_go_home(uint8_t dev)
{
    if (!pwr_enabled) {
        LOG_WRN("Attempt to home while power disabled");
        return -EACCES;
    }
    return l6470_write_reg_1b(dev, L6470_GO_HOME, 0);
}

int l6470_reset_pos(uint8_t dev)
{
    /* RESET_POS does not require power outputs enabled; safe at any time. */
    return l6470_write_reg_1b(dev, L6470_RESET_POS, 0);
}

int l6470_get_abs_pos(uint8_t dev, int32_t *out_pos)
{
    if (!out_pos) return -EINVAL;
    uint32_t raw = 0;
    int rc = l6470_read_param_3b(dev, L6470_PARAM_ABS_POS, &raw);
    if (rc) return rc;
    /* Sign-extend 22-bit to 32-bit signed */
    int32_t val = (int32_t)(raw & 0x3FFFFF);
    if (val & 0x200000) {
        val |= ~0x3FFFFF; /* set upper bits */
    }
    *out_pos = val;
    return 0;
}

int l6470_decode_status_verbose(uint16_t status, char *buf, size_t len)
{
    if (!buf || len == 0) {
        return -EINVAL;
    }
    int offs = 0;
    offs += snprintf(buf + offs, len - offs, "status=0x%04x;", status);
    if (status & (1 << 9)) {
        offs += snprintf(buf + offs, len - offs, " OCD");
    }
    if (status & (1 << 7)) {
        offs += snprintf(buf + offs, len - offs, " TH_SD");
    }
    if (status & (1 << 5)) {
        offs += snprintf(buf + offs, len - offs, " UVLO");
    }
    if (status & (1 << 10)) {
        offs += snprintf(buf + offs, len - offs, " STEP_LOSS_A");
    }
    if (status & (1 << 11)) {
        offs += snprintf(buf + offs, len - offs, " STEP_LOSS_B");
    }
    if (status & (1 << 1)) {
        offs += snprintf(buf + offs, len - offs, " BUSY");
    }
    return offs;
}

/* Very small human-readable decoder for key status bits.
 * Returns a static string describing major flags/faults.
 */
const char *l6470_decode_status(uint16_t status)
{
    /* Status bit definitions (from L6470 datasheet, abbreviated):
     * BIT 0: HiZ
     * BIT 1: BUSY
     * BIT 2: SW_F
     * BIT 3: SW_EVN
     * BIT 4: WRONG_CMD
     * BIT 5: UVLO
     * BIT 6: OVL
     * BIT 7: TH_SD (thermal shutdown)
     * BIT 8: TH_WRN (thermal warning)
     * BIT 9: OCD (overcurrent)
     * BIT 10: STEP_LOSS_A
     * BIT 11: STEP_LOSS_B
     * BIT 12: SCK_MOD
     */
    static char buf[128];
    buf[0] = '\0';

    if (status == 0) {
        return "NO DATA";
    }

    if (status & (1 << 9)) {
        strcat(buf, "OCD ");
    }
    if (status & (1 << 7)) {
        strcat(buf, "TH_SD ");
    }
    if (status & (1 << 5)) {
        strcat(buf, "UVLO ");
    }
    if (status & (1 << 10)) {
        strcat(buf, "STEP_LOSS_A ");
    }
    if (status & (1 << 11)) {
        strcat(buf, "STEP_LOSS_B ");
    }
    if (status & (1 << 1)) {
        strcat(buf, "BUSY ");
    }
    if (buf[0] == '\0') {
        /* Unknown bits set; print hex */
        snprintf(buf, sizeof(buf), "STATUS=0x%04x", status);
    }
    return buf;
}
