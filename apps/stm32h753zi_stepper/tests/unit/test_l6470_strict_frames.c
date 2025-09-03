#include <zephyr/ztest.h>
#include "l6470.h"
/* Register the strict SPI mock to validate full TX buffers */
void strict_spi_mock_register(void);

/* A small deterministic test that checks the exact frames produced by the
 * l6470_pack_run_frames and l6470_pack_move_frames helpers for a single
 * device. This validates the byte-level protocol expected on the SPI bus.
 */

static uint8_t expected_run_frame[4] = { 0x50, 0x00, 0x03, 0xE8 };
static uint8_t expected_move_frame[4] = { 0x40, 0x00, 0x00, 0x64 };

ZTEST(l6470_strict, test_run_frame_bytes)
{
    strict_spi_mock_register();
    uint8_t buf[L6470_FRAME_SIZE * L6470_DAISY_SIZE];
    memset(buf, 0xFF, sizeof(buf));
    /* dev=0, dir=0, speed=1000 */
    l6470_pack_run_frames(0, 0, 1000, buf);
    zassert_mem_equal(buf + 0, expected_run_frame, 4, "run frame mismatch");
}

ZTEST(l6470_strict, test_move_frame_bytes)
{
    uint8_t buf[L6470_FRAME_SIZE * L6470_DAISY_SIZE];
    memset(buf, 0xFF, sizeof(buf));
    /* dev=0, dir=0, steps=100 */
    l6470_pack_move_frames(0, 0, 100, buf);
    zassert_mem_equal(buf + 0, expected_move_frame, 4, "move frame mismatch");
}

ZTEST_SUITE(l6470_strict, NULL, NULL, NULL, NULL, NULL);
