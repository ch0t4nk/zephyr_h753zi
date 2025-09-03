#include "l6470_codec.h"
#include <string.h>

void pack_run_frame(uint8_t *tx, size_t tx_len, uint8_t dev, uint8_t dir, uint32_t speed)
{
    size_t frames = L6470_FRAME_SIZE * L6470_DAISY_SIZE;
    if (!tx || tx_len < frames) return;
    size_t base = dev * L6470_FRAME_SIZE;
    tx[base + 0] = L6470_RUN | (dir & 0x1);
    tx[base + 1] = (speed >> 16) & 0x0F;
    tx[base + 2] = (speed >> 8) & 0xFF;
    tx[base + 3] = speed & 0xFF;
}

void pack_move_frame(uint8_t *tx, size_t tx_len, uint8_t dev, uint8_t dir, uint32_t steps)
{
    size_t frames = L6470_FRAME_SIZE * L6470_DAISY_SIZE;
    if (!tx || tx_len < frames) return;
    size_t base = dev * L6470_FRAME_SIZE;
    tx[base + 0] = L6470_MOVE | (dir & 0x1);
    tx[base + 1] = (steps >> 16) & 0x03; /* top bits */
    tx[base + 2] = (steps >> 8) & 0xFF;
    tx[base + 3] = steps & 0xFF;
}

void build_get_status_tx(uint8_t *tx, size_t tx_len)
{
    size_t frames = L6470_FRAME_SIZE * L6470_DAISY_SIZE;
    if (!tx || tx_len < frames) return;
    memset(tx, 0, frames);
    for (int i = 0; i < L6470_DAISY_SIZE; i++) {
        tx[i * L6470_FRAME_SIZE + 0] = 0xD0; /* GET_STATUS opcode */
    }
}

uint16_t parse_status_from_rx(const uint8_t *rx, size_t rx_len, uint8_t dev)
{
    size_t frames = L6470_FRAME_SIZE * L6470_DAISY_SIZE;
    if (!rx || rx_len < frames) return 0;
    size_t base = dev * L6470_FRAME_SIZE;
    return ((uint16_t)rx[base + 1] << 8) | rx[base + 2];
}
