#include <zephyr/ztest.h>
#include "../src/l6470_codec.h"

ZTEST(l6470_codec, test_pack_run_frame_dev0)
{
    uint8_t tx[L6470_FRAME_SIZE * L6470_DAISY_SIZE];
    memset(tx, 0xFF, sizeof(tx));
    pack_run_frame(tx, sizeof(tx), 0, 1, 0xABCDE);
    /* Check opcode and speed bytes for dev0 */
    zassert_equal(tx[0] & 0xF0, L6470_RUN);
    zassert_equal((tx[1] << 16) | (tx[2] << 8) | tx[3] & 0xFFFF, (0xABCDE & 0xFFFF));
}

ZTEST(l6470_codec, test_pack_move_frame_dev1)
{
    uint8_t tx[L6470_FRAME_SIZE * L6470_DAISY_SIZE];
    memset(tx, 0, sizeof(tx));
    pack_move_frame(tx, sizeof(tx), 1, 0, 0x12345);
    int base = 1 * L6470_FRAME_SIZE;
    zassert_equal(tx[base + 0] & 0xF0, L6470_MOVE);
    zassert_equal(((tx[base+1] & 0x03) << 16) | (tx[base+2] << 8) | tx[base+3], 0x12345 & 0x3FFFFF);
}

ZTEST(l6470_codec, test_build_get_status_tx_and_parse)
{
    uint8_t tx[L6470_FRAME_SIZE * L6470_DAISY_SIZE];
    uint8_t rx[L6470_FRAME_SIZE * L6470_DAISY_SIZE];
    build_get_status_tx(tx, sizeof(tx));
    zassert_equal(tx[0], 0xD0);
    zassert_equal(tx[L6470_FRAME_SIZE], 0xD0);

    /* Simulate rx where device0 status is 0x0A0B and device1 is 0x0102 */
    memset(rx, 0, sizeof(rx));
    rx[1] = 0x0A; rx[2] = 0x0B; /* dev0 */
    rx[L6470_FRAME_SIZE + 1] = 0x01; rx[L6470_FRAME_SIZE + 2] = 0x02; /* dev1 */
    uint16_t s0 = parse_status_from_rx(rx, sizeof(rx), 0);
    uint16_t s1 = parse_status_from_rx(rx, sizeof(rx), 1);
    zassert_equal(s0, 0x0A0B);
    zassert_equal(s1, 0x0102);
}

ZTEST_SUITE(l6470_codec, NULL, NULL, NULL, NULL, NULL);
