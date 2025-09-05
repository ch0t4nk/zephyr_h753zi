#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include "l6470.h"
#include "status_poll.h"

LOG_MODULE_REGISTER(stepper_poll, LOG_LEVEL_INF);

#if defined(CONFIG_STEPPER_STATUS_POLL)

#define RING_DEPTH 32
static struct {
    uint16_t buf[RING_DEPTH];
    uint8_t head;
    uint8_t count;
} g_status_ring[L6470_DAISY_SIZE];

static struct k_work_delayable poll_work;
static bool g_poll_enabled = true;

static void ring_push(uint8_t dev, uint16_t st)
{
    struct { uint16_t *b; uint8_t *h; uint8_t *c; } r = { g_status_ring[dev].buf, &g_status_ring[dev].head, &g_status_ring[dev].count };
    r.b[*r.h] = st;
    *r.h = (uint8_t)((*r.h + 1) % RING_DEPTH);
    if (*r.c < RING_DEPTH) (*r.c)++;
}

static void poll_fn(struct k_work *work)
{
    ARG_UNUSED(work);
    if (g_poll_enabled) {
        uint16_t st[L6470_DAISY_SIZE] = {0};
        if (l6470_get_status_all(st) == 0) {
            for (uint8_t d = 0; d < L6470_DAISY_SIZE; d++) {
                ring_push(d, st[d]);
            }
        }
    }
    k_work_reschedule(&poll_work, K_MSEC(CONFIG_STEPPER_STATUS_POLL_PERIOD_MS));
}

static int poll_init(const struct device *unused)
{
    ARG_UNUSED(unused);
    k_work_init_delayable(&poll_work, poll_fn);
    k_work_schedule(&poll_work, K_MSEC(CONFIG_STEPPER_STATUS_POLL_PERIOD_MS));
    return 0;
}

SYS_INIT(poll_init, APPLICATION, 50);

/* TODO: expose shell commands to enable/disable and dump ring buffers */

#endif /* CONFIG_STEPPER_STATUS_POLL */

#if defined(CONFIG_STEPPER_STATUS_POLL)
void stepper_poll_set_enabled(bool en) { g_poll_enabled = en; }
bool stepper_poll_is_enabled(void) { return g_poll_enabled; }
uint8_t stepper_poll_get_recent(uint8_t dev, uint8_t n, uint16_t *out)
{
    if (dev >= L6470_DAISY_SIZE || !out || n == 0) return 0;
    uint8_t count = g_status_ring[dev].count;
    if (n > count) n = count;
    /* Pull most-recent first: head points to next write position */
    int idx = (int)g_status_ring[dev].head - 1;
    if (idx < 0) idx += RING_DEPTH;
    uint8_t wrote = 0;
    while (wrote < n) {
        out[wrote++] = g_status_ring[dev].buf[idx];
        idx = (idx - 1);
        if (idx < 0) idx += RING_DEPTH;
    }
    return wrote;
}
#endif

