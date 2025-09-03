#if defined(CONFIG_ZTEST)
#include <zephyr/drivers/spi.h>
#include <zephyr/ztest.h>
#include "l6470.h"

/* Strict SPI mock for daisy-chain L6470: validates full TX buffer for
 * two devices (L6470_DAISY_SIZE). It checks for RUN/MOVE opcodes in the
 * first byte of each device frame and accepts other frames (NOP/GET_STATUS).
 * Returns 0 on success, negative errno on mismatch.
 */

static int strict_spi_xfer(const struct device *dev, const struct spi_config *cfg,
                           const struct spi_buf_set *tx, struct spi_buf_set *rx)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cfg);

    /* Expect contiguous tx buffers: single buffer with L6470_FRAME_SIZE * L6470_DAISY_SIZE bytes */
    if (!tx || tx->buffers == NULL || tx->count == 0) {
        zassert_not_null(tx, "tx must be provided");
        return -EINVAL;
    }

    const struct spi_buf *b = &tx->buffers[0];
    const uint8_t *data = (const uint8_t *)b->buf;
    size_t len = b->len;

    zassert_equal(len, L6470_FRAME_SIZE * L6470_DAISY_SIZE,
                  "unexpected tx len: %zu", len);

    /* Validate each device frame */
    for (int dev_idx = 0; dev_idx < L6470_DAISY_SIZE; dev_idx++) {
        const uint8_t *frame = data + dev_idx * L6470_FRAME_SIZE;
        uint8_t opcode = frame[0];

        /* Accept RUN (0x50) and MOVE (0x40) opcodes and NOP/GET_STATUS
         * but fail on unrecognized opcodes in strict mode.
         */
        switch (opcode) {
        case L6470_RUN:
        case L6470_MOVE:
        case L6470_NOP:
        case L6470_GET_STATUS:
            /* ok */
            break;
        default:
            zassert_true(false, "unexpected opcode 0x%02x in frame %d", opcode, dev_idx);
            return -EINVAL;
        }
    }

    /* If rx buffer present, zero it to indicate no meaningful response in this mock */
    if (rx && rx->buffers && rx->count > 0) {
        for (size_t i = 0; i < rx->count; i++) {
            struct spi_buf *rb = &((struct spi_buf *)rx->buffers)[i];
            if (rb->buf && rb->len) {
                memset(rb->buf, 0, rb->len);
            }
        }
    }

    return 0;
}

/* Simple registration helper for tests */
void strict_spi_mock_register(void)
{
    l6470_set_spi_xfer(strict_spi_xfer);
}

#endif /* CONFIG_ZTEST */
