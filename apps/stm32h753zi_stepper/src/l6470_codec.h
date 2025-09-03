/* L6470 codec helpers: pure functions to pack RUN/MOVE frames and parse status
 * Designed for unit testing and simulation (no hardware access).
 */
#ifndef L6470_CODEC_H_
#define L6470_CODEC_H_

#include <stdint.h>
#include <stddef.h>

#define L6470_DAISY_SIZE 2
#define L6470_FRAME_SIZE 4

#define L6470_NOP      0x00
#define L6470_MOVE     0x40
#define L6470_RUN      0x50

/* Pack a RUN frame for device `dev` (0..L6470_DAISY_SIZE-1) into `tx` buffer
 * tx must be at least L6470_FRAME_SIZE * L6470_DAISY_SIZE bytes long; the
 * function will leave other device frames unchanged.
 */
void pack_run_frame(uint8_t *tx, size_t tx_len, uint8_t dev, uint8_t dir, uint32_t speed);

/* Pack a MOVE frame for device `dev` */
void pack_move_frame(uint8_t *tx, size_t tx_len, uint8_t dev, uint8_t dir, uint32_t steps);

/* Build a GET_STATUS opcode sequence (one opcode per device) into tx */
void build_get_status_tx(uint8_t *tx, size_t tx_len);

/* Parse a status word from rx buffer for device `dev`. Assumes a linear
 * frame layout where each device has L6470_FRAME_SIZE bytes and status
 * is located at offset +1..+2 inside its frame.
 */
uint16_t parse_status_from_rx(const uint8_t *rx, size_t rx_len, uint8_t dev);

#endif // L6470_CODEC_H_
