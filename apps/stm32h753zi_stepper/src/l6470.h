/* Minimal L6470 helper for daisy-chain operations (supports 2 devices)
 *
 * Notes & References:
 *  - L6470 (dSPIN) datasheet: see sections "Serial interface" and
 *    "Digital interface" for command framing and maximum SCLK recommendations.
 *  - For hardware traceability, prefer exposing control pins (power switch,
 *    reset) via device-tree nodes so the binding is explicit in overlays.
 *    Example DT node name suggestions: "ihm02a1-reset", "ihm02a1-vmot-switch".
 */
#ifndef L6470_H_
#define L6470_H_

#include <zephyr/drivers/spi.h>
#include <stdint.h>
#include <stdbool.h>

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
#define L6470_HARDHIZ  0xA8

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
