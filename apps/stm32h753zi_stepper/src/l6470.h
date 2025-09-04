/* Minimal L6470 helper for daisy-chain operations (supports 2 devices) */
#ifndef L6470_H_
#define L6470_H_

#include <zephyr/drivers/spi.h>
#include <stdint.h>
#include <stdbool.h>
#include "stepper_models.h"

#define L6470_DAISY_SIZE 2
#define L6470_GET_STATUS 0xD0

/* Frame size used by the driver and tests (opcode + up to 3 parameter bytes) */
#define L6470_FRAME_SIZE 4

/* Common opcodes moved to header so unit tests can reference them */
#define L6470_NOP      0x00
#define L6470_MOVE     0x40
#define L6470_RUN      0x50
#define L6470_SOFTSTOP 0xB0
#define L6470_HARDSTOP 0xB8
#define L6470_SOFTHIZ  0xA0
#define L6470_HARDHIZ  0xA8
/* Positioning and utility opcodes (subset) */
#define L6470_GOTO     0x60    /* GOTO ABS_POS */
#define L6470_GOTO_DIR 0x68    /* GOTO_DIR | dir */
#define L6470_GO_HOME  0x70
#define L6470_GO_MARK  0x78
#define L6470_RESET_POS 0xD8   /* Reset ABS_POS to 0 */
#define L6470_GET_PARAM_BASE 0x20 /* GET_PARAM | param_addr */
#define L6470_GET_PARAM_OP(addr) (uint8_t)(L6470_GET_PARAM_BASE | ((addr) & 0x1F))
#if defined(CONFIG_ZTEST)
/* Test helpers: allow unit tests to substitute a fake spi_transceive and
 * to force the driver into ready state without a real SPI device.
 */
typedef int (*l6470_spi_xfer_t)(const struct device *dev, const struct spi_config *cfg,
								const struct spi_buf_set *tx, struct spi_buf_set *rx);
void l6470_set_spi_xfer(l6470_spi_xfer_t fn);
void l6470_test_force_ready(void);
void l6470_set_power_enabled(bool en);
#endif

/* Common SET_PARAM register opcodes for configuration (subset) */
#define L6470_SET_PARAM_STEP_MODE 0x16
#define L6470_SET_PARAM_KVAL_HOLD 0x09
#define L6470_SET_PARAM_KVAL_RUN  0x0A
#define L6470_SET_PARAM_KVAL_ACC  0x0B
#define L6470_SET_PARAM_KVAL_DEC  0x0C
#define L6470_SET_PARAM_OCD_TH    0x13
#define L6470_SET_PARAM_ACC       0x05 /* 2 bytes */
#define L6470_SET_PARAM_DEC       0x06 /* 2 bytes */
#define L6470_SET_PARAM_MAX_SPEED 0x07 /* 2 bytes */
#define L6470_SET_PARAM_STALL_TH  0x14 /* 1 byte */
/* Parameter addresses for GET_PARAM (subset) */
#define L6470_PARAM_ABS_POS  0x01 /* 22-bit signed */
#define L6470_PARAM_MARK     0x03 /* 22-bit signed */

int l6470_init(const struct device *spi_dev, struct spi_config *cfg);
int l6470_get_status_all(uint16_t status_out[L6470_DAISY_SIZE]);
const char *l6470_decode_status(uint16_t status);
bool l6470_is_ready(void);
/* Hardware reset pulse using RESET GPIO from the shield overlay.
 * Returns 0 on success or negative errno.
 */
int l6470_reset_pulse(void);

/* Motion command stubs - not fully implemented yet. Return -ENOSYS.
 * Device index is 0..L6470_DAISY_SIZE-1.
 */
int l6470_run(uint8_t dev, uint8_t dir, uint32_t speed);
int l6470_move(uint8_t dev, uint8_t dir, uint32_t steps);
/* Absolute positioning helpers (fast wins) */
int l6470_goto(uint8_t dev, uint32_t abs_pos);
int l6470_goto_dir(uint8_t dev, uint8_t dir, uint32_t abs_pos);
int l6470_go_home(uint8_t dev);
int l6470_reset_pos(uint8_t dev);
int l6470_get_abs_pos(uint8_t dev, int32_t *out_pos);
/* Stop/Busy helpers */
int l6470_softstop(uint8_t dev);
int l6470_hardstop(uint8_t dev);
int l6470_is_busy(uint8_t dev, bool *out_busy);
int l6470_wait_while_busy(uint8_t dev, int32_t timeout_ms, int32_t poll_ms);

/* Place outputs in Hi-Z: soft or hard. hard=true -> HARDHIZ, hard=false -> SOFTHIZ
 * Returns 0 on success.
 */
int l6470_disable_outputs(uint8_t dev, bool hard);

/* Verbose decoder writes a human-readable description of status into buf
 * Returns number of bytes written (excluding NUL) or negative on error.
 */
int l6470_decode_status_verbose(uint16_t status, char *buf, size_t len);

/* Safety limit accessors (values come from Kconfig defaults or board-specific config) */
uint32_t l6470_get_max_speed_steps_per_sec(void);
uint32_t l6470_get_max_current_ma(void);
uint32_t l6470_get_max_steps_per_command(void);
uint32_t l6470_get_max_microstep(void);
/* Apply a model's L6470 parameters safely to the given device index. */
int l6470_apply_model_params(uint8_t dev, const stepper_model_t *m);

/* Testable helper APIs (pure functions) to build and parse daisy-chain frames.
 * These are safe to call from unit tests and do not touch hardware.
 */
void l6470_pack_run_frames(uint8_t dev, uint8_t dir, uint32_t speed,
						   uint8_t out[L6470_FRAME_SIZE * L6470_DAISY_SIZE]);
void l6470_pack_move_frames(uint8_t dev, uint8_t dir, uint32_t steps,
							uint8_t out[L6470_FRAME_SIZE * L6470_DAISY_SIZE]);
void l6470_pack_get_status_opcodes(uint8_t out[L6470_FRAME_SIZE * L6470_DAISY_SIZE]);
void l6470_parse_status_frames(const uint8_t rx[], uint16_t status_out[L6470_DAISY_SIZE]);

/* Power control for motor supply (enable/disable). Returns 0 on success. */
int l6470_power_enable(void);
int l6470_power_disable(void);
int l6470_power_status(void); /* returns 1 if enabled, 0 if disabled, negative on error */

#endif // L6470_H_
