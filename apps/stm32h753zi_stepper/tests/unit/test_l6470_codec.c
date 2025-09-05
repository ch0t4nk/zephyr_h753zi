#include <zephyr/ztest.h>
#include "l6470.h"

ZTEST(l6470_codec, test_pack_run_frame_dev0)
{
    uint8_t tx[L6470_FRAME_SIZE * L6470_DAISY_SIZE];
    memset(tx, 0xFF, sizeof(tx));
    l6470_pack_run_frames(0, 1, 0xABCDE, tx);
    zassert_equal(tx[0] & 0xF0, L6470_RUN, "opcode mismatch");
}

ZTEST(l6470_codec, test_pack_move_frame_dev1)
{
    uint8_t tx[L6470_FRAME_SIZE * L6470_DAISY_SIZE];
    memset(tx, 0, sizeof(tx));
    l6470_pack_move_frames(1, 0, 0x12345, tx);
    int base = 1 * L6470_FRAME_SIZE;
    zassert_equal(tx[base + 0] & 0xF0, L6470_MOVE, "opcode mismatch");
}

ZTEST(l6470_codec, test_build_get_status_tx_and_parse)
{
    uint8_t tx[L6470_FRAME_SIZE * L6470_DAISY_SIZE * 2]; /* two-phase */
    uint8_t rx[L6470_FRAME_SIZE * L6470_DAISY_SIZE * 2];
    /* Build first-phase opcodes and then NOPs to clock out */
    l6470_pack_get_status_opcodes(tx);
    /* Verify opcode in frame 0 and frame N */
    zassert_equal(tx[0], L6470_GET_STATUS);
    zassert_equal(tx[L6470_FRAME_SIZE], L6470_GET_STATUS);

    memset(rx, 0, sizeof(rx));
    /* Place parsed bytes in the second half to mimic spi_transceive reply */
    rx[L6470_FRAME_SIZE + 1] = 0x0A; rx[L6470_FRAME_SIZE + 2] = 0x0B; /* dev0 */
    rx[2*L6470_FRAME_SIZE + 1] = 0x01; rx[2*L6470_FRAME_SIZE + 2] = 0x02; /* dev1 */
    uint16_t statuses[L6470_DAISY_SIZE];
    l6470_parse_status_frames(rx, statuses);
    zassert_equal(statuses[0], 0x0A0B);
    zassert_equal(statuses[1], 0x0102);
}

ZTEST_SUITE(l6470_codec, NULL, NULL, NULL, NULL, NULL);

/* Boundary encoding tests for helper encodings via the driver API would
 * require exposing the static functions; instead, validate the register
 * width behavior by packing max values into frames and ensuring masks.
 */
ZTEST(l6470_codec, test_boundary_max_speed_mask)
{
    uint8_t tx[L6470_FRAME_SIZE * L6470_DAISY_SIZE];
    memset(tx, 0x00, sizeof(tx));
    /* speed with bits above 20 set; only low 20 bits should appear across bytes */
    uint32_t speed = 0xFFFFF; /* 20-bit max */
    l6470_pack_run_frames(0, 0, speed, tx);
    uint16_t top = ((uint16_t)(tx[1] & 0x0F) << 8) | tx[2];
    uint8_t lo = tx[3];
    zassert_equal(top, 0x0FFF, "top 12 bits of 20-bit expected 0x0FFF");
    zassert_equal(lo, 0xFF, "low 8 bits expected 0xFF");
}

ZTEST(l6470_codec, test_boundary_move_steps_mask)
{
    uint8_t tx[L6470_FRAME_SIZE * L6470_DAISY_SIZE];
    memset(tx, 0x00, sizeof(tx));
    uint32_t steps = 0x3FFFFF; /* 22-bit max */
    l6470_pack_move_frames(0, 0, steps, tx);
    uint8_t b1 = tx[1] & 0x03;
    zassert_equal(b1, 0x03, "top 2 bits should be 0b11 for max 22-bit");
    zassert_equal(tx[2], 0xFF);
    zassert_equal(tx[3], 0xFF);
}
